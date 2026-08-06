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

// Tests for op-size.sv (tags: 7.5.2)
//   module top ();
//     bit [7:0] arr[];
//     initial begin
//       arr = new [ 16 ];
//       $display(":assert: (%d == 16)", arr.size);
//       arr = new [ 8 ];
//       $display(":assert: (%d == 8)", arr.size);
//     end
//   endmodule
//
// IEEE 1800-2023 Sec 7.5.2 (Size()) defines the size() built-in method:
// "function int size();" -- it returns the current size of a dynamic array,
// or zero if the array has not been created. Sec 7.5.1 additionally notes
// that resizing/reinitializing a previously-created dynamic array via new is
// destructive (no previous data preserved unless explicitly reinitialized
// with its old contents). op-size.sv exercises this: arr is sized to 16,
// its size is read back, then it is resized to 8 (destructively replacing
// the previous 16-element array), and its size is read back again.
//
// Checked:
//   - design has module top
//   - module has exactly 1 variable: "arr" (RefTypespec->ArrayTypespec)
//   - ArrayTypespec: vpiArrayType=vpiDynamicArray, no index typespec (only
//     associative arrays have one)
//   - ArrayTypespec ElemTypespec: RefTypespec->BitTypespec, vpiVector=true,
//     getSigned()==false, 1 Range [7:0] (left=7, right=0), range consts are
//     vpiUIntConst
//   - module has exactly 1 Initial process, body is a Begin with 4
//     statements
//   - Stmt[0]: arr = new [16] -- blocking Assignment, RHS=ArrayExpr with 1
//     Constant "16" (vpiUIntConst)
//   - Stmt[1]: SysTaskCall "$display" with 2 arguments; 2nd argument is a
//     HierPath "arr.size" -- COMPILER BEHAVIOR: arr.size is stored as a
//     HierPath, not as a recognized built-in method call (same behavior
//     documented for op-delete.sv), with 2 path elements: RefObj "arr" and
//     RefObj "size"
//   - Stmt[2]: arr = new [8] -- a SECOND, independent blocking Assignment,
//     RHS=ArrayExpr with 1 Constant "8" (vpiUIntConst); per Sec 7.5.1 this
//     destructively resizes arr from 16 elements down to 8
//   - Stmt[3]: SysTaskCall "$display" with 2 arguments; 2nd argument is a
//     HierPath "arr.size" (same COMPILER BEHAVIOR as Stmt[1])
//   - design has 3 typespecs: ModuleTypespec "top", IntTypespec,
//     StringTypespec (StringTypespec present because $display uses string
//     literal arguments; a single shared StringTypespec instance is reused
//     across both $display calls, matching the established shape in
//     op-delete.sv, which also has 2 $display calls but only 1
//     StringTypespec in the design-level list)
//   - no continuous assignments
//
// Not checked:
//   - SysFuncCall::getFuncType() (not set by HLC for $display)
//   - Assignment LHS (vpiLhs not in the object model dump; same gap
//     documented for op-delete.sv)
//   - variable boolean flags beyond getVector()/getSigned() (all false, same
//     as basic.sv/op-delete.sv)
//   - actual runtime value returned by arr.size() -- this harness only
//     compiles/elaborates op-size.sv; it does not run a simulator, so the
//     size() method's return value cannot be observed here (nor can it be
//     observed at all given the HierPath COMPILER BEHAVIOR above, since
//     arr.size is never modeled as an actual MethodFuncCall to begin with).
//     op-size.sv's own $display format strings document the expected sizes.

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
#include <hldb/hier_path.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/variable.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class OpSizeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "op-size.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module ----

TEST_F(OpSizeTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

// --- variable arr ----

TEST_F(OpSizeTest, ModuleHasOneVariable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u);
}

TEST_F(OpSizeTest, VariableNameIsArr) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->at(0)->getName(), "arr");
}

// --- ArrayTypespec ----

TEST_F(OpSizeTest, VariableHasArrayTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const variable = top->getVariables()->at(0);
  ASSERT_NE(variable, nullptr);
  const hldb::RefTypespec *const rt = variable->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::ArrayTypespec>(), nullptr);
}

TEST_F(OpSizeTest, ArrayTypespecIsDynamic) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiDynamicArray);
}

TEST_F(OpSizeTest, ArrayTypespecElemIsBitTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::BitTypespec>(), nullptr);
}

TEST_F(OpSizeTest, ArrayTypespecHasNoIndexTypespec) {
  // Dynamic arrays have no index typespec (only associative arrays do)
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getIndexTypespec(), nullptr);
}

// --- BitTypespec [7:0] ----

TEST_F(OpSizeTest, BitTypespecIsVector) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
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

TEST_F(OpSizeTest, BitTypespecIsNotSigned) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
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

TEST_F(OpSizeTest, RangeLeftIs7) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
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

TEST_F(OpSizeTest, RangeRightIs0) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
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

TEST_F(OpSizeTest, RangeConstTypesAreUInt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
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
  EXPECT_EQ(left->getConstType(), vpiUIntConst);
  EXPECT_EQ(right->getConstType(), vpiUIntConst);
}

// --- Initial process ----

TEST_F(OpSizeTest, ModuleHasOneProcess) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 1u);
}

TEST_F(OpSizeTest, ProcessIsInitial) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(OpSizeTest, InitialBodyIsBegin) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  EXPECT_NE(init->getStmt<hldb::Begin>(), nullptr);
}

// --- Begin statements ----

TEST_F(OpSizeTest, BeginHasFourStatements) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const begin = init->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 4u);
}

// --- Stmt[0]: arr = new [16] ----

TEST_F(OpSizeTest, FirstStmtIsAssignment) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_NE(any_cast<hldb::Assignment>(begin->getStmts()->at(0)), nullptr);
}

TEST_F(OpSizeTest, FirstAssignmentIsBlocking) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(
      any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
}

TEST_F(OpSizeTest, FirstAssignmentRhsIsArrayExprSize16) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(
      any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  const hldb::ArrayExpr *const ae = assign->getRhs<hldb::ArrayExpr>();
  ASSERT_NE(ae, nullptr);
  ASSERT_NE(ae->getExprs(), nullptr);
  ASSERT_EQ(ae->getExprs()->size(), 1u);
  const hldb::Constant *const c = any_cast<hldb::Constant>(ae->getExprs()->at(0));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "16");
  EXPECT_EQ(c->getConstType(), vpiUIntConst);
}

// --- Stmt[1]: first $display(arr.size) ----

TEST_F(OpSizeTest, SecondStmtIsSysTaskCall) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_NE(any_cast<hldb::SysTaskCall>(begin->getStmts()->at(1)), nullptr);
}

TEST_F(OpSizeTest, FirstDisplayNameAndFormatString) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(
      any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$display");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 16)");
}

TEST_F(OpSizeTest, FirstDisplaySecondArgIsArrSizeHierPath) {
  // COMPILER BEHAVIOR: arr.size is stored as HierPath, not as a method call
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(
      any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(1));
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>(call->getArguments()->at(1));
  ASSERT_NE(hp, nullptr);
  EXPECT_EQ(hp->getName(), "arr.size");
  ASSERT_NE(hp->getPathElems(), nullptr);
  ASSERT_EQ(hp->getPathElems()->size(), 2u);
  const hldb::RefObj *const first = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(first, nullptr);
  EXPECT_EQ(first->getName(), "arr");
}

// --- Stmt[2]: arr = new [8] (destructive resize) ----

TEST_F(OpSizeTest, ThirdStmtIsAssignment) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_NE(any_cast<hldb::Assignment>(begin->getStmts()->at(2)), nullptr);
}

TEST_F(OpSizeTest, ThirdAssignmentIsBlocking) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(
      any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(2));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
}

TEST_F(OpSizeTest, ThirdAssignmentRhsIsArrayExprSize8) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(
      any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(2));
  ASSERT_NE(assign, nullptr);
  const hldb::ArrayExpr *const ae = assign->getRhs<hldb::ArrayExpr>();
  ASSERT_NE(ae, nullptr);
  ASSERT_NE(ae->getExprs(), nullptr);
  ASSERT_EQ(ae->getExprs()->size(), 1u);
  const hldb::Constant *const c = any_cast<hldb::Constant>(ae->getExprs()->at(0));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "8");
  EXPECT_EQ(c->getConstType(), vpiUIntConst);
}

// --- Stmt[3]: second $display(arr.size) ----

TEST_F(OpSizeTest, FourthStmtIsSysTaskCall) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_NE(any_cast<hldb::SysTaskCall>(begin->getStmts()->at(3)), nullptr);
}

TEST_F(OpSizeTest, SecondDisplayNameAndFormatString) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(
      any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(3));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$display");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 8)");
}

TEST_F(OpSizeTest, SecondDisplaySecondArgIsArrSizeHierPath) {
  // COMPILER BEHAVIOR: arr.size is stored as HierPath, not as a method call
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(
      any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(3));
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>(call->getArguments()->at(1));
  ASSERT_NE(hp, nullptr);
  EXPECT_EQ(hp->getName(), "arr.size");
}

// --- design-level typespecs ----

TEST_F(OpSizeTest, DesignHasThreeTypespecs) {
  // ModuleTypespec "top" + IntTypespec + StringTypespec (a single shared
  // StringTypespec instance, reused by both $display calls -- see op-delete.sv)
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(OpSizeTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(OpSizeTest, DesignHasIntTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_NE(any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1)), nullptr);
}

TEST_F(OpSizeTest, DesignHasStringTypespec) {
  // StringTypespec is added because $display uses string literal arguments
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_NE(any_cast<hldb::StringTypespec>(m_design->getTypespecs()->at(2)), nullptr);
}

// --- structural completeness ----

TEST_F(OpSizeTest, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
