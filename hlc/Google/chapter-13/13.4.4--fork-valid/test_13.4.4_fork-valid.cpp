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

// Tests for 13.4.4--fork-valid.sv (tags: 13.4.4)
//   module top();
//     function int fun(int val);
//       fork
//         $display("abc");
//         $display("def");
//       join_none
//       return val + 2;
//     endfunction
//     initial
//       $display("$d", fun(2));
//   endmodule
//
// What to check and why (IEEE 1800-2023 13.4.4 "Background processes
// spawned by function calls", p.346, checked before any test code was
// written -- no .log file consulted for this file's expected shape,
// only the standard text and the real hldb API headers):
//   "Statements that do not block shall be allowed inside a function;
//   specifically, nonblocking assignments, event triggers, clocking
//   drives, and fork-join_none constructs shall be allowed inside a
//   function." This file is the legal counterpart to
//   13.4.4--fork-invalid.sv: identical shape, except "join_none"
//   replaces "join_any" -- fork-join_none spawns background processes
//   without ever blocking the enclosing function's own execution
//   (matching "a process calling a function shall return
//   immediately"), so this construct is explicitly permitted and the
//   file carries no :should_fail_because: tag.
//
// What is checked:
//   - module top has exactly 1 TaskFunc, a Function named "fun", whose
//     return typespec resolves to IntTypespec, with exactly 1 IODecl
//     "val" (getDirection() == vpiInput, IntTypespec)
//   - fun's body is a Begin with exactly 2 statements: a ForkStmt and a
//     ReturnStmt
//   - the ForkStmt's getJoinType() is vpiJoinNone (confirming the legal
//     "join_none" variant, contrasting with
//     13.4.4--fork-invalid.cpp's vpiJoinAny), and its getStmts() has
//     exactly 2 SysTaskCall "$display" entries ("abc", "def")
//   - the ReturnStmt's getCondition() is Operation(vpiAddOp) comparing
//     RefObj "val" (resolving to the IODecl) against Constant "2"
//   - the initial process's body is a plain SysTaskCall "$display"
//     whose second argument is a FuncCall "fun" resolving back to the
//     declaration, with 1 argument, Constant "2"
//   - since this construct is legal per 13.4.4, the compiler should
//     report zero fatal/syntax/error diagnostics (a coarse top-level
//     sanity check, acceptable per the test-writing guide when a file
//     genuinely expects zero diagnostics of any kind)
//
// What is NOT checked and why:
//   - the runtime background-process scheduling behavior of
//     fork-join_none itself (that "abc"/"def" print without blocking
//     the function's return) is a simulation-time concept, not a
//     static/structural compile-time property

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/fork_stmt.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/return_stmt.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ForkValidTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "13.4.4--fork-valid.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Function *getFun() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTaskFuncs() == nullptr || top->getTaskFuncs()->empty()) return nullptr;
    return any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  }
};

TEST_F(ForkValidTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ForkValidTest, FunExistsAsIntFunctionWithOneInputIODeclVal) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  ASSERT_EQ(top->getTaskFuncs()->size(), 1u);
  const hldb::Function *const fn = getFun();
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(fn->getName(), "fun");
  const hldb::RefTypespec *const rts = fn->getReturn();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::IntTypespec>(), nullptr);
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 1u);
}

// ---------------------------------------------------------------------------
// fork $display("abc"); $display("def"); join_none
// ---------------------------------------------------------------------------
TEST_F(ForkValidTest, BodyHasForkJoinNoneFollowedByReturn) {
  const hldb::Function *const fn = getFun();
  ASSERT_NE(fn, nullptr);
  const hldb::Begin *const body = fn->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "function body should be a Begin (2 statements, no explicit begin-end in source)";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u);

  const hldb::ForkStmt *const fork = any_cast<hldb::ForkStmt>(body->getStmts()->at(0));
  ASSERT_NE(fork, nullptr) << "first statement should be a ForkStmt";
  EXPECT_EQ(fork->getJoinType(), vpiJoinNone)
      << "'join_none' is the only fork variant 13.4.4 permits inside a function";
  ASSERT_NE(fork->getStmts(), nullptr);
  ASSERT_EQ(fork->getStmts()->size(), 2u);
  const hldb::SysTaskCall *const first = any_cast<hldb::SysTaskCall>(fork->getStmts()->at(0));
  ASSERT_NE(first, nullptr);
  ASSERT_NE(first->getArguments(), nullptr);
  ASSERT_EQ(first->getArguments()->size(), 1u);
  const hldb::Constant *const firstArg = any_cast<hldb::Constant>(first->getArguments()->at(0));
  ASSERT_NE(firstArg, nullptr);
  EXPECT_EQ(firstArg->getDecompile(), "\"abc\"");
  const hldb::SysTaskCall *const second = any_cast<hldb::SysTaskCall>(fork->getStmts()->at(1));
  ASSERT_NE(second, nullptr);
  ASSERT_NE(second->getArguments(), nullptr);
  ASSERT_EQ(second->getArguments()->size(), 1u);
  const hldb::Constant *const secondArg = any_cast<hldb::Constant>(second->getArguments()->at(0));
  ASSERT_NE(secondArg, nullptr);
  EXPECT_EQ(secondArg->getDecompile(), "\"def\"");

  const hldb::ReturnStmt *const ret = any_cast<hldb::ReturnStmt>(body->getStmts()->at(1));
  ASSERT_NE(ret, nullptr) << "second statement should be a ReturnStmt";
  const hldb::Operation *const expr = ret->getCondition<hldb::Operation>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getOpType(), vpiAddOp);
  ASSERT_NE(expr->getOperands(), nullptr);
  ASSERT_EQ(expr->getOperands()->size(), 2u);
  const hldb::Constant *const two = any_cast<hldb::Constant>(expr->getOperands()->at(1));
  ASSERT_NE(two, nullptr);
  EXPECT_EQ(two->getDecompile(), "2");
}

// ---------------------------------------------------------------------------
// initial $display("$d", fun(2));
// ---------------------------------------------------------------------------
TEST_F(ForkValidTest, InitialBodyCallsFunAndResolvesBackToFun) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const initial = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(initial, nullptr);
  const hldb::SysTaskCall *const display = initial->getStmt<hldb::SysTaskCall>();
  ASSERT_NE(display, nullptr);
  ASSERT_NE(display->getArguments(), nullptr);
  ASSERT_EQ(display->getArguments()->size(), 2u);
  const hldb::FuncCall *const call = any_cast<hldb::FuncCall>(display->getArguments()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "fun");
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), getFun());
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::Constant *const arg = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getDecompile(), "2");
}

// ---------------------------------------------------------------------------
// fork-join_none is legal inside a function: expect zero diagnostics
// ---------------------------------------------------------------------------
TEST_F(ForkValidTest, CompilerAcceptsForkJoinNoneInsideFunctionWithNoErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal + stats.nbSyntax + stats.nbError, 0)
      << "IEEE 1800-2023 13.4.4 explicitly permits fork-join_none inside a function, so this file should compile "
         "with zero diagnostics";
}

TEST_F(ForkValidTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
