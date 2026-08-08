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

// Tests for 11.11--min_max_avg_delay.sv (tags: 11.11)
//   module top();
//     initial begin
//       #(100:200:300) $display("Done");
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023 Sec 11.11 "Minimum, typical, and
// maximum delay expressions", p.306-307, checked before any test code
// was written):
//   "SystemVerilog delay expressions can be specified as three
//   expressions separated by colons and enclosed by parentheses. This
//   is intended to represent minimum, typical, and maximum values -- in
//   that order." "(a:b:c) + (d:e:f)" is the spec's own example syntax
//   shape; this file's "#(100:200:300)" applies that exact min:typ:max
//   triplet as an intra-statement delay control before a "$display".
//
//   The vpi constant for this construct is confirmed directly from the
//   real header (build/hlc_windows_release_459/hlc_windows_release_459/
//   include/hldb/vpi_user.h): "#define vpiMinTypMaxOp 38 /* min:typ:max:
//   delay expression */" -- so "(100:200:300)" must elaborate as an
//   Operation whose opType is vpiMinTypMaxOp, not a plain Constant or a
//   3-way conditional.
//
// What is checked:
//   - module top has no nets, no variables, no ports -- this file
//     declares none
//   - module has exactly 1 process: an Initial whose stmt is a Begin
//     (explicit begin/end around the single statement -- still produces
//     a Begin, per the confirmed rule in chapter-10/10.4.2) with exactly
//     1 statement: a DelayControl
//   - the DelayControl's getDelay<Operation>() is the min:typ:max
//     triplet: opType vpiMinTypMaxOp, exactly 3 operands, each a
//     Constant: "100", "200", "300", in that order (min, typ, max per
//     Sec 11.11's own ordering statement)
//   - the DelayControl's getStmt<SysTaskCall>() is "$display" with
//     exactly 1 argument: Constant string "Done"
//   - compiler emits zero errors
//
// What is NOT checked and why:
//   - the exact design-level typespec count is inferred (ModuleTypespec
//     + shared IntTypespec for "100"/"200"/"300" + shared StringTypespec
//     for "Done" = 3), following this codebase's typespec-sharing
//     convention, not independently re-run for this new file -- build
//     and run to confirm.
//   - which of the three delay values (min/typ/max) HLC would actually
//     select when scheduling the "$display" -- that selection is a
//     simulator/timing-mode concept (Sec 11.11 says the format lets a
//     design "be tested with minimum, typical, or maximum delay
//     values"), not something the static object model records; HLC only
//     needs to preserve all three values, which is what is checked
//     above.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/delay_control.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class MinMaxAvgDelayTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.11--min_max_avg_delay.hlc"}); }
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

// --- module ------------------------------------------------------------------

TEST_F(MinMaxAvgDelayTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(MinMaxAvgDelayTest, ModuleHasNoNetsNoVariablesNoPorts) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty());
  EXPECT_TRUE(top->getVariables() == nullptr || top->getVariables()->empty());
  EXPECT_TRUE(top->getPorts() == nullptr || top->getPorts()->empty());
}

// --- initial block: #(100:200:300) $display("Done"); ------------------------

TEST_F(MinMaxAvgDelayTest, ModuleHasOneProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 1u);
}

TEST_F(MinMaxAvgDelayTest, InitialBlockHasOneStatement) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr) << "explicit begin/end around the single statement should still produce a Begin";
  ASSERT_NE(blk->getStmts(), nullptr);
  EXPECT_EQ(blk->getStmts()->size(), 1u);
}

TEST_F(MinMaxAvgDelayTest, StatementIsDelayControlWithMinTypMaxTriplet) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  const hldb::DelayControl *const delayCtrl = any_cast<hldb::DelayControl>(blk->getStmts()->at(0));
  ASSERT_NE(delayCtrl, nullptr) << "'#(100:200:300) $display(\"Done\");' should be a DelayControl";

  const hldb::Operation *const minTypMax = delayCtrl->getDelay<hldb::Operation>();
  ASSERT_NE(minTypMax, nullptr) << "'(100:200:300)' should be an Operation";
  EXPECT_EQ(minTypMax->getOpType(), vpiMinTypMaxOp)
      << "IEEE 1800-2023 Sec 11.11: a min:typ:max delay expression must decode to vpiMinTypMaxOp";
  ASSERT_NE(minTypMax->getOperands(), nullptr);
  ASSERT_EQ(minTypMax->getOperands()->size(), 3u) << "min, typ, and max values must all be preserved";
  const char *const expected[3] = {"100", "200", "300"};
  for (uint32_t i = 0; i < 3u; ++i) {
    const hldb::Constant *const val = any_cast<hldb::Constant>(minTypMax->getOperands()->at(i));
    ASSERT_NE(val, nullptr) << "operand index " << i;
    EXPECT_EQ(val->getDecompile(), expected[i])
        << "IEEE 1800-2023 Sec 11.11: min, typical, and maximum values, in that order";
  }
}

TEST_F(MinMaxAvgDelayTest, DelayControlStmtIsDisplayDone) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  const hldb::DelayControl *const delayCtrl = any_cast<hldb::DelayControl>(blk->getStmts()->at(0));
  ASSERT_NE(delayCtrl, nullptr);

  const hldb::SysTaskCall *const disp = delayCtrl->getStmt<hldb::SysTaskCall>();
  ASSERT_NE(disp, nullptr) << "the delay-controlled statement should be a SysTaskCall";
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);
  EXPECT_EQ(any_cast<hldb::Constant>(disp->getArguments()->at(0))->getValue(), "Done");
}

// --- compiler diagnostics ---------------------------------------------------

TEST_F(MinMaxAvgDelayTest, CompilerReportsZeroErrors) {
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
