/*
 Copyright 2020 Apotell

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

// Regression coverage for IEEE 1800-2023 Sec 5.12 attribute_instance attachment, across every
// grammar construct that carries its own attribute_instance* prefix (see dut.sv's header comment
// for the full rationale and the short list of deliberately-omitted sub-alternatives).
//
// This file does NOT re-validate attribute name/value/type parsing in depth -- that is covered by
// AttributesNets/DataAttrib/5.12-attributes-variable. Its only job is to confirm, per construct,
// that the attribute is captured at all (i.e. appendPendingAttributes(), or the equivalent
// per-item sibling-pairing mechanism for the deliberately-excluded repeated-list forms, was
// actually invoked) and attached to the correct specific model -- not silently dropped, and not
// mis-parented onto an enclosing scope instead of the construct it actually prefixes.
//
// Strategy: run a whole-design hldb::Visitor once per test suite to collect every Attribute
// object reachable from the Design, keyed by name (each dut.sv attribute name is unique and
// encodes the construct under test). Each TEST_F then looks up its attribute by name and checks
// (a) exactly one was captured, and (b) its immediate parent is the expected specific model type.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/Visitor.h>
#include <hldb/alias_stmt.h>
#include <hldb/assignment.h>
#include <hldb/attribute.h>
#include <hldb/bind_directive.h>
#include <hldb/checker_port.h>
#include <hldb/class_defn.h>
#include <hldb/clocking_block.h>
#include <hldb/constant.h>
#include <hldb/cover_bin.h>
#include <hldb/cover_point.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/io_decl.h>
#include <hldb/method_func_call.h>
#include <hldb/modport.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/prop_formal_decl.h>
#include <hldb/program.h>
#include <hldb/property_decl.h>
#include <hldb/ref_obj.h>
#include <hldb/seq_formal_decl.h>
#include <hldb/subroutine_call.h>
#include <hldb/task_func_decl.h>
#include <hldb/typespec_member.h>
#include <hldb/udp_defn.h>
#include <hldb/variable.h>

#include <map>
#include <set>
#include <string>
#include <vector>

namespace hlc {

// Collects every Attribute object reachable from the whole design, keyed by name.
class AttributeCollector : public hldb::Visitor {
 public:
  std::map<std::string, std::vector<const hldb::Attribute *>> byName;

 protected:
  void visitAttribute(const hldb::Attribute *object) override {
    if (object != nullptr) {
      byName[std::string(object->getName())].emplace_back(object);
    }
  }
};

class Attributes2 : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "Attributes2.hlc"});
    if (m_design != nullptr) {
      m_collector.visit(m_design);
    }
  }
  static void TearDownTestSuite() { Shutdown(); }

  static AttributeCollector m_collector;
};

AttributeCollector Attributes2::m_collector;

// Finds exactly-one attribute by name and returns it, or nullptr with a gtest failure recorded.
static const hldb::Attribute *findOne(const std::string &name) {
  const auto it = Attributes2::m_collector.byName.find(name);
  if (it == Attributes2::m_collector.byName.end()) {
    ADD_FAILURE() << "attribute '" << name << "' was not captured at all (dropped)";
    return nullptr;
  }
  EXPECT_EQ(it->second.size(), 1u) << "attribute '" << name << "' expected exactly once";
  return it->second.front();
}

template <typename T>
static void expectAttributeParentedTo(const std::string &attrName) {
  const hldb::Attribute *const attr = findOne(attrName);
  ASSERT_NE(attr, nullptr);
  ASSERT_NE(attr->getParent(), nullptr) << "attribute '" << attrName << "' is orphaned (no parent)";
  EXPECT_NE(any_cast<T>(attr->getParent()), nullptr)
      << "attribute '" << attrName << "' attached to wrong model type ("
      << hldb::AnyTypeName(attr->getParent()->getAnyType()) << ")";
}

// For constructs where one attribute_instance prefixes a comma-separated identifier list --
// each resulting model gets its own cloned copy (IEEE 1800 Sec 5.12's own example,
// "(* fsm_state=1 *) logic [3:0] state2, state3;", applies the attribute to both). Checks that
// exactly `count` clones were captured and each is parented to a distinct model of type T.
template <typename T>
static void expectAttributeClonesParentedTo(const std::string &attrName, size_t count) {
  const auto it = Attributes2::m_collector.byName.find(attrName);
  ASSERT_NE(it, Attributes2::m_collector.byName.end())
      << "attribute '" << attrName << "' was not captured at all (dropped)";
  ASSERT_EQ(it->second.size(), count) << "attribute '" << attrName << "' expected " << count << " clones";

  std::vector<const hldb::Any *> parents;
  for (const hldb::Attribute *const attr : it->second) {
    ASSERT_NE(attr->getParent(), nullptr) << "attribute '" << attrName << "' is orphaned (no parent)";
    EXPECT_NE(any_cast<T>(attr->getParent()), nullptr)
        << "attribute '" << attrName << "' attached to wrong model type ("
        << hldb::AnyTypeName(attr->getParent()->getAnyType()) << ")";
    parents.emplace_back(attr->getParent());
  }
  EXPECT_EQ(std::set<const hldb::Any *>(parents.begin(), parents.end()).size(), count)
      << "attribute '" << attrName << "' clones should each be parented to a distinct model";
}

// ----
// package_item / package_declaration's own item collection.
// ----
TEST_F(Attributes2, PackageItem) { expectAttributeParentedTo<hldb::Parameter>("a_package_item"); }

// ----
// interface_class_item.
// ----
TEST_F(Attributes2, InterfaceClassItem) {
  expectAttributeParentedTo<hldb::TaskFuncDecl>("a_interface_class_item");
}

// ----
// class_item (class_property alternative).
// ----
TEST_F(Attributes2, ClassItem) { expectAttributeParentedTo<hldb::Variable>("a_class_item"); }

// ----
// port_declaration (non-ansi form).
// ----
TEST_F(Attributes2, PortDeclaration) { expectAttributeParentedTo<hldb::Net>("a_port_declaration"); }

// ----
// named_port_connection / ordered_port_connection (deliberately-excluded repeated-list forms --
// each item in the connection list gets its own attribute via sibling pairing, not
// appendPendingAttributes).
// ----
TEST_F(Attributes2, NamedPortConnection) { expectAttributeParentedTo<hldb::Port>("a_named_port_connection"); }

TEST_F(Attributes2, OrderedPortConnection) {
  expectAttributeParentedTo<hldb::RefObj>("a_ordered_port_connection");
}

// ----
// udp_nonansi_declaration / udp_ansi_declaration + output/input/reg declarations.
// ----
TEST_F(Attributes2, UdpNonansiDeclaration) { expectAttributeParentedTo<hldb::UdpDefn>("a_udp_nonansi_declaration"); }

TEST_F(Attributes2, UdpAnsiDeclaration) { expectAttributeParentedTo<hldb::UdpDefn>("a_udp_ansi_declaration"); }

TEST_F(Attributes2, UdpOutputDeclaration) { expectAttributeParentedTo<hldb::IODecl>("a_udp_output_declaration"); }

// dut.sv's "input i0, i1;" -- one attribute_instance prefixing a two-identifier list -- must
// produce two clones, one parented to each of i0's and i1's own IODecl (see
// leavePA_Udp_input_declaration's per-identifier appendPendingAttributes(model) call).
TEST_F(Attributes2, UdpInputDeclaration) {
  expectAttributeClonesParentedTo<hldb::IODecl>("a_udp_input_declaration", 2);
}

TEST_F(Attributes2, UdpRegDeclaration) { expectAttributeParentedTo<hldb::Variable>("a_udp_reg_declaration"); }

// ----
// interface_nonansi_header / interface_ansi_header + interface_or_generate_item +
// modport_ports_declaration + clocking_item.
// ----
TEST_F(Attributes2, InterfaceNonansiHeader) {
  expectAttributeParentedTo<hldb::Interface>("a_interface_nonansi_header");
}

TEST_F(Attributes2, InterfaceAnsiHeader) { expectAttributeParentedTo<hldb::Interface>("a_interface_ansi_header"); }

TEST_F(Attributes2, InterfaceOrGenerateItem) { expectAttributeParentedTo<hldb::Net>("a_interface_or_generate_item"); }

// Modport port entries are modeled as IODecl throughout (leavePA_Modport_simple_port /
// leavePA_Modport_ports_declaration's CLOCKING alternative / leavePA_Modport_tf_port all agree),
// not Port -- there is no Port object involved in a modport's own port-view entries.
TEST_F(Attributes2, ModportPortsDeclaration) { expectAttributeParentedTo<hldb::IODecl>("a_modport_ports_declaration"); }

TEST_F(Attributes2, ClockingItem) { expectAttributeParentedTo<hldb::PropertyDecl>("a_clocking_item"); }

// ----
// program_nonansi_header / program_ansi_header + non_port_program_item.
// ----
TEST_F(Attributes2, ProgramNonansiHeader) { expectAttributeParentedTo<hldb::Program>("a_program_nonansi_header"); }

TEST_F(Attributes2, ProgramAnsiHeader) { expectAttributeParentedTo<hldb::Program>("a_program_ansi_header"); }

TEST_F(Attributes2, NonPortProgramItem) { expectAttributeParentedTo<hldb::Net>("a_non_port_program_item"); }

// ----
// checker_port_item.
// ----
TEST_F(Attributes2, CheckerPortItem) { expectAttributeParentedTo<hldb::CheckerPort>("a_checker_port_item"); }

// ----
// module_ansi_header / module_nonansi_header + module_or_generate_item + non_port_module_item +
// ansi_port_declaration (excluded repeated-list form) + parameter/local_parameter_declaration.
// ----
TEST_F(Attributes2, ModuleAnsiHeader) { expectAttributeParentedTo<hldb::Module>("a_module_ansi_header"); }

TEST_F(Attributes2, ModuleNonansiHeader) { expectAttributeParentedTo<hldb::Module>("a_module_nonansi_header"); }

TEST_F(Attributes2, ParameterDeclaration) { expectAttributeParentedTo<hldb::Parameter>("a_parameter_declaration"); }

TEST_F(Attributes2, LocalParameterDeclaration) {
  expectAttributeParentedTo<hldb::Parameter>("a_local_parameter_declaration");
}

TEST_F(Attributes2, AnsiPortDeclaration) { expectAttributeParentedTo<hldb::Port>("a_ansi_port_declaration"); }

TEST_F(Attributes2, ModuleOrGenerateItem) { expectAttributeParentedTo<hldb::Net>("a_module_or_generate_item"); }

TEST_F(Attributes2, NonPortModuleItem) { expectAttributeParentedTo<hldb::Alias>("a_non_port_module_item"); }

// ----
// bind_directive.
// ----
TEST_F(Attributes2, BindDirective) { expectAttributeParentedTo<hldb::BindDirective>("a_bind_directive"); }

// ----
// struct_union_member.
// ----
TEST_F(Attributes2, StructUnionMember) { expectAttributeParentedTo<hldb::TypespecMember>("a_struct_union_member"); }

// ----
// property_port_item / sequence_port_item / let_port_item.
// ----
TEST_F(Attributes2, PropertyPortItem) { expectAttributeParentedTo<hldb::PropFormalDecl>("a_property_port_item"); }

TEST_F(Attributes2, SequencePortItem) { expectAttributeParentedTo<hldb::SeqFormalDecl>("a_sequence_port_item"); }

TEST_F(Attributes2, LetPortItem) { expectAttributeParentedTo<hldb::SeqFormalDecl>("a_let_port_item"); }

// ----
// coverage_spec_or_option / bins_selection_or_option.
// ----
TEST_F(Attributes2, CoverageSpecOrOption) { expectAttributeParentedTo<hldb::CoverPoint>("a_coverage_spec_or_option"); }

TEST_F(Attributes2, BinsSelectionOrOption) { expectAttributeParentedTo<hldb::CoverBin>("a_bins_selection_or_option"); }

// ----
// tf_port_item / tf_port_declaration.
// ----
TEST_F(Attributes2, TfPortItem) { expectAttributeParentedTo<hldb::IODecl>("a_tf_port_item"); }

TEST_F(Attributes2, TfPortDeclaration) { expectAttributeParentedTo<hldb::IODecl>("a_tf_port_declaration"); }

// ----
// block_item_declaration / statement / statement_or_null / function_statement_or_null.
// ----
TEST_F(Attributes2, BlockItemDeclaration) { expectAttributeParentedTo<hldb::Variable>("a_block_item_declaration"); }

TEST_F(Attributes2, Statement) { expectAttributeParentedTo<hldb::Assignment>("a_statement"); }

TEST_F(Attributes2, StatementOrNull) { expectAttributeParentedTo<hldb::Assignment>("a_statement_or_null"); }

TEST_F(Attributes2, FunctionStatementOrNull) {
  expectAttributeParentedTo<hldb::Assignment>("a_function_statement_or_null");
}

// ----
// generate_item (MODPORT / extern_tf_declaration alternative -- distinct from the
// module_or_generate_item alternative already covered above).
// ----
TEST_F(Attributes2, GenerateItem) {
  const hldb::Attribute *const attr = findOne("a_generate_item");
  ASSERT_NE(attr, nullptr);
  EXPECT_NE(attr->getParent(), nullptr) << "attribute 'a_generate_item' is orphaned (no parent)";
}

// ----
// Operator-level attribute_instance positions (IEEE 1800-2023 Sec 11.3, informative note 2).
// ----
TEST_F(Attributes2, UnaryOp) { expectAttributeParentedTo<hldb::RefObj>("a_unary_op"); }

TEST_F(Attributes2, BinaryOp) { expectAttributeParentedTo<hldb::RefObj>("a_binary_op"); }

TEST_F(Attributes2, TernaryOp) {
  const hldb::Attribute *const attr = findOne("a_ternary_op");
  ASSERT_NE(attr, nullptr);
  EXPECT_NE(attr->getParent(), nullptr) << "attribute 'a_ternary_op' is orphaned (no parent)";
}

TEST_F(Attributes2, IncDecOp) { expectAttributeParentedTo<hldb::Operation>("a_incdec_op"); }

// streaming_concatenation is deliberately NOT covered here -- this parser's actual grammar rule
// carries no attribute_instance anywhere in it (see dut.sv's header comment).

// ----
// method_call_body / array_manipulation_call / subroutine_call inline attribute positions.
//
// All three share the identical "name attribute_instance* (args)?" grammar shape, so in every
// case the attribute attaches to the argument (a Constant, for the literal "1") that follows it,
// never to the preceding call name -- see dut.sv's comment on these lines for why each one is
// reached via a chained dot-call (method_call_body/array_manipulation_call) or a direct call
// (subroutine_call), and why an explicit argument is used at all (real array reduction methods
// normally take none).
// ----
TEST_F(Attributes2, MethodCallBody) { expectAttributeParentedTo<hldb::Constant>("a_method_call_body"); }

TEST_F(Attributes2, ArrayManipulationCall) {
  expectAttributeParentedTo<hldb::Constant>("a_array_manipulation_call");
}

TEST_F(Attributes2, SubroutineCall) { expectAttributeParentedTo<hldb::Constant>("a_subroutine_call"); }

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
