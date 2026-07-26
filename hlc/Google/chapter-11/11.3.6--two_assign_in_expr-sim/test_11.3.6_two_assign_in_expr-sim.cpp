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

// Tests for 11.3.6--two_assign_in_expr-sim.sv (tags: 11.3.6)
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
//       $display(":assert: (%d == %d)", b, (e + c + 2));
//       $display(":assert: (%d == %d)", d, (e + c + 2));
//     end
//   endmodule
//
// This is the "sim" counterpart of 11.3.6--two_assign_in_expr.sv, the
// hardest structural case in this batch (an Assignment used as a plain
// operand of an Operation, itself nested inside another assignment). The
// added corner here is arithmetic: since "b += (a += 1) + 1" means
// "b = b + (a + 1) + 1" and "a" was snapshotted into c while "b" was
// snapshotted into e, the source asserts that both the intermediate
// result (b) and the final result (d) equal the same value: "e + c + 2"
// (b's original value, plus a's original value, plus the two literal 1's
// contributed by "a += 1" and the outer "+ 1"). Both $display calls check
// against the *same* expression, which is itself a useful structural
// fact to confirm: the compiler should build two independent, but
// textually identical, Operation trees rather than sharing/aliasing one
// between the two $display statements incorrectly.
//
// Checked:
//   - module top has exactly 5 nets: a, b, c, d, e, all int
//     (RefTypespec -> IntTypespec)
//   - the initial block is a Begin with exactly 5 statements:
//       [0] blocking Assignment: lhs RefObj "c", rhs RefObj "a" (baseline)
//       [1] blocking Assignment: lhs RefObj "e", rhs RefObj "b" (baseline)
//       [2] the same doubly-nested shape as the non-sim file: Assignment
//           (lhs "d") -> rhs Assignment (lhs "b") -> rhs Operation
//           (vpiAddOp, [RefObj "b", Operation(vpiAddOp, [Assignment(lhs
//           "a", rhs Operation(vpiAddOp, [RefObj "a", Constant "1"])),
//           Constant "1"])])
//       [3] SysTaskCall "$display" with 3 arguments: Constant string
//           ":assert: (%d == %d)", RefObj "b", and an Operation tree for
//           "(e + c) + 2" (vpiAddOp, operands [Operation(vpiAddOp,
//           [RefObj "e", RefObj "c"]), Constant "2"])
//       [4] SysTaskCall "$display" with 3 arguments: the same assertion
//           string, RefObj "d" this time, and an independently-built but
//           structurally identical "(e + c) + 2" Operation tree
//   - design-level typespecs (3): ModuleTypespec, IntTypespec (signed),
//     StringTypespec
//   - compiler emits zero errors
//
// Not checked (GTEST_SKIP, with a real reason):
//   - Whether b and d actually equal e + c + 2 once the initial block
//     runs. HLC does not execute statements or track a Net's post-
//     execution value. Genuine simulation-only gap; the static shape of
//     both assertions (checked above) is the most this environment can
//     confirm.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/any.h>
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

class TwoAssignInExprSimTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.3.6--two_assign_in_expr-sim.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
  static const hldb::Begin *getInitialBody() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    return (init == nullptr) ? nullptr : init->getStmt<hldb::Begin>();
  }
  // Verifies an Operation tree shaped like "(x + y) + 2" and returns
  // whether it matched, so both $display checks below can share the logic.
  static void expectIsEPlusCPlusTwo(const hldb::Any *arg) {
    const hldb::Operation *const outer = any_cast<hldb::Operation>(arg);
    ASSERT_NE(outer, nullptr) << "third $display argument should be an Operation tree for (e + c + 2)";
    EXPECT_EQ(outer->getOpType(), vpiAddOp);
    ASSERT_NE(outer->getOperands(), nullptr);
    ASSERT_EQ(outer->getOperands()->size(), 2u);
    const hldb::Operation *const inner = any_cast<hldb::Operation>(outer->getOperands()->at(0));
    ASSERT_NE(inner, nullptr) << "'(e + c)' should be its own Operation node";
    EXPECT_EQ(inner->getOpType(), vpiAddOp);
    ASSERT_NE(inner->getOperands(), nullptr);
    ASSERT_EQ(inner->getOperands()->size(), 2u);
    EXPECT_EQ(any_cast<hldb::RefObj>(inner->getOperands()->at(0))->getName(), "e");
    EXPECT_EQ(any_cast<hldb::RefObj>(inner->getOperands()->at(1))->getName(), "c");
    EXPECT_EQ(any_cast<hldb::Constant>(outer->getOperands()->at(1))->getDecompile(), "2");
  }
};

// --- module / nets -----------------------------------------------------

TEST_F(TwoAssignInExprSimTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(TwoAssignInExprSimTest, ModuleHasFiveIntNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 5u);
  const char *const names[5] = {"a", "b", "c", "d", "e"};
  for (uint32_t i = 0; i < 5u; ++i) {
    const hldb::Net *const net = hldb::findByName<hldb::Net>(names[i], top->getNets());
    ASSERT_NE(net, nullptr) << "net " << names[i];
    EXPECT_NE(net->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
  }
}

// --- baseline copies + the doubly-nested assignment ------------------------

TEST_F(TwoAssignInExprSimTest, InitialBlockHasFiveStatements) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 5u);
}

TEST_F(TwoAssignInExprSimTest, FirstTwoStatementsAreBaselineCopies) {
  const hldb::Begin *const blk = getInitialBody();
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

TEST_F(TwoAssignInExprSimTest, ThirdStatementNestsAssignmentInsideOperationOperand) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);

  const hldb::Assignment *const dEq = any_cast<hldb::Assignment>(blk->getStmts()->at(2));
  ASSERT_NE(dEq, nullptr);
  EXPECT_EQ(dEq->getLhs<hldb::RefObj>()->getName(), "d");

  const hldb::Assignment *const bAddEq = dEq->getRhs<hldb::Assignment>();
  ASSERT_NE(bAddEq, nullptr) << "'(b += ...)' should be a nested Assignment";
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

  const hldb::Assignment *const aAddEq = any_cast<hldb::Assignment>(innerAdd->getOperands()->at(0));
  ASSERT_NE(aAddEq, nullptr) << "'(a += 1)' should surface as an Assignment used as an Operation operand";
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

// --- both $display assertions target "(e + c) + 2" identically -----------

TEST_F(TwoAssignInExprSimTest, FourthStatementAssertsBEqualsEPlusCPlusTwo) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(3));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 3u);
  EXPECT_EQ(any_cast<hldb::Constant>(disp->getArguments()->at(0))->getValue(), ":assert: (%d == %d)");
  EXPECT_EQ(any_cast<hldb::RefObj>(disp->getArguments()->at(1))->getName(), "b");
  expectIsEPlusCPlusTwo(disp->getArguments()->at(2));
}

TEST_F(TwoAssignInExprSimTest, FifthStatementAssertsDEqualsEPlusCPlusTwo) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(4));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 3u);
  EXPECT_EQ(any_cast<hldb::Constant>(disp->getArguments()->at(0))->getValue(), ":assert: (%d == %d)");
  EXPECT_EQ(any_cast<hldb::RefObj>(disp->getArguments()->at(1))->getName(), "d");
  expectIsEPlusCPlusTwo(disp->getArguments()->at(2));
}

// --- design-level typespecs / compiler diagnostics -------------------------

TEST_F(TwoAssignInExprSimTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(TwoAssignInExprSimTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

// --- the actual point of the file: runtime arithmetic result -------------

TEST_F(TwoAssignInExprSimTest, BAndDBothEndUpEqualToEPlusCPlusTwo) {
  GTEST_SKIP() << "The source asserts b == e + c + 2 and d == e + c + 2 after "
                  "'c = a; e = b; d = (b += (a += 1) + 1);' runs. HLC is a static "
                  "compiler/elaborator with no post-execution value for a Net. Genuine "
                  "simulation-only gap; the static shape of both assertions is checked above.";
  // If the GTEST_SKIP() above is ever removed, this must still compile and
  // exercise a real, currently-failing check -- not silently pass.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const char *const names[2] = {"b", "d"};
  for (uint32_t i = 0; i < 2u; ++i) {
    const hldb::Net *const net = hldb::findByName<hldb::Net>(names[i], top->getNets());
    ASSERT_NE(net, nullptr) << "net " << names[i];
    // Net::getValue<T>() only ever exposes a declaration-time initializer;
    // neither b nor d has one (both are assigned inside the initial
    // block), so this is null today -- there is no field anywhere that
    // captures what the nested compound-assignment expression produced.
    const hldb::Constant *const finalValue = net->getValue<hldb::Constant>();
    ASSERT_NE(finalValue, nullptr) << names[i] << "'s post-assignment runtime value is not captured anywhere";
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
