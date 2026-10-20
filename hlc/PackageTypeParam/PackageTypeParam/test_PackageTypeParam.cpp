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

// Validates the HLDB model built for tests/PackageTypeParam/dut.sv:
//
//   package new_package;
//   typedef struct packed {
//       logic [128:0] a;
//       logic [4:0]   b;
//       logic         c;
//       logic         d;
//       logic         e;
//   } zzz;
//   endpackage : new_package;
//
//   module module_a
//     #(
//       parameter type TYPE_PARAMETER = new_package::zzz
//       )
//     (
//      input TYPE_PARAMETER input_struct,
//      output TYPE_PARAMETER output_struct,
//      input clk
//      );
//     always_ff @(posedge clk) begin
//       output_struct <= input_struct;
//     end
//   endmodule : module_a
//
// The point of the fixture is a type parameter (IEEE 1800-2023 6.20.3) whose
// default is a typedef named with the package scope resolution operator
// (26.3), used as the data type of two ports. The regression this file exists
// to catch is HLC failing to resolve the package-scoped default type, or
// failing to give the ports that type.
//
// What is checked, and why:
//   Package new_package (26.2)
//     - exists, with the end label "new_package". The ';' after the label is
//       a legal empty package_item at compilation-unit scope (A.1.2)
//     - exactly 1 Typedef, 'zzz', whose alias is a packed StructTypespec
//       (7.2.1) with exactly 5 members in source order: a (logic [128:0]),
//       b (logic [4:0]), and c, d and e (logic with no packed range)
//   Module module_a (23.2)
//     - end label "module_a"
//     - exactly 1 parameter, the TypeParameter TYPE_PARAMETER. It is declared
//       in the parameter port list, so it is not a local parameter (6.20.1)
//     - its default type is the package-scoped name new_package::zzz: a
//       RefTypespec path whose prefix is bound to package new_package and
//       whose last element, zzz, resolves to new_package's typedef
//     - exactly 3 ports, in order: input_struct (input), output_struct
//       (output), clk (input), with port indexes 0, 1 and 2: vpiPortIndex
//       gives the port order and the first port has index 0 (37.14 detail 9)
//     - port kinds follow 23.2.2.3 for ANSI ports with no port kind:
//       input_struct is an input, so it is a net; output_struct is an output
//       whose data type is written with the explicit data_type syntax, so it
//       is a variable; clk is an input with no data type, so it is a net. The
//       module has exactly 2 Nets (input_struct, clk) and exactly 1 Variable
//       (output_struct), each the low connection of its port, and every net
//       is of the default net type wire (22.8)
//     - input_struct and output_struct are typed by TYPE_PARAMETER: each
//       RefTypespec resolves to the module's TypeParameter
//   always_ff @(posedge clk) begin output_struct <= input_struct; end
//     - exactly 1 process, an Always of type vpiAlwaysFF (9.2.2.4)
//     - its statement is an event control (9.4.2) whose condition is
//       Operation vpiPosedgeOp over RefObj 'clk', bound to the clk net
//     - the controlled statement is a begin-end holding exactly 1
//       nonblocking Assignment (10.4.2), LHS bound to the variable
//       output_struct and RHS bound to the net input_struct
//   Elaboration (23.3.1)
//     - module_a appears in no instantiation, so on an elaborated design it
//       is the only top-level instance, named "module_a"
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: nothing reduces. The type parameter's value is
// its default because module_a is never instantiated with an override; only
// the instance tree is checked under getElaborated().
//
// KNOWN COMPILER BUG (port index not set), not a defect in this test:
// HLC leaves vpiPortIndex at 0 for every port.
// 37.14 detail 9 says the port index gives the port order and the first port
// has index 0, so output_struct is 1 and clk is 2.
// PortIndexesFollowDeclarationOrder is expected to fail until HLC is fixed; it is
// intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - The name HLC gives the module definition: it appends the parameter
//     values ("module_a #(...)"), so the definition is looked up by its
//     definition name instead.
//   - Where HLC stores the type parameter's default (on the TypeParameter or
//     as the RHS of a ParamAssign) is a model convention; both are searched.
//   - Whether the package element of a scoped type path is a RefObj or a
//     RefTypespec wrapping one is a model convention; either is accepted.
//   - The width of zzz (129 + 5 + 1 + 1 + 1 = 137 bits). Nothing in the
//     source computes it, so there is no expression whose value could be
//     asserted.
//   - What output_struct holds after each clock edge only exists while
//     simulation runs. Permanently out of scope; the static half, which
//     declarations the assignment reads and writes, is covered by
//     AlwaysFFBodyIsNonblockingCopy.
//   - How the path's last element refers to zzz: it may resolve to the
//     TypedefTypespec or to the StructTypespec zzz aliases. Both are that
//     type (6.18), so either is accepted.
//   - HLC represents a package-scoped type name as a RefTypespec path (the
//     package, then the type). That shape is a model convention; the test
//     follows it and asserts both the package and the type binding.
//   - Whether other packages (for example a built-in one) also appear in
//     Design::getAllPackages() is a tool convention, so the package count is
//     not asserted; the package is looked up by name.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/type_parameter.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class PackageTypeParamTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackageTypeParam.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // The reference to the package in the first element of a package-scoped
  // type path. HLC records it either as a RefObj or as a RefTypespec whose own
  // path holds that RefObj; which one is a model convention.
  static const hldb::RefObj *getScopePackageRef(const hldb::Any *elem) {
    if (const hldb::RefObj *const ref = any_cast<hldb::RefObj>(elem)) return ref;
    const hldb::RefTypespec *const rts = any_cast<hldb::RefTypespec>(elem);
    if (rts == nullptr || rts->getPathElems() == nullptr || rts->getPathElems()->empty()) return nullptr;
    return any_cast<hldb::RefObj>(rts->getPathElems()->at(0));
  }

  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("new_package", m_design->getAllPackages());
  }

  static const hldb::Typedef *getZzz() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("zzz", pkg->getTypedefs());
  }

  // The module definition, found by its definition name: HLC names a
  // parameterized definition after its parameter values as well.
  static const hldb::Module *getModuleA() {
    return hldb::findByDefName<hldb::Module>("module_a", m_design->getAllModules());
  }

  // The default type of TYPE_PARAMETER. HLC records it either on the
  // TypeParameter itself or as the RHS of the module's ParamAssign for it;
  // which one is a model convention.
  static const hldb::RefTypespec *getTypeParameterDefault() {
    const hldb::TypeParameter *const tp = getTypeParameter();
    if (tp == nullptr) return nullptr;
    if (tp->getExpr() != nullptr) return tp->getExpr();
    const hldb::Module *const m = getModuleA();
    if (m == nullptr || m->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *m->getParamAssigns()) {
      const hldb::RefTypespec *const lhs = pa->getLhs<hldb::RefTypespec>();
      if ((lhs != nullptr) && (lhs->getActual() == tp)) return pa->getRhs<hldb::RefTypespec>();
    }
    return nullptr;
  }

  static const hldb::TypeParameter *getTypeParameter() {
    const hldb::Module *const m = getModuleA();
    if (m == nullptr) return nullptr;
    return hldb::findByName<hldb::TypeParameter>("TYPE_PARAMETER", m->getParameters());
  }

  static const hldb::Net *getNet(std::string_view name) {
    const hldb::Module *const m = getModuleA();
    if (m == nullptr) return nullptr;
    return hldb::findByName<hldb::Net>(name, m->getNets());
  }

  static const hldb::Variable *getVariable(std::string_view name) {
    const hldb::Module *const m = getModuleA();
    if (m == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>(name, m->getVariables());
  }

  static const hldb::Always *getAlways() {
    const hldb::Module *const m = getModuleA();
    if (m == nullptr || m->getProcesses() == nullptr || m->getProcesses()->empty()) return nullptr;
    return any_cast<hldb::Always>(m->getProcesses()->at(0));
  }

  // Verifies the port at 'index' is named 'name', has direction 'direction'
  // and is connected below to 'decl'.
  static void ExpectPort(size_t index, std::string_view name, int32_t direction, const hldb::Any *decl) {
    const hldb::Module *const m = getModuleA();
    ASSERT_NE(m, nullptr);
    ASSERT_NE(m->getPorts(), nullptr);
    ASSERT_GT(m->getPorts()->size(), index);
    const hldb::Port *const port = m->getPorts()->at(index);
    ASSERT_NE(port, nullptr);
    EXPECT_EQ(port->getName(), name) << "port " << index;
    EXPECT_EQ(port->getDirection(), direction) << "'" << name << "'";
    const hldb::RefObj *const low = port->getLowConn<hldb::RefObj>();
    ASSERT_NE(low, nullptr) << "'" << name << "' has a low connection";
    ASSERT_NE(decl, nullptr) << "the declaration of '" << name << "' was not found";
    EXPECT_EQ(low->getActual(), decl) << "37.14 detail 4: '" << name << "' connects to its own declaration";
  }

  // Verifies 'rts' resolves to the module's TypeParameter.
  static void ExpectTypedByTypeParameter(const hldb::RefTypespec *rts, std::string_view what) {
    ASSERT_NE(rts, nullptr) << what << " has no typespec";
    ASSERT_NE(getTypeParameter(), nullptr);
    EXPECT_EQ(rts->getActual(), getTypeParameter()) << what << " is declared with the type TYPE_PARAMETER";
  }
};

// ---------------------------------------------------------------------------
// package new_package; typedef struct packed { ... } zzz; endpackage
// ---------------------------------------------------------------------------

TEST_F(PackageTypeParamTest, PackageExistsWithEndLabel) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr) << "package 'new_package' not found";
  EXPECT_EQ(pkg->getEndLabel(), "new_package") << "'endpackage : new_package' carries an end label";
}

TEST_F(PackageTypeParamTest, PackageDeclaresOneTypedefZzz) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getTypedefs(), nullptr);
  EXPECT_EQ(pkg->getTypedefs()->size(), 1u) << "'zzz' is the package's only typedef";
  const hldb::Typedef *const zzz = getZzz();
  ASSERT_NE(zzz, nullptr) << "typedef 'zzz' not found";
  ASSERT_NE(zzz->getAlias(), nullptr);
  const hldb::StructTypespec *const st = zzz->getAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr) << "6.18: 'zzz' names a structure type";
  ASSERT_NE(st->getStruct(), nullptr);
  EXPECT_TRUE(st->getStruct()->getPacked()) << "7.2.1: declared 'struct packed'";
}

TEST_F(PackageTypeParamTest, ZzzHasFiveLogicMembersInOrder) {
  const hldb::Typedef *const zzz = getZzz();
  ASSERT_NE(zzz, nullptr);
  ASSERT_NE(zzz->getAlias(), nullptr);
  const hldb::StructTypespec *const st = zzz->getAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr);
  ASSERT_NE(st->getStruct(), nullptr);
  const hldb::TypespecMemberCollection *const members = st->getStruct()->getMembers();
  ASSERT_NE(members, nullptr);
  ASSERT_EQ(members->size(), 5u);
  struct MemberShape final {
    std::string_view m_name;
    std::string_view m_left;  // empty: no packed range
  };
  const MemberShape shapes[] = {{"a", "128"}, {"b", "4"}, {"c", ""}, {"d", ""}, {"e", ""}};
  for (size_t i = 0; i < 5; ++i) {
    const hldb::TypespecMember *const m = members->at(i);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->getName(), shapes[i].m_name) << "member " << i << ", in source order";
    ASSERT_NE(m->getTypespec(), nullptr);
    const hldb::LogicTypespec *const lt = m->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr) << "'" << shapes[i].m_name << "' is declared 'logic'";
    if (shapes[i].m_left.empty()) {
      EXPECT_TRUE(lt->getRanges() == nullptr || lt->getRanges()->empty())
          << "'" << shapes[i].m_name << "' has no packed dimension";
    } else {
      ASSERT_NE(lt->getRanges(), nullptr);
      ASSERT_EQ(lt->getRanges()->size(), 1u) << "'" << shapes[i].m_name << "' has one packed dimension";
      const hldb::Range *const r = lt->getRanges()->at(0);
      ASSERT_NE(r, nullptr);
      const hldb::Constant *const left = r->getLeftExpr<hldb::Constant>();
      const hldb::Constant *const right = r->getRightExpr<hldb::Constant>();
      ASSERT_NE(left, nullptr);
      ASSERT_NE(right, nullptr);
      EXPECT_EQ(left->getDecompile(), shapes[i].m_left) << "'" << shapes[i].m_name << "'";
      EXPECT_EQ(right->getDecompile(), "0") << "'" << shapes[i].m_name << "'";
    }
  }
}

// ---------------------------------------------------------------------------
// module module_a #(parameter type TYPE_PARAMETER = new_package::zzz) (...);
// ---------------------------------------------------------------------------

TEST_F(PackageTypeParamTest, ModuleAExistsWithEndLabel) {
  const hldb::Module *const m = getModuleA();
  ASSERT_NE(m, nullptr) << "module 'module_a' not found";
  EXPECT_EQ(m->getEndLabel(), "module_a") << "'endmodule : module_a' carries an end label";
}

TEST_F(PackageTypeParamTest, ModuleAHasOneTypeParameterThatIsNotLocal) {
  const hldb::Module *const m = getModuleA();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 1u) << "TYPE_PARAMETER is module_a's only parameter";
  const hldb::TypeParameter *const tp = getTypeParameter();
  ASSERT_NE(tp, nullptr) << "6.20.3: 'parameter type TYPE_PARAMETER' declares a type parameter";
  EXPECT_FALSE(tp->getLocalParam()) << "6.20.1: a parameter in the parameter port list can be overridden";
}

TEST_F(PackageTypeParamTest, TypeParameterDefaultIsNewPackageZzz) {
  const hldb::TypeParameter *const tp = getTypeParameter();
  ASSERT_NE(tp, nullptr);
  const hldb::RefTypespec *const def = getTypeParameterDefault();
  ASSERT_NE(def, nullptr) << "6.20.3: TYPE_PARAMETER has the default type new_package::zzz";
  ASSERT_NE(def->getPathElems(), nullptr) << "'new_package::zzz' should be a package-scoped path";
  ASSERT_EQ(def->getPathElems()->size(), 2u) << "the package, then the type";
  const hldb::RefObj *const pkgRef = getScopePackageRef(def->getPathElems()->at(0));
  ASSERT_NE(pkgRef, nullptr) << "the path starts with the package";
  EXPECT_EQ(pkgRef->getName(), "new_package");
  ASSERT_NE(getPkg(), nullptr);
  EXPECT_EQ(pkgRef->getActual<hldb::Package>(), getPkg()) << "26.3: the scope prefix names package new_package";

  const hldb::RefTypespec *const type = any_cast<hldb::RefTypespec>(def->getPathElems()->at(1));
  ASSERT_NE(type, nullptr);
  EXPECT_EQ(type->getName(), "zzz");
  const hldb::Typedef *const zzz = getZzz();
  ASSERT_NE(zzz, nullptr);
  ASSERT_NE(zzz->getAlias(), nullptr);
  const hldb::Typespec *const actual = type->getActual();
  ASSERT_NE(actual, nullptr) << "26.3: 'zzz' is declared in new_package, so the scoped type name must resolve";
  const hldb::TypedefTypespec *const viaTypedef = any_cast<hldb::TypedefTypespec>(actual);
  const bool isZzz =
      ((viaTypedef != nullptr) && (viaTypedef->getTypedef() == zzz)) || (actual == zzz->getAlias()->getActual());
  EXPECT_TRUE(isZzz) << "26.3: the default type is new_package's zzz";
}

TEST_F(PackageTypeParamTest, ModuleAHasThreePortsInOrder) {
  const hldb::Module *const m = getModuleA();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getPorts(), nullptr);
  ASSERT_EQ(m->getPorts()->size(), 3u);
  ExpectPort(0, "input_struct", vpiInput, getNet("input_struct"));
  ExpectPort(1, "output_struct", vpiOutput, getVariable("output_struct"));
  ExpectPort(2, "clk", vpiInput, getNet("clk"));
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG
// (port index not set) in the file header.
TEST_F(PackageTypeParamTest, PortIndexesFollowDeclarationOrder) {
  const hldb::Module *const m = getModuleA();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getPorts(), nullptr);
  ASSERT_EQ(m->getPorts()->size(), 3u);
  for (size_t i = 0; i < m->getPorts()->size(); ++i) {
    ASSERT_NE(m->getPorts()->at(i), nullptr);
    EXPECT_EQ(m->getPorts()->at(i)->getPortIndex(), static_cast<int32_t>(i))
        << "37.14 detail 9: vpiPortIndex gives the port order, and the first port has index 0; port '"
        << m->getPorts()->at(i)->getName() << "'";
  }
}

TEST_F(PackageTypeParamTest, PortKindsFollowTheAnsiDefaults) {
  const hldb::Module *const m = getModuleA();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr);
  EXPECT_EQ(m->getNets()->size(), 2u) << "23.2.2.3: the two input ports are nets";
  ASSERT_NE(getNet("input_struct"), nullptr) << "23.2.2.3: an input port with no port kind is a net";
  ASSERT_NE(getNet("clk"), nullptr) << "23.2.2.3: an input port with no port kind is a net";
  EXPECT_EQ(getNet("input_struct")->getNetType(), vpiWire) << "22.8: the default net type is wire";
  EXPECT_EQ(getNet("clk")->getNetType(), vpiWire) << "22.8: the default net type is wire";
  ASSERT_NE(m->getVariables(), nullptr);
  EXPECT_EQ(m->getVariables()->size(), 1u) << "23.2.2.3: only the output port with an explicit data type is a variable";
  EXPECT_NE(getVariable("output_struct"), nullptr)
      << "23.2.2.3: an output port whose data type uses the explicit data_type syntax is a variable";
}

TEST_F(PackageTypeParamTest, StructPortsAreTypedByTypeParameter) {
  ASSERT_NE(getNet("input_struct"), nullptr);
  ASSERT_NE(getVariable("output_struct"), nullptr);
  ExpectTypedByTypeParameter(getNet("input_struct")->getTypespec(), "'input_struct'");
  ExpectTypedByTypeParameter(getVariable("output_struct")->getTypespec(), "'output_struct'");
}

// ---------------------------------------------------------------------------
// always_ff @(posedge clk) begin output_struct <= input_struct; end
// ---------------------------------------------------------------------------

TEST_F(PackageTypeParamTest, ModuleAHasOneAlwaysFF) {
  const hldb::Module *const m = getModuleA();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getProcesses(), nullptr);
  ASSERT_EQ(m->getProcesses()->size(), 1u);
  const hldb::Always *const always = getAlways();
  ASSERT_NE(always, nullptr) << "the only process is an always procedure";
  EXPECT_EQ(always->getAlwaysType(), vpiAlwaysFF) << "9.2.2.4: declared 'always_ff'";
}

TEST_F(PackageTypeParamTest, AlwaysFFIsTriggeredByPosedgeClk) {
  const hldb::Always *const always = getAlways();
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr) << "9.4.2: '@(posedge clk)' is an event control";
  const hldb::Operation *const edge = ec->getCondition<hldb::Operation>();
  ASSERT_NE(edge, nullptr) << "'posedge clk' is an Operation";
  EXPECT_EQ(edge->getOpType(), vpiPosedgeOp);
  ASSERT_NE(edge->getOperands(), nullptr);
  ASSERT_EQ(edge->getOperands()->size(), 1u);
  const hldb::RefObj *const clk = any_cast<hldb::RefObj>(edge->getOperands()->at(0));
  ASSERT_NE(clk, nullptr);
  EXPECT_EQ(clk->getName(), "clk");
  ASSERT_NE(getNet("clk"), nullptr);
  EXPECT_EQ(clk->getActual(), getNet("clk"));
}

TEST_F(PackageTypeParamTest, AlwaysFFBodyIsNonblockingCopy) {
  const hldb::Always *const always = getAlways();
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  const hldb::Begin *const body = ec->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "the controlled statement is a begin-end";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_FALSE(assign->getBlocking()) << "10.4.2: '<=' is a nonblocking assignment";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "output_struct");
  ASSERT_NE(getVariable("output_struct"), nullptr);
  EXPECT_EQ(lhs->getActual(), getVariable("output_struct"));
  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "input_struct");
  ASSERT_NE(getNet("input_struct"), nullptr);
  EXPECT_EQ(rhs->getActual(), getNet("input_struct"));
}

// ---------------------------------------------------------------------------
// Elaboration
// ---------------------------------------------------------------------------

TEST_F(PackageTypeParamTest, ModuleAIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: 'module_a' appears in no instantiation";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getName(), "module_a")
        << "23.3.1: a top-level instance is named after its module";
  }
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

TEST_F(PackageTypeParamTest, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "the file is legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
