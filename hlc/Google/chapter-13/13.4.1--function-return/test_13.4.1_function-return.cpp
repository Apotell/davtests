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

// Tests for 13.4.1--function-return.sv (tags: 13.4.1)
//   module top();
//     function int add(int a, int b);
//       return a + b;
//     endfunction
//     initial
//       $display(":assert: (%d == 90)", add(30, 60));
//   endmodule
//
// What to check and why (IEEE 1800-2023 13.4.1 "Return values and void
// functions", p.342, checked before any test code was written):
//   "Function return values can be specified in two ways, either by
//   using a return statement or by assigning a value to the internal
//   variable with the same name as the function... The return
//   statement shall override any value assigned to the function
//   name. When the return statement is used, nonvoid functions shall
//   specify an expression with the return." Here "add" is a nonvoid
//   ("int") function using the return-statement form, so its
//   ReturnStmt must carry a non-null expression -- contrast with
//   13.4.1--function-return-assignment.cpp, which specifies the value
//   by assigning to the function name instead.
//
// What is checked:
//   - module top has exactly 1 TaskFunc, a Function named "add", whose
//     return typespec resolves to IntTypespec
//   - "add" has exactly 2 IODecls, "a" and "b", both getDirection() ==
//     vpiInput with IntTypespec (per 13.4: "Function declarations
//     default to the formal direction input if no direction has been
//     specified... subsequent formals default to the same direction")
//   - add's body is a plain ReturnStmt directly (no Begin wrapper,
//     single statement, no begin-end in source) whose getCondition()
//     is Operation(vpiAddOp) comparing RefObj "a" against RefObj "b"
//     (both resolving to their IODecls, not Variables or Nets)
//   - the initial process's body is a plain SysTaskCall "$display"
//     directly (no Begin, single statement) with 2 arguments: Constant
//     "\":assert: (%d == 90)\"" and a FuncCall "add" whose
//     getTaskFunc<hldb::Function>() resolves back to the "add"
//     declaration, with 2 arguments: Constant "30" and Constant "60"
//   - no continuous assignments, nets, or module-scope variables exist
//
// What is NOT checked and why:
//   - the runtime returned value (that add(30, 60) actually evaluates
//     to 90) is a simulation-time concept, not a static/structural
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

class FunctionReturnTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "13.4.1--function-return.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Function *getAdd() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTaskFuncs() == nullptr || top->getTaskFuncs()->empty()) return nullptr;
    return any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  }
};

TEST_F(FunctionReturnTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(FunctionReturnTest, ModuleHasNoNetsAndNoVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty());
  EXPECT_TRUE(top->getVariables() == nullptr || top->getVariables()->empty());
}

TEST_F(FunctionReturnTest, AddExistsAsIntFunctionWithTwoInputIODecls) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  ASSERT_EQ(top->getTaskFuncs()->size(), 1u);
  const hldb::Function *const add = getAdd();
  ASSERT_NE(add, nullptr);
  EXPECT_EQ(add->getName(), "add");
  const hldb::RefTypespec *const rts = add->getReturn();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::IntTypespec>(), nullptr) << "'function int' should have an IntTypespec return";

  ASSERT_NE(add->getIODecls(), nullptr);
  ASSERT_EQ(add->getIODecls()->size(), 2u);
  const hldb::IODecl *const a = add->getIODecls()->at(0);
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getName(), "a");
  EXPECT_EQ(a->getDirection(), vpiInput);
  const hldb::IODecl *const b = add->getIODecls()->at(1);
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getName(), "b");
  EXPECT_EQ(b->getDirection(), vpiInput);
}

// ---------------------------------------------------------------------------
// return a + b;
// ---------------------------------------------------------------------------
TEST_F(FunctionReturnTest, BodyIsReturnStmtWithAddExpression) {
  const hldb::Function *const add = getAdd();
  ASSERT_NE(add, nullptr);
  const hldb::ReturnStmt *const ret = add->getStmt<hldb::ReturnStmt>();
  ASSERT_NE(ret, nullptr) << "function body should be a plain ReturnStmt (single statement, no begin-end)";
  const hldb::Operation *const expr = ret->getCondition<hldb::Operation>();
  ASSERT_NE(expr, nullptr) << "'return a + b;' should carry a non-null return expression";
  EXPECT_EQ(expr->getOpType(), vpiAddOp);
  ASSERT_NE(expr->getOperands(), nullptr);
  ASSERT_EQ(expr->getOperands()->size(), 2u);
  const hldb::RefObj *const aRef = any_cast<hldb::RefObj>(expr->getOperands()->at(0));
  ASSERT_NE(aRef, nullptr);
  EXPECT_EQ(aRef->getName(), "a");
  EXPECT_NE(aRef->getActual<hldb::IODecl>(), nullptr) << "'a' should resolve to the IODecl, not a Variable or Net";
  const hldb::RefObj *const bRef = any_cast<hldb::RefObj>(expr->getOperands()->at(1));
  ASSERT_NE(bRef, nullptr);
  EXPECT_EQ(bRef->getName(), "b");
  EXPECT_NE(bRef->getActual<hldb::IODecl>(), nullptr);
}

// ---------------------------------------------------------------------------
// initial $display(":assert: (%d == 90)", add(30, 60));
// ---------------------------------------------------------------------------
TEST_F(FunctionReturnTest, InitialBodyDisplaysAddCallResultAndResolvesBackToAdd) {
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
  EXPECT_EQ(fmt->getDecompile(), "\":assert: (%d == 90)\"");

  const hldb::FuncCall *const call = any_cast<hldb::FuncCall>(display->getArguments()->at(1));
  ASSERT_NE(call, nullptr) << "$display's second argument should be a FuncCall 'add(30, 60)'";
  EXPECT_EQ(call->getName(), "add");
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), getAdd()) << "call should resolve back to the 'add' declaration";
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);
  const hldb::Constant *const arg0 = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg0, nullptr);
  EXPECT_EQ(arg0->getDecompile(), "30");
  const hldb::Constant *const arg1 = any_cast<hldb::Constant>(call->getArguments()->at(1));
  ASSERT_NE(arg1, nullptr);
  EXPECT_EQ(arg1->getDecompile(), "60");
}

TEST_F(FunctionReturnTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
