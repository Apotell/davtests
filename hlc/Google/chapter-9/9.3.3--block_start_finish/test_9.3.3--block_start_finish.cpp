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
// tests/Google/chapter-9/9.3.3--block_start_finish.sv
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
// :name: block_start_finish
// :description: block start finish check
// :tags: 9.3.3
// */
// module block_tb ();
// 	reg [3:0] a = 0;
// 	initial begin
// 		fork
// 			#200 a = 'h1;
// 			#150 a = 'h2;
// 			#100 a = 'h3;
// 			#50  a = 'h4;
// 		join
//
// 		fork
// 			#200 a = 'h5;
// 			#150 a = 'h6;
// 			#100 a = 'h7;
// 			#50  a = 'h8;
// 		join
// 	end
// endmodule
// ============================================================================
//
// IEEE 1800-2023 construct under test (Sec 9.3.3, "Parallel blocks"):
// `fork ... join` groups its statements so they start together (in
// parallel) when the block is entered, and (for the plain "join" keyword)
// the block itself does not complete until every one of its statements has
// completed. This file exercises two sequential fork/join blocks, each
// containing four delayed blocking assignments to the same variable "a".
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - module "block_tb" exists.
//   - the initial block's explicit "begin...end" produces a Begin wrapping
//     exactly two statements, each a ForkStmt (the two fork/join blocks).
//   - each ForkStmt's getJoinType() is vpiJoin (the plain "join" keyword,
//     not join_any/join_none).
//   - each ForkStmt contains exactly four statements, each a DelayControl
//     (the leading "#N") wrapping an Assignment whose getLhs() is a RefObj
//     named "a" and whose getDelayControl()->getDelay() is a Constant
//     matching the source's delay value (200, 150, 100, 50 respectively).
//   - each Assignment's getRhs() is present and is a Constant (the literal
//     'h1..'h8 assigned to "a") -- confirming an operand is bound, without
//     asserting its decompiled hex text (see NOT CHECKED).
//
// NOT CHECKED (out of scope; every assertion below states only what IEEE
// 1800-2023 requires -- none of it is based on reading a .log file or any
// other tool-output dump):
//   - The exact decompiled text of the hex literals ('h1..'h8): this
//     object model's Constant::getDecompile() format for a radix literal
//     is not specified by the standard itself, so only presence/type is
//     asserted, not exact text (unlike the plain decimal delay values,
//     whose getDecompile() format is already confirmed elsewhere, e.g.
//     test_16.14--assume-property.cpp's "1" check).
//   - Runtime scheduling/interleaving of the four parallel statements and
//     the exact instant the fork/join block completes: HLC is a
//     compiler/elaborator with no simulation, so no execution ever
//     happens for this test to observe.
// ============================================================================

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/delay_control.h>
#include <hldb/design.h>
#include <hldb/fork_stmt.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_obj.h>

#include <hlc/Tests/Test.h>

namespace hlc {
namespace {
// Confirms operand at "stmt" is "#delayText a = <some Constant>;" and
// returns the outer DelayControl for further inspection, or null on
// mismatch.
const hldb::DelayControl *CheckDelayedAssignToA(const hldb::Any *stmt, std::string_view delayText) {
  const hldb::DelayControl *const delay = any_cast<hldb::DelayControl>(stmt);
  if (delay == nullptr) return nullptr;

  const hldb::Constant *const delayValue = delay->getDelay<hldb::Constant>();
  if (delayValue == nullptr || delayValue->getDecompile() != delayText) return nullptr;

  const hldb::Assignment *const assign = delay->getStmt<hldb::Assignment>();
  if (assign == nullptr) return nullptr;

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  if (lhs == nullptr || lhs->getName() != "a") return nullptr;

  if (any_cast<hldb::Constant>(assign->getRhs()) == nullptr) return nullptr;

  return delay;
}
}  // namespace

class BlockStartFinishTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.3.3--block_start_finish.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to BlockStartFinishTest go here!

TEST_F(BlockStartFinishTest, ModuleBlockTbExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'block_tb' not found";
}

TEST_F(BlockStartFinishTest, InitialBeginWrapsTwoForkJoinBlocks) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u) << "module block_tb has exactly one process (the initial block)";

  const hldb::Process *const process = top->getProcesses()->front();
  ASSERT_NE(process, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(process);
  ASSERT_NE(init, nullptr) << "block_tb's process should specifically be an Initial block";

  ASSERT_NE(init->getStmt(), nullptr) << "'initial begin ... end' should always produce a Begin";
  const hldb::Begin *const body = any_cast<hldb::Begin>(init->getStmt());
  ASSERT_NE(body, nullptr) << "explicit begin/end should produce a Begin scope node";

  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u) << "the begin/end block contains exactly two statements: the two "
                                              "fork/join blocks";

  EXPECT_NE(any_cast<hldb::ForkStmt>(body->getStmts()->at(0)), nullptr) << "the first statement should be a ForkStmt";
  EXPECT_NE(any_cast<hldb::ForkStmt>(body->getStmts()->at(1)), nullptr)
      << "the second statement should be a ForkStmt";
}

TEST_F(BlockStartFinishTest, FirstForkJoinHasPlainJoinAndFourDelayedAssignments) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const body = any_cast<hldb::Begin>(init->getStmt());
  ASSERT_NE(body, nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u);

  const hldb::ForkStmt *const fork = any_cast<hldb::ForkStmt>(body->getStmts()->at(0));
  ASSERT_NE(fork, nullptr) << "the first block should be a ForkStmt";
  EXPECT_EQ(fork->getJoinType(), vpiJoin) << "plain 'join' should be vpiJoin, not join_any/join_none";

  ASSERT_NE(fork->getStmts(), nullptr);
  ASSERT_EQ(fork->getStmts()->size(), 4u) << "the first fork/join block has exactly four parallel statements";

  EXPECT_NE(CheckDelayedAssignToA(fork->getStmts()->at(0), "200"), nullptr) << "'#200 a = 'h1;' should match";
  EXPECT_NE(CheckDelayedAssignToA(fork->getStmts()->at(1), "150"), nullptr) << "'#150 a = 'h2;' should match";
  EXPECT_NE(CheckDelayedAssignToA(fork->getStmts()->at(2), "100"), nullptr) << "'#100 a = 'h3;' should match";
  EXPECT_NE(CheckDelayedAssignToA(fork->getStmts()->at(3), "50"), nullptr) << "'#50  a = 'h4;' should match";
}

TEST_F(BlockStartFinishTest, SecondForkJoinHasPlainJoinAndFourDelayedAssignments) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const body = any_cast<hldb::Begin>(init->getStmt());
  ASSERT_NE(body, nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u);

  const hldb::ForkStmt *const firstFork = any_cast<hldb::ForkStmt>(body->getStmts()->at(0));
  const hldb::ForkStmt *const secondFork = any_cast<hldb::ForkStmt>(body->getStmts()->at(1));
  ASSERT_NE(secondFork, nullptr) << "the second block should be a ForkStmt";
  EXPECT_NE(secondFork, firstFork) << "the second fork/join block must be a distinct object from the first";
  EXPECT_EQ(secondFork->getJoinType(), vpiJoin) << "plain 'join' should be vpiJoin, not join_any/join_none";

  ASSERT_NE(secondFork->getStmts(), nullptr);
  ASSERT_EQ(secondFork->getStmts()->size(), 4u) << "the second fork/join block has exactly four parallel statements";

  EXPECT_NE(CheckDelayedAssignToA(secondFork->getStmts()->at(0), "200"), nullptr) << "'#200 a = 'h5;' should match";
  EXPECT_NE(CheckDelayedAssignToA(secondFork->getStmts()->at(1), "150"), nullptr) << "'#150 a = 'h6;' should match";
  EXPECT_NE(CheckDelayedAssignToA(secondFork->getStmts()->at(2), "100"), nullptr) << "'#100 a = 'h7;' should match";
  EXPECT_NE(CheckDelayedAssignToA(secondFork->getStmts()->at(3), "50"), nullptr) << "'#50  a = 'h8;' should match";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
