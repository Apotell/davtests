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

// Tests for 11.4.11--cond_op.sv (tags: 11.4.11)
//   int a = 12;
//   int b = 5;
//   int c;
//   initial begin
//     c = (a > b) ? 11 : 13;
//   end
//
// IEEE 1800-2017 11.4.11 defines the conditional (ternary) operator
// "cond ? true_expr : false_expr" as a single 3-operand construct, not a
// pair of separate binary operators. The corner this file exercises is
// that the compiler builds exactly one Operation node (vpiConditionOp)
// holding all three operands in order -- condition, true-value, false-
// value -- with the condition itself being a full sub-expression
// (a > b), not a bare boolean variable.
//
// Checked:
//   - module top has exactly 3 variables (bare "int" has no net-type
//     keyword, so these are hldb::Variable, not hldb::Net -- IEEE 1800-2023
//     Sec 6.7/6.8), "a" (int, decl value 12), "b" (int, decl value 5), "c"
//     (int, no decl value)
//   - the initial block is a Begin with exactly 1 statement: a blocking
//     Assignment, lhs RefObj "c", rhs an Operation (vpiConditionOp,
//     3 operands):
//       operand 0: Operation (vpiGtOp, 2 operands: RefObj "a", RefObj "b")
//       operand 1: Constant "11"
//       operand 2: Constant "13"
//     -- confirming the ternary is a single 3-ary node, and that its
//     condition operand is itself a nested comparison Operation rather
//     than some flattened/pre-evaluated form
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed)
//   - compiler emits zero errors
//
// Not checked:
//   - this file has no $display assertion at all (unlike its "-sim"
//     sibling, 11.4.11--cond_op-sim.sv), so there is no runtime numeric
//     outcome authored into the source to check even in principle.

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
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class CondOpTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.4.11--cond_op.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / nets -----------------------------------------------------

TEST_F(CondOpTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(CondOpTest, VariablesAAndBHaveDeclaredValuesCHasNone) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  ASSERT_EQ(top->getVariables()->size(), 3u);

  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  const hldb::Variable *const c = hldb::findByName<hldb::Variable>("c", top->getVariables());
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  ASSERT_NE(c, nullptr);
  EXPECT_NE(a->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
  EXPECT_NE(b->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
  EXPECT_NE(c->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);

  ASSERT_NE(a->getValue<hldb::Constant>(), nullptr);
  ASSERT_NE(b->getValue<hldb::Constant>(), nullptr);
  EXPECT_EQ(a->getValue<hldb::Constant>()->getDecompile(), "12");
  EXPECT_EQ(b->getValue<hldb::Constant>()->getDecompile(), "5");
  EXPECT_EQ(c->getValue<hldb::Constant>(), nullptr) << "'c' has no decl-assignment";
}

// --- the point of the file: the ternary is one 3-ary Operation node -----

TEST_F(CondOpTest, InitialBlockHasOneStatement) {
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

TEST_F(CondOpTest, AssignmentRhsIsConditionOperatorOverGreaterThan) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);

  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "c");

  const hldb::Operation *const cond = assign->getRhs<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiConditionOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 3u) << "the ternary must have exactly 3 operands: "
                                                 "condition, true-value, false-value";

  const hldb::Operation *const gt = any_cast<hldb::Operation>(cond->getOperands()->at(0));
  ASSERT_NE(gt, nullptr) << "the ternary's condition should itself be a comparison Operation";
  EXPECT_EQ(gt->getOpType(), vpiGtOp);
  ASSERT_NE(gt->getOperands(), nullptr);
  ASSERT_EQ(gt->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::RefObj>(gt->getOperands()->at(0))->getName(), "a");
  EXPECT_EQ(any_cast<hldb::RefObj>(gt->getOperands()->at(1))->getName(), "b");

  EXPECT_EQ(any_cast<hldb::Constant>(cond->getOperands()->at(1))->getDecompile(), "11");
  EXPECT_EQ(any_cast<hldb::Constant>(cond->getOperands()->at(2))->getDecompile(), "13");
}

// --- design-level typespecs / compiler diagnostics -------------------------

TEST_F(CondOpTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(CondOpTest, CompilerReportsZeroErrors) {
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
