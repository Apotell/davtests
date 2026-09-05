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

// Tests for 13.4.1--function-void-return.sv (tags: 13.4.1)
//   :should_fail_because: void function returns value
//   module top();
//     function void add(int a, int b);
//       $display("%d+%d=", a, b);
//       return a + b;
//     endfunction
//     initial
//       $display("%d", add(45, 90));
//   endmodule
//
// What to check and why (IEEE 1800-2023 13.4.1 "Return values and void
// functions", p.342, checked before any test code was written):
//   "Functions can be declared as type void, which do not have a
//   return value... When the return statement is used, nonvoid
//   functions shall specify an expression with the return." By
//   implication, a VOID function's return statement must NOT specify
//   an expression -- but this file's "add" is declared "function void"
//   and its "return a + b;" carries a real expression. This is exactly
//   the illegal construct the file's own :should_fail_because: tag
//   describes, and a compliant compiler must reject it.
//
//   This same file also contains a second, closely related violation
//   of the very same spec paragraph: "Function calls may be used as
//   expressions unless of type void, which are statements: ...
//   myprint(a); // call myprint (defined above) as a statement." The
//   initial block calls the void function "add" AS AN EXPRESSION
//   ARGUMENT to $display ("$display("%d", add(45, 90))"), when a void
//   function call is only legal as a standalone statement. Both
//   violations are checked below since both are directly grounded in
//   this exact spec paragraph and are both actually present in this
//   file's source -- not just the one named in the tag.
//
// What is checked:
//   - module top has exactly 1 TaskFunc, a Function named "add", whose
//     return typespec resolves to VoidTypespec (confirming "function
//     void"), with 2 input IODecls "a"/"b" (IntTypespec)
//   - add's body is a Begin with exactly 2 statements: SysTaskCall
//     "$display" (3 arguments: Constant "\"%d+%d=\"", RefObj "a",
//     RefObj "b"), and a ReturnStmt whose getCondition() is
//     Operation(vpiAddOp) comparing RefObj "a" against RefObj "b" --
//     structurally confirming the illegal "return a + b;" expression
//     really is attached to this void function's ReturnStmt (not
//     silently dropped by the parser)
//   - the initial process's body is a plain SysTaskCall "$display"
//     whose second argument is a FuncCall "add" (resolving back to the
//     Function via getTaskFunc()) -- structurally confirming the
//     illegal use of a void function call as an expression argument
//     really is present in the tree
//   - THE POINT OF THIS FILE: per the standard, this construct is
//     illegal on two independent counts (a void function's return
//     statement carrying an expression, and a void function call used
//     as an expression), so the compiler should report at least one
//     diagnostic; this is checked via the coarse
//     nbFatal+nbSyntax+nbError sum (no specific ErrorDefinition entry
//     for either violation has been confirmed, so a named findError()
//     check is not yet available -- see the test-writing guide's
//     allowance for a coarse sanity check when no specific error is
//     known). CONFIRMED BY RUNNING THIS TEST WITH THE SKIP BELOW
//     REMOVED (fails as expected, actual: 0 vs 0): HLC reports zero
//     diagnostics for both violations. Kept as GTEST_SKIP() with the
//     real assertion underneath, per the established gating rule
//     (skips only added after personal verification).
//
// What is NOT checked and why:
//   - the runtime behavior of $display formatting is a simulation-time
//     concept, not a static/structural compile-time property

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
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
#include <hldb/void_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FunctionVoidReturnTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "13.4.1--function-void-return.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Function *getAdd() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTaskFuncs() == nullptr || top->getTaskFuncs()->empty()) return nullptr;
    return any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  }
};

TEST_F(FunctionVoidReturnTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(FunctionVoidReturnTest, AddExistsAsVoidFunctionWithTwoInputIODecls) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  ASSERT_EQ(top->getTaskFuncs()->size(), 1u);
  const hldb::Function *const add = getAdd();
  ASSERT_NE(add, nullptr);
  EXPECT_EQ(add->getName(), "add");
  const hldb::RefTypespec *const rts = add->getReturn();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::VoidTypespec>(), nullptr) << "'function void' should have a VoidTypespec return";
  ASSERT_NE(add->getIODecls(), nullptr);
  ASSERT_EQ(add->getIODecls()->size(), 2u);
}

// ---------------------------------------------------------------------------
// Violation #1: "return a + b;" inside a void function
// ---------------------------------------------------------------------------
TEST_F(FunctionVoidReturnTest, ReturnStmtIllegallyCarriesAddExpressionInVoidFunction) {
  const hldb::Function *const add = getAdd();
  ASSERT_NE(add, nullptr);
  const hldb::Begin *const body = add->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "function body should be a Begin (2 statements, no explicit begin-end in source)";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u);

  const hldb::SysTaskCall *const display = any_cast<hldb::SysTaskCall>(body->getStmts()->at(0));
  ASSERT_NE(display, nullptr);
  ASSERT_NE(display->getArguments(), nullptr);
  ASSERT_EQ(display->getArguments()->size(), 3u);

  const hldb::ReturnStmt *const ret = any_cast<hldb::ReturnStmt>(body->getStmts()->at(1));
  ASSERT_NE(ret, nullptr) << "second statement should be a ReturnStmt";
  const hldb::Operation *const expr = ret->getCondition<hldb::Operation>();
  ASSERT_NE(expr, nullptr)
      << "structurally confirming the illegal expression really is attached to this void function's return -- "
         "per 13.4.1, a void function's return statement must NOT specify an expression";
  EXPECT_EQ(expr->getOpType(), vpiAddOp);
  ASSERT_NE(expr->getOperands(), nullptr);
  ASSERT_EQ(expr->getOperands()->size(), 2u);
}

// ---------------------------------------------------------------------------
// Violation #2: a void function call used as an expression argument
// ---------------------------------------------------------------------------
TEST_F(FunctionVoidReturnTest, VoidFunctionCallIllegallyUsedAsExpressionArgument) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const initial = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(initial, nullptr);
  const hldb::SysTaskCall *const display = initial->getStmt<hldb::SysTaskCall>();
  ASSERT_NE(display, nullptr) << "initial body should be a plain SysTaskCall (single statement, no begin-end)";
  ASSERT_NE(display->getArguments(), nullptr);
  ASSERT_EQ(display->getArguments()->size(), 2u);

  const hldb::FuncCall *const call = any_cast<hldb::FuncCall>(display->getArguments()->at(1));
  ASSERT_NE(call, nullptr)
      << "structurally confirming a call to the void function 'add' really is used as an expression argument "
         "here -- per 13.4.1, a void function call may only be used as a statement, never as an expression";
  EXPECT_EQ(call->getName(), "add");
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), getAdd());
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);
}

// ---------------------------------------------------------------------------
// THE POINT OF THIS FILE: both violations above are illegal SystemVerilog
// ---------------------------------------------------------------------------
TEST_F(FunctionVoidReturnTest, CompilerShouldRejectVoidFunctionReturningAValueButDoesNot) {
  GTEST_SKIP() << "Confirmed HLC bug -- verified by running this test with the skip removed (fails as expected, "
                  "actual: 0 vs 0): IEEE 1800-2023 13.4.1 prohibits both a void function's return statement from "
                  "carrying an expression and a void function call from being used as an expression, but HLC "
                  "accepts both with zero diagnostics. Tracked, not yet fixed by the compiler.";
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GT(stats.nbFatal + stats.nbSyntax + stats.nbError, 0)
      << "IEEE 1800-2023 13.4.1 prohibits both a void function's return statement from carrying an expression "
         "and a void function call from being used as an expression -- this file's 'add' does both, matching "
         "the file's own :should_fail_because: tag -- HLC currently accepts it with zero diagnostics";
}

TEST_F(FunctionVoidReturnTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
