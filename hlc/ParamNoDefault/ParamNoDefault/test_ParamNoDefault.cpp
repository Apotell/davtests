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

// Validates the HLDB model built for tests/ParamNoDefault/dut.sv:
//
//   module dut();
//      parameter logic [4:0] A;
//
//   endmodule // dut
//
//   module top();
//      typedef struct packed {
//         logic [1:0] a;
//
//      } struct_1;
//
//      parameter struct_1 X = '{a: '1, b: '0};
//
//      dut #(.A(X)) u_dut();
//   endmodule
//
// The fixture holds two illegal constructs, and the regression this file
// exists to catch is HLC accepting either of them silently:
//   - 'parameter logic [4:0] A;' in a module body has no value. Syntax 6-6
//     footnote 22: "It shall be legal to omit the constant_param_expression
//     from a param_assignment or the data_type from a type_assignment only
//     within a parameter_port_list." dut has no parameter port list, so the
//     omission is illegal even though u_dut overrides A.
//   - '{a: '1, b: '0} keys a member 'b' that struct_1 does not have. 10.9.2:
//     "A member:value specifies an explicit value for a named member of the
//     structure. The named member shall be at the top level of the
//     structure".
//
// What is checked, and why:
//   Module dut and module top
//     - each '()' declares one null port (37.14 detail 10, "module M();")
//   Module dut
//     - exactly 1 parameter, A, overridable (dut has no parameter port list,
//       6.20.1), of type logic [4:0], with no value
//   Module top
//     - struct_1 is a packed structure (7.2.1) with exactly 1 member, a, of
//       type logic [1:0]
//     - X is an overridable parameter of type struct_1 whose value is
//       Operation vpiAssignmentPatternOp (10.9.2) with 2 member-keyed items:
//       a: '1 and b: '0
//     - the key 'a' is bound to struct_1's member a
//     - u_dut instantiates dut and overrides A with the RefObj X, bound to
//       top's Parameter (23.10.2.2)
//   Diagnostics
//     - the body parameter A without a value is reported (Syntax 6-6
//       footnote 22)
//     - the member key b is reported as an error (10.9.2)
//     - there is no fatal diagnostic
//
// Reduction and elaboration: none is checked. Both constructs are illegal,
// so the standard gives no elaborated value for A or X.
//
// KNOWN COMPILER BUG (null port missing), not a defect in this test: HLC
// models an empty port list '()' with no port at all, although 37.14 detail
// 10 names "module M();" as declaring a null port.
// EmptyPortListsDeclareOneNullPort is expected to fail until HLC is fixed;
// it is intentionally not skipped or relaxed.
//
// KNOWN COMPILER BUG (body parameter without value unreported), not a
// defect in this test: HLC accepts 'parameter logic [4:0] A;' in dut's body
// and reports nothing, although Syntax 6-6 footnote 22 allows the value to
// be omitted "only within a parameter_port_list".
// BodyParameterWithoutValueIsReported is expected to fail until HLC is
// fixed; it is intentionally not skipped or relaxed.
//
// KNOWN COMPILER BUG (unknown pattern member only warned), not a defect in
// this test: HLC reports the key 'b' in '{a: '1, b: '0} with a "Failed to
// bind" warning only, although struct_1 has no member b and 10.9.2 says
// "The named member shall be at the top level of the structure".
// UnknownMemberKeyBIsAnError is expected to fail until HLC is fixed; it is
// intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - Whether HLC reports the missing value as a syntax error or as a
//     semantic error is a tool convention: the restriction is a footnote to
//     the grammar, so a diagnostic of either severity naming A is accepted.
//   - Which error identifier HLC uses is a tool convention; any diagnostic
//     at the accepted severity naming the offending name is accepted.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/tagged_pattern.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class ParamNoDefaultTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamNoDefault.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Whether a diagnostic of one of the severities 'error' (and, when
  // 'allowSyntax' is set, 'syntax') names 'symbol'. Which error identifier
  // HLC picks for it is a tool convention, so any is accepted.
  bool reportsErrorNaming(std::string_view symbol, bool allowSyntax) {
    for (const ErrorDefinition::ErrorMap::value_type &entry : ErrorDefinition::getErrorInfoMap()) {
      const ErrorDefinition::ErrorSeverity severity = entry.second.m_severity;
      const bool accepted =
          (severity == ErrorDefinition::ERROR) || (allowSyntax && (severity == ErrorDefinition::SYNTAX));
      if (accepted && (findError(entry.first, symbol) != nullptr)) return true;
    }
    return false;
  }

  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Parameter *getParam(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, scope->getParameters());
  }

  // The ParamAssign in 'scope' whose LHS names 'name'.
  static const hldb::ParamAssign *getParamAssign(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr || scope->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *scope->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == name)) return pa;
      const hldb::Parameter *const p = pa->getLhs<hldb::Parameter>();
      if ((p != nullptr) && (p->getName() == name)) return pa;
    }
    return nullptr;
  }

  static const hldb::Typedef *getStruct1() {
    const hldb::Module *const top = getModule("top");
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("struct_1", top->getTypedefs());
  }

  // Whether 'rts' names struct_1.
  static bool namesStruct1(const hldb::RefTypespec *rts) {
    const hldb::Typedef *const td = getStruct1();
    if (rts == nullptr || td == nullptr || td->getAlias() == nullptr || rts->getActual() == nullptr) return false;
    const hldb::TypedefTypespec *const viaTypedef = rts->getActual<hldb::TypedefTypespec>();
    return ((viaTypedef != nullptr) && (viaTypedef->getTypedef() == td)) ||
           (rts->getActual() == td->getAlias()->getActual());
  }

  // The key a member-keyed pattern item writes. Whether HLC keeps the key as
  // a tag reference, as the item's name or as its typespec's name is a model
  // convention.
  static std::string_view getKey(const hldb::TaggedPattern *tp) {
    if (const hldb::RefObj *const tag = tp->getTag<hldb::RefObj>()) return tag->getName();
    if (!tp->getName().empty()) return tp->getName();
    if (tp->getTypespec() != nullptr) return tp->getTypespec()->getName();
    return {};
  }
};

// ---------------------------------------------------------------------------
// module dut(); and module top();
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (null port
// missing) in the file header.
TEST_F(ParamNoDefaultTest, EmptyPortListsDeclareOneNullPort) {
  for (std::string_view name : {"dut", "top"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << "module '" << name << "' not found";
    ASSERT_NE(m->getPorts(), nullptr) << "37.14 detail 10: " << name << "'s '()' declares a null port";
    ASSERT_EQ(m->getPorts()->size(), 1u) << "37.14 detail 10: " << name << "'s '()' declares exactly one null port";
    EXPECT_EQ(m->getPorts()->at(0)->getName(), "") << "37.14 detail 8: a null port has no name";
    EXPECT_EQ(m->getPorts()->at(0)->getLowConn(), nullptr) << "37.14 detail 10: no low connection";
  }
}

TEST_F(ParamNoDefaultTest, DutDeclaresOverridableLogic4To0AWithoutValue) {
  const hldb::Module *const dut = getModule("dut");
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getParameters(), nullptr);
  EXPECT_EQ(dut->getParameters()->size(), 1u);
  const hldb::Parameter *const a = getParam(dut, "A");
  ASSERT_NE(a, nullptr) << "parameter 'A' not found";
  EXPECT_FALSE(a->getLocalParam()) << "6.20.1: dut has no parameter port list, so 'A' is overridable";
  ASSERT_NE(a->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = a->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "'A' is declared 'logic [4:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  ASSERT_NE(lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>(), nullptr);
  EXPECT_EQ(lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "4");
  ASSERT_NE(lt->getRanges()->at(0)->getRightExpr<hldb::Constant>(), nullptr);
  EXPECT_EQ(lt->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
  const hldb::ParamAssign *const pa = getParamAssign(dut, "A");
  EXPECT_TRUE(pa == nullptr || pa->getRhs() == nullptr) << "the source writes no value for 'A'";
}

// ---------------------------------------------------------------------------
// module top
// ---------------------------------------------------------------------------

TEST_F(ParamNoDefaultTest, Struct1IsPackedWithOneMemberA) {
  const hldb::Typedef *const td = getStruct1();
  ASSERT_NE(td, nullptr) << "typedef 'struct_1' not found in top";
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr) << "struct_1 is a structure";
  ASSERT_NE(st->getStruct(), nullptr);
  EXPECT_TRUE(st->getStruct()->getPacked()) << "7.2.1: struct_1 is declared 'packed'";
  ASSERT_NE(st->getStruct()->getMembers(), nullptr);
  ASSERT_EQ(st->getStruct()->getMembers()->size(), 1u) << "struct_1 has exactly one member";
  const hldb::TypespecMember *const a = st->getStruct()->getMembers()->at(0);
  EXPECT_EQ(a->getName(), "a");
  ASSERT_NE(a->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = a->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "'a' is declared 'logic [1:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  ASSERT_NE(lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>(), nullptr);
  EXPECT_EQ(lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "1");
}

TEST_F(ParamNoDefaultTest, XIsOverridableStruct1) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParameters(), nullptr);
  EXPECT_EQ(top->getParameters()->size(), 1u);
  const hldb::Parameter *const x = getParam(top, "X");
  ASSERT_NE(x, nullptr) << "parameter 'X' not found";
  EXPECT_FALSE(x->getLocalParam()) << "6.20.1: top has no parameter port list, so 'X' is overridable";
  EXPECT_TRUE(namesStruct1(x->getTypespec())) << "'X' is declared 'struct_1'";
}

TEST_F(ParamNoDefaultTest, XValueKeysMembersAAndB) {
  const hldb::ParamAssign *const pa = getParamAssign(getModule("top"), "X");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const pattern = pa->getRhs<hldb::Operation>();
  ASSERT_NE(pattern, nullptr) << "'{a: '1, b: '0} is an Operation";
  EXPECT_EQ(pattern->getOpType(), vpiAssignmentPatternOp) << "10.9.2: a structure assignment pattern";
  ASSERT_NE(pattern->getOperands(), nullptr);
  ASSERT_EQ(pattern->getOperands()->size(), 2u) << "the pattern writes 2 items";
  const std::string_view keys[] = {"a", "b"};
  const std::string_view values[] = {"'1", "'0"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::TaggedPattern *const item = any_cast<hldb::TaggedPattern>(pattern->getOperands()->at(i));
    ASSERT_NE(item, nullptr) << "item " << i << " is keyed by a member name";
    EXPECT_EQ(getKey(item), keys[i]) << "item " << i;
    const hldb::Constant *const value = item->getPattern<hldb::Constant>();
    ASSERT_NE(value, nullptr) << "item " << i << "'s value is a literal";
    EXPECT_EQ(value->getDecompile(), values[i]) << "item " << i;
  }
}

TEST_F(ParamNoDefaultTest, MemberKeyABindsToStruct1MemberA) {
  const hldb::ParamAssign *const pa = getParamAssign(getModule("top"), "X");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const pattern = pa->getRhs<hldb::Operation>();
  ASSERT_NE(pattern, nullptr);
  ASSERT_NE(pattern->getOperands(), nullptr);
  ASSERT_EQ(pattern->getOperands()->size(), 2u);
  const hldb::TaggedPattern *const item = any_cast<hldb::TaggedPattern>(pattern->getOperands()->at(0));
  ASSERT_NE(item, nullptr);
  const hldb::RefObj *const tag = item->getTag<hldb::RefObj>();
  ASSERT_NE(tag, nullptr) << "the key 'a' is a reference";
  const hldb::Typedef *const td = getStruct1();
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr);
  ASSERT_NE(st->getStruct(), nullptr);
  const hldb::TypespecMember *const a = hldb::findByName<hldb::TypespecMember>("a", st->getStruct()->getMembers());
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(tag->getActual(), a) << "10.9.2: the key 'a' names struct_1's member a";
}

TEST_F(ParamNoDefaultTest, UDutOverridesAWithX) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  const hldb::RefInstance *const inst = hldb::findByName<hldb::RefInstance>("u_dut", top->getRefInstances());
  ASSERT_NE(inst, nullptr) << "instance 'u_dut' not found";
  ASSERT_NE(inst->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getModule(), getModule("dut"));
  ASSERT_NE(mt->getParamAssigns(), nullptr);
  ASSERT_EQ(mt->getParamAssigns()->size(), 1u);
  const hldb::RefObj *const lhs = mt->getParamAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "A");
  const hldb::RefObj *const x = mt->getParamAssigns()->at(0)->getRhs<hldb::RefObj>();
  ASSERT_NE(x, nullptr) << "the override value is the name 'X'";
  EXPECT_EQ(x->getName(), "X");
  ASSERT_NE(getParam(top, "X"), nullptr);
  EXPECT_EQ(x->getActual(), getParam(top, "X")) << "'X' is top's own parameter";
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (body
// parameter without value unreported) in the file header.
TEST_F(ParamNoDefaultTest, BodyParameterWithoutValueIsReported) {
  EXPECT_TRUE(reportsErrorNaming("A", true))
      << "Syntax 6-6 footnote 22: the value can be omitted only within a parameter_port_list";
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (unknown
// pattern member only warned) in the file header.
TEST_F(ParamNoDefaultTest, UnknownMemberKeyBIsAnError) {
  EXPECT_TRUE(reportsErrorNaming("b", false))
      << "10.9.2: a member:value key names a member of the structure; struct_1 has no member 'b'";
}

TEST_F(ParamNoDefaultTest, NoFatalDiagnostic) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  EXPECT_EQ(m_session->getErrorContainer()->getErrorStats().nbFatal, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
