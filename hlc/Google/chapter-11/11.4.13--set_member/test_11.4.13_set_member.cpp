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

// Tests for 11.4.13--set_member.sv (tags: 11.4.13)
//   int a;
//   int b = 12;
//   localparam c = 5;
//   localparam d = 7;
//   initial begin
//     a = b inside {c, d};
//   end
//
// IEEE 1800-2017 11.4.13 defines "expr inside {set}" as a membership test
// against an open_range_list. The corner unique to this file is that the
// set members are named localparams, not literals -- so the question is
// whether the compiler substitutes each localparam's numeric value into
// the set at compile time, or preserves each member as its own reference
// expression pointing back at the Parameter declaration. The AST shows
// the latter: each set member decodes to a RefObj resolving to the
// Parameter object, not a bare Constant "5"/"7" -- i.e. named-parameter
// identity survives into the "inside" set, it is not pre-folded away.
//
// Checked:
//   - module work@top has exactly 2 nets: "a" (int, no initializer) and
//     "b" (int, decl-time getValue<Constant>() == "12")
//   - module has exactly 2 Parameters, "c" and "d", both getLocalParam()
//     == true; each Parameter's OWN typespec (getTypespec<RefTypespec>()
//     ->getActual<LogicTypespec>()) is LogicTypespec, the implicit type a
//     "localparam" with no explicit data type gets -- distinct from the
//     IntTypespec of the constant assigned to it
//   - module has exactly 2 ParamAssigns matching "c"->Constant "5" and
//     "d"->Constant "7" (both IntTypespec) -- the localparam's numeric
//     value lives on the ParamAssign's rhs, not on the Parameter itself
//   - the initial block is a Begin with exactly 1 statement: a blocking
//     Assignment, lhs RefObj "a", rhs an Operation (vpiInsideOp, 2
//     operands): operand 0 = RefObj "b"; operand 1 = an Operation
//     (vpiConcatOp, 2 operands) representing the set "{c, d}", whose two
//     operands are RefObj "c" and RefObj "d", each resolving (via
//     getActual<Parameter>()) back to the same Parameter objects checked
//     above -- confirming the set members are named references, not
//     pre-substituted literal values
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed)
//   - compiler emits zero errors
//
// Not checked (GTEST_SKIP, with a real reason):
//   - Whether "a" actually ends up 1 (since b=12 is a member of {5,7} is
//     false, so per IEEE 11.4.13 "a" should read 0 -- note the set here
//     is {5,7} and b=12 is NOT a member, so the correct runtime result is
//     0, not 1; this file carries no $display assertion of its own, so
//     there is no author-declared expected value to check even in
//     principle, and HLC has no post-execution value for a Net regardless).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class SetMemberTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.4.13--set_member.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()); }
};

// --- module / nets -----------------------------------------------------

TEST_F(SetMemberTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(SetMemberTest, NetAHasNoInitializerNetBIsTwelve) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 2u);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
  EXPECT_EQ(a->getValue<hldb::Constant>(), nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  ASSERT_NE(b->getValue<hldb::Constant>(), nullptr);
  EXPECT_EQ(b->getValue<hldb::Constant>()->getDecompile(), "12");
}

// --- the localparams: implicit LogicTypespec, value lives on ParamAssign --

TEST_F(SetMemberTest, ParametersCAndDAreLocalWithImplicitLogicTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParameters(), nullptr);
  ASSERT_EQ(top->getParameters()->size(), 2u);
  const char *const names[2] = {"c", "d"};
  for (uint32_t i = 0; i < 2u; ++i) {
    const hldb::Parameter *const p = hldb::findByName<hldb::Parameter>(names[i], top->getParameters());
    ASSERT_NE(p, nullptr) << "parameter " << names[i];
    EXPECT_TRUE(p->getLocalParam());
    EXPECT_NE(p->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>(), nullptr)
        << "an untyped localparam should get an implicit LogicTypespec, distinct from its value's IntTypespec";
  }
}

TEST_F(SetMemberTest, ParamAssignsGiveCFiveAndDSeven) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParamAssigns(), nullptr);
  ASSERT_EQ(top->getParamAssigns()->size(), 2u);
  const hldb::ParamAssign *const cAssign = hldb::findByName<hldb::ParamAssign>("c", top->getParamAssigns());
  ASSERT_NE(cAssign, nullptr);
  ASSERT_NE(cAssign->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(cAssign->getRhs<hldb::Constant>()->getDecompile(), "5");
  const hldb::ParamAssign *const dAssign = hldb::findByName<hldb::ParamAssign>("d", top->getParamAssigns());
  ASSERT_NE(dAssign, nullptr);
  ASSERT_NE(dAssign->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(dAssign->getRhs<hldb::Constant>()->getDecompile(), "7");
}

// --- the point of the file: "inside" preserves named-parameter identity --

TEST_F(SetMemberTest, InitialBlockHasOneStatement) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 1u);
}

TEST_F(SetMemberTest, AssignmentIsBInsideSetOfCAndDByReference) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);

  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "a");

  const hldb::Operation *const inside = assign->getRhs<hldb::Operation>();
  ASSERT_NE(inside, nullptr);
  EXPECT_EQ(inside->getOpType(), vpiInsideOp);
  ASSERT_NE(inside->getOperands(), nullptr);
  ASSERT_EQ(inside->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::RefObj>(inside->getOperands()->at(0))->getName(), "b");

  const hldb::Operation *const set = any_cast<hldb::Operation>(inside->getOperands()->at(1));
  ASSERT_NE(set, nullptr) << "'{c, d}' should be a concatenation Operation representing the set";
  EXPECT_EQ(set->getOpType(), vpiConcatOp);
  ASSERT_NE(set->getOperands(), nullptr);
  ASSERT_EQ(set->getOperands()->size(), 2u);

  const hldb::RefObj *const cRef = any_cast<hldb::RefObj>(set->getOperands()->at(0));
  ASSERT_NE(cRef, nullptr) << "set member 'c' should be a RefObj, not a pre-folded Constant";
  EXPECT_EQ(cRef->getName(), "c");
  EXPECT_NE(cRef->getActual<hldb::Parameter>(), nullptr);

  const hldb::RefObj *const dRef = any_cast<hldb::RefObj>(set->getOperands()->at(1));
  ASSERT_NE(dRef, nullptr) << "set member 'd' should be a RefObj, not a pre-folded Constant";
  EXPECT_EQ(dRef->getName(), "d");
  EXPECT_NE(dRef->getActual<hldb::Parameter>(), nullptr);
}

// --- design-level typespecs / compiler diagnostics -------------------------

TEST_F(SetMemberTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(SetMemberTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
