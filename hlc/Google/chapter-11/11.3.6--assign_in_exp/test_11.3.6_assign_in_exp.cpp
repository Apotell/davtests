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

// Tests for 11.3.6--assign_in_exp.sv (tags: 11.3.6)
//   module top();
//     int a;
//     int b;
//
//     initial begin
//       b = (a -= 1);
//     end
//   endmodule
//
// IEEE 1800-2017 11.3.6 permits an assignment operator inside an
// expression provided it is enclosed in parentheses. This file is the
// legal, parenthesized counterpart of 11.3.6--assign_in_expr_inv.sv's
// illegal, unparenthesized "a = b = c = 5;". The corner it exercises is
// twofold: (1) that a *compound* assignment operator ("-=", not just
// plain "="), when parenthesized, is accepted exactly like plain "=" is
// in the other files of this batch, and (2) that the parser actually
// nests the inner assignment as a real Assignment node inside the outer
// one's RHS -- i.e. it must not flatten "(a -= 1)" into some other shape
// that would lose the fact that assigning to "a" is itself a side effect
// of evaluating the RHS of "b = ...".
//
// Checked:
//   - module work@top has exactly 2 nets, "a" and "b", both int
//     (RefTypespec -> IntTypespec)
//   - the initial block is a Begin with exactly 1 statement: a blocking
//     Assignment with lhs RefObj "b" whose rhs is itself a blocking
//     Assignment with lhs RefObj "a" and rhs an Operation (vpiSubOp,
//     2 operands: RefObj "a", Constant "1") -- confirming "(a -= 1)" is
//     represented faithfully as "a" being reassigned to "a - 1", nested
//     inside the outer assignment to "b", not collapsed or reordered
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed)
//   - compiler emits zero errors, confirming the parenthesized compound
//     assignment is accepted per IEEE 11.3.6
//
// Not checked:
//   - the actual runtime values of a/b after execution (e.g. that if a
//     started at some value N, a ends up N-1 and b also ends up N-1).
//     This file has no initial value for "a" and no $display assertion,
//     so there is no expected numeric outcome authored into the source to
//     check even in principle -- unlike 11.3.6--assign_in_exp-sim.sv,
//     which does carry $display assertions and is tested separately.

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
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AssignInExpTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.3.6--assign_in_exp.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()); }
};

// --- module / nets -----------------------------------------------------

TEST_F(AssignInExpTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(AssignInExpTest, ModuleHasTwoIntNets) {
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

// --- the point of the file: parenthesized compound assignment nests correctly

TEST_F(AssignInExpTest, InitialBlockHasOneStatement) {
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

TEST_F(AssignInExpTest, OuterAssignmentToBNestsInnerCompoundAssignmentToA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);

  const hldb::Assignment *const outer = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(outer, nullptr);
  EXPECT_TRUE(outer->getBlocking());
  EXPECT_EQ(outer->getLhs<hldb::RefObj>()->getName(), "b");

  const hldb::Assignment *const inner = outer->getRhs<hldb::Assignment>();
  ASSERT_NE(inner, nullptr) << "'(a -= 1)' should be a nested Assignment, not collapsed away";
  EXPECT_TRUE(inner->getBlocking());
  EXPECT_EQ(inner->getLhs<hldb::RefObj>()->getName(), "a");

  const hldb::Operation *const sub = inner->getRhs<hldb::Operation>();
  ASSERT_NE(sub, nullptr);
  EXPECT_EQ(sub->getOpType(), vpiSubOp);
  ASSERT_NE(sub->getOperands(), nullptr);
  ASSERT_EQ(sub->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::RefObj>(sub->getOperands()->at(0))->getName(), "a");
  EXPECT_EQ(any_cast<hldb::Constant>(sub->getOperands()->at(1))->getDecompile(), "1");
}

// --- design-level typespecs / compiler diagnostics -------------------------

TEST_F(AssignInExpTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(AssignInExpTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "IEEE 11.3.6: a parenthesized compound assignment operator inside "
                                  "an expression is legal and must not be rejected";
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
