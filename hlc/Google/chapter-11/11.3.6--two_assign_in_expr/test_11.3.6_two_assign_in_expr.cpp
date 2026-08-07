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

// Tests for 11.3.6--two_assign_in_expr.sv (tags: 11.3.6)
//   module top();
//     int a;
//     int b;
//     int c;
//     int d;
//     int e;
//
//     initial begin
//       c = a;
//       e = b;
//       d = (b += (a += 1) + 1);
//     end
//   endmodule
//
// Every other 11.3.6 file in this batch parenthesizes an assignment
// operator that is directly the entire RHS (or, at most, one level of
// nesting inside another assignment's RHS). This file is the hardest
// corner: "(a += 1)" is not the whole parenthesized sub-expression, it is
// itself just one *operand* of a larger arithmetic "+" -- i.e.
// "(a += 1) + 1" -- which is in turn the RHS of the outer "b += ...".
// Per IEEE 1800-2017 11.3.6, only the embedded assignment itself needs
// parentheses, not the whole arithmetic expression it sits inside. This
// checks the parser can place an Assignment node as a plain operand of an
// Operation (not just as the direct RHS of another Assignment), and that
// two independent embedded assignments ("b +=" and "a +=") both nest
// correctly at the same time.
//
// Checked:
//   - module top has exactly 5 variables: a, b, c, d, e, all int
//     (RefTypespec -> IntTypespec). Per IEEE 1800-2023 Sec 6.7/6.8: "int"
//     has no net-type keyword and there is no port list, so all five are
//     Variables, not Nets; module has no nets (getNets() is null).
//   - the initial block is a Begin with exactly 3 statements:
//       [0] blocking Assignment: lhs RefObj "c", rhs RefObj "a" (plain
//           copy, sets up a known baseline before "a" gets mutated below)
//       [1] blocking Assignment: lhs RefObj "e", rhs RefObj "b" (same,
//           baseline for "b")
//       [2] blocking Assignment: lhs RefObj "d", rhs a nested Assignment:
//             lhs RefObj "b", rhs Operation (vpiAddOp, 2 operands):
//               operand 0 = RefObj "b"
//               operand 1 = Operation (vpiAddOp, 2 operands):
//                 operand 0 = Assignment: lhs RefObj "a", rhs Operation
//                             (vpiAddOp, [RefObj "a", Constant "1"])
//                 operand 1 = Constant "1"
//           -- confirming "(a += 1)" surfaces as an Assignment sitting
//           inside an Operation's operand list, not just inside another
//           Assignment's RHS, and that "b += ..." itself nests one level
//           further inside the outermost "d = ..."
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed)
//   - compiler emits zero errors, confirming this doubly-nested,
//     operand-level embedded assignment is accepted per IEEE 11.3.6
//
// Not checked:
//   - the actual runtime values of b/d/e after execution (that they
//     reflect the arithmetic described above). This file has no $display
//     assertion, so there is no expected numeric outcome authored into
//     the source to check even in principle -- unlike
//     11.3.6--two_assign_in_expr-sim.sv, the sim counterpart of this
//     exact construct, which does carry $display assertions and is
//     tested separately.

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

class TwoAssignInExprTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.3.6--two_assign_in_expr.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / nets ----

TEST_F(TwoAssignInExprTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(TwoAssignInExprTest, ModuleHasFiveIntVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  ASSERT_EQ(top->getVariables()->size(), 5u);
  const char *const names[5] = {"a", "b", "c", "d", "e"};
  for (uint32_t i = 0; i < 5u; ++i) {
    const hldb::Variable *const var = hldb::findByName<hldb::Variable>(names[i], top->getVariables());
    ASSERT_NE(var, nullptr) << "variable " << names[i];
    EXPECT_NE(var->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
  }
}

TEST_F(TwoAssignInExprTest, ModuleHasNoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getNets(), nullptr);
}

// --- baseline copies before the nested-assignment expression ----

TEST_F(TwoAssignInExprTest, InitialBlockHasThreeStatements) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 3u);
}

TEST_F(TwoAssignInExprTest, FirstTwoStatementsAreBaselineCopies) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);

  const hldb::Assignment *const cEqA = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(cEqA, nullptr);
  EXPECT_EQ(cEqA->getLhs<hldb::RefObj>()->getName(), "c");
  EXPECT_EQ(cEqA->getRhs<hldb::RefObj>()->getName(), "a");

  const hldb::Assignment *const eEqB = any_cast<hldb::Assignment>(blk->getStmts()->at(1));
  ASSERT_NE(eEqB, nullptr);
  EXPECT_EQ(eEqB->getLhs<hldb::RefObj>()->getName(), "e");
  EXPECT_EQ(eEqB->getRhs<hldb::RefObj>()->getName(), "b");
}

// --- the point of the file: an Assignment as a plain Operation operand ----

TEST_F(TwoAssignInExprTest, ThirdStatementNestsAssignmentInsideOperationOperand) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);

  const hldb::Assignment *const dEq = any_cast<hldb::Assignment>(blk->getStmts()->at(2));
  ASSERT_NE(dEq, nullptr);
  EXPECT_TRUE(dEq->getBlocking());
  EXPECT_EQ(dEq->getLhs<hldb::RefObj>()->getName(), "d");

  const hldb::Assignment *const bAddEq = dEq->getRhs<hldb::Assignment>();
  ASSERT_NE(bAddEq, nullptr) << "'(b += ...)' should be a nested Assignment";
  EXPECT_TRUE(bAddEq->getBlocking());
  EXPECT_EQ(bAddEq->getLhs<hldb::RefObj>()->getName(), "b");

  const hldb::Operation *const outerAdd = bAddEq->getRhs<hldb::Operation>();
  ASSERT_NE(outerAdd, nullptr);
  EXPECT_EQ(outerAdd->getOpType(), vpiAddOp);
  ASSERT_NE(outerAdd->getOperands(), nullptr);
  ASSERT_EQ(outerAdd->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::RefObj>(outerAdd->getOperands()->at(0))->getName(), "b");

  const hldb::Operation *const innerAdd = any_cast<hldb::Operation>(outerAdd->getOperands()->at(1));
  ASSERT_NE(innerAdd, nullptr) << "'(a += 1) + 1' should be its own Operation node";
  EXPECT_EQ(innerAdd->getOpType(), vpiAddOp);
  ASSERT_NE(innerAdd->getOperands(), nullptr);
  ASSERT_EQ(innerAdd->getOperands()->size(), 2u);

  // The interesting corner: operand 0 of this inner "+" is not a RefObj
  // or Constant like every other Operation operand in this batch -- it is
  // a full Assignment, because "(a += 1)" is parenthesized and legally
  // embedded as a sub-expression per IEEE 11.3.6.
  const hldb::Assignment *const aAddEq = any_cast<hldb::Assignment>(innerAdd->getOperands()->at(0));
  ASSERT_NE(aAddEq, nullptr) << "'(a += 1)' should surface as an Assignment used as an Operation operand";
  EXPECT_TRUE(aAddEq->getBlocking());
  EXPECT_EQ(aAddEq->getLhs<hldb::RefObj>()->getName(), "a");
  const hldb::Operation *const aAdd = aAddEq->getRhs<hldb::Operation>();
  ASSERT_NE(aAdd, nullptr);
  EXPECT_EQ(aAdd->getOpType(), vpiAddOp);
  ASSERT_NE(aAdd->getOperands(), nullptr);
  ASSERT_EQ(aAdd->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::RefObj>(aAdd->getOperands()->at(0))->getName(), "a");
  EXPECT_EQ(any_cast<hldb::Constant>(aAdd->getOperands()->at(1))->getDecompile(), "1");

  EXPECT_EQ(any_cast<hldb::Constant>(innerAdd->getOperands()->at(1))->getDecompile(), "1");
}

// --- design-level typespecs / compiler diagnostics ----

TEST_F(TwoAssignInExprTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(TwoAssignInExprTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "IEEE 11.3.6: a parenthesized assignment used as one operand "
                                  "of a larger arithmetic expression, itself nested inside "
                                  "another parenthesized assignment, is legal and must not be "
                                  "rejected";
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
