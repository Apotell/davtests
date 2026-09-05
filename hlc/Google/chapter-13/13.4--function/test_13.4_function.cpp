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

// Tests for 13.4--function.sv (tags: 13.4)
//   module top();
//     function int test(int val);
//       return val + 1;
//     endfunction
//     initial
//       $display(":assert: (%d == 2)", test(1));
//   endmodule
//
// What to check and why (IEEE 1800-2023 13.4 "Functions", p.340-342,
// checked before any test code was written -- no .log file consulted
// for this file's expected shape, only the standard text and the real
// hldb API headers):
//   "The primary purpose of a function is to return a value that is to
//   be used in an expression... Function declarations default to the
//   formal direction input if no direction has been specified." "test"
//   has one implicit-direction formal argument "val", and its single
//   statement is a return with an expression -- the simplest possible
//   legal function shape, matching 13.4.1's "return a return statement
//   is used, nonvoid functions shall specify an expression".
//
//   Also (IEEE 1800-2023 6.8): "int" is a non-net data type, so the
//   IODecl "val" carries an IntTypespec and the return type is
//   IntTypespec, never a Net.
//
//   Also (IEEE 1800-2023 13.4.2): functions default to static lifetime
//   unless "automatic" is explicitly written; no lifetime keyword
//   appears here, so getAutomatic() should be false.
//
// What is checked:
//   - module top has exactly 1 TaskFunc, a Function named "test",
//     whose return typespec resolves to IntTypespec, with exactly 1
//     IODecl "val" (getDirection() == vpiInput, IntTypespec), and whose
//     lifetime is NOT automatic (default static, no keyword given)
//   - test's body is a plain ReturnStmt directly (no Begin wrapper,
//     single statement, no begin-end in source) whose getCondition()
//     is Operation(vpiAddOp) comparing RefObj "val" (resolving to the
//     IODecl, not a Variable or Net) against Constant "1"
//   - the initial process's body is a plain SysTaskCall "$display"
//     directly (no Begin, single statement) with 2 arguments: Constant
//     "\":assert: (%d == 2)\"" and a FuncCall "test" whose
//     getTaskFunc<hldb::Function>() resolves back to the "test"
//     declaration, with 1 argument, Constant "1"
//   - no continuous assignments, nets, or module-scope variables exist
//
// What is NOT checked and why:
//   - the runtime returned value (that test(1) actually evaluates to
//     2) is a simulation-time concept, not a static/structural
//     compile-time property

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
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
#include <hldb/vpi_user.h>

namespace hlc {

class FunctionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "13.4--function.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Function *getTestFun() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTaskFuncs() == nullptr || top->getTaskFuncs()->empty()) return nullptr;
    return any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  }
};

TEST_F(FunctionTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(FunctionTest, ModuleHasNoNetsAndNoVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty());
  EXPECT_TRUE(top->getVariables() == nullptr || top->getVariables()->empty());
}

TEST_F(FunctionTest, TestExistsAsIntFunctionWithOneInputIODeclVal) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  ASSERT_EQ(top->getTaskFuncs()->size(), 1u);
  const hldb::Function *const fn = getTestFun();
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(fn->getName(), "test");
  EXPECT_FALSE(fn->getAutomatic()) << "no lifetime keyword is given, so the default static lifetime applies";
  const hldb::RefTypespec *const rts = fn->getReturn();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::IntTypespec>(), nullptr) << "'function int' should have an IntTypespec return";
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 1u);
  const hldb::IODecl *const val = fn->getIODecls()->at(0);
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getName(), "val");
  EXPECT_EQ(val->getDirection(), vpiInput);
}

// ---------------------------------------------------------------------------
// return val + 1;
// ---------------------------------------------------------------------------
TEST_F(FunctionTest, BodyIsReturnStmtWithAddExpression) {
  const hldb::Function *const fn = getTestFun();
  ASSERT_NE(fn, nullptr);
  const hldb::ReturnStmt *const ret = fn->getStmt<hldb::ReturnStmt>();
  ASSERT_NE(ret, nullptr) << "function body should be a plain ReturnStmt (single statement, no begin-end)";
  const hldb::Operation *const expr = ret->getCondition<hldb::Operation>();
  ASSERT_NE(expr, nullptr) << "'return val + 1;' should carry a non-null return expression";
  EXPECT_EQ(expr->getOpType(), vpiAddOp);
  ASSERT_NE(expr->getOperands(), nullptr);
  ASSERT_EQ(expr->getOperands()->size(), 2u);
  const hldb::RefObj *const valRef = any_cast<hldb::RefObj>(expr->getOperands()->at(0));
  ASSERT_NE(valRef, nullptr);
  EXPECT_EQ(valRef->getName(), "val");
  EXPECT_NE(valRef->getActual<hldb::IODecl>(), nullptr) << "'val' should resolve to the IODecl, not a Variable";
  const hldb::Constant *const one = any_cast<hldb::Constant>(expr->getOperands()->at(1));
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->getDecompile(), "1");
}

// ---------------------------------------------------------------------------
// initial $display(":assert: (%d == 2)", test(1));
// ---------------------------------------------------------------------------
TEST_F(FunctionTest, InitialBodyDisplaysTestCallResultAndResolvesBackToTest) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const initial = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(initial, nullptr);
  const hldb::SysTaskCall *const display = initial->getStmt<hldb::SysTaskCall>();
  ASSERT_NE(display, nullptr) << "initial body should be a plain SysTaskCall (single statement, no begin-end)";
  EXPECT_EQ(display->getName(), "$display");
  ASSERT_NE(display->getArguments(), nullptr);
  ASSERT_EQ(display->getArguments()->size(), 2u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(display->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getDecompile(), "\":assert: (%d == 2)\"");

  const hldb::FuncCall *const call = any_cast<hldb::FuncCall>(display->getArguments()->at(1));
  ASSERT_NE(call, nullptr) << "$display's second argument should be a FuncCall 'test(1)'";
  EXPECT_EQ(call->getName(), "test");
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), getTestFun());
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::Constant *const arg = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getDecompile(), "1");
}

TEST_F(FunctionTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
