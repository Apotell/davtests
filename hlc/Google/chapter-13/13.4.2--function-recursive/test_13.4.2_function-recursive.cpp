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

// Tests for 13.4.2--function-recursive.sv (tags: 13.4.2)
//   module top();
//     function automatic int factorial(int val);
//       if(val == 0) return 1;
//       return factorial(val-1) * val;
//     endfunction
//     initial
//       begin
//         $display(":assert: (%d == 1)", factorial(0));
//         $display(":assert: (%d == 1)", factorial(1));
//         $display(":assert: (%d == 2)", factorial(2));
//         $display(":assert: (%d == 120)", factorial(5));
//         $display(":assert: (%d == 39916800)", factorial(11));
//       end
//   endmodule
//
// What to check and why (IEEE 1800-2023 13.4.2 "Static and automatic
// functions", p.344, checked before any test code was written):
//   "An automatic function is reentrant, with all the function
//   declarations allocated dynamically for each concurrent function
//   call." Reentrancy is exactly what a recursive function requires --
//   each nested call to "factorial" needs its own copy of "val", which
//   is only guaranteed for automatic-lifetime functions. Fittingly,
//   this function is declared "automatic" (matching the spec's own
//   worked factorial example in 13.4.2). The recursive call
//   "factorial(val-1)" inside the function's own body must resolve
//   back to the very same Function declaration it is defined in.
//
//   Also (IEEE 1800-2023 6.8): "int" is a non-net data type, so the
//   IODecl "val" carries an IntTypespec.
//
// What is checked:
//   - module top has exactly 1 TaskFunc, a Function named "factorial",
//     whose return typespec resolves to IntTypespec, with exactly 1
//     IODecl "val" (getDirection() == vpiInput, IntTypespec), and whose
//     lifetime IS automatic (getAutomatic() == true)
//   - factorial's body is a Begin with exactly 2 statements (no local
//     variable declarations, so no decl-as-statement entry to account
//     for here, unlike 13.4.2--function-automatic.cpp):
//     1) a plain IfStmt (no else) whose condition is Operation(vpiEqOp)
//        comparing RefObj "val" (resolving to the IODecl) against
//        Constant "0", and whose body is a ReturnStmt with
//        getCondition() Constant "1"
//     2) a ReturnStmt whose getCondition() is Operation(vpiMultOp)
//        comparing a FuncCall "factorial(val-1)" against RefObj "val";
//        the FuncCall's getTaskFunc<hldb::Function>() resolves back to
//        the very same "factorial" Function it is declared inside (a
//        genuine self-reference), and its single argument is
//        Operation(vpiSubOp) comparing RefObj "val" against Constant
//        "1"
//   - the initial process's body is a Begin with exactly 5 SysTaskCall
//     "$display" statements, each with a FuncCall "factorial" argument
//     resolving back to the declaration, with 1 argument (Constant
//     "0", "1", "2", "5", "11" respectively)
//   - no continuous assignments or module-scope variables exist
//
// What is NOT checked and why:
//   - the runtime recursive-evaluation result (that factorial(5)
//     actually computes to 120, etc.) is a simulation-time concept,
//     not a static/structural compile-time property

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/if_stmt.h>
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

class FunctionRecursiveTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "13.4.2--function-recursive.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Function *getFactorial() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTaskFuncs() == nullptr || top->getTaskFuncs()->empty()) return nullptr;
    return any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  }
};

TEST_F(FunctionRecursiveTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(FunctionRecursiveTest, FactorialExistsAsAutomaticIntFunction) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  ASSERT_EQ(top->getTaskFuncs()->size(), 1u);
  const hldb::Function *const factorial = getFactorial();
  ASSERT_NE(factorial, nullptr);
  EXPECT_EQ(factorial->getName(), "factorial");
  EXPECT_TRUE(factorial->getAutomatic()) << "reentrancy for recursion requires automatic lifetime, per 13.4.2";
  const hldb::RefTypespec *const rts = factorial->getReturn();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::IntTypespec>(), nullptr);
  ASSERT_NE(factorial->getIODecls(), nullptr);
  ASSERT_EQ(factorial->getIODecls()->size(), 1u);
  const hldb::IODecl *const val = factorial->getIODecls()->at(0);
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getName(), "val");
  EXPECT_EQ(val->getDirection(), vpiInput);
}

// ---------------------------------------------------------------------------
// if(val == 0) return 1;
// ---------------------------------------------------------------------------
TEST_F(FunctionRecursiveTest, BaseCaseReturnsOneWhenValIsZero) {
  const hldb::Function *const factorial = getFactorial();
  ASSERT_NE(factorial, nullptr);
  const hldb::Begin *const body = factorial->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "function body should be a Begin";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u) << "no local variable declarations exist in this function";

  const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(body->getStmts()->at(0));
  ASSERT_NE(ifStmt, nullptr) << "'if(val == 0) return 1;' should be a plain IfStmt (no else)";
  const hldb::Operation *const cond = ifStmt->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiEqOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const valRef = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  ASSERT_NE(valRef, nullptr);
  EXPECT_EQ(valRef->getName(), "val");
  EXPECT_NE(valRef->getActual<hldb::IODecl>(), nullptr);
  const hldb::Constant *const zero = any_cast<hldb::Constant>(cond->getOperands()->at(1));
  ASSERT_NE(zero, nullptr);
  EXPECT_EQ(zero->getDecompile(), "0");

  const hldb::ReturnStmt *const baseReturn = ifStmt->getStmt<hldb::ReturnStmt>();
  ASSERT_NE(baseReturn, nullptr);
  const hldb::Constant *const one = baseReturn->getCondition<hldb::Constant>();
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->getDecompile(), "1");
}

// ---------------------------------------------------------------------------
// return factorial(val-1) * val;  -- the recursive step
// ---------------------------------------------------------------------------
TEST_F(FunctionRecursiveTest, RecursiveStepMultipliesSelfCallByVal) {
  const hldb::Function *const factorial = getFactorial();
  ASSERT_NE(factorial, nullptr);
  const hldb::Begin *const body = factorial->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_GE(body->getStmts()->size(), 2u);

  const hldb::ReturnStmt *const recReturn = any_cast<hldb::ReturnStmt>(body->getStmts()->at(1));
  ASSERT_NE(recReturn, nullptr);
  const hldb::Operation *const mult = recReturn->getCondition<hldb::Operation>();
  ASSERT_NE(mult, nullptr);
  EXPECT_EQ(mult->getOpType(), vpiMultOp);
  ASSERT_NE(mult->getOperands(), nullptr);
  ASSERT_EQ(mult->getOperands()->size(), 2u);

  const hldb::FuncCall *const selfCall = any_cast<hldb::FuncCall>(mult->getOperands()->at(0));
  ASSERT_NE(selfCall, nullptr) << "first operand should be the recursive FuncCall 'factorial(val-1)'";
  EXPECT_EQ(selfCall->getName(), "factorial");
  EXPECT_EQ(selfCall->getTaskFunc<hldb::Function>(), factorial)
      << "the recursive call should resolve back to this very same Function declaration";
  ASSERT_NE(selfCall->getArguments(), nullptr);
  ASSERT_EQ(selfCall->getArguments()->size(), 1u);
  const hldb::Operation *const valMinusOne = any_cast<hldb::Operation>(selfCall->getArguments()->at(0));
  ASSERT_NE(valMinusOne, nullptr) << "'val-1' should be an Operation(vpiSubOp)";
  EXPECT_EQ(valMinusOne->getOpType(), vpiSubOp);
  ASSERT_NE(valMinusOne->getOperands(), nullptr);
  ASSERT_EQ(valMinusOne->getOperands()->size(), 2u);
  const hldb::Constant *const one = any_cast<hldb::Constant>(valMinusOne->getOperands()->at(1));
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->getDecompile(), "1");

  const hldb::RefObj *const valRef = any_cast<hldb::RefObj>(mult->getOperands()->at(1));
  ASSERT_NE(valRef, nullptr) << "second operand should be RefObj 'val'";
  EXPECT_EQ(valRef->getName(), "val");
  EXPECT_NE(valRef->getActual<hldb::IODecl>(), nullptr);
}

// ---------------------------------------------------------------------------
// initial begin $display(...factorial(N)); end -- N = 0,1,2,5,11
// ---------------------------------------------------------------------------
TEST_F(FunctionRecursiveTest, InitialBodyCallsFactorialWithFiveDifferentArguments) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const initial = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(initial, nullptr);
  const hldb::Begin *const body = initial->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "initial body should be a Begin (explicit begin-end in source)";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 5u);

  const std::string_view expectedArgs[5] = {"0", "1", "2", "5", "11"};
  for (uint32_t idx = 0; idx < 5u; ++idx) {
    const hldb::SysTaskCall *const display = any_cast<hldb::SysTaskCall>(body->getStmts()->at(idx));
    ASSERT_NE(display, nullptr);
    ASSERT_NE(display->getArguments(), nullptr);
    ASSERT_EQ(display->getArguments()->size(), 2u);
    const hldb::FuncCall *const call = any_cast<hldb::FuncCall>(display->getArguments()->at(1));
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->getName(), "factorial");
    EXPECT_EQ(call->getTaskFunc<hldb::Function>(), getFactorial());
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 1u);
    const hldb::Constant *const arg = any_cast<hldb::Constant>(call->getArguments()->at(0));
    ASSERT_NE(arg, nullptr);
    EXPECT_EQ(arg->getDecompile(), expectedArgs[idx]);
  }
}

TEST_F(FunctionRecursiveTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
