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

// Tests for 13.3.1--task-automatic.sv (tags: 13.3.1)
//   module top();
//     task automatic mytask;
//       int a = 0;
//       a++;
//       $display(":assert:(%d == 1)", a);
//     endtask
//     initial begin
//       mytask;
//       mytask;
//       mytask;
//       mytask;
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023 13.3.1 "Static and automatic
// tasks", p.339, checked before any test code was written):
//   "Tasks can be defined to use automatic storage... Explicitly
//   declared using the optional automatic keyword as part of the task
//   declaration... All items declared inside automatic tasks are
//   allocated dynamically for each invocation." The task's own
//   lifetime should therefore be recorded as automatic.
//
//   IMPORTANT compiler-behavior note, confirmed structurally in this
//   exact file (not guessed): the task's own declared variable "a" IS
//   listed as one of the entries in the task body's own getStmts()
//   (alongside "a++;" and the $display), in addition to appearing in
//   getVariables() -- this differs from what was found for a plain
//   "initial begin ... end" block in 12.7.4--while.cpp/
//   12.7.5--dowhile.cpp, where a declared variable did NOT also appear
//   in getStmts(). Task/function bodies apparently include their own
//   declarations as statement entries; ordinary begin-end blocks do
//   not. This shape is confirmed narrowly via this file's own log, not
//   assumed from the chapter-12 precedent.
//
//   Also (IEEE 1800-2023 6.8): "int" is a non-net data type, so "a"
//   must be a Variable, never a Net.
//
// What is checked:
//   - module top has exactly 1 TaskFunc, a Task named "mytask", with
//     no IODecls (no formal arguments)
//   - mytask's own lifetime SHOULD be automatic per the explicit
//     "automatic" keyword (getAutomatic() == true)
//   - mytask's body is a Begin whose own scope has exactly 1 Variable
//     "a" (initial value Constant "0"), and whose getStmts() has
//     exactly 3 entries: the Variable "a" declaration itself, an
//     Operation(vpiPostIncOp) on RefObj "a" ("a++;"), and a SysTaskCall
//     "$display" with 2 arguments (Constant "\":assert:(%d == 1)\"" and
//     RefObj "a", both resolving to the Variable)
//   - the initial process's body is a Begin with exactly 4 statements,
//     each a plain RefObj "mytask" resolving (getActual<hldb::Task>())
//     to the task -- matching the bare-task-enable shape confirmed in
//     13.3--task.cpp
//   - no continuous assignments exist in the module
//
// What is NOT checked and why:
//   - the runtime per-invocation re-initialization behavior of
//     automatic storage (that each of the 4 calls sees "a" freshly
//     re-declared as 0) is a simulation-time concept, not a static/
//     structural compile-time property

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/sys_task_call.h>
#include <hldb/task.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class TaskAutomaticTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "13.3.1--task-automatic.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Task *getMytask() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTaskFuncs() == nullptr || top->getTaskFuncs()->empty()) return nullptr;
    return any_cast<hldb::Task>(top->getTaskFuncs()->at(0));
  }
};

TEST_F(TaskAutomaticTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(TaskAutomaticTest, MytaskExistsWithNoIODecls) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  ASSERT_EQ(top->getTaskFuncs()->size(), 1u);
  const hldb::Task *const mytask = getMytask();
  ASSERT_NE(mytask, nullptr);
  EXPECT_EQ(mytask->getName(), "mytask");
  EXPECT_TRUE(mytask->getIODecls() == nullptr || mytask->getIODecls()->empty());
}

TEST_F(TaskAutomaticTest, MytaskLifetimeIsAutomatic) {
  const hldb::Task *const mytask = getMytask();
  ASSERT_NE(mytask, nullptr);
  EXPECT_TRUE(mytask->getAutomatic()) << "'task automatic mytask;' should record automatic lifetime, per 13.3.1";
}

TEST_F(TaskAutomaticTest, BodyScopeHasVariableAInitializedToZero) {
  const hldb::Task *const mytask = getMytask();
  ASSERT_NE(mytask, nullptr);
  const hldb::Begin *const body = mytask->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "task body should be a Begin";
  ASSERT_NE(body->getVariables(), nullptr);
  ASSERT_EQ(body->getVariables()->size(), 1u);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", body->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getDecompile(), "0");
}

TEST_F(TaskAutomaticTest, BodyHasThreeStatementsDeclIncrementDisplay) {
  const hldb::Task *const mytask = getMytask();
  ASSERT_NE(mytask, nullptr);
  const hldb::Begin *const body = mytask->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 3u)
      << "task body statements include the 'int a = 0;' declaration itself as entry 0";

  const hldb::Variable *const declEntry = any_cast<hldb::Variable>(body->getStmts()->at(0));
  ASSERT_NE(declEntry, nullptr) << "first getStmts() entry should be the Variable 'a' declaration itself";
  EXPECT_EQ(declEntry->getName(), "a");

  const hldb::Operation *const inc = any_cast<hldb::Operation>(body->getStmts()->at(1));
  ASSERT_NE(inc, nullptr) << "'a++;' should be a standalone post-increment Operation statement";
  EXPECT_EQ(inc->getOpType(), vpiPostIncOp);
  ASSERT_NE(inc->getOperands(), nullptr);
  ASSERT_EQ(inc->getOperands()->size(), 1u);
  const hldb::RefObj *const incOperand = any_cast<hldb::RefObj>(inc->getOperands()->at(0));
  ASSERT_NE(incOperand, nullptr);
  EXPECT_EQ(incOperand->getName(), "a");

  const hldb::SysTaskCall *const display = any_cast<hldb::SysTaskCall>(body->getStmts()->at(2));
  ASSERT_NE(display, nullptr);
  EXPECT_EQ(display->getName(), "$display");
  ASSERT_NE(display->getArguments(), nullptr);
  ASSERT_EQ(display->getArguments()->size(), 2u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(display->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getDecompile(), "\":assert:(%d == 1)\"");
  const hldb::RefObj *const aArg = any_cast<hldb::RefObj>(display->getArguments()->at(1));
  ASSERT_NE(aArg, nullptr);
  EXPECT_EQ(aArg->getName(), "a");
  EXPECT_NE(aArg->getActual<hldb::Variable>(), nullptr);
}

// ---------------------------------------------------------------------------
// initial begin mytask; mytask; mytask; mytask; end
// ---------------------------------------------------------------------------
TEST_F(TaskAutomaticTest, InitialBodyCallsMytaskFourTimes) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const initial = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(initial, nullptr);
  const hldb::Begin *const body = initial->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "initial body should be a Begin (explicit begin-end in source)";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 4u);
  for (uint32_t idx = 0; idx < 4u; ++idx) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(body->getStmts()->at(idx));
    ASSERT_NE(ref, nullptr) << "bare 'mytask;' should be a plain RefObj, not a TaskCall";
    EXPECT_EQ(ref->getName(), "mytask");
    EXPECT_NE(ref->getActual<hldb::Task>(), nullptr);
  }
}

TEST_F(TaskAutomaticTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
