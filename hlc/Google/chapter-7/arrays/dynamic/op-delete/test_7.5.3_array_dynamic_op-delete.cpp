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

// Tests for op-delete.sv (tags: 7.5.3)
//   module top ();
//     bit [7:0] arr[];
//     initial begin
//       arr = new [ 16 ];
//       $display(":assert: (%d == 16)", arr.size);
//       arr.delete;
//       $display(":assert: (%d == 0)", arr.size);
//     end
//   endmodule
//
// Checked:
//   - design has module top
//   - module has exactly 1 variable: "arr" (RefTypespec->ArrayTypespec)
//   - ArrayTypespec: vpiArrayType=dynamic(2)
//   - ArrayTypespec ElemTypespec: RefTypespec->BitTypespec
//   - BitTypespec: vpiVector=true, getSigned()==false, 1 Range [7:0] (left=7, right=0)
//   - Range constants are vpiIntConst
//   - module has exactly 1 Initial process
//   - Initial body is a Begin block with 4 statements
//   - Stmt[0]: blocking Assignment, RHS=ArrayExpr with 1 Constant "16" (vpiIntConst)
//   - Stmt[1]: SysTaskCall "$display" with 2 arguments
//   - Stmt[2]: arr.delete is a MethodFuncCall "delete" statement (IEEE
//       1800-2023 37.42 detail 2): vpiPrefix RefObj "arr", no arguments,
//       vpiIsProperty set (no parentheses, 13.5.5), bound to delete()
//   - Stmt[3]: SysTaskCall "$display" with 2 arguments
//   - arr.size inside $display args is a MethodFuncCall "size" with
//     vpiPrefix RefObj "arr"
//   - design has 3 typespecs: ModuleTypespec "top", IntTypespec, StringTypespec
//   - StringTypespec present because $display uses string literal arguments
//   - no continuous assignments
//
// Not checked:
//   - SysFuncCall::getFuncType() (not set by HLC for $display)
//   - Assignment LHS (vpiLhs not in UHDM dump)
//   - variable boolean flags (same as basic.sv -- all false; see 7.5_test_basic.cpp)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_expr.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class OpDeleteTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "op-delete.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module ----

TEST_F(OpDeleteTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

// --- variable arr ----

TEST_F(OpDeleteTest, ModuleHasOneVariable) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u);
}

TEST_F(OpDeleteTest, VariableNameIsArr) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->at(0)->getName(), "arr");
}

// --- ArrayTypespec ----

TEST_F(OpDeleteTest, VariableHasArrayTypespec) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const variable = top->getVariables()->at(0);
  ASSERT_NE(variable, nullptr);
  const hldb::RefTypespec *const rt = variable->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::ArrayTypespec>(), nullptr);
}

TEST_F(OpDeleteTest, ArrayTypespecIsDynamic) {
  // vpiArrayType: dynamic (2)
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiDynamicArray);
}

TEST_F(OpDeleteTest, ArrayTypespecElemIsBitTypespec) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::BitTypespec>(), nullptr);
}

TEST_F(OpDeleteTest, ArrayTypespecHasNoIndexTypespec) {
  // Dynamic arrays have no index typespec (only associative arrays do)
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getIndexTypespec(), nullptr);
}

// --- BitTypespec [7:0] ----

TEST_F(OpDeleteTest, BitTypespecIsVector) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::BitTypespec *const bt = top->getVariables()
                                          ->at(0)
                                          ->getTypespec<hldb::RefTypespec>()
                                          ->getActual<hldb::ArrayTypespec>()
                                          ->getElemTypespec()
                                          ->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  EXPECT_TRUE(bt->getVector());
}

TEST_F(OpDeleteTest, BitTypespecIsNotSigned) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::BitTypespec *const bt = top->getVariables()
                                          ->at(0)
                                          ->getTypespec<hldb::RefTypespec>()
                                          ->getActual<hldb::ArrayTypespec>()
                                          ->getElemTypespec()
                                          ->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  EXPECT_FALSE(bt->getSigned());
}

TEST_F(OpDeleteTest, RangeLeftIs7) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::BitTypespec *const bt = top->getVariables()
                                          ->at(0)
                                          ->getTypespec<hldb::RefTypespec>()
                                          ->getActual<hldb::ArrayTypespec>()
                                          ->getElemTypespec()
                                          ->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  ASSERT_NE(bt->getRanges(), nullptr);
  const hldb::Constant *const left = bt->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "7");
}

TEST_F(OpDeleteTest, RangeRightIs0) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::BitTypespec *const bt = top->getVariables()
                                          ->at(0)
                                          ->getTypespec<hldb::RefTypespec>()
                                          ->getActual<hldb::ArrayTypespec>()
                                          ->getElemTypespec()
                                          ->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  ASSERT_NE(bt->getRanges(), nullptr);
  const hldb::Constant *const right = bt->getRanges()->at(0)->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
}

TEST_F(OpDeleteTest, RangeConstTypesAreInt) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::BitTypespec *const bt = top->getVariables()
                                          ->at(0)
                                          ->getTypespec<hldb::RefTypespec>()
                                          ->getActual<hldb::ArrayTypespec>()
                                          ->getElemTypespec()
                                          ->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  ASSERT_NE(bt->getRanges(), nullptr);
  const hldb::Constant *const left = bt->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
  const hldb::Constant *const right = bt->getRanges()->at(0)->getRightExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(left->getConstType(), vpiIntConst);
  EXPECT_EQ(right->getConstType(), vpiIntConst);
}

// --- Initial process ----

TEST_F(OpDeleteTest, ModuleHasOneProcess) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 1u);
}

TEST_F(OpDeleteTest, ProcessIsInitial) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(OpDeleteTest, InitialBodyIsBegin) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  EXPECT_NE(init->getStmt<hldb::Begin>(), nullptr);
}

// --- Begin statements ----

TEST_F(OpDeleteTest, BeginHasFourStatements) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const begin = init->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 4u);
}

// --- Stmt[0]: Assignment ----

TEST_F(OpDeleteTest, FirstStmtIsAssignment) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_NE(any_cast<hldb::Assignment>(begin->getStmts()->at(0)), nullptr);
}

TEST_F(OpDeleteTest, AssignmentIsBlocking) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(
      any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
}

TEST_F(OpDeleteTest, AssignmentRhsIsArrayExpr) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(
      any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_NE(assign->getRhs<hldb::ArrayExpr>(), nullptr);
}

TEST_F(OpDeleteTest, ArrayExprHasOneExpr) {
  // new[16] -- the size expression list has exactly 1 item
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayExpr *const ae =
      any_cast<hldb::Assignment>(
          any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(0))
          ->getRhs<hldb::ArrayExpr>();
  ASSERT_NE(ae, nullptr);
  ASSERT_NE(ae->getExprs(), nullptr);
  EXPECT_EQ(ae->getExprs()->size(), 1u);
}

TEST_F(OpDeleteTest, ArrayExprSizeIs16) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayExpr *const ae =
      any_cast<hldb::Assignment>(
          any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(0))
          ->getRhs<hldb::ArrayExpr>();
  ASSERT_NE(ae, nullptr);
  const hldb::Constant *const c = any_cast<hldb::Constant>(ae->getExprs()->at(0));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "16");
}

TEST_F(OpDeleteTest, ArrayExprSizeConstTypeIsInt) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayExpr *const ae =
      any_cast<hldb::Assignment>(
          any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(0))
          ->getRhs<hldb::ArrayExpr>();
  ASSERT_NE(ae, nullptr);
  const hldb::Constant *const c = any_cast<hldb::Constant>(ae->getExprs()->at(0));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), vpiIntConst);
}

// --- Stmt[1]: first $display ----

TEST_F(OpDeleteTest, SecondStmtIsSysTaskCall) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_NE(any_cast<hldb::SysTaskCall>(begin->getStmts()->at(1)), nullptr);
}

TEST_F(OpDeleteTest, FirstDisplayNameIsDisplay) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(
      any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$display");
}

TEST_F(OpDeleteTest, FirstDisplayHasTwoArguments) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(
      any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(1));
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  EXPECT_EQ(call->getArguments()->size(), 2u);
}

TEST_F(OpDeleteTest, FirstDisplaySecondArgIsArrSizeCall) {
  // arr.size (no parentheses, 13.5.5) is a MethodFuncCall "size" with
  // vpiPrefix RefObj "arr" (IEEE 1800-2023 37.42 detail 2)
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(
      any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(1));
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  const hldb::NamedArgument *const arg1 = call->getArguments()->at(1);
  ASSERT_NE(arg1, nullptr);
  const hldb::MethodFuncCall *const size = arg1->getHighConn<hldb::MethodFuncCall>();
  ASSERT_NE(size, nullptr);
  EXPECT_EQ(size->getName(), std::string_view("size"));
  EXPECT_TRUE(size->getIsProperty());
  EXPECT_NE(size->getTaskFunc(), nullptr);
  const hldb::RefObj *const arrRef = size->getPrefix<hldb::RefObj>();
  ASSERT_NE(arrRef, nullptr);
  EXPECT_EQ(arrRef->getName(), std::string_view("arr"));
  EXPECT_NE(arrRef->getActual<hldb::Variable>(), nullptr);
}

// --- Stmt[2]: arr.delete ----

TEST_F(OpDeleteTest, ThirdStmtIsMethodFuncCall) {
  // arr.delete is a call of the built-in void function delete() (7.5.3)
  // written without parentheses (13.5.5), used as a statement
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_NE(any_cast<hldb::MethodFuncCall>(begin->getStmts()->at(2)), nullptr);
}

TEST_F(OpDeleteTest, DeleteCallNameIsDelete) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::MethodFuncCall *const del = any_cast<hldb::MethodFuncCall>(
      any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(2));
  ASSERT_NE(del, nullptr);
  EXPECT_EQ(del->getName(), std::string_view("delete"));
  EXPECT_TRUE(del->getIsProperty());
}

TEST_F(OpDeleteTest, DeleteCallHasNoArguments) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::MethodFuncCall *const del = any_cast<hldb::MethodFuncCall>(
      any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(2));
  ASSERT_NE(del, nullptr);
  EXPECT_EQ(del->getArguments(), nullptr);
}

TEST_F(OpDeleteTest, DeleteCallPrefixIsArr) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::MethodFuncCall *const del = any_cast<hldb::MethodFuncCall>(
      any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(2));
  ASSERT_NE(del, nullptr);
  const hldb::RefObj *const elem = del->getPrefix<hldb::RefObj>();
  ASSERT_NE(elem, nullptr);
  EXPECT_EQ(elem->getName(), std::string_view("arr"));
  EXPECT_NE(elem->getActual<hldb::Variable>(), nullptr);
}

TEST_F(OpDeleteTest, DeleteCallBindsToBuiltinDelete) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::MethodFuncCall *const del = any_cast<hldb::MethodFuncCall>(
      any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(2));
  ASSERT_NE(del, nullptr);
  ASSERT_NE(del->getTaskFunc(), nullptr);
  EXPECT_EQ(del->getTaskFunc()->getAnyType(), hldb::AnyType::Function);
}

// --- Stmt[3]: second $display ----

TEST_F(OpDeleteTest, FourthStmtIsSysFuncCall) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_NE(any_cast<hldb::SysTaskCall>(begin->getStmts()->at(3)), nullptr);
}

TEST_F(OpDeleteTest, SecondDisplayNameIsDisplay) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(
      any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(3));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$display");
}

TEST_F(OpDeleteTest, SecondDisplayHasTwoArguments) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(
      any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(3));
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  EXPECT_EQ(call->getArguments()->size(), 2u);
}

TEST_F(OpDeleteTest, SecondDisplaySecondArgIsArrSizeCall) {
  // second $display also gets arr.size as a MethodFuncCall with prefix "arr"
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(
      any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(3));
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  const hldb::NamedArgument *const arg1 = call->getArguments()->at(1);
  ASSERT_NE(arg1, nullptr);
  const hldb::MethodFuncCall *const size = arg1->getHighConn<hldb::MethodFuncCall>();
  ASSERT_NE(size, nullptr);
  EXPECT_EQ(size->getName(), std::string_view("size"));
  const hldb::RefObj *const arrRef = size->getPrefix<hldb::RefObj>();
  ASSERT_NE(arrRef, nullptr);
  EXPECT_EQ(arrRef->getName(), std::string_view("arr"));
}

// --- design-level typespecs ----

TEST_F(OpDeleteTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = hldb::findByDefName<hldb::ModuleTypespec>("top", m_design->getTypespecs());
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(OpDeleteTest, DesignHasIntTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *it = nullptr;
  for (hldb::Typespec *ts : *m_design->getTypespecs()) {
    if ((it = any_cast<hldb::IntTypespec>(ts))) {
      break;
    }
  }
  ASSERT_NE(it, nullptr);
}

TEST_F(OpDeleteTest, DesignHasStringTypespec) {
  // StringTypespec is added because $display uses string literal arguments
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::StringTypespec *st = nullptr;
  for (hldb::Typespec *ts : *m_design->getTypespecs()) {
    if ((st = any_cast<hldb::StringTypespec>(ts))) {
      break;
    }
  }
  EXPECT_NE(st, nullptr);
}

// --- structural completeness ----

TEST_F(OpDeleteTest, NoContAssigns) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
