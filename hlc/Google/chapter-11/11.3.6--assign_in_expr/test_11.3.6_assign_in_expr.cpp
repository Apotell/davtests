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

// Tests for 11.3.6--assign_in_expr.sv (tags: 11.3.6)
//   module top();
//     int a;
//     int b;
//     int c;
//
//     initial begin
//       a = (b = (c = 5));
//     end
//   endmodule
//
// This is the direct legal counterpart of 11.3.6--assign_in_expr_inv.sv's
// illegal "a = b = c = 5;" (no parens): IEEE 1800-2017 11.3.6 requires an
// embedded assignment to be parenthesized, and here every embedded
// assignment is. The corner this file exercises is chaining *depth*: not
// just one assignment nested inside another (as in 11.3.6--assign_in_exp.sv
// and 11.3.6--assign_in_expression.sv), but two levels of nesting --
// "a = (b = (c = 5))" -- to confirm the parser builds a genuine 3-deep
// Assignment chain rather than only handling a single level correctly.
//
// Checked:
//   - module top has exactly 3 variables, "a", "b", "c", all int
//     (RefTypespec -> IntTypespec). Per IEEE 1800-2023 Sec 6.7/6.8: "int"
//     has no net-type keyword and there is no port list, so all three are
//     Variables, not Nets; module has no nets (getNets() is null).
//   - the initial block is a Begin with exactly 1 statement: a blocking
//     Assignment chain nested 3 deep --
//       Assignment(lhs a) -> rhs Assignment(lhs b) -> rhs Assignment(lhs c)
//       -> rhs Constant "5"
//     -- with every level's getBlocking() true, confirming the parser
//     preserves both the nesting depth and the blocking-assignment kind
//     at each level, not just the outermost one
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed)
//   - compiler emits zero errors, confirming multi-level parenthesized
//     assignment chains are accepted per IEEE 11.3.6
//
// Not checked:
//   - the actual runtime values of a/b/c after execution (that all three
//     end up 5). This file has no $display assertion, so there is no
//     expected numeric outcome authored into the source to check even in
//     principle -- unlike 11.3.6--assign_in_expr-sim.sv, the sim
//     counterpart of this exact construct, which does carry $display
//     assertions and is tested separately.

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
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>

namespace hlc {

class AssignInExprTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.3.6--assign_in_expr.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / nets ----

TEST_F(AssignInExprTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(AssignInExprTest, ModuleHasThreeIntVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  ASSERT_EQ(top->getVariables()->size(), 3u);
  const char *const names[3] = {"a", "b", "c"};
  for (uint32_t i = 0; i < 3u; ++i) {
    const hldb::Variable *const var = hldb::findByName<hldb::Variable>(names[i], top->getVariables());
    ASSERT_NE(var, nullptr) << "variable " << names[i];
    EXPECT_NE(var->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
  }
}

TEST_F(AssignInExprTest, ModuleHasNoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getNets(), nullptr);
}

// --- the point of the file: a 3-deep parenthesized assignment chain ----

TEST_F(AssignInExprTest, InitialBlockHasOneStatement) {
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

TEST_F(AssignInExprTest, AssignmentChainIsThreeLevelsDeepAToBToCToConstant) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);

  const hldb::Assignment *const toA = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(toA, nullptr);
  EXPECT_TRUE(toA->getBlocking());
  EXPECT_EQ(toA->getLhs<hldb::RefObj>()->getName(), "a");

  const hldb::Assignment *const toB = toA->getRhs<hldb::Assignment>();
  ASSERT_NE(toB, nullptr) << "'(b = (c = 5))' should be a nested Assignment, not collapsed away";
  EXPECT_TRUE(toB->getBlocking());
  EXPECT_EQ(toB->getLhs<hldb::RefObj>()->getName(), "b");

  const hldb::Assignment *const toC = toB->getRhs<hldb::Assignment>();
  ASSERT_NE(toC, nullptr) << "'(c = 5)' should be a second, distinct nested Assignment";
  EXPECT_TRUE(toC->getBlocking());
  EXPECT_EQ(toC->getLhs<hldb::RefObj>()->getName(), "c");

  const hldb::Constant *const five = toC->getRhs<hldb::Constant>();
  ASSERT_NE(five, nullptr);
  EXPECT_EQ(five->getDecompile(), "5");
}

// --- design-level typespecs / compiler diagnostics ----

TEST_F(AssignInExprTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(AssignInExprTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "IEEE 11.3.6: a 3-deep parenthesized assignment chain is legal "
                                  "and must not be rejected";
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
