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
// tests/Google/chapter-9/9.4.1--delay_control-sim.sv
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
// :name: delay_control_sim
// :description: delay control simulation
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
// endmodule
// ============================================================================
//
// IEEE 1800-2023 construct under test (Sec 9.4.1, "Delay control", Sec
// 20.2 "$display", and Sec 20.3 "$time"): three bare "#10;" delay control
// statements (with no controlled statement) interleaved with four
// "$display(...)" calls that print the running $time -- a self-checking
// simulation pattern (":assert: (N == %d)") verifying the compiler's own
// delay-control accounting is not something this compile-only object model
// can execute (see NOT CHECKED).
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - module "top" exists.
//   - the initial block's explicit "begin...end" produces a Begin wrapping
//     exactly eight statements, in order: SysTaskCall("$display"),
//     DelayControl, SysTaskCall("$display"), DelayControl,
//     SysTaskCall("$display"), DelayControl, SysTaskCall("$display"),
//     SysTaskCall("$finish").
//   - each of the three DelayControl statements has getStmt() == null
//     (bare "#10;", no controlled statement) and getDelay() a Constant
//     whose getDecompile() is "10".
//   - each "$display(...)" SysTaskCall has exactly two arguments: a
//     Constant (the format-string literal) and a SysFuncCall named
//     "$time" with no arguments (getArguments() == null, since "$time" is
//     called with no parentheses/args).
//   - the final "$finish;" is a SysTaskCall named "$finish" with no
//     arguments.
//
// NOT CHECKED (out of scope; every assertion below states only what IEEE
// 1800-2023 requires -- none of it is based on reading a .log file or any
// other tool-output dump):
//   - The exact decompiled text of each format-string Constant (the
//     ":assert: (N == %d)" literals): only presence/type is asserted, for
//     the same reason as the hex-literal caveat in
//     test_9.3.3--block_start_finish.cpp.
//   - Runtime behavior: whether $time actually reads 0, 10, 20, 30 at each
//     $display call, and whether $finish actually ends simulation. HLC is
//     a compiler/elaborator with no simulation, so no execution ever
//     happens for this test to observe -- the ":assert:" markers in the
//     source are meant for an external simulator's self-check, not for
//     this test suite.
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
// Confirms "stmt" is '$display(<format-string Constant>, $time);' and
// returns the SysTaskCall for further inspection, or null on mismatch.
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

const hldb::DelayControl *CheckTenUnitBareDelay(const hldb::Any *stmt) {
  const hldb::DelayControl *const delay = any_cast<hldb::DelayControl>(stmt);
  if (delay == nullptr || delay->getStmt() != nullptr) return nullptr;

  const hldb::Constant *const delayValue = delay->getDelay<hldb::Constant>();
  if (delayValue == nullptr || delayValue->getDecompile() != "10") return nullptr;

  return delay;
}
}  // namespace

class DelayControlSimTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.4.1--delay_control-sim.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to DelayControlSimTest go here!

TEST_F(DelayControlSimTest, ModuleTopExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'top' not found";
}

TEST_F(DelayControlSimTest, InitialBeginHasFourDisplaysThreeDelaysAndFinishInOrder) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);

  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);

  ASSERT_NE(init->getStmt(), nullptr) << "'initial begin ... end' should always produce a Begin";
  const hldb::Begin *const body = any_cast<hldb::Begin>(init->getStmt());
  ASSERT_NE(body, nullptr) << "explicit begin/end should produce a Begin scope node";

  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 8u) << "four $display calls, three bare delays, and $finish are exactly "
                                              "eight statements";

  EXPECT_NE(CheckDisplayOfTime(body->getStmts()->at(0)), nullptr) << "1st '$display(..., $time);' should match";
  EXPECT_NE(CheckTenUnitBareDelay(body->getStmts()->at(1)), nullptr) << "1st '#10;' should match";
  EXPECT_NE(CheckDisplayOfTime(body->getStmts()->at(2)), nullptr) << "2nd '$display(..., $time);' should match";
  EXPECT_NE(CheckTenUnitBareDelay(body->getStmts()->at(3)), nullptr) << "2nd '#10;' should match";
  EXPECT_NE(CheckDisplayOfTime(body->getStmts()->at(4)), nullptr) << "3rd '$display(..., $time);' should match";
  EXPECT_NE(CheckTenUnitBareDelay(body->getStmts()->at(5)), nullptr) << "3rd '#10;' should match";
  EXPECT_NE(CheckDisplayOfTime(body->getStmts()->at(6)), nullptr) << "4th '$display(..., $time);' should match";

  const hldb::SysTaskCall *const finish = any_cast<hldb::SysTaskCall>(body->getStmts()->at(7));
  ASSERT_NE(finish, nullptr) << "'$finish;' should be a SysTaskCall";
  EXPECT_EQ(finish->getName(), "$finish");
  EXPECT_TRUE(finish->getArguments() == nullptr || finish->getArguments()->empty())
      << "'$finish;' takes no arguments here";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
