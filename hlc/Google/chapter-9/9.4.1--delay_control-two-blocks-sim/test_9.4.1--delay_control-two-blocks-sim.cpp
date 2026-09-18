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

// ============================================================================
// SystemVerilog source under test:
// tests/Google/chapter-9/9.4.1--delay_control-two-blocks-sim.sv
// ----------------------------------------------------------------------------
// // Copyright (C) 2019-2021  The SymbiFlow Authors.
// //
// // Use of this source code is governed by a ISC-style
// // license that can be found in the LICENSE file or at
// // https://opensource.org/licenses/ISC
// //
// // SPDX-License-Identifier: ISC
//
// /*
// :name: delay_control_two_blocks_sim
// :description: delay control simulation with two blocks
// :tags: 9.4.1
// :type: simulation
// */
// module top();
//
//    initial begin
//       $display(":assert: (0 == %d)", $time);
//
//       #10;
//       $display(":assert: (10 == %d)", $time);
//
//       #10;
//       $display(":assert: (20 == %d)", $time);
//
//       #10;
//       $display(":assert: (30 == %d)", $time);
//
//       $finish;
//    end
//
//    initial begin
//       #5;
//       #10;
//       #10;
//    end
// endmodule
// ============================================================================
//
// IEEE 1800-2023 construct under test (Sec 9.4.1, "Delay control"): the
// same self-checking "$display + #10 delay" pattern as
// test_9.4.1--delay_control-sim.sv, but here module "top" additionally
// declares a SECOND, independent initial block containing three bare
// delay-control statements (#5, #10, #10) and no other statements --
// confirming that multiple processes with independent delay-control
// statements are each modeled as their own Process/Begin, not merged.
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - module "top" exists and has exactly two processes (two initial
//     blocks).
//   - the first process is structurally identical to
//     test_9.4.1--delay_control-sim.sv's single initial block: a Begin
//     wrapping four "$display(..., $time)" SysTaskCalls, three bare
//     10-unit DelayControl statements, and a final "$finish;"
//     SysTaskCall, in that order.
//   - the second process is a distinct Begin wrapping exactly three bare
//     DelayControl statements, each with getStmt() == null: "#5;" (delay
//     Constant "5"), then "#10;" and "#10;" (delay Constant "10" each).
//
// NOT CHECKED (out of scope; every assertion below states only what IEEE
// 1800-2023 requires -- none of it is based on reading a .log file or any
// other tool-output dump):
//   - The exact decompiled text of each format-string Constant in the
//     first process's $display calls (same caveat as
//     test_9.4.1--delay_control-sim.sv).
//   - Runtime interleaving between the two independent initial blocks
//     (e.g. whether the second block's delays run concurrently with the
//     first's): HLC is a compiler/elaborator with no simulation, so no
//     execution ever happens for this test to observe.
// ============================================================================

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/delay_control.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/process_stmt.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>

#include <hlc/Tests/Test.h>

namespace hlc {
namespace {
const hldb::SysTaskCall *CheckDisplayOfTime(const hldb::Any *stmt) {
  const hldb::SysTaskCall *const display = any_cast<hldb::SysTaskCall>(stmt);
  if (display == nullptr || display->getName() != "$display") return nullptr;

  if (display->getArguments() == nullptr || display->getArguments()->size() != 2u) return nullptr;

  if (any_cast<hldb::Constant>(display->getArguments()->at(0)) == nullptr) return nullptr;

  const hldb::SysFuncCall *const time = any_cast<hldb::SysFuncCall>(display->getArguments()->at(1));
  if (time == nullptr || time->getName() != "$time") return nullptr;
  if (time->getArguments() != nullptr && !time->getArguments()->empty()) return nullptr;

  return display;
}

const hldb::DelayControl *CheckBareDelay(const hldb::Any *stmt, std::string_view delayText) {
  const hldb::DelayControl *const delay = any_cast<hldb::DelayControl>(stmt);
  if (delay == nullptr || delay->getStmt() != nullptr) return nullptr;

  const hldb::Constant *const delayValue = delay->getDelay<hldb::Constant>();
  if (delayValue == nullptr || delayValue->getDecompile() != delayText) return nullptr;

  return delay;
}
}  // namespace

class DelayControlTwoBlocksSimTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.4.1--delay_control-two-blocks-sim.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to DelayControlTwoBlocksSimTest go here!

TEST_F(DelayControlTwoBlocksSimTest, ModuleTopExistsWithTwoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 2u) << "module top has exactly two processes: the two initial blocks";
}

TEST_F(DelayControlTwoBlocksSimTest, FirstInitialHasFourDisplaysThreeDelaysAndFinishInOrder) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 2u);

  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr) << "the first process should specifically be an Initial block";

  ASSERT_NE(init->getStmt(), nullptr) << "'initial begin ... end' should always produce a Begin";
  const hldb::Begin *const body = any_cast<hldb::Begin>(init->getStmt());
  ASSERT_NE(body, nullptr) << "explicit begin/end should produce a Begin scope node";

  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 8u) << "four $display calls, three bare delays, and $finish are exactly "
                                              "eight statements";

  EXPECT_NE(CheckDisplayOfTime(body->getStmts()->at(0)), nullptr) << "1st '$display(..., $time);' should match";
  EXPECT_NE(CheckBareDelay(body->getStmts()->at(1), "10"), nullptr) << "1st '#10;' should match";
  EXPECT_NE(CheckDisplayOfTime(body->getStmts()->at(2)), nullptr) << "2nd '$display(..., $time);' should match";
  EXPECT_NE(CheckBareDelay(body->getStmts()->at(3), "10"), nullptr) << "2nd '#10;' should match";
  EXPECT_NE(CheckDisplayOfTime(body->getStmts()->at(4)), nullptr) << "3rd '$display(..., $time);' should match";
  EXPECT_NE(CheckBareDelay(body->getStmts()->at(5), "10"), nullptr) << "3rd '#10;' should match";
  EXPECT_NE(CheckDisplayOfTime(body->getStmts()->at(6)), nullptr) << "4th '$display(..., $time);' should match";

  const hldb::SysTaskCall *const finish = any_cast<hldb::SysTaskCall>(body->getStmts()->at(7));
  ASSERT_NE(finish, nullptr) << "'$finish;' should be a SysTaskCall";
  EXPECT_EQ(finish->getName(), "$finish");
}

TEST_F(DelayControlTwoBlocksSimTest, SecondInitialIsThreeBareDelaysOnly) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 2u);

  const hldb::Initial *const firstInit = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  const hldb::Initial *const secondInit = any_cast<hldb::Initial>(top->getProcesses()->at(1));
  ASSERT_NE(secondInit, nullptr) << "the second process should specifically be an Initial block";
  EXPECT_NE(secondInit, firstInit) << "the second initial block must be a distinct object from the first";

  ASSERT_NE(secondInit->getStmt(), nullptr) << "'initial begin ... end' should always produce a Begin";
  const hldb::Begin *const body = any_cast<hldb::Begin>(secondInit->getStmt());
  ASSERT_NE(body, nullptr) << "explicit begin/end should produce a Begin scope node";

  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 3u) << "'#5;', '#10;', '#10;' are exactly three bare delay statements";

  EXPECT_NE(CheckBareDelay(body->getStmts()->at(0), "5"), nullptr) << "'#5;' should match";
  EXPECT_NE(CheckBareDelay(body->getStmts()->at(1), "10"), nullptr) << "1st '#10;' should match";
  EXPECT_NE(CheckBareDelay(body->getStmts()->at(2), "10"), nullptr) << "2nd '#10;' should match";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
