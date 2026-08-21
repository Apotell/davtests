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

// Tests for 12.8--return_val.sv (tags: 12.8)
//   module jump_tb ();
//     function int fun(input int a);
//       return a * 3;
//     endfunction
//     initial begin
//       for (int i = 0; i < 256; i++)begin
//         $display(fun(i));
//       end
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023 12.8 "Jump statements", p.334,
// checked before any test code was written):
//   "return [ expression ] ; ... In a function returning a value, the
//   return statement shall have an expression of the correct type."
//   "fun" is declared "function int", so its single "return a * 3;"
//   statement must carry a non-null return expression (contrast with
//   12.8--return.cpp's "function void fun", where both "return;"
//   statements carry no expression at all). Since the function body is
//   a single statement with no begin-end in the source, the function's
//   getStmt() is the ReturnStmt directly, not wrapped in a Begin.
//
//   Also (IEEE 1800-2023 6.8): "int" (both the function's "input int
//   a" port and the for-loop's "int i") is a non-net data type;
//   function ports are modeled as IODecl (not Net or Variable, and
//   distinct from both), per 13.4's tf_port_item / io_decl grammar.
//
// What is checked:
//   - module jump_tb has exactly 1 TaskFunc, a Function named "fun",
//     whose return typespec resolves to IntTypespec (confirming
//     "function int", not void)
//   - "fun" has exactly 1 IODecl "a", getDirection() == vpiInput,
//     typespec resolving to IntTypespec
//   - fun's body is a plain ReturnStmt directly (no Begin wrapper,
//     single statement, no begin-end in source) whose getCondition()
//     (the field holding the optional return expression) is
//     Operation(vpiMultOp) comparing RefObj "a" (resolving to the
//     IODecl) against Constant "3" -- confirming "return a * 3;" DOES
//     carry a return expression, unlike the void function's bare
//     "return;"
//   - module jump_tb's Initial process contains a ForStmt (matching
//     the shape confirmed in 12.7.1--for.cpp/12.8--continue.cpp: "int
//     i" declared inline, so ForStmt owns Variable "i")
//   - the for-loop body is a Begin with exactly 1 statement, a
//     SysTaskCall "$display" whose single argument is a FuncCall "fun"
//     (used here as an expression argument, not a standalone
//     statement, contrasting with 12.8--return.cpp's "fun(i);"), whose
//     getTaskFunc<hldb::Function>() resolves back to the "fun"
//     declaration, with 1 argument: RefObj "i" resolving to the
//     Variable "i"
//   - no continuous assignments, nets, or module-scope variables exist
//
// What is NOT checked and why:
//   - the runtime returned value (that fun(i) actually evaluates to
//     i*3) is a simulation-time concept, not a static/structural
//     compile-time property
//   - vpiFuncType (the funcType classification of the Function/FuncCall
//     objects) is not asserted: unlike getReturn()'s typespec, this
//     field was not observed to be populated in any structural
//     confirmation available for this file, and guessing an
//     unconfirmed value would violate "assert the standard, not a
//     guess" -- getReturn()'s IntTypespec already establishes the
//     function is non-void and returns int

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/for_stmt.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/return_stmt.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ReturnValTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "12.8--return_val.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("jump_tb", m_design->getAllModules());
  }

  static const hldb::Function *getFun() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTaskFuncs() == nullptr || top->getTaskFuncs()->empty()) return nullptr;
    return any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  }

  static const hldb::ForStmt *getForStmt() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const initial = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (initial == nullptr) return nullptr;
    const hldb::Begin *const body = initial->getStmt<hldb::Begin>();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) return nullptr;
    return any_cast<hldb::ForStmt>(body->getStmts()->at(0));
  }
};

TEST_F(ReturnValTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ReturnValTest, ModuleHasNoNetsAndNoModuleScopeVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty());
  EXPECT_TRUE(top->getVariables() == nullptr || top->getVariables()->empty())
      << "'i' is scoped to the for-loop, not the module";
}

// ---------------------------------------------------------------------------
// function int fun(input int a);
// ---------------------------------------------------------------------------
TEST_F(ReturnValTest, FunExistsAsIntFunction) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  ASSERT_EQ(top->getTaskFuncs()->size(), 1u);
  const hldb::Function *const fun = getFun();
  ASSERT_NE(fun, nullptr) << "'fun' should resolve to a Function";
  EXPECT_EQ(fun->getName(), "fun");
  const hldb::RefTypespec *const rts = fun->getReturn();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::IntTypespec>(), nullptr) << "'function int' should have an IntTypespec return";
}

TEST_F(ReturnValTest, FunHasOneInputIODeclA) {
  const hldb::Function *const fun = getFun();
  ASSERT_NE(fun, nullptr);
  ASSERT_NE(fun->getIODecls(), nullptr);
  ASSERT_EQ(fun->getIODecls()->size(), 1u);
  const hldb::IODecl *const a = fun->getIODecls()->at(0);
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getName(), "a");
  EXPECT_EQ(a->getDirection(), vpiInput);
  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::IntTypespec>(), nullptr);
}

// ---------------------------------------------------------------------------
// return a * 3;
// ---------------------------------------------------------------------------
TEST_F(ReturnValTest, FunBodyIsReturnStmtWithMultiplyExpression) {
  const hldb::Function *const fun = getFun();
  ASSERT_NE(fun, nullptr);
  const hldb::ReturnStmt *const ret = fun->getStmt<hldb::ReturnStmt>();
  ASSERT_NE(ret, nullptr) << "function body should be a plain ReturnStmt (single statement, no begin-end)";

  const hldb::Operation *const expr = ret->getCondition<hldb::Operation>();
  ASSERT_NE(expr, nullptr) << "'return a * 3;' should carry a non-null return expression";
  EXPECT_EQ(expr->getOpType(), vpiMultOp);
  ASSERT_NE(expr->getOperands(), nullptr);
  ASSERT_EQ(expr->getOperands()->size(), 2u);
  const hldb::RefObj *const aRef = any_cast<hldb::RefObj>(expr->getOperands()->at(0));
  ASSERT_NE(aRef, nullptr);
  EXPECT_EQ(aRef->getName(), "a");
  EXPECT_NE(aRef->getActual<hldb::IODecl>(), nullptr) << "'a' should resolve to the IODecl, not a Variable or Net";
  const hldb::Constant *const three = any_cast<hldb::Constant>(expr->getOperands()->at(1));
  ASSERT_NE(three, nullptr);
  EXPECT_EQ(three->getDecompile(), "3");
}

// ---------------------------------------------------------------------------
// for (int i = 0; i < 256; i++) begin $display(fun(i)); end
// ---------------------------------------------------------------------------
TEST_F(ReturnValTest, ForStmtScopeHasVariableI) {
  const hldb::ForStmt *const forStmt = getForStmt();
  ASSERT_NE(forStmt, nullptr) << "the single statement in the initial body should resolve to ForStmt";
  ASSERT_NE(forStmt->getVariables(), nullptr);
  ASSERT_EQ(forStmt->getVariables()->size(), 1u);
  EXPECT_NE(hldb::findByName<hldb::Variable>("i", forStmt->getVariables()), nullptr);
}

TEST_F(ReturnValTest, LoopBodyDisplaysFunCallResultAndResolvesBackToFun) {
  const hldb::ForStmt *const forStmt = getForStmt();
  ASSERT_NE(forStmt, nullptr);
  const hldb::Begin *const loopBody = forStmt->getStmt<hldb::Begin>();
  ASSERT_NE(loopBody, nullptr) << "for-loop body should be a Begin (explicit begin-end in source)";
  ASSERT_NE(loopBody->getStmts(), nullptr);
  ASSERT_EQ(loopBody->getStmts()->size(), 1u);

  const hldb::SysTaskCall *const display = any_cast<hldb::SysTaskCall>(loopBody->getStmts()->at(0));
  ASSERT_NE(display, nullptr);
  EXPECT_EQ(display->getName(), "$display");
  ASSERT_NE(display->getArguments(), nullptr);
  ASSERT_EQ(display->getArguments()->size(), 1u);

  const hldb::FuncCall *const call = any_cast<hldb::FuncCall>(display->getArguments()->at(0));
  ASSERT_NE(call, nullptr) << "$display's argument should be a FuncCall 'fun(i)', used as an expression here";
  EXPECT_EQ(call->getName(), "fun");
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), getFun()) << "call should resolve back to the 'fun' declaration";
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), "i");
  EXPECT_NE(arg->getActual<hldb::Variable>(), nullptr);
}

TEST_F(ReturnValTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
