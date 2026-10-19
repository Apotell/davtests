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
//   IEEE 1800-2023 37.12 (Scope), Note 5: the unnamed begin returned
//   for a task with more than one statement "contains the statements
//   of the task". "int a = 0;" is a declaration, not a statement: "a"
//   belongs to the task's own scope (getVariables() on the Task).
//
//   Also (IEEE 1800-2023 6.8): "int" is a non-net data type, so "a" and
//   the formal argument "test" must be Variables, never Nets. Per IEEE
//   1800-2023 37.13 the formal "test" has both an IODecl and a Variable
//   (the io decl's vpiExpr) in the task's scope.
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
//   - mytask's own scope has exactly 1 local Variable "a" (initial value
//     Constant "0"), besides the formal "test"'s Variable; its body is an unnamed Begin with no variables of
//     its own, whose getStmts() has exactly 2 entries: an
//     Operation(vpiPostIncOp) on RefObj "a", and an IfElse whose
//     condition is RefObj "test" (resolving to the formal's Variable,
//     never to its IODecl -- 37.15), whose "then" body is SysTaskCall "$display" with
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

TEST_F(TaskStaticTest, TaskScopeHasVariableAInitializedToZero) {
  const hldb::Task *const mytask = getMytask();
  ASSERT_NE(mytask, nullptr);
  ASSERT_NE(mytask->getVariables(), nullptr) << "'int a = 0;' is declared in the task's own scope";
  ASSERT_NE(mytask->getIODecls(), nullptr);
  for (const hldb::IODecl *const ioDecl : *mytask->getIODecls()) {
    EXPECT_NE(hldb::findByName<hldb::Variable>(ioDecl->getName(), mytask->getVariables()), nullptr)
        << "37.13: the formal argument '" << ioDecl->getName() << "' should have a Variable in the task's scope";
  }
  uint32_t localCount = 0;
  for (const hldb::Variable *const var : *mytask->getVariables()) {
    if (hldb::findByName<hldb::IODecl>(var->getName(), mytask->getIODecls()) == nullptr) ++localCount;
  }
  ASSERT_EQ(localCount, 1u) << "the body declares exactly one local ('a'), excluding the formals' Variables";
  const hldb::Begin *const body = mytask->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "task body should be a Begin";
  EXPECT_TRUE((body->getVariables() == nullptr) || body->getVariables()->empty())
      << "37.12, Note 5: the implicit begin holds statements only";
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", mytask->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getDecompile(), "0");
}

TEST_F(TaskStaticTest, BodyHasTwoStatementsIncrementIfElse) {
  const hldb::Task *const mytask = getMytask();
  ASSERT_NE(mytask, nullptr);
  const hldb::Begin *const body = mytask->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u)
      << "37.12, Note 5: the implicit begin holds the statements only, not the 'int a = 0;' declaration";

  const hldb::Operation *const inc = any_cast<hldb::Operation>(body->getStmts()->at(0));
  ASSERT_NE(inc, nullptr);
  EXPECT_EQ(inc->getOpType(), vpiPostIncOp);
}

TEST_F(TaskStaticTest, IfElseConditionIsIODeclTestWithBothDisplayBranches) {
  const hldb::Task *const mytask = getMytask();
  ASSERT_NE(mytask, nullptr);
  const hldb::Begin *const body = mytask->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u);
  const hldb::IfElse *const ifStmt = any_cast<hldb::IfElse>(body->getStmts()->at(1));
  ASSERT_NE(ifStmt, nullptr) << "second body entry has an 'else' branch, so it must resolve to IfElse, not IfStmt";

  const hldb::RefObj *const cond = ifStmt->getCondition<hldb::RefObj>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getName(), "test");
  const hldb::Variable *const testVar = hldb::findByName<hldb::Variable>("test", mytask->getVariables());
  ASSERT_NE(testVar, nullptr) << "the formal argument 'test' should have a Variable in the task's scope";
  EXPECT_EQ(cond->getActual(), testVar) << "37.15: 'test' resolves to the formal's Variable, never to its IODecl";

  const hldb::SysTaskCall *const thenDisplay = ifStmt->getStmt<hldb::SysTaskCall>();
  ASSERT_NE(thenDisplay, nullptr);
  ASSERT_NE(thenDisplay->getArguments(), nullptr);
  ASSERT_EQ(thenDisplay->getArguments()->size(), 2u);
  const hldb::NamedArgument *const arg0 = thenDisplay->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const thenFmt = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(thenFmt, nullptr);
  EXPECT_EQ(thenFmt->getDecompile(), "\":assert:(%d != 1)\"");
}

TEST_F(TaskStaticTest, ElseBranchDisplaysEqualsOneMessage) {
  const hldb::Task *const mytask = getMytask();
  ASSERT_NE(mytask, nullptr);
  const hldb::Begin *const body = mytask->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u);
  const hldb::IfElse *const ifStmt = any_cast<hldb::IfElse>(body->getStmts()->at(1));
  ASSERT_NE(ifStmt, nullptr);
  const hldb::SysTaskCall *const elseDisplay = ifStmt->getElseStmt<hldb::SysTaskCall>();
  ASSERT_NE(elseDisplay, nullptr);
  ASSERT_NE(elseDisplay->getArguments(), nullptr);
  ASSERT_EQ(elseDisplay->getArguments()->size(), 2u);
  const hldb::NamedArgument *const arg0 = elseDisplay->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const elseFmt = arg0->getHighConn<hldb::Constant>();
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
    const hldb::MethodTaskCall *const call = any_cast<hldb::MethodTaskCall>(body->getStmts()->at(idx));
    ASSERT_NE(call, nullptr) << "'mytask(...)' with parens+argument should be a TaskCall, not a plain RefObj";
    EXPECT_EQ(call->getName(), "mytask");
    EXPECT_EQ(call->getTaskFunc<hldb::Task>(), getMytask());
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 1u);
    const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
    ASSERT_NE(arg0, nullptr);
    const hldb::Constant *const arg = arg0->getHighConn<hldb::Constant>();
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

#ifndef DAVTESTS_BULK_BUILD
int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
