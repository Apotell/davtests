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

// Tests for 12.8--return.sv (tags: 12.8)
//   module jump_tb ();
//     function void fun(input int a);
//       $display("a");
//       if(a == 21)
//         return;
//       $display(a);
//       return;
//     endfunction
//     initial begin
//       for (int i = 0; i < 256; i++)begin
//         fun(i);
//       end
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023 12.8 "Jump statements", p.334,
// checked before any test code was written):
//   "return [ expression ] ; ... return expression -- exit from a
//   function; return -- exit from a task or void function." "fun" is
//   declared "function void", so both of its "return;" statements must
//   carry no expression -- the first exits early when "a == 21" (an
//   early-exit guard clause), the second is the normal fall-through
//   exit at the end of the function body.
//
//   Also (IEEE 1800-2023 6.8): "int" (both the function's "input int
//   a" port and the for-loop's "int i") is a non-net data type;
//   function ports are modeled as IODecl (not Net or Variable), per
//   13.4's tf_port_item / io_decl grammar, and are a third, distinct
//   classification alongside the Net/Variable split used everywhere
//   else in this suite.
//
// What is checked:
//   - module jump_tb has exactly 1 TaskFunc, a Function named "fun",
//     whose return typespec resolves to VoidTypespec (confirming
//     "function void")
//   - "fun" has exactly 1 IODecl "a", getDirection() == vpiInput,
//     typespec resolving to IntTypespec
//   - fun's body is a Begin with exactly 4 statements:
//     1) SysTaskCall "$display" with Constant "\"a\""
//     2) IfStmt (no else) whose condition is Operation(vpiEqOp)
//        comparing RefObj "a" (resolving to the IODecl, not a Variable)
//        against Constant "21", and whose body is a ReturnStmt with a
//        null getCondition() (the field that holds the optional return
//        expression) -- confirming "return;" with no expression
//     3) SysTaskCall "$display" with RefObj "a" (resolving to the
//        IODecl)
//     4) a final ReturnStmt, also with a null getCondition()
//   - module jump_tb's Initial process contains a ForStmt (matching
//     the shape confirmed in 12.7.1--for.cpp/12.8--continue.cpp: "int
//     i" declared inline, so ForStmt owns Variable "i")
//   - the for-loop body is a Begin with exactly 1 statement, a FuncCall
//     "fun" whose getTaskFunc<hldb::Function>() resolves back to the
//     "fun" declaration itself, with 1 argument: RefObj "i" resolving
//     to the Variable "i"
//   - no continuous assignments, nets, or module-scope variables exist
//
// What is NOT checked and why:
//   - the runtime early-return control-flow behavior itself (that the
//     second $display and second return are skipped when a == 21) is a
//     simulation-time concept, not a static/structural compile-time
//     property
//   - vpiVisibility ("public") and other subroutine metadata unrelated
//     to the return statement itself are out of scope for this chapter-
//     12.8 file

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
#include <hldb/variable.h>
#include <hldb/void_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ReturnTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "12.8--return.hlc"}); }
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

TEST_F(ReturnTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ReturnTest, ModuleHasNoNetsAndNoModuleScopeVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty());
  EXPECT_TRUE(top->getVariables() == nullptr || top->getVariables()->empty())
      << "'i' is scoped to the for-loop, not the module";
}

// ---------------------------------------------------------------------------
// function void fun(input int a);
// ---------------------------------------------------------------------------
TEST_F(ReturnTest, FunExistsAsVoidFunction) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  ASSERT_EQ(top->getTaskFuncs()->size(), 1u);
  const hldb::Function *const fun = getFun();
  ASSERT_NE(fun, nullptr) << "'fun' should resolve to a Function";
  EXPECT_EQ(fun->getName(), "fun");
  const hldb::RefTypespec *const rts = fun->getReturn();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::VoidTypespec>(), nullptr) << "'function void' should have a VoidTypespec return";
}

TEST_F(ReturnTest, FunHasOneInputIODeclA) {
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
// $display("a"); if(a == 21) return; $display(a); return;
// ---------------------------------------------------------------------------
TEST_F(ReturnTest, FunBodyHasFourStatements) {
  const hldb::Function *const fun = getFun();
  ASSERT_NE(fun, nullptr);
  const hldb::Begin *const body = fun->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "function body should be a Begin";
  ASSERT_NE(body->getStmts(), nullptr);
  EXPECT_EQ(body->getStmts()->size(), 4u);
}

TEST_F(ReturnTest, GuardIfReturnsWithNoExpressionWhenAEquals21) {
  const hldb::Function *const fun = getFun();
  ASSERT_NE(fun, nullptr);
  const hldb::Begin *const body = fun->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_GE(body->getStmts()->size(), 2u);
  const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(body->getStmts()->at(1));
  ASSERT_NE(ifStmt, nullptr) << "second statement should be a plain IfStmt (no else)";

  const hldb::Operation *const cond = ifStmt->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiEqOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const aRef = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  ASSERT_NE(aRef, nullptr);
  EXPECT_EQ(aRef->getName(), "a");
  EXPECT_NE(aRef->getActual<hldb::IODecl>(), nullptr) << "'a' should resolve to the IODecl, not a Variable or Net";
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(cond->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "21");

  const hldb::ReturnStmt *const ret = ifStmt->getStmt<hldb::ReturnStmt>();
  ASSERT_NE(ret, nullptr) << "if-body should be a plain ReturnStmt";
  EXPECT_EQ(ret->getCondition(), nullptr) << "'return;' with no expression should have a null return expression";
}

TEST_F(ReturnTest, SecondDisplayReferencesAAndFinalReturnHasNoExpression) {
  const hldb::Function *const fun = getFun();
  ASSERT_NE(fun, nullptr);
  const hldb::Begin *const body = fun->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_GE(body->getStmts()->size(), 4u);

  const hldb::SysTaskCall *const display = any_cast<hldb::SysTaskCall>(body->getStmts()->at(2));
  ASSERT_NE(display, nullptr);
  ASSERT_NE(display->getArguments(), nullptr);
  ASSERT_EQ(display->getArguments()->size(), 1u);
  const hldb::RefObj *const aArg = any_cast<hldb::RefObj>(display->getArguments()->at(0));
  ASSERT_NE(aArg, nullptr);
  EXPECT_EQ(aArg->getName(), "a");
  EXPECT_NE(aArg->getActual<hldb::IODecl>(), nullptr);

  const hldb::ReturnStmt *const finalReturn = any_cast<hldb::ReturnStmt>(body->getStmts()->at(3));
  ASSERT_NE(finalReturn, nullptr) << "fourth statement should be a plain ReturnStmt";
  EXPECT_EQ(finalReturn->getCondition(), nullptr)
      << "final 'return;' with no expression should have a null return expression";
}

// ---------------------------------------------------------------------------
// for (int i = 0; i < 256; i++) begin fun(i); end
// ---------------------------------------------------------------------------
TEST_F(ReturnTest, ForStmtScopeHasVariableI) {
  const hldb::ForStmt *const forStmt = getForStmt();
  ASSERT_NE(forStmt, nullptr) << "the single statement in the initial body should resolve to ForStmt";
  ASSERT_NE(forStmt->getVariables(), nullptr);
  ASSERT_EQ(forStmt->getVariables()->size(), 1u);
  EXPECT_NE(hldb::findByName<hldb::Variable>("i", forStmt->getVariables()), nullptr);
}

TEST_F(ReturnTest, LoopBodyCallsFunWithIAndResolvesBackToFun) {
  const hldb::ForStmt *const forStmt = getForStmt();
  ASSERT_NE(forStmt, nullptr);
  const hldb::Begin *const loopBody = forStmt->getStmt<hldb::Begin>();
  ASSERT_NE(loopBody, nullptr) << "for-loop body should be a Begin (explicit begin-end in source)";
  ASSERT_NE(loopBody->getStmts(), nullptr);
  ASSERT_EQ(loopBody->getStmts()->size(), 1u);
  const hldb::FuncCall *const call = any_cast<hldb::FuncCall>(loopBody->getStmts()->at(0));
  ASSERT_NE(call, nullptr) << "'fun(i);' should be a standalone FuncCall statement";
  EXPECT_EQ(call->getName(), "fun");
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), getFun()) << "call should resolve back to the 'fun' declaration";
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), "i");
  EXPECT_NE(arg->getActual<hldb::Variable>(), nullptr);
}

TEST_F(ReturnTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
