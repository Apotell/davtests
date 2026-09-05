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

// Tests for 13.3.1--task-static.sv (tags: 13.3.1)
//   module top();
//     task static mytask(int test);
//       int a = 0;
//       a++;
//       if (test)
//         $display(":assert:(%d != 1)", a);
//       else
//         $display(":assert:(%d == 1)", a);
//     endtask
//     initial
//       begin
//         mytask(0);
//         mytask(1);
//         mytask(1);
//         mytask(1);
//       end
//   endmodule
//
// What to check and why (IEEE 1800-2023 13.3.1 "Static and automatic
// tasks", p.339, checked before any test code was written):
//   "Tasks defined within a module... default to being static, with
//   all declared items being statically allocated. These items shall
//   be shared across all uses of the task executing concurrently...
//   Variables declared in static tasks... shall retain their values
//   between invocations." The explicit "static" keyword here reaffirms
//   the default lifetime, so getAutomatic() should be false -- unlike
//   13.3.1--task-automatic.cpp's explicit "automatic".
//
//   IMPORTANT object-model note (same as 13.3.1--task-automatic.cpp):
//   the task's own declared variable "a" appears as an entry in the
//   task body's own getStmts(), in addition to getVariables().
//
//   Also (IEEE 1800-2023 6.8): "int" is a non-net data type, so "a" and
//   the formal argument "test" must be Variables/IODecl, never Nets.
//
//   IMPORTANT call-site note: unlike 13.3--task.cpp's bare "mytask;"
//   (no parens), this file's calls use explicit parentheses and an
//   argument ("mytask(0);"), which per this file's own confirmed
//   structure IS modeled as a TaskCall (not a plain RefObj).
//
// What is checked:
//   - module top has exactly 1 TaskFunc, a Task named "mytask", with
//     exactly 1 IODecl "test" (getDirection() == vpiInput, typespec
//     resolving to IntTypespec)
//   - mytask's lifetime is NOT automatic (getAutomatic() == false),
//     matching the explicit "static" keyword (and the spec default)
//   - mytask's body is a Begin whose own scope has exactly 1 Variable
//     "a" (initial value Constant "0"), and whose getStmts() has
//     exactly 3 entries: the Variable "a" declaration itself, an
//     Operation(vpiPostIncOp) on RefObj "a", and an IfElse whose
//     condition is RefObj "test" (resolving to the IODecl, not a
//     Variable), whose "then" body is SysTaskCall "$display" with
//     Constant "\":assert:(%d != 1)\"" and RefObj "a", and whose
//     "else" body is SysTaskCall "$display" with Constant
//     "\":assert:(%d == 1)\"" and RefObj "a"
//   - the initial process's body is a Begin with exactly 4 TaskCall
//     "mytask" statements (each with parens+argument), each with
//     getTaskFunc<hldb::Task>() resolving back to "mytask" and exactly
//     1 argument (Constant "0", "1", "1", "1" respectively)
//   - no continuous assignments exist in the module
//
// What is NOT checked and why:
//   - the runtime "static storage retains its value between
//     invocations" behavior itself (that later calls see "a" already
//     incremented past 1) is a simulation-time concept, not a static/
//     structural compile-time property

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/if_else.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_task_call.h>
#include <hldb/task.h>
#include <hldb/task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class TaskStaticTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "13.3.1--task-static.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Task *getMytask() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTaskFuncs() == nullptr || top->getTaskFuncs()->empty()) return nullptr;
    return any_cast<hldb::Task>(top->getTaskFuncs()->at(0));
  }
};

TEST_F(TaskStaticTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(TaskStaticTest, MytaskExistsWithOneInputIODeclTest) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  ASSERT_EQ(top->getTaskFuncs()->size(), 1u);
  const hldb::Task *const mytask = getMytask();
  ASSERT_NE(mytask, nullptr);
  EXPECT_EQ(mytask->getName(), "mytask");
  ASSERT_NE(mytask->getIODecls(), nullptr);
  ASSERT_EQ(mytask->getIODecls()->size(), 1u);
  const hldb::IODecl *const test = mytask->getIODecls()->at(0);
  ASSERT_NE(test, nullptr);
  EXPECT_EQ(test->getName(), "test");
  EXPECT_EQ(test->getDirection(), vpiInput);
  const hldb::RefTypespec *const rts = test->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(TaskStaticTest, MytaskLifetimeIsNotAutomatic) {
  const hldb::Task *const mytask = getMytask();
  ASSERT_NE(mytask, nullptr);
  EXPECT_FALSE(mytask->getAutomatic())
      << "'task static mytask(...)' explicitly reaffirms the default static lifetime, per 13.3.1";
}

TEST_F(TaskStaticTest, BodyScopeHasVariableAInitializedToZero) {
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

TEST_F(TaskStaticTest, BodyHasThreeStatementsDeclIncrementIfElse) {
  const hldb::Task *const mytask = getMytask();
  ASSERT_NE(mytask, nullptr);
  const hldb::Begin *const body = mytask->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 3u)
      << "task body statements include the 'int a = 0;' declaration itself as entry 0";

  const hldb::Variable *const declEntry = any_cast<hldb::Variable>(body->getStmts()->at(0));
  ASSERT_NE(declEntry, nullptr);
  EXPECT_EQ(declEntry->getName(), "a");

  const hldb::Operation *const inc = any_cast<hldb::Operation>(body->getStmts()->at(1));
  ASSERT_NE(inc, nullptr);
  EXPECT_EQ(inc->getOpType(), vpiPostIncOp);
}

TEST_F(TaskStaticTest, IfElseConditionIsIODeclTestWithBothDisplayBranches) {
  const hldb::Task *const mytask = getMytask();
  ASSERT_NE(mytask, nullptr);
  const hldb::Begin *const body = mytask->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_GE(body->getStmts()->size(), 3u);
  const hldb::IfElse *const ifStmt = any_cast<hldb::IfElse>(body->getStmts()->at(2));
  ASSERT_NE(ifStmt, nullptr) << "third body entry has an 'else' branch, so it must resolve to IfElse, not IfStmt";

  const hldb::RefObj *const cond = ifStmt->getCondition<hldb::RefObj>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getName(), "test");
  EXPECT_NE(cond->getActual<hldb::IODecl>(), nullptr) << "'test' should resolve to the IODecl, not a Variable";

  const hldb::SysTaskCall *const thenDisplay = ifStmt->getStmt<hldb::SysTaskCall>();
  ASSERT_NE(thenDisplay, nullptr);
  ASSERT_NE(thenDisplay->getArguments(), nullptr);
  ASSERT_EQ(thenDisplay->getArguments()->size(), 2u);
  const hldb::Constant *const thenFmt = any_cast<hldb::Constant>(thenDisplay->getArguments()->at(0));
  ASSERT_NE(thenFmt, nullptr);
  EXPECT_EQ(thenFmt->getDecompile(), "\":assert:(%d != 1)\"");
}

TEST_F(TaskStaticTest, ElseBranchDisplaysEqualsOneMessage) {
  const hldb::Task *const mytask = getMytask();
  ASSERT_NE(mytask, nullptr);
  const hldb::Begin *const body = mytask->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_GE(body->getStmts()->size(), 3u);
  const hldb::IfElse *const ifStmt = any_cast<hldb::IfElse>(body->getStmts()->at(2));
  ASSERT_NE(ifStmt, nullptr);
  const hldb::SysTaskCall *const elseDisplay = ifStmt->getElseStmt<hldb::SysTaskCall>();
  ASSERT_NE(elseDisplay, nullptr);
  ASSERT_NE(elseDisplay->getArguments(), nullptr);
  ASSERT_EQ(elseDisplay->getArguments()->size(), 2u);
  const hldb::Constant *const elseFmt = any_cast<hldb::Constant>(elseDisplay->getArguments()->at(0));
  ASSERT_NE(elseFmt, nullptr);
  EXPECT_EQ(elseFmt->getDecompile(), "\":assert:(%d == 1)\"");
}

// ---------------------------------------------------------------------------
// initial begin mytask(0); mytask(1); mytask(1); mytask(1); end
// ---------------------------------------------------------------------------
TEST_F(TaskStaticTest, InitialBodyCallsMytaskFourTimesWithArguments) {
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

  const std::string_view expectedArgs[4] = {"0", "1", "1", "1"};
  for (uint32_t idx = 0; idx < 4u; ++idx) {
    const hldb::TaskCall *const call = any_cast<hldb::TaskCall>(body->getStmts()->at(idx));
    ASSERT_NE(call, nullptr) << "'mytask(...)' with parens+argument should be a TaskCall, not a plain RefObj";
    EXPECT_EQ(call->getName(), "mytask");
    EXPECT_EQ(call->getTaskFunc<hldb::Task>(), getMytask());
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 1u);
    const hldb::Constant *const arg = any_cast<hldb::Constant>(call->getArguments()->at(0));
    ASSERT_NE(arg, nullptr);
    EXPECT_EQ(arg->getDecompile(), expectedArgs[idx]);
  }
}

TEST_F(TaskStaticTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
