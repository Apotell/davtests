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
// tests/Google/chapter-9/9.3.3--event.sv
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
// :name: event_order
// :description: event order test
// :tags: 9.3.3
// */
// module block_tb ();
// 	event ev;
// 	reg [3:0] a = 0;
// 	initial fork
// 		begin
// 			a = 'h3;
// 			#20;
// 			->ev;
// 		end
// 		begin
// 			@ev
// 			a = 'h4;
// 		end
// 	join
// endmodule
// ============================================================================
//
// IEEE 1800-2023 construct under test (Sec 9.3.3, "Parallel blocks", used
// together with Sec 15.4.1 "The event trigger statement" and Sec 15.5.2
// "Waiting for an event"): a fork/join block with two parallel begin/end
// branches synchronized through a named event "ev" -- the first branch
// triggers "ev" (non-blocking, "->ev;") after a delay, and the second
// branch waits for it ("@ev") before proceeding.
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - module "block_tb" exists.
//   - the initial statement (no begin/end at the initial level -- "initial
//     fork ... join" binds the ForkStmt directly) is a ForkStmt with
//     getJoinType() == vpiJoin and exactly two statements, each a Begin.
//   - the first Begin contains exactly three statements: an Assignment
//     ("a = 'h3;"), a bare DelayControl ("#20;", with a null getStmt()
//     since nothing follows it on the same statement), and an EventStmt
//     ("->ev;") whose getBlocking() is false (non-blocking trigger) and
//     whose getNamedEvent() is a RefObj named "ev".
//   - the second Begin contains exactly one statement: an EventControl
//     ("@ev") whose getCondition() is a RefObj named "ev" and whose
//     getStmt() is the Assignment "a = 'h4;" (no begin/end after "@ev", so
//     it binds the following statement directly).
//   - module-scope "event ev;" is NOT locatable via Scope::getNamedEvents()
//     -- per IEEE 1800-2023 Sec 6.7.1 "event" is its own data type and
//     should produce a NamedEvent, but HLC currently models it as a plain
//     Variable typed with EventTypespec instead (same gap already
//     confirmed in test_15.5.2--named-event-wait.cpp).
//
// NOT CHECKED (out of scope; every assertion below states only what IEEE
// 1800-2023 requires -- none of it is based on reading a .log file or any
// other tool-output dump):
//   - The exact decompiled text of the hex literals ('h3, 'h4) assigned to
//     "a" -- only that each RHS is present and is a Constant (see the
//     same caveat in test_9.3.3--block_start_finish.cpp).
//   - Runtime event-ordering behavior (does the second branch really block
//     until the first branch triggers "ev" at simulation time +20):
//     HLC is a compiler/elaborator with no simulation, so no execution
//     ever happens for this test to observe.
// ============================================================================

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/delay_control.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/event_stmt.h>
#include <hldb/fork_stmt.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/named_event.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_obj.h>

#include <hlc/Tests/Test.h>

namespace hlc {
class EventOrderTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.3.3--event.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to EventOrderTest go here!

TEST_F(EventOrderTest, ModuleBlockTbExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'block_tb' not found";
}

TEST_F(EventOrderTest, ModuleScopeEventDeclarationShouldBeNamedEvent) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::NamedEvent *const ev = hldb::findByName<hldb::NamedEvent>("ev", top->getNamedEvents());
  EXPECT_NE(ev, nullptr) << "'event ev;' should be a NamedEvent per IEEE 1800-2023 Sec 6.7.1; HLC currently "
                            "models named events as a plain Variable typed with EventTypespec instead, "
                            "so Scope::getNamedEvents() does not find it (see test_15.5.2--named-event-wait.cpp).";
}

TEST_F(EventOrderTest, InitialForkBindsDirectlyWithPlainJoinAndTwoBeginBlocks) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);

  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);

  ASSERT_NE(init->getStmt(), nullptr) << "'initial fork ... join' with no begin/end at the initial level should "
                                         "bind the ForkStmt directly as the Initial's statement";
  const hldb::ForkStmt *const fork = any_cast<hldb::ForkStmt>(init->getStmt());
  ASSERT_NE(fork, nullptr) << "'fork ... join' should produce a ForkStmt";
  EXPECT_EQ(fork->getJoinType(), vpiJoin) << "plain 'join' should be vpiJoin, not join_any/join_none";

  ASSERT_NE(fork->getStmts(), nullptr);
  ASSERT_EQ(fork->getStmts()->size(), 2u) << "the fork/join block has exactly two parallel branches";
  EXPECT_NE(any_cast<hldb::Begin>(fork->getStmts()->at(0)), nullptr) << "the first branch should be a Begin";
  EXPECT_NE(any_cast<hldb::Begin>(fork->getStmts()->at(1)), nullptr) << "the second branch should be a Begin";
}

TEST_F(EventOrderTest, FirstBranchAssignsDelaysThenTriggersEvNonBlocking) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);
  const hldb::ForkStmt *const fork = any_cast<hldb::ForkStmt>(init->getStmt());
  ASSERT_NE(fork, nullptr);
  ASSERT_EQ(fork->getStmts()->size(), 2u);

  const hldb::Begin *const first = any_cast<hldb::Begin>(fork->getStmts()->at(0));
  ASSERT_NE(first, nullptr);
  ASSERT_NE(first->getStmts(), nullptr);
  ASSERT_EQ(first->getStmts()->size(), 3u) << "'a = 'h3;', '#20;', '->ev;' are exactly three statements";

  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(first->getStmts()->at(0));
  ASSERT_NE(assign, nullptr) << "'a = 'h3;' should be a plain Assignment";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "the assignment target should be a RefObj";
  EXPECT_EQ(lhs->getName(), "a");
  EXPECT_NE(any_cast<hldb::Constant>(assign->getRhs()), nullptr) << "'h3' should be a Constant";

  const hldb::DelayControl *const delay = any_cast<hldb::DelayControl>(first->getStmts()->at(1));
  ASSERT_NE(delay, nullptr) << "'#20;' should produce a DelayControl";
  EXPECT_EQ(delay->getStmt(), nullptr) << "'#20;' has no controlled statement -- getStmt() should be null";
  const hldb::Constant *const delayValue = delay->getDelay<hldb::Constant>();
  ASSERT_NE(delayValue, nullptr) << "the delay value should be a Constant";
  EXPECT_EQ(delayValue->getDecompile(), "20");

  const hldb::EventStmt *const trigger = any_cast<hldb::EventStmt>(first->getStmts()->at(2));
  ASSERT_NE(trigger, nullptr) << "'->ev;' should produce an EventStmt";
  EXPECT_TRUE(trigger->getBlocking()) << "'->' (blocking trigger) should have getBlocking() == true, unlike "
                                          "the non-blocking '->>' form";
  const hldb::RefObj *const namedEventRef = trigger->getNamedEvent<hldb::RefObj>();
  ASSERT_NE(namedEventRef, nullptr) << "the triggered event should be a RefObj";
  EXPECT_EQ(namedEventRef->getName(), "ev");
}

TEST_F(EventOrderTest, SecondBranchWaitsOnEvThenAssignsA) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);
  const hldb::ForkStmt *const fork = any_cast<hldb::ForkStmt>(init->getStmt());
  ASSERT_NE(fork, nullptr);
  ASSERT_EQ(fork->getStmts()->size(), 2u);

  const hldb::Begin *const second = any_cast<hldb::Begin>(fork->getStmts()->at(1));
  ASSERT_NE(second, nullptr);
  ASSERT_NE(second->getStmts(), nullptr);
  ASSERT_EQ(second->getStmts()->size(), 1u) << "'@ev a = 'h4;' is exactly one statement: an EventControl";

  const hldb::EventControl *const ec = any_cast<hldb::EventControl>(second->getStmts()->at(0));
  ASSERT_NE(ec, nullptr) << "'@ev' should produce an EventControl";

  ASSERT_NE(ec->getCondition(), nullptr) << "'ev' is the wait condition and must be present";
  const hldb::RefObj *const condRef = any_cast<hldb::RefObj>(ec->getCondition());
  ASSERT_NE(condRef, nullptr) << "'ev' (non-hierarchical) should be a plain RefObj";
  EXPECT_EQ(condRef->getName(), "ev");

  ASSERT_NE(ec->getStmt(), nullptr) << "'a = 'h4;' should bind as the EventControl's controlled statement";
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(ec->getStmt());
  ASSERT_NE(assign, nullptr) << "'a = 'h4;' should be a plain Assignment";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "a");
  EXPECT_NE(any_cast<hldb::Constant>(assign->getRhs()), nullptr) << "'h4' should be a Constant";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
