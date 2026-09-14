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
// tests/Google/chapter-9/9.4.1--delay_control.sv
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
// :name: delay_control
// :description: delay control
// :tags: 9.4.1
// */
// module block_tb ();
// 	reg [3:0] a = 0;
// 	initial begin
// 		#10 a = 'h1;
// 		#10 a = 'h2;
// 		#10 a = 'h3;
// 		#10 a = 'h4;
// 	end
// endmodule
// ============================================================================
//
// IEEE 1800-2023 construct under test (Sec 9.4.1, "Delay control"): a
// procedural timing control statement, "# delay_value statement", used
// here four times in sequence, each delaying its own blocking assignment
// to "a" by 10 time units.
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - module "block_tb" exists.
//   - the initial block's explicit "begin...end" produces a Begin wrapping
//     exactly four statements, each a DelayControl.
//   - each DelayControl's getDelay() is a Constant whose getDecompile() is
//     "10" (the delay value is uniform, unlike
//     test_9.3.3--block_start_finish.cpp's varying delays).
//   - each DelayControl's getStmt() is an Assignment whose getLhs() is a
//     RefObj named "a" and whose getRhs() is present and is a Constant
//     (the literal 'h1..'h4), confirming an operand is bound without
//     asserting its decompiled hex text (see NOT CHECKED).
//
// NOT CHECKED (out of scope; every assertion below states only what IEEE
// 1800-2023 requires -- none of it is based on reading a .log file or any
// other tool-output dump):
//   - The exact decompiled text of the hex literals ('h1..'h4): as in
//     test_9.3.3--block_start_finish.cpp, only presence/type is asserted.
//   - Runtime timing behavior (does "a" actually hold each value for 10
//     time units before the next assignment): HLC is a compiler/
//     elaborator with no simulation, so no execution ever happens for this
//     test to observe.
// ============================================================================

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/delay_control.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_obj.h>

#include <hlc/Tests/Test.h>

namespace hlc {
namespace {
const hldb::DelayControl *CheckTenUnitDelayedAssignToA(const hldb::Any *stmt) {
  const hldb::DelayControl *const delay = any_cast<hldb::DelayControl>(stmt);
  if (delay == nullptr) return nullptr;

  const hldb::Constant *const delayValue = delay->getDelay<hldb::Constant>();
  if (delayValue == nullptr || delayValue->getDecompile() != "10") return nullptr;

  const hldb::Assignment *const assign = delay->getStmt<hldb::Assignment>();
  if (assign == nullptr) return nullptr;

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  if (lhs == nullptr || lhs->getName() != "a") return nullptr;

  if (any_cast<hldb::Constant>(assign->getRhs()) == nullptr) return nullptr;

  return delay;
}
}  // namespace

class DelayControlTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.4.1--delay_control.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to DelayControlTest go here!

TEST_F(DelayControlTest, ModuleBlockTbExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'block_tb' not found";
}

TEST_F(DelayControlTest, InitialBeginWrapsFourTenUnitDelayedAssignments) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);

  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);

  ASSERT_NE(init->getStmt(), nullptr) << "'initial begin ... end' should always produce a Begin";
  const hldb::Begin *const body = any_cast<hldb::Begin>(init->getStmt());
  ASSERT_NE(body, nullptr) << "explicit begin/end should produce a Begin scope node";

  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 4u) << "the begin/end block contains exactly four statements";

  EXPECT_NE(CheckTenUnitDelayedAssignToA(body->getStmts()->at(0)), nullptr) << "'#10 a = 'h1;' should match";
  EXPECT_NE(CheckTenUnitDelayedAssignToA(body->getStmts()->at(1)), nullptr) << "'#10 a = 'h2;' should match";
  EXPECT_NE(CheckTenUnitDelayedAssignToA(body->getStmts()->at(2)), nullptr) << "'#10 a = 'h3;' should match";
  EXPECT_NE(CheckTenUnitDelayedAssignToA(body->getStmts()->at(3)), nullptr) << "'#10 a = 'h4;' should match";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
