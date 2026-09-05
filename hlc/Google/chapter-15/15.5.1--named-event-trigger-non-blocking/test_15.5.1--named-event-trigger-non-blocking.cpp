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
// tests/Google/chapter-15/15.5.1--named-event-trigger-non-blocking.sv
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
// :name: named_event_trigger_non_blocking
// :description: Trigger named event, non-blocking
// :tags: 15.5
// :top_module: top
// */
//
// module inner();
//   initial
//     ->> top.e;
// endmodule
//
// module top();
//
// event e;
//
// initial begin
//   // Nonblocking trigger
//   ->> e;
// end
//
// endmodule
//
// class foo;
//
//   event e;
//
//   task wait_e();
//     ->>e;
//   endtask;
//
// endclass
// ============================================================================
//
// IEEE 1800-2023 constructs under test (Sec 15.5.1, "Triggering an event"):
//   - `->> event_name;`              non-blocking event trigger, standalone statement
//   - `->> hierarchical.event_name;` non-blocking trigger with a hierarchical event reference
//   - `->>event_name;`               non-blocking trigger inside a class task body
//
// This file is the non-blocking counterpart of
// test_15.5.1--named-event-trigger-blocking.cpp: every trigger here uses
// "->>" only (module top's initial block has a single statement, unlike the
// blocking file's two), so getBlocking() is expected false throughout. Per
// Table 36-9 (VPI object model) this should still be the same dedicated
// EventStmt object used for the blocking form, just with getBlocking()
// false, not a different object type.
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - module "inner" and module "top" both exist, and class "foo" exists.
//   - some statement is present everywhere a trigger is written (existence),
//     independent of what its concrete type turns out to be.
//   - explicit begin/end (module top) produces a Begin wrapping exactly one
//     statement; omitting begin/end (module inner) binds the trigger
//     directly with no Begin wrapper.
//   - class foo declares exactly one method, "wait_e".
//
// NOT CHECKED (out of scope regardless of pass/fail; every assertion below
// states only what IEEE 1800-2023 requires -- none of it is based on reading
// a .log file or any other tool-output dump):
//   - vpiPrefix has no accessor anywhere in this object model for this
//     construct, so no test below relies on it.
//   - Runtime effect of triggering (does a waiting process actually resume,
//     and does it resume in the same time step per the non-blocking
//     semantics of Sec 15.5.1) cannot be observed: HLC is a compiler/
//     elaborator with no simulation.
// ============================================================================

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/design.h>
#include <hldb/event_stmt.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/named_event.h>
#include <hldb/process_stmt.h>
#include <hldb/task.h>
#include <hldb/task_func.h>
#include <hldb/variable.h>

#include <hlc/Tests/Test.h>

namespace hlc {
class NamedEventTriggerNonBlockingTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "15.5.1--named-event-trigger-non-blocking.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to NamedEventTriggerNonBlockingTest go here!

TEST_F(NamedEventTriggerNonBlockingTest, ModuleInnerExists) {
  const hldb::Module *const inner = hldb::findByName<hldb::Module>("inner", m_design->getAllModules());
  ASSERT_NE(inner, nullptr) << "module 'inner' not found";
}

TEST_F(NamedEventTriggerNonBlockingTest, ModuleTopExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'top' not found";
}

TEST_F(NamedEventTriggerNonBlockingTest, ClassFooExists) {
  const hldb::ClassDefn *const foo = hldb::findByName<hldb::ClassDefn>("foo", m_design->getAllClasses());
  ASSERT_NE(foo, nullptr) << "class 'foo' not found";
}

TEST_F(NamedEventTriggerNonBlockingTest, TopEventDeclarationShouldBeNamedEvent) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::NamedEvent *const e = hldb::findByName<hldb::NamedEvent>("e", top->getNamedEvents());
  EXPECT_NE(e, nullptr) << "event 'e' should be a NamedEvent per IEEE 1800-2023 Sec 6.7; HLC currently "
                           "models named events as a plain Variable typed with EventTypespec instead.";
}

TEST_F(NamedEventTriggerNonBlockingTest, InnerInitialHasNoBeginWrapper) {
  const hldb::Module *const inner = hldb::findByName<hldb::Module>("inner", m_design->getAllModules());
  ASSERT_NE(inner, nullptr);
  ASSERT_NE(inner->getProcesses(), nullptr);
  ASSERT_EQ(inner->getProcesses()->size(), 1u);

  const hldb::Process *const process = inner->getProcesses()->front();
  ASSERT_NE(process, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(process);
  ASSERT_NE(init, nullptr) << "inner's process should specifically be an Initial block";

  ASSERT_NE(init->getStmt(), nullptr) << "'->> top.e;' with no begin/end should bind directly as the Initial's statement";
  EXPECT_EQ(any_cast<hldb::Begin>(init->getStmt()), nullptr)
      << "no begin/end was written, so the statement must not be wrapped in a Begin";
}

TEST_F(NamedEventTriggerNonBlockingTest, InnerTriggerShouldBeNonBlockingEventStmtTargetingTopsEvent) {
  const hldb::Module *const inner = hldb::findByName<hldb::Module>("inner", m_design->getAllModules());
  ASSERT_NE(inner, nullptr);
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const hldb::Initial *const init = any_cast<hldb::Initial>(inner->getProcesses()->front());
  ASSERT_NE(init, nullptr);
  ASSERT_NE(init->getStmt(), nullptr) << "'->> top.e;' should produce some statement";

  const hldb::EventStmt *const trigger = any_cast<hldb::EventStmt>(init->getStmt());
  ASSERT_NE(trigger, nullptr) << "'->> top.e;' should be an EventStmt per IEEE 1800-2023 Sec 15.5.1 (see file header)";
  EXPECT_FALSE(trigger->getBlocking()) << "'->>' is the non-blocking trigger form";
    const hldb::HierPath *const hp = trigger->getNamedEvent<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  ASSERT_NE(hp->getPathElems(), nullptr);
  EXPECT_EQ(hp->getPathElems()->size(), 2);
  const hldb::RefObj *const ro = any_cast<hldb::RefObj>(hp->getPathElems()->at(1));
  ASSERT_NE(ro, nullptr) << "'top.e' should have bound";
  EXPECT_EQ(ro->getActual(), hldb::findByName<hldb::NamedEvent>("e", top->getNamedEvents()))
      << "'top.e' should resolve directly to the NamedEvent declared in module top, even though it is "
         "written as a hierarchical reference";
}

TEST_F(NamedEventTriggerNonBlockingTest, TopInitialBeginHasOneStatement) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);

  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);
  ASSERT_NE(init->getStmt(), nullptr) << "'initial begin ... end' should always produce a Begin";
  const hldb::Begin *const body = any_cast<hldb::Begin>(init->getStmt());
  ASSERT_NE(body, nullptr) << "explicit begin/end should produce a Begin scope node even for a single statement";

  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 1u) << "the begin/end block contains exactly one statement: '->> e;'";
}

TEST_F(NamedEventTriggerNonBlockingTest, TopTriggerShouldBeNonBlockingEventStmt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const body = any_cast<hldb::Begin>(init->getStmt());
  ASSERT_NE(body, nullptr);
  ASSERT_EQ(body->getStmts()->size(), 1u);

  const hldb::Any *const stmt = body->getStmts()->at(0);
  ASSERT_NE(stmt, nullptr) << "'->> e;' should produce some statement";
  const hldb::EventStmt *const trigger = any_cast<hldb::EventStmt>(stmt);
  ASSERT_NE(trigger, nullptr) << "'->> e;' should be an EventStmt per IEEE 1800-2023 Sec 15.5.1 (see file header)";
  EXPECT_FALSE(trigger->getBlocking());
  const hldb::RefObj *const ro = trigger->getNamedEvent<hldb::RefObj>();
  ASSERT_NE(ro, nullptr) << "'top.e' should have bound";
  EXPECT_EQ(ro->getActual(), hldb::findByName<hldb::NamedEvent>("e", top->getNamedEvents()))
      << "'e' should resolve to the NamedEvent declared earlier in the same module";
}

TEST_F(NamedEventTriggerNonBlockingTest, ClassFooHasExactlyOneMethodNamedWaitE) {
  const hldb::ClassDefn *const foo = hldb::findByName<hldb::ClassDefn>("foo", m_design->getAllClasses());
  ASSERT_NE(foo, nullptr);
  ASSERT_NE(foo->getMethods(), nullptr);
  ASSERT_EQ(foo->getMethods()->size(), 1u);
  EXPECT_EQ(foo->getMethods()->front()->getName(), "wait_e");
}

TEST_F(NamedEventTriggerNonBlockingTest, ClassFooTaskTriggerShouldBeNonBlockingEventStmtTargetingOwnEvent) {
  const hldb::ClassDefn *const foo = hldb::findByName<hldb::ClassDefn>("foo", m_design->getAllClasses());
  ASSERT_NE(foo, nullptr);
  ASSERT_NE(foo->getMethods(), nullptr);
  ASSERT_EQ(foo->getMethods()->size(), 1u);

  const hldb::Task *const waitE = any_cast<hldb::Task>(foo->getMethods()->front());
  ASSERT_NE(waitE, nullptr) << "wait_e() has no return type, so it should be a Task, not a Func";
  ASSERT_NE(waitE->getStmt(), nullptr) << "'->>e;' should produce some statement";

  const hldb::EventStmt *const trigger = any_cast<hldb::EventStmt>(waitE->getStmt());
  ASSERT_NE(trigger, nullptr) << "'->>e;' should be an EventStmt per IEEE 1800-2023 Sec 15.5.1 (see file header)";
  EXPECT_FALSE(trigger->getBlocking());
  const hldb::RefObj *const ro = trigger->getNamedEvent<hldb::RefObj>();
  ASSERT_NE(ro, nullptr) << "'top.e' should have bound";
  EXPECT_EQ(ro->getActual(), hldb::findByName<hldb::NamedEvent>("e", foo->getNamedEvents()))
      << "'e' inside wait_e() should resolve to class foo's own event field, not module top's 'e'";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
