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

// Tests for find-index.sv (tags: 7.12.1 7.12 7.10)
//   module top ();
//     string s[] = { "hello", "sad", "world" };
//     int qi[$];
//     initial begin
//       qi = s.find_index with ( item == "world" );
//       $display(":assert: (%d == 1)", qi.size);
//       $display(":assert: (%d == 2)", qi[0]);
//     end
//   endmodule
//
// Checked:
//   - design has module top with exactly 2 variables: "s" (dynamic array)
//     and "qi" (queue of int)
//   - variable "s": ArrayTypespec vpiArrayType=dynamic(2), ElemTypespec ->
//     StringTypespec; initial value is a 3-operand concatenation
//     ("hello","sad","world")
//   - variable "qi": ArrayTypespec vpiArrayType=static(1) -- the compiler models
//     a queue ("int qi[$]") the same way as a static array with an unbounded
//     left-range ("$"), there is no distinct queue array type; ElemTypespec
//     -> IntTypespec
//   - Initial process: 1 Begin with 3 stmts (Assignment + 2 SysFuncCall)
//   - Assignment: qi = s.find_index rhs is a MethodFuncCall "find_index" (IEEE 1800-2023
//     37.42 detail 2) whose vpiPrefix is RefObj "s" (bound to the
//     Variable); written without parentheses, so vpiIsProperty is set
//     (13.5.5); any with clause stays on the call (vpiWith)
//   - both $display calls and their MethodFuncCall("size", prefix "qi")/BitSelect("qi[0]")
//     arguments
//   - design-level typespecs (3): ModuleTypespec, StringTypespec, IntTypespec

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ArrayLocatorFindIndexTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "find-index.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module / variables ----

TEST_F(ArrayLocatorFindIndexTest, ModuleExists) {
  EXPECT_NE(hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

TEST_F(ArrayLocatorFindIndexTest, ModuleHasTwoVariables) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 2u);
}

TEST_F(ArrayLocatorFindIndexTest, VariableSNameIsS) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const s = hldb::findByName<hldb::Variable>("s", top->getVariables());
  ASSERT_NE(s, nullptr);
}

TEST_F(ArrayLocatorFindIndexTest, VariableQiNameIsQi) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const qi = hldb::findByName<hldb::Variable>("qi", top->getVariables());
  ASSERT_NE(qi, nullptr);
}

// --- variable "s": dynamic array of string ----

TEST_F(ArrayLocatorFindIndexTest, VariableSTypespecIsDynamicArray) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const s = hldb::findByName<hldb::Variable>("s", top->getVariables());
  ASSERT_NE(s, nullptr);
  const hldb::ArrayTypespec *const at = s->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiDynamicArray);
}

TEST_F(ArrayLocatorFindIndexTest, VariableSElemTypespecIsString) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const s = hldb::findByName<hldb::Variable>("s", top->getVariables());
  ASSERT_NE(s, nullptr);
  const hldb::ArrayTypespec *const at = s->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::StringTypespec>(), nullptr);
}

TEST_F(ArrayLocatorFindIndexTest, VariableSInitialValueIsThreeElemConcat) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const s = hldb::findByName<hldb::Variable>("s", top->getVariables());
  ASSERT_NE(s, nullptr);
  const hldb::Operation *const concat = s->getValue<hldb::Operation>();
  ASSERT_NE(concat, nullptr);
  EXPECT_EQ(concat->getOpType(), vpiConcatOp);
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 3u);
  const hldb::Constant *const c0 = any_cast<hldb::Constant>(concat->getOperands()->at(0));
  const hldb::Constant *const c1 = any_cast<hldb::Constant>(concat->getOperands()->at(1));
  const hldb::Constant *const c2 = any_cast<hldb::Constant>(concat->getOperands()->at(2));
  ASSERT_NE(c0, nullptr);
  ASSERT_NE(c1, nullptr);
  ASSERT_NE(c2, nullptr);
  EXPECT_EQ(c0->getValue(), "hello");
  EXPECT_EQ(c1->getValue(), "sad");
  EXPECT_EQ(c2->getValue(), "world");
}

// --- variable "qi": queue of int, modeled as a "queue" array with unbounded range -

TEST_F(ArrayLocatorFindIndexTest, VariableQiTypespecIsQueueArray) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const qi = hldb::findByName<hldb::Variable>("qi", top->getVariables());
  ASSERT_NE(qi, nullptr);
  const hldb::ArrayTypespec *const at = qi->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiQueueArray);
}

TEST_F(ArrayLocatorFindIndexTest, VariableQiRangeLeftIsUnboundedDollar) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const qi = hldb::findByName<hldb::Variable>("qi", top->getVariables());
  ASSERT_NE(qi, nullptr);
  const hldb::ArrayTypespec *const at = qi->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr);
  const hldb::Constant *const dollar = at->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(dollar, nullptr);
  EXPECT_EQ(dollar->getDecompile(), "$");
  EXPECT_EQ(dollar->getConstType(), vpiUnboundedConst);  // "unbounded" (not in vpi_user.h; hlc-specific)
}

TEST_F(ArrayLocatorFindIndexTest, VariableQiElemTypespecIsInt) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const qi = hldb::findByName<hldb::Variable>("qi", top->getVariables());
  ASSERT_NE(qi, nullptr);
  const hldb::ArrayTypespec *const at = qi->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(ArrayLocatorFindIndexTest, VariableQiHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const qi = hldb::findByName<hldb::Variable>("qi", top->getVariables());
  ASSERT_NE(qi, nullptr);
  EXPECT_EQ(qi->getValue(), nullptr);
}

// --- initial process: qi = s.find_index with (item == "world") ----

TEST_F(ArrayLocatorFindIndexTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(ArrayLocatorFindIndexTest, InitialBeginHasThreeStmts) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const begin = init->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 3u);
}

TEST_F(ArrayLocatorFindIndexTest, AssignmentIsBlockingToQi) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const begin = init->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "qi");
  EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr);
}

TEST_F(ArrayLocatorFindIndexTest, AssignmentRhsIsMethodFuncCallFindIndexOnS) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const begin = init->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  const hldb::MethodFuncCall *const rhs = assign->getRhs<hldb::MethodFuncCall>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), std::string_view("find_index"));
  const hldb::RefObj *const sRef = rhs->getPrefix<hldb::RefObj>();
  ASSERT_NE(sRef, nullptr);
  EXPECT_EQ(sRef->getName(), std::string_view("s"));
  EXPECT_NE(sRef->getActual<hldb::Variable>(), nullptr);
}

TEST_F(ArrayLocatorFindIndexTest, MethodFuncCallIsNamedFindIndex) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::MethodFuncCall *const call =
      any_cast<hldb::Assignment>(init->getStmt<hldb::Begin>()->getStmts()->at(0))->getRhs<hldb::MethodFuncCall>();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "find_index");
}

TEST_F(ArrayLocatorFindIndexTest, MethodFuncCallWithClauseComparesItemToWorld) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::MethodFuncCall *const call =
      any_cast<hldb::Assignment>(init->getStmt<hldb::Begin>()->getStmts()->at(0))->getRhs<hldb::MethodFuncCall>();
  ASSERT_NE(call, nullptr);
  const hldb::Operation *const with = call->getWith<hldb::Operation>();
  ASSERT_NE(with, nullptr);
  EXPECT_EQ(with->getOpType(), vpiEqOp);
  ASSERT_NE(with->getOperands(), nullptr);
  ASSERT_EQ(with->getOperands()->size(), 2u);

  const hldb::RefObj *const item = any_cast<hldb::RefObj>(with->getOperands()->at(0));
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(item->getName(), "item");
  EXPECT_NE(item->getActual(), nullptr);
  EXPECT_NE(item->getActual<hldb::Variable>(), nullptr);

  const hldb::Constant *const world = any_cast<hldb::Constant>(with->getOperands()->at(1));
  ASSERT_NE(world, nullptr);
  EXPECT_EQ(world->getConstType(), vpiStringConst);
  EXPECT_EQ(world->getValue(), "world");
}

// --- $display(":assert: (%d == 1)", qi.size) ----

TEST_F(ArrayLocatorFindIndexTest, FirstDisplayFormatStringIsSizeAssert) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(init->getStmt<hldb::Begin>()->getStmts()->at(1));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 2u);
  const hldb::NamedArgument *const arg0 = disp->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const fmt = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 1)");
}

TEST_F(ArrayLocatorFindIndexTest, FirstDisplaySecondArgIsQiDotSize) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(init->getStmt<hldb::Begin>()->getStmts()->at(1));
  ASSERT_NE(disp, nullptr);
  const hldb::NamedArgument *const arg1 = disp->getArguments()->at(1);
  ASSERT_NE(arg1, nullptr);
  const hldb::MethodFuncCall *const size = arg1->getHighConn<hldb::MethodFuncCall>();
  ASSERT_NE(size, nullptr);
  EXPECT_EQ(size->getName(), std::string_view("size"));
  // Written without parentheses (13.5.5, 7.10.2).
  EXPECT_TRUE(size->getIsProperty());
  EXPECT_NE(size->getTaskFunc(), nullptr);
  const hldb::RefObj *const qiRef = size->getPrefix<hldb::RefObj>();
  ASSERT_NE(qiRef, nullptr);
  EXPECT_EQ(qiRef->getName(), std::string_view("qi"));
  EXPECT_NE(qiRef->getActual<hldb::Variable>(), nullptr);
}

// --- $display(":assert: (%d == 2)", qi[0]) ----

TEST_F(ArrayLocatorFindIndexTest, SecondDisplayFormatStringIsIndexAssert) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(init->getStmt<hldb::Begin>()->getStmts()->at(2));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  const hldb::NamedArgument *const arg0 = disp->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const fmt = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 2)");
}

TEST_F(ArrayLocatorFindIndexTest, SecondDisplaySecondArgIsQiBitSelectZero) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(init->getStmt<hldb::Begin>()->getStmts()->at(2));
  ASSERT_NE(disp, nullptr);
  const hldb::NamedArgument *const arg1 = disp->getArguments()->at(1);
  ASSERT_NE(arg1, nullptr);
  const hldb::BitSelect *const sel = arg1->getHighConn<hldb::BitSelect>();
  ASSERT_NE(sel, nullptr);
  EXPECT_EQ(sel->getName(), "qi[0]");
  const hldb::RefObj *const prefix = sel->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->getName(), "qi");
  const hldb::Constant *const index = sel->getIndex<hldb::Constant>();
  ASSERT_NE(index, nullptr);
  EXPECT_EQ(index->getDecompile(), "0");
  EXPECT_EQ(index->getConstType(), vpiIntConst);
  EXPECT_NE(index->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
}

// --- design-level typespecs / structural completeness ----

TEST_F(ArrayLocatorFindIndexTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = hldb::findByDefName<hldb::ModuleTypespec>("top", m_design->getTypespecs());
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(ArrayLocatorFindIndexTest, DesignHasIntTypespecSigned) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *it = nullptr;
  for (hldb::Typespec *ts : *m_design->getTypespecs()) {
    if ((it = any_cast<hldb::IntTypespec>(ts))) {
      if (it->getSigned()) break;
    }
  }
  ASSERT_NE(it, nullptr);
}

TEST_F(ArrayLocatorFindIndexTest, NoContAssigns) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
