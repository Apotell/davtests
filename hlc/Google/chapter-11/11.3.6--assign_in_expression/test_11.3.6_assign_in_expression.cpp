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

// Tests for 11.3.6--assign_in_expression.sv (tags: 11.3.6)
//   module top();
//     int a;
//     int b;
//
//     initial begin
//       b = (++a);
//     end
//   endmodule
//
// This file exercises a variant of IEEE 1800-2017 11.3.6 that the other
// files in this batch do not: the "assignment operator" embedded in the
// parenthesized expression is not "=" or a compound op like "-="/"+=" but
// the increment operator "++" (IEEE 11.4.2), which is itself a form of
// assignment (a = a + 1) and is likewise only permitted inside an
// expression when parenthesized. The corner here is confirming the
// compiler represents "(++a)" as an Operation (pre-increment) feeding the
// assignment to "b", not as some other statement shape, and that this
// parenthesized pre-increment-in-expression is accepted without error.
//
// Checked:
//   - module work@top has exactly 2 nets, "a" and "b", both int
//     (RefTypespec -> IntTypespec)
//   - the initial block is a Begin with exactly 1 statement: a blocking
//     Assignment with lhs RefObj "b" and rhs an Operation (vpiPreIncOp,
//     1 operand: RefObj "a") -- confirming "(++a)" is represented as a
//     genuine pre-increment operation on "a", supplying its post-
//     increment value as the value assigned to "b"
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed)
//   - compiler emits zero errors, confirming a parenthesized pre-
//     increment used as a sub-expression is accepted per IEEE 11.3.6
//
// Not checked:
//   - the actual runtime values of a/b after execution (that b ends up
//     one more than a's original value, and a itself is incremented).
//     This file has no initial value for "a" and no $display assertion,
//     so there is no expected numeric outcome authored into the source to
//     check even in principle -- unlike 11.3.6--assign_in_expression-sim.sv,
//     the sim counterpart of this exact construct, which does carry
//     $display assertions and is tested separately.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>

namespace hlc {

class AssignInExpressionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.3.6--assign_in_expression.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()); }
};

// --- module / nets -----------------------------------------------------

TEST_F(AssignInExpressionTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(AssignInExpressionTest, ModuleHasTwoIntNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 2u);
  const char *const names[2] = {"a", "b"};
  for (uint32_t i = 0; i < 2u; ++i) {
    const hldb::Net *const net = hldb::findByName<hldb::Net>(names[i], top->getNets());
    ASSERT_NE(net, nullptr) << "net " << names[i];
    EXPECT_NE(net->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
  }
}

// --- the point of the file: parenthesized pre-increment as a sub-expression

TEST_F(AssignInExpressionTest, InitialBlockHasOneStatement) {
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

TEST_F(AssignInExpressionTest, AssignmentToBHasPreIncrementOfAAsRhs) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);

  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "b");

  const hldb::Operation *const preInc = assign->getRhs<hldb::Operation>();
  ASSERT_NE(preInc, nullptr) << "'(++a)' should be represented as a pre-increment Operation";
  EXPECT_EQ(preInc->getOpType(), vpiPreIncOp);
  ASSERT_NE(preInc->getOperands(), nullptr);
  ASSERT_EQ(preInc->getOperands()->size(), 1u);
  EXPECT_EQ(any_cast<hldb::RefObj>(preInc->getOperands()->at(0))->getName(), "a");
}

// --- design-level typespecs / compiler diagnostics -------------------------

TEST_F(AssignInExpressionTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(AssignInExpressionTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "IEEE 11.3.6: a parenthesized pre-increment used as a "
                                  "sub-expression is legal and must not be rejected";
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
