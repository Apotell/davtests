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

// Tests for 11.3.6--assignment_in_expression.sv (tags: 11.3.6)
//   module top();
//     int a;
//     int b;
//
//     initial begin
//       b = (a += 1);
//     end
//   endmodule
//
// This is the "+=" sibling of 11.3.6--assign_in_exp.sv's "-=": IEEE
// 1800-2017 11.3.6 requires an embedded assignment operator to be
// parenthesized, and "(a += 1)" satisfies that. Having both the "-=" and
// "+=" compound-assignment variants as separate files (rather than just
// one) exercises that the parser handles ADD_ASSIGN and SUB_ASSIGN as
// distinct, independently-recognized tokens rather than one of them being
// an accidental fallthrough of the other -- if the grammar or the AST
// builder only special-cased "-=", this file would either fail to parse
// or silently produce the wrong vpiOpType.
//
// Checked:
//   - module top has exactly 2 variables, "a" and "b", both int
//     (RefTypespec -> IntTypespec). Per IEEE 1800-2023 Sec 6.7/6.8: "int"
//     has no net-type keyword and there is no port list, so both are
//     Variables, not Nets; module has no nets (getNets() is null).
//   - the initial block is a Begin with exactly 1 statement: a blocking
//     Assignment with lhs RefObj "b" whose rhs is itself a blocking
//     Assignment with lhs RefObj "a" and rhs an Operation (vpiAddOp,
//     2 operands: RefObj "a", Constant "1") -- confirming "(a += 1)" is
//     represented as "a" being reassigned to "a + 1" (not "a - 1"),
//     nested inside the outer assignment to "b"
//   - design-level typespecs (2): ModuleTypespec, IntTypespec (signed)
//   - compiler emits zero errors, confirming the parenthesized compound
//     assignment is accepted per IEEE 11.3.6
//
// Not checked:
//   - the actual runtime values of a/b after execution. This file has no
//     $display assertion, so there is no expected numeric outcome
//     authored into the source to check even in principle -- unlike
//     11.3.6--assignment_in_expression-sim.sv, the sim counterpart of
//     this exact construct, which does carry $display assertions and is
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

class AssignmentInExpressionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.3.6--assignment_in_expression.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / nets ----

TEST_F(AssignmentInExpressionTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(AssignmentInExpressionTest, ModuleHasTwoIntVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  ASSERT_EQ(top->getVariables()->size(), 2u);
  const char *const names[2] = {"a", "b"};
  for (uint32_t i = 0; i < 2u; ++i) {
    const hldb::Variable *const var = hldb::findByName<hldb::Variable>(names[i], top->getVariables());
    ASSERT_NE(var, nullptr) << "variable " << names[i];
    EXPECT_NE(var->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
  }
}

TEST_F(AssignmentInExpressionTest, ModuleHasNoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getNets(), nullptr);
}

// --- the point of the file: "+=" is its own distinct opcode, not "-=" ----

TEST_F(AssignmentInExpressionTest, InitialBlockHasOneStatement) {
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

TEST_F(AssignmentInExpressionTest, OuterAssignmentToBNestsInnerAddAssignToA) {
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
  ASSERT_NE(inner, nullptr) << "'(a += 1)' should be a nested Assignment, not collapsed away";
  EXPECT_TRUE(inner->getBlocking());
  EXPECT_EQ(inner->getLhs<hldb::RefObj>()->getName(), "a");

  const hldb::Operation *const add = inner->getRhs<hldb::Operation>();
  ASSERT_NE(add, nullptr);
  EXPECT_EQ(add->getOpType(), vpiAddOp) << "'+=' must decode to vpiAddOp, distinct from '-='s vpiSubOp";
  ASSERT_NE(add->getOperands(), nullptr);
  ASSERT_EQ(add->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::RefObj>(add->getOperands()->at(0))->getName(), "a");
  EXPECT_EQ(any_cast<hldb::Constant>(add->getOperands()->at(1))->getDecompile(), "1");
}

// --- design-level typespecs / compiler diagnostics ----

TEST_F(AssignmentInExpressionTest, DesignHasTwoTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 2u);
}

TEST_F(AssignmentInExpressionTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "IEEE 11.3.6: a parenthesized '+=' compound assignment inside "
                                  "an expression is legal and must not be rejected";
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
