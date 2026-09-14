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
// tests/Google/chapter-9/9.3.3--fork_return.sv
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
// :name: fork_return
// :description: illegal return from fork
// :should_fail_because: illegal return from fork
// :tags: 9.3.3
// :type: simulation
// */
// module block_tb ();
// 	task fork_test;
// 		fork
// 			#20;
// 			return;
// 		join_none
// 	endtask
// endmodule
// ============================================================================
//
// IEEE 1800-2023 construct under test (Sec 9.3.3, "Parallel blocks", and
// Sec 13.4, "Return statement"): a task whose body is a "fork ... join_none"
// block containing a bare "return;" as one of its parallel branches. Per
// IEEE 1800-2023 Sec 13.4: "It shall be an error for a return statement to
// appear within a fork...join, fork...join_any, or fork...join_none
// construct" -- the source's own ":should_fail_because:" annotation
// documents exactly this illegality.
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - module "block_tb" exists.
//   - task "fork_test" is declared in module block_tb (via
//     Instance::getTaskFuncs()), has no return type, and is specifically a
//     Task (not a Function).
//   - fork_test's body is a ForkStmt with getJoinType() == vpiJoinNone (the
//     "join_none" keyword), containing exactly two statements: a bare
//     DelayControl ("#20;", getStmt() == null) and a ReturnStmt (bare
//     "return;", getCondition() == null -- no return value, matching a
//     task's void return type).
//   - hlc's own ErrorReporting/ErrorDefinition.h (an API, not a log file)
//     was inspected for a dedicated diagnostic covering "return statement
//     inside fork...join_none"; no such ErrorType constant exists in this
//     build, so no findError() check is made here -- inventing an
//     ErrorType to search for would not be standard-derived.
//
// NOT CHECKED (out of scope; every assertion below states only what IEEE
// 1800-2023 requires -- none of it is based on reading a .log file or any
// other tool-output dump):
//   - Whether HLC actually rejects this construct as illegal. The source's
//     own ":type: simulation" tag and ":should_fail_because:" annotation
//     indicate the illegality is meant to be caught by a simulator, not
//     necessarily by this compiler/elaborator; HLC has no simulation
//     capability, so no execution ever happens for this test to observe,
//     and (per the point above) no matching compile-time diagnostic exists
//     to check for either.
// ============================================================================

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/delay_control.h>
#include <hldb/design.h>
#include <hldb/fork_stmt.h>
#include <hldb/module.h>
#include <hldb/return_stmt.h>
#include <hldb/task.h>
#include <hldb/task_func.h>

#include <hlc/Tests/Test.h>

namespace hlc {
class ForkReturnTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.3.3--fork_return.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to ForkReturnTest go here!

TEST_F(ForkReturnTest, ModuleBlockTbExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'block_tb' not found";
}

TEST_F(ForkReturnTest, TaskForkTestExistsAndIsATask) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  ASSERT_EQ(top->getTaskFuncs()->size(), 1u) << "module block_tb declares exactly one task/function: fork_test";

  const hldb::TaskFunc *const method = top->getTaskFuncs()->front();
  ASSERT_NE(method, nullptr);
  const hldb::Task *const forkTest = any_cast<hldb::Task>(method);
  ASSERT_NE(forkTest, nullptr) << "'task fork_test;' has no return type, so it should be a Task, not a Function";
  EXPECT_EQ(forkTest->getName(), "fork_test");
}

TEST_F(ForkReturnTest, ForkTestBodyIsJoinNoneForkWithDelayAndReturn) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Task *const forkTest = any_cast<hldb::Task>(top->getTaskFuncs()->front());
  ASSERT_NE(forkTest, nullptr);

  ASSERT_NE(forkTest->getStmt(), nullptr) << "fork_test's single statement (the fork/join_none block) should bind "
                                              "directly, no begin/end used";
  const hldb::ForkStmt *const fork = any_cast<hldb::ForkStmt>(forkTest->getStmt());
  ASSERT_NE(fork, nullptr) << "'fork ... join_none' should produce a ForkStmt";
  EXPECT_EQ(fork->getJoinType(), vpiJoinNone) << "'join_none' should be vpiJoinNone, not join/join_any";

  ASSERT_NE(fork->getStmts(), nullptr);
  ASSERT_EQ(fork->getStmts()->size(), 2u) << "'#20;' and 'return;' are exactly two parallel statements";

  const hldb::DelayControl *const delay = any_cast<hldb::DelayControl>(fork->getStmts()->at(0));
  ASSERT_NE(delay, nullptr) << "'#20;' should produce a DelayControl";
  EXPECT_EQ(delay->getStmt(), nullptr) << "'#20;' has no controlled statement -- getStmt() should be null";

  const hldb::ReturnStmt *const ret = any_cast<hldb::ReturnStmt>(fork->getStmts()->at(1));
  ASSERT_NE(ret, nullptr) << "'return;' should produce a ReturnStmt";
  EXPECT_EQ(ret->getCondition(), nullptr) << "bare 'return;' (task, void) has no return value -- getCondition() "
                                              "should be null";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
