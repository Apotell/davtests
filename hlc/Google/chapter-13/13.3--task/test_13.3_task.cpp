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

// Tests for 13.3--task.sv (tags: 13.3)
//   module top();
//     task mytask;
//       $display(":assert: True");
//     endtask
//     initial
//       mytask;
//   endmodule
//
// What to check and why (IEEE 1800-2023 13.3 "Tasks", p.335-338,
// checked before any test code was written):
//   "A task exits when the endtask is reached... Multiple statements
//   can be written between the task declaration and endtask.
//   Statements are executed sequentially, the same as if they were
//   enclosed in a begin...end group. It shall also be legal to have no
//   statements at all." Here "mytask" has exactly one statement
//   ($display), so no begin-end wrapping is needed and the task's own
//   getStmt() is the SysTaskCall directly.
//
//   IMPORTANT object-model note, confirmed structurally in this exact
//   file (not guessed): the bare task-enable statement "mytask;" (no
//   parentheses, no arguments) is modeled as a plain RefObj resolving
//   to the Task -- NOT as a TaskCall. This differs from
//   13.3.1--task-static.cpp's "mytask(0);", which uses explicit
//   parentheses/arguments and IS modeled as a TaskCall. This is a
//   tool-specific object-model shape, not something the standard
//   itself dictates, so it is confirmed via this file's own structure
//   rather than assumed from another file's shape.
//
// What is checked:
//   - module top has exactly 1 TaskFunc, a Task named "mytask", with
//     no IODecls (no formal arguments in the source)
//   - mytask's body is a plain SysTaskCall "$display" with 1 argument,
//     Constant "\":assert: True\"" (no Begin wrapper, single statement)
//   - the initial process's single statement is a plain RefObj
//     "mytask" resolving (getActual<hldb::Task>()) to the task itself
//   - no continuous assignments, nets, or variables exist in the module
//
// What is NOT checked and why:
//   - the runtime execution of $display (that it actually prints
//     ":assert: True") is a simulation-time concept, not a static/
//     structural compile-time property

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/sys_task_call.h>
#include <hldb/task.h>
#include <hldb/vpi_user.h>

namespace hlc {

class TaskTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "13.3--task.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Task *getMytask() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTaskFuncs() == nullptr || top->getTaskFuncs()->empty()) return nullptr;
    return any_cast<hldb::Task>(top->getTaskFuncs()->at(0));
  }
};

TEST_F(TaskTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(TaskTest, ModuleHasNoNetsAndNoVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty());
  EXPECT_TRUE(top->getVariables() == nullptr || top->getVariables()->empty());
}

TEST_F(TaskTest, MytaskExistsWithNoIODecls) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  ASSERT_EQ(top->getTaskFuncs()->size(), 1u);
  const hldb::Task *const mytask = getMytask();
  ASSERT_NE(mytask, nullptr) << "'mytask' should resolve to a Task";
  EXPECT_EQ(mytask->getName(), "mytask");
  EXPECT_TRUE(mytask->getIODecls() == nullptr || mytask->getIODecls()->empty())
      << "'task mytask;' declares no formal arguments";
}

TEST_F(TaskTest, MytaskBodyIsDisplayStatement) {
  const hldb::Task *const mytask = getMytask();
  ASSERT_NE(mytask, nullptr);
  const hldb::SysTaskCall *const display = mytask->getStmt<hldb::SysTaskCall>();
  ASSERT_NE(display, nullptr) << "task body should be a plain SysTaskCall (single statement, no begin-end)";
  EXPECT_EQ(display->getName(), "$display");
  ASSERT_NE(display->getArguments(), nullptr);
  ASSERT_EQ(display->getArguments()->size(), 1u);
  const hldb::Constant *const arg = any_cast<hldb::Constant>(display->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getDecompile(), "\":assert: True\"");
}

// ---------------------------------------------------------------------------
// initial mytask;  -- bare task-enable, modeled as a plain RefObj
// ---------------------------------------------------------------------------
TEST_F(TaskTest, InitialBodyIsRefObjResolvingToMytask) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const initial = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(initial, nullptr);
  const hldb::RefObj *const ref = initial->getStmt<hldb::RefObj>();
  ASSERT_NE(ref, nullptr) << "bare 'mytask;' (no parens) should be a plain RefObj, not a TaskCall";
  EXPECT_EQ(ref->getName(), "mytask");
  EXPECT_NE(ref->getActual<hldb::Task>(), nullptr) << "'mytask' should resolve to the Task itself";
}

TEST_F(TaskTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
