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

// Tests for 11.3.6--assignment_in_expression-sim.sv (tags: 11.3.6)
//   module top();
//     int a;
//     int b;
//     int c;
//
//     initial begin
//       c = a;
//       b = (a += 1);
//       $display(":assert: (%d == %d)", b, (c + 1));
//     end
//   endmodule
//
// This is the "sim" counterpart of 11.3.6--assignment_in_expression.sv's
// "(a += 1)", and the "+=" sibling of 11.3.6--assign_in_exp-sim.sv's "-=".
// Same structure -- snapshot a into c, mutate a through a parenthesized
// compound assignment, then assert the relationship between b and the
// snapshot -- but with addition instead of subtraction, confirming the
// "+=" opcode (vpiAddOp) is what actually gets asserted against, not a
// copy-pasted vpiSubOp left over from the "-=" file.
//
// Checked:
//   - module top has exactly 3 nets, "a", "b", "c", all int
//     (RefTypespec -> IntTypespec)
//   - the initial block is a Begin with exactly 3 statements:
//       [0] blocking Assignment: lhs RefObj "c", rhs RefObj "a" --
//           baseline snapshot before "a" is mutated
//       [1] blocking Assignment: lhs RefObj "b", rhs a nested Assignment
//           (lhs RefObj "a", rhs Operation vpiAddOp [RefObj "a", Constant
//           "1"]) -- the same nested-compound-assignment shape as
//           11.3.6--assign_in_exp-sim.sv, but with vpiAddOp
//       [2] SysTaskCall "$display" with 3 arguments: Constant string
//           ":assert: (%d == %d)", RefObj "b", and an Operation
//           (vpiAddOp, 2 operands: RefObj "c", Constant "1") -- confirming
//           the assertion is genuinely "b == c + 1", using the same
//           add opcode under test
//   - design-level typespecs (3): ModuleTypespec, IntTypespec (signed),
//     StringTypespec
//   - compiler emits zero errors
//
// Not checked (GTEST_SKIP, with a real reason):
//   - Whether b actually equals c + 1 once the initial block runs, i.e.
//     whether "+=" really incremented a by 1 relative to its original
//     value (preserved in c). HLC does not execute statements or track a
//     Net's post-execution value. Genuine simulation-only gap.

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
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AssignmentInExpressionSimTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.3.6--assignment_in_expression-sim.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
  static const hldb::Begin *getInitialBody() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    return (init == nullptr) ? nullptr : init->getStmt<hldb::Begin>();
  }
};

// --- module / nets -----------------------------------------------------

TEST_F(AssignmentInExpressionSimTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(AssignmentInExpressionSimTest, ModuleHasThreeIntNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 3u);
  const char *const names[3] = {"a", "b", "c"};
  for (uint32_t i = 0; i < 3u; ++i) {
    const hldb::Net *const net = hldb::findByName<hldb::Net>(names[i], top->getNets());
    ASSERT_NE(net, nullptr) << "net " << names[i];
    EXPECT_NE(net->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
  }
}

// --- baseline snapshot + the nested compound assignment --------------------

TEST_F(AssignmentInExpressionSimTest, InitialBlockHasThreeStatements) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 3u);
}

TEST_F(AssignmentInExpressionSimTest, FirstStatementSnapshotsAIntoC) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const cEqA = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(cEqA, nullptr);
  EXPECT_EQ(cEqA->getLhs<hldb::RefObj>()->getName(), "c");
  EXPECT_EQ(cEqA->getRhs<hldb::RefObj>()->getName(), "a");
}

TEST_F(AssignmentInExpressionSimTest, SecondStatementNestsAddAssignOfAIntoB) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const bEq = any_cast<hldb::Assignment>(blk->getStmts()->at(1));
  ASSERT_NE(bEq, nullptr);
  EXPECT_EQ(bEq->getLhs<hldb::RefObj>()->getName(), "b");
  const hldb::Assignment *const aAddEq = bEq->getRhs<hldb::Assignment>();
  ASSERT_NE(aAddEq, nullptr) << "'(a += 1)' should be a nested Assignment";
  EXPECT_EQ(aAddEq->getLhs<hldb::RefObj>()->getName(), "a");
  const hldb::Operation *const add = aAddEq->getRhs<hldb::Operation>();
  ASSERT_NE(add, nullptr);
  EXPECT_EQ(add->getOpType(), vpiAddOp) << "'+=' must decode to vpiAddOp";
  ASSERT_NE(add->getOperands(), nullptr);
  ASSERT_EQ(add->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::RefObj>(add->getOperands()->at(0))->getName(), "a");
  EXPECT_EQ(any_cast<hldb::Constant>(add->getOperands()->at(1))->getDecompile(), "1");
}

TEST_F(AssignmentInExpressionSimTest, DisplayAssertsBEqualsCPlusOne) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(2));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 3u);
  EXPECT_EQ(any_cast<hldb::Constant>(disp->getArguments()->at(0))->getValue(), ":assert: (%d == %d)");
  EXPECT_EQ(any_cast<hldb::RefObj>(disp->getArguments()->at(1))->getName(), "b");

  const hldb::Operation *const cPlusOne = any_cast<hldb::Operation>(disp->getArguments()->at(2));
  ASSERT_NE(cPlusOne, nullptr) << "third $display argument should be '(c + 1)', not a bare RefObj";
  EXPECT_EQ(cPlusOne->getOpType(), vpiAddOp);
  ASSERT_NE(cPlusOne->getOperands(), nullptr);
  ASSERT_EQ(cPlusOne->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::RefObj>(cPlusOne->getOperands()->at(0))->getName(), "c");
  EXPECT_EQ(any_cast<hldb::Constant>(cPlusOne->getOperands()->at(1))->getDecompile(), "1");
}

// --- design-level typespecs / compiler diagnostics -------------------------

TEST_F(AssignmentInExpressionSimTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(AssignmentInExpressionSimTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

// --- the actual point of the file: does "+=" really add at runtime -------

TEST_F(AssignmentInExpressionSimTest, BEndsUpEqualToCPlusOne) {
  GTEST_SKIP() << "The source asserts b == c + 1 after 'c = a; b = (a += 1);' runs -- i.e. that "
                  "'+=' really added 1 to a's original value (preserved in c). HLC is a static "
                  "compiler/elaborator with no post-execution value for a Net. Genuine "
                  "simulation-only gap. If simulation/co-sim support is ever added, replace "
                  "this with a real check of the printed ':assert: (%d == %d)' output.";
  // If the GTEST_SKIP() above is ever removed, this must still compile and
  // exercise a real, currently-failing check -- not silently pass.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  // Net::getValue<T>() only ever exposes a declaration-time initializer;
  // 'b' has none (it is assigned inside the initial block), so this is
  // null today -- there is no field anywhere that captures what '+='
  // actually produced at runtime.
  const hldb::Constant *const finalValue = b->getValue<hldb::Constant>();
  ASSERT_NE(finalValue, nullptr) << "no field captures b's post-assignment runtime value";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
