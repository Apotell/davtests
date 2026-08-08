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

// Tests for op-new.sv (tags: 7.5.1)
//   module top ();
//     bit [7:0] arr[];
//     initial begin
//       arr = new [ 4 ];
//       arr[ 0 ] = 5;
//       arr[ 1 ] = 6;
//       arr[ 2 ] = 7;
//       arr[ 3 ] = 8;
//       $display(":assert: ((%d == 5) and (%d == 6) and (%d == 7) and (%d == 8))",
//           arr[ 0 ], arr[ 1 ], arr[ 2 ], arr[ 3 ]);
//     end
//   endmodule
//
// IEEE 1800-2023 Sec 7.5.1 (New[ ]) defines the "new [ expression ]" dynamic
// array constructor: it sets the size of a dynamic array and initializes its
// elements, and may appear as the right-hand side of a blocking procedural
// assignment whose left-hand side is a dynamic array
// ("nonrange_variable_lvalue = dynamic_array_new", "dynamic_array_new ::=
// new [ expression ] [ ( expression ) ]"). op-new.sv exercises the no-args
// form (no initializer expression), followed by per-element writes/reads of
// the newly-sized array.
//
// Checked:
//   - design has module top
//   - module has exactly 1 variable: "arr" (RefTypespec->ArrayTypespec)
//   - ArrayTypespec: vpiArrayType=vpiDynamicArray, no index typespec (only
//     associative arrays have one)
//   - ArrayTypespec ElemTypespec: RefTypespec->BitTypespec, vpiVector=true,
//     getSigned()==false, 1 Range [7:0] (left=7, right=0), range consts are
//     vpiUIntConst
//   - module has exactly 1 Initial process, body is a Begin with 6 statements
//   - Stmt[0]: arr = new [4] -- blocking Assignment, RHS=ArrayExpr with 1
//     Constant "4" (vpiUIntConst) -- per Sec 7.5.1, this sets arr's size to 4
//     and (absent an initializer) default-initializes its elements
//   - Stmt[1..4]: arr[i] = v -- blocking Assignment, LHS=BitSelect "arr[i]"
//     (prefix RefObj "arr" resolving to the Variable, index Constant i,
//     vpiUIntConst), RHS=Constant v (vpiUIntConst)
//   - Stmt[5]: SysTaskCall "$display" with 5 arguments: format-string
//     Constant + 4 BitSelect reads "arr[0]".."arr[3]", each resolving its
//     prefix RefObj to the Variable
//   - design has 3 typespecs: ModuleTypespec "top", IntTypespec,
//     StringTypespec (StringTypespec present because $display uses a string
//     literal argument)
//   - no continuous assignments
//
// Not checked:
//   - SysFuncCall::getFuncType() (not set by HLC for $display)
//   - Assignment LHS for Stmt[0] (vpiLhs not in the object model dump; same
//     gap documented for op-delete.sv)
//   - variable boolean flags beyond getVector()/getSigned() (all false, same
//     as basic.sv/op-delete.sv)
//   - actual runtime values of arr[0..3] after the writes -- this harness
//     only compiles/elaborates op-new.sv; it does not run a simulator, so
//     the array-element writes/reads can only be checked structurally here.
//     op-new.sv's own $display format string documents the expected values.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_expr.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/bit_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
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

class DynamicArrayDynamicArrayOpNewTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "op-new.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- module ----

TEST_F(DynamicArrayOpNewTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

// --- variable arr ----

TEST_F(DynamicArrayOpNewTest, ModuleHasOneVariable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u);
}

TEST_F(DynamicArrayOpNewTest, VariableNameIsArr) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->at(0)->getName(), "arr");
}

// --- ArrayTypespec ----

TEST_F(DynamicArrayOpNewTest, VariableHasArrayTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const variable = top->getVariables()->at(0);
  ASSERT_NE(variable, nullptr);
  const hldb::RefTypespec *const rt = variable->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::ArrayTypespec>(), nullptr);
}

TEST_F(DynamicArrayOpNewTest, ArrayTypespecIsDynamic) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiDynamicArray);
}

TEST_F(DynamicArrayOpNewTest, ArrayTypespecElemIsBitTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::BitTypespec>(), nullptr);
}

TEST_F(DynamicArrayOpNewTest, ArrayTypespecHasNoIndexTypespec) {
  // Dynamic arrays have no index typespec (only associative arrays do)
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayTypespec *const at =
      top->getVariables()->at(0)->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getIndexTypespec(), nullptr);
}

// --- BitTypespec [7:0] ----

TEST_F(DynamicArrayOpNewTest, BitTypespecIsVector) {
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

TEST_F(DynamicArrayOpNewTest, BitTypespecIsNotSigned) {
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

TEST_F(DynamicArrayOpNewTest, RangeLeftIs7) {
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

TEST_F(DynamicArrayOpNewTest, RangeRightIs0) {
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

TEST_F(DynamicArrayOpNewTest, RangeConstTypesAreUInt) {
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

TEST_F(DynamicArrayOpNewTest, ModuleHasOneProcess) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 1u);
}

TEST_F(DynamicArrayOpNewTest, ProcessIsInitial) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(DynamicArrayOpNewTest, InitialBodyIsBegin) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  EXPECT_NE(init->getStmt<hldb::Begin>(), nullptr);
}

// --- Begin statements ----

TEST_F(DynamicArrayOpNewTest, BeginHasSixStatements) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const begin = init->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 6u);
}

// --- Stmt[0]: arr = new [4] ----

TEST_F(DynamicArrayOpNewTest, FirstStmtIsAssignment) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_NE(any_cast<hldb::Assignment>(begin->getStmts()->at(0)), nullptr);
}

TEST_F(DynamicArrayOpNewTest, FirstAssignmentIsBlocking) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(
      any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
}

TEST_F(DynamicArrayOpNewTest, FirstAssignmentRhsIsArrayExpr) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(
      any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_NE(assign->getRhs<hldb::ArrayExpr>(), nullptr);
}

TEST_F(DynamicArrayOpNewTest, ArrayExprHasOneExpr) {
  // new[4] -- the size expression list has exactly 1 item
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayExpr *const ae =
      any_cast<hldb::Assignment>(
          any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(0))
          ->getRhs<hldb::ArrayExpr>();
  ASSERT_NE(ae, nullptr);
  ASSERT_NE(ae->getExprs(), nullptr);
  EXPECT_EQ(ae->getExprs()->size(), 1u);
}

TEST_F(DynamicArrayOpNewTest, ArrayExprSizeIs4) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayExpr *const ae =
      any_cast<hldb::Assignment>(
          any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(0))
          ->getRhs<hldb::ArrayExpr>();
  ASSERT_NE(ae, nullptr);
  const hldb::Constant *const c = any_cast<hldb::Constant>(ae->getExprs()->at(0));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "4");
}

TEST_F(DynamicArrayOpNewTest, ArrayExprSizeConstTypeIsUInt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ArrayExpr *const ae =
      any_cast<hldb::Assignment>(
          any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(0))
          ->getRhs<hldb::ArrayExpr>();
  ASSERT_NE(ae, nullptr);
  const hldb::Constant *const c = any_cast<hldb::Constant>(ae->getExprs()->at(0));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), vpiUIntConst);
}

// --- Stmt[1..4]: arr[0..3] = 5..8 ----

TEST_F(DynamicArrayOpNewTest, ElementAssignmentsSetArrZeroThroughThree) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);

  const size_t stmtIndices[4] = {1, 2, 3, 4};
  const char *const expectedNames[4] = {"arr[0]", "arr[1]", "arr[2]", "arr[3]"};
  const char *const expectedIndex[4] = {"0", "1", "2", "3"};
  const char *const expectedRhs[4] = {"5", "6", "7", "8"};
  for (size_t i = 0; i < 4; ++i) {
    const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(stmtIndices[i]));
    ASSERT_NE(assign, nullptr);
    EXPECT_TRUE(assign->getBlocking());

    const hldb::BitSelect *const lhs = assign->getLhs<hldb::BitSelect>();
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getName(), expectedNames[i]);
    const hldb::RefObj *const prefix = lhs->getPrefix<hldb::RefObj>();
    ASSERT_NE(prefix, nullptr);
    EXPECT_EQ(prefix->getName(), "arr");
    EXPECT_NE(prefix->getActual<hldb::Variable>(), nullptr);
    const hldb::Constant *const index = lhs->getIndex<hldb::Constant>();
    ASSERT_NE(index, nullptr);
    EXPECT_EQ(index->getDecompile(), expectedIndex[i]);
    EXPECT_EQ(index->getConstType(), vpiUIntConst);

    const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
    ASSERT_NE(rhs, nullptr);
    EXPECT_EQ(rhs->getDecompile(), expectedRhs[i]);
    EXPECT_EQ(rhs->getConstType(), vpiUIntConst);
  }
}

// --- Stmt[5]: $display ----

TEST_F(DynamicArrayOpNewTest, SixthStmtIsSysTaskCall) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Begin *const begin = any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_NE(any_cast<hldb::SysTaskCall>(begin->getStmts()->at(5)), nullptr);
}

TEST_F(DynamicArrayOpNewTest, DisplayNameIsDisplay) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(
      any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(5));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$display");
}

TEST_F(DynamicArrayOpNewTest, DisplayHasFiveArguments) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(
      any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(5));
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  EXPECT_EQ(call->getArguments()->size(), 5u);
}

TEST_F(DynamicArrayOpNewTest, DisplayFormatStringIsExpected) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(
      any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(5));
  ASSERT_NE(call, nullptr);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: ((%d == 5) and (%d == 6) and (%d == 7) and (%d == 8))");
}

TEST_F(DynamicArrayOpNewTest, DisplayArgumentsAreArrZeroThroughThree) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(
      any_cast<hldb::Initial>(top->getProcesses()->at(0))->getStmt<hldb::Begin>()->getStmts()->at(5));
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  const char *const expectedNames[4] = {"arr[0]", "arr[1]", "arr[2]", "arr[3]"};
  for (size_t i = 0; i < 4; ++i) {
    const hldb::BitSelect *const arg = any_cast<hldb::BitSelect>(call->getArguments()->at(i + 1));
    ASSERT_NE(arg, nullptr);
    EXPECT_EQ(arg->getName(), expectedNames[i]);
    const hldb::RefObj *const prefix = arg->getPrefix<hldb::RefObj>();
    ASSERT_NE(prefix, nullptr);
    EXPECT_EQ(prefix->getName(), "arr");
  }
}

// --- design-level typespecs ----

TEST_F(DynamicArrayOpNewTest, DesignHasThreeTypespecs) {
  // ModuleTypespec "top" + IntTypespec + StringTypespec (from $display)
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(DynamicArrayOpNewTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(DynamicArrayOpNewTest, DesignHasIntTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_NE(any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1)), nullptr);
}

TEST_F(DynamicArrayOpNewTest, DesignHasStringTypespec) {
  // StringTypespec is added because $display uses a string literal argument
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_NE(any_cast<hldb::StringTypespec>(m_design->getTypespecs()->at(2)), nullptr);
}

// --- structural completeness ----

TEST_F(DynamicArrayOpNewTest, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
