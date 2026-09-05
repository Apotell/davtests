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
// tests/Google/chapter-15/15.4--mailbox-blocking.sv
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
// :name: mailbox_blocking
// :description: blocking mailbox test
// :tags: 15.4
// */
// module top();
//
// mailbox m;
//
// initial begin
//   m = new();
//   string msg = "abc";
//   string r;
//   string r_peek;
//   m.put(msg);
//   m.peek(r_peek);
//   $display(":assert: (%d == 1)", m.num());
//   m.get(r);
//   $display(":assert: ('%s' == '%s')", r, r_peek);
// end
//
// endmodule
// ============================================================================
//
// IEEE 1800-2023 constructs under test (Sec 15.4, "Mailboxes"):
//   - `mailbox m;`      declaration of an (unparameterized/generic) mailbox handle
//   - `m = new();`      mailbox construction
//   - `m.put(msg);`     blocking put -- suspends the caller until room is available
//   - `m.peek(r_peek);` blocking, non-destructive read of the front message
//   - `m.num()`         returns the number of messages currently queued
//   - `m.get(r);`       blocking get -- suspends the caller until a message is available
//
// `mailbox` is a builtin SystemVerilog class per Sec 15.4: it is usable
// directly, with no import or user declaration required.
//
// ----------------------------------------------------------------------------
// Every assertion below states only what IEEE 1800-2023 Sec 15.4 requires --
// none of it is based on reading a .log file or any other tool-output dump.
// `mailbox m;` is legal with no special import, so module "top" and its
// full body (construction, put/peek/get/num, the local string variables)
// are all expected to exist and to have the shapes asserted below.
//
// NOT CHECKED (out of scope regardless of the above):
//   - The exact object type produced by `new()` for a builtin class: no
//     header anywhere in build/include/hldb/ models a class-construction
//     expression (no "new"/"class_new" class exists), so the assignment's
//     right-hand side is only checked for presence, not for a specific type.
//   - vpiPrefix (which handle a method call such as "m.put(...)" targets)
//     has no accessor anywhere on TFCall/MethodTaskCall/MethodFuncCall in
//     this object model, so which variable owns each method call cannot be
//     checked beyond what is asserted here.
//   - Runtime values: whether num() actually reports 1 after a put(), or
//     whether get() actually returns the same message peek() saw, cannot be
//     observed -- HLC is a compiler/elaborator with no simulation.
// ============================================================================

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/class_typespec.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/method_func_call.h>
#include <hldb/method_task_call.h>
#include <hldb/module.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>

#include <hlc/Tests/Test.h>

namespace hlc {
class MailboxBlockingTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "15.4--mailbox-blocking.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to MailboxBlockingTest go here!

TEST_F(MailboxBlockingTest, ModuleTopShouldExistButDoesNot) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'top' should exist -- 'mailbox m;' is legal per IEEE 1800-2023 "
                              "Sec 15.4, but HLC's parser currently rejects it with a syntax error "
                              "(see file header), so no module in this design ends up named 'top'.";
}

TEST_F(MailboxBlockingTest, MailboxVariableShouldHaveMailboxClassTypeButDoesNot) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const hldb::Variable *const m = hldb::findByName<hldb::Variable>("m", top->getVariables());
  ASSERT_NE(m, nullptr) << "'mailbox m;' should declare a Variable named 'm' inside module top";

  ASSERT_NE(m->getTypespec(), nullptr) << "'m' should have a typespec reference";
  const hldb::Typespec *const actual = m->getTypespec()->getActual();
  ASSERT_NE(actual, nullptr) << "'m's typespec reference should resolve to something";
  const hldb::ClassTypespec *const classType = any_cast<hldb::ClassTypespec>(actual);
  ASSERT_NE(classType, nullptr) << "'m' should be typed with a ClassTypespec (its declared type is the "
                                    "builtin class 'mailbox')";
  EXPECT_EQ(classType->getName(), "mailbox");
  EXPECT_NE(classType->getClassDefn(), nullptr) << "'mailbox' should resolve to a builtin ClassDefn";
}

TEST_F(MailboxBlockingTest, ConstructionAssignmentShouldExistButDoesNot) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u) << "module top should have exactly one process (the initial block)";

  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);
  ASSERT_NE(init->getStmt(), nullptr) << "'initial begin ... end' should always produce a Begin";
  const hldb::Begin *const body = any_cast<hldb::Begin>(init->getStmt());
  ASSERT_NE(body, nullptr) << "explicit begin/end should produce a Begin scope node";

  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 6u)
      << "the begin/end block should contain exactly six statements: 'm = new();', 'm.put(msg);', "
         "'m.peek(r_peek);', the first $display, 'm.get(r);', the second $display -- the three local "
         "'string' declarations (msg, r, r_peek) are variable declarations, not statements";

  ASSERT_FALSE(body->getStmts()->empty());
  const hldb::Any *const firstStmt = body->getStmts()->at(0);
  ASSERT_NE(firstStmt, nullptr) << "'m = new();' should produce some statement";
  const hldb::Assignment *const construct = any_cast<hldb::Assignment>(firstStmt);
  ASSERT_NE(construct, nullptr) << "'m = new();' should be an Assignment";

  ASSERT_NE(construct->getLhs(), nullptr);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(construct->getLhs());
  ASSERT_NE(lhs, nullptr) << "the assignment target should be a RefObj";
  EXPECT_EQ(lhs->getName(), "m");

  EXPECT_NE(construct->getRhs(), nullptr) << "'new()' should produce some right-hand-side expression "
                                              "(see file header: no dedicated 'new' object type exists "
                                              "in this object model to check further than presence)";
}

TEST_F(MailboxBlockingTest, LocalStringVariablesShouldExistButDoNot) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const body = any_cast<hldb::Begin>(init->getStmt());
  ASSERT_NE(body, nullptr);

  EXPECT_NE(hldb::findByName<hldb::Variable>("msg", body->getVariables()), nullptr)
      << "'string msg = \"abc\";' should declare a local Variable 'msg' in the begin/end block's scope";
  EXPECT_NE(hldb::findByName<hldb::Variable>("r", body->getVariables()), nullptr)
      << "'string r;' should declare a local Variable 'r'";
  EXPECT_NE(hldb::findByName<hldb::Variable>("r_peek", body->getVariables()), nullptr)
      << "'string r_peek;' should declare a local Variable 'r_peek'";
}

TEST_F(MailboxBlockingTest, PutShouldBeBlockingMethodTaskCallWithOneArgumentButDoesNot) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const body = any_cast<hldb::Begin>(init->getStmt());
  ASSERT_NE(body, nullptr);
  ASSERT_EQ(body->getStmts()->size(), 6u);

  const hldb::Any *const secondStmt = body->getStmts()->at(1);
  ASSERT_NE(secondStmt, nullptr) << "'m.put(msg);' should produce some statement";
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>(secondStmt);
  ASSERT_NE(hp, nullptr) << "m.put(msg) should be a HierPath";
  ASSERT_NE(hp->getPathElems(), nullptr) << "m.put(msg) should be non-empty path";
  EXPECT_EQ(hp->getPathElems()->size(), 2) << "m.put(msg) should have exactly 2 path elements";
  const hldb::RefObj *const ro = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(ro, nullptr) << "0th element in m.put(msg) should be a reference";
  EXPECT_NE(ro->getActual(), nullptr);
  EXPECT_EQ(ro->getActual(), hldb::findByName<hldb::Variable>("m", top->getVariables()));
  const hldb::MethodTaskCall *const put = any_cast<hldb::MethodTaskCall>(hp->getPathElems()->at(1));
  ASSERT_NE(put, nullptr) << "put() is a task (Sec 15.4: it may block), so this should be a MethodTaskCall";
  EXPECT_EQ(put->getName(), "put");
  ASSERT_NE(put->getArguments(), nullptr);
  ASSERT_EQ(put->getArguments()->size(), 1u) << "put() takes exactly one argument, the message";
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(put->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), "msg");
}

TEST_F(MailboxBlockingTest, PeekShouldBeBlockingMethodTaskCallWithOneArgumentButDoesNot) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const body = any_cast<hldb::Begin>(init->getStmt());
  ASSERT_NE(body, nullptr);
  ASSERT_EQ(body->getStmts()->size(), 6u);

  const hldb::Any *const thirdStmt = body->getStmts()->at(2);
  ASSERT_NE(thirdStmt, nullptr) << "'m.peek(r_peek);' should produce some statement";
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>(thirdStmt);
  ASSERT_NE(hp, nullptr) << "m.put(msg) should be a HierPath";
  ASSERT_NE(hp->getPathElems(), nullptr) << "m.put(msg) should be non-empty path";
  EXPECT_EQ(hp->getPathElems()->size(), 2) << "m.put(msg) should have exactly 2 path elements";
  const hldb::RefObj *const ro = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(ro, nullptr) << "0th element in m.put(msg) should be a reference";
  EXPECT_NE(ro->getActual(), nullptr);
  EXPECT_EQ(ro->getActual(), hldb::findByName<hldb::Variable>("m", top->getVariables()));
  const hldb::MethodTaskCall *const peek = any_cast<hldb::MethodTaskCall>(hp->getPathElems()->at(1));
  // Sec 15.4: peek() blocks like get(), but does not remove the message from
  // the mailbox -- that non-destructive behavior is a runtime effect and
  // cannot be checked here (see file header).
  ASSERT_NE(peek, nullptr) << "peek() is a task (Sec 15.4: it may block), so this should be a MethodTaskCall";
  EXPECT_EQ(peek->getName(), "peek");
  ASSERT_NE(peek->getArguments(), nullptr);
  ASSERT_EQ(peek->getArguments()->size(), 1u) << "peek() takes exactly one argument, the output variable";
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(peek->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), "r_peek");
}

TEST_F(MailboxBlockingTest, GetShouldBeBlockingMethodTaskCallWithOneArgumentButDoesNot) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const body = any_cast<hldb::Begin>(init->getStmt());
  ASSERT_NE(body, nullptr);
  ASSERT_EQ(body->getStmts()->size(), 6u);

  const hldb::Any *const fifthStmt = body->getStmts()->at(4);
  ASSERT_NE(fifthStmt, nullptr) << "'m.get(r);' should produce some statement";
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>(fifthStmt);
  ASSERT_NE(hp, nullptr) << "m.put(msg) should be a HierPath";
  ASSERT_NE(hp->getPathElems(), nullptr) << "m.put(msg) should be non-empty path";
  EXPECT_EQ(hp->getPathElems()->size(), 2) << "m.put(msg) should have exactly 2 path elements";
  const hldb::RefObj *const ro = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(ro, nullptr) << "0th element in m.put(msg) should be a reference";
  EXPECT_NE(ro->getActual(), nullptr);
  EXPECT_EQ(ro->getActual(), hldb::findByName<hldb::Variable>("m", top->getVariables()));
  const hldb::MethodTaskCall *const get = any_cast<hldb::MethodTaskCall>(hp->getPathElems()->at(1));
  ASSERT_NE(get, nullptr) << "get() is a task (Sec 15.4: it blocks until a message is available), so this "
                              "should be a MethodTaskCall";
  EXPECT_EQ(get->getName(), "get");
  ASSERT_NE(get->getArguments(), nullptr);
  ASSERT_EQ(get->getArguments()->size(), 1u) << "get() takes exactly one argument, the output variable";
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(get->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), "r");
}

TEST_F(MailboxBlockingTest, NumShouldBeMethodFuncCallUsedAsDisplayArgumentButDoesNot) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const body = any_cast<hldb::Begin>(init->getStmt());
  ASSERT_NE(body, nullptr);
  ASSERT_EQ(body->getStmts()->size(), 6u);

  const hldb::Any *const fourthStmt = body->getStmts()->at(3);
  ASSERT_NE(fourthStmt, nullptr) << "'$display(\":assert: (%d == 1)\", m.num());' should produce some statement";
  const hldb::SysTaskCall *const display = any_cast<hldb::SysTaskCall>(fourthStmt);
  ASSERT_NE(display, nullptr) << "'$display(...)' should be a SysTaskCall";
  EXPECT_EQ(display->getName(), "$display");
  ASSERT_NE(display->getArguments(), nullptr);
  ASSERT_EQ(display->getArguments()->size(), 2u) << "$display() here takes the format string plus m.num()";

  const hldb::Any *const secondArg = display->getArguments()->at(1);
  ASSERT_NE(secondArg, nullptr) << "'m.num()' should produce some expression";
    const hldb::HierPath *const hp = any_cast<hldb::HierPath>(secondArg);
  ASSERT_NE(hp, nullptr) << "m.put(msg) should be a HierPath";
  ASSERT_NE(hp->getPathElems(), nullptr) << "m.put(msg) should be non-empty path";
  EXPECT_EQ(hp->getPathElems()->size(), 2) << "m.put(msg) should have exactly 2 path elements";
  const hldb::RefObj *const ro = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(ro, nullptr) << "0th element in m.put(msg) should be a reference";
  EXPECT_NE(ro->getActual(), nullptr);
  EXPECT_EQ(ro->getActual(), hldb::findByName<hldb::Variable>("m", top->getVariables()));
  const hldb::MethodFuncCall *const num = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(num, nullptr) << "num() returns a value (Sec 15.4: the number of queued messages), so this "
                              "should be a MethodFuncCall, not a MethodTaskCall";
  EXPECT_EQ(num->getName(), "num");
  EXPECT_TRUE(num->getArguments() == nullptr || num->getArguments()->empty()) << "num() takes no arguments";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
