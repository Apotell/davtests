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

// Tests for 12.8--break.sv (tags: 12.8)
//   module jump_tb ();
//     initial begin
//       int i;
//       for (i = 0; i < 256; i++)begin
//         if(i > 100)
//           break;
//       end
//       $display(":assert:(%d == 101)", i);
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023 12.8 "Jump statements", p.334,
// checked before any test code was written):
//   "jump_statement ::= return [ expression ] ; | break ; | continue ;
//   ... The break statement jumps out of the loop." Here "int i;" is
//   declared as an ordinary statement in the enclosing "initial begin"
//   (with no initializer), and the for-loop's own for_initialization
//   is a plain assignment "i = 0" to that pre-existing variable -- not
//   an inline declaration -- so (unlike 12.7.1--for.cpp, where "int i"
//   was declared inline) the ForStmt itself has no local variables of
//   its own, and its for_initialization's LHS is a RefObj, not the
//   Variable directly.
//
//   Also (IEEE 1800-2023 6.8): "int" is an integer_atom_type keyword, a
//   non-net data type, so "i" must be a Variable, never a Net.
//
// What is checked:
//   - module jump_tb has zero Nets; the enclosing "initial begin" Begin
//     has exactly 1 Variable "i" with no initial value (getValue() is
//     null, since "int i;" has no "= expression")
//   - the Begin's first statement is ForStmt; ForStmt itself has no
//     local Variables (contrast with 12.7.1--for.cpp/12.8--continue.cpp,
//     where "int i" is declared inline)
//   - ForStmt.getForInitStmts() has 1 item: Assignment with LHS RefObj
//     "i" (resolving to the Variable, not the Variable directly) and
//     RHS Constant "0"
//   - ForStmt.getCondition() is Operation(vpiLtOp) comparing RefObj "i"
//     against Constant "256"; getForIncStmts() has 1 item,
//     Operation(vpiPostIncOp) on RefObj "i"
//   - ForStmt.getStmt() is a Begin (explicit begin-end in source) with
//     exactly 1 statement: IfStmt whose condition is Operation(vpiGtOp)
//     comparing RefObj "i" against Constant "100", and whose body is a
//     plain BreakStmt (AnyType::BreakStmt)
//   - after the loop, the Begin's second statement is SysTaskCall
//     "$display" with 2 arguments: Constant "\":assert:(%d == 101)\""
//     and RefObj "i" (resolving to the same Variable, still in scope
//     after the loop since it was declared outside the loop)
//
// What is NOT checked and why:
//   - the runtime value that "i" actually holds when the loop
//     terminates (the ":assert:" comment in the source implies a
//     simulation-time expectation of 101) is a simulation-time concept,
//     not a static/structural compile-time property
//   - the "break jumps out of the entire loop" runtime control-flow
//     behavior itself is likewise a simulation-time concept

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/break_stmt.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/for_stmt.h>
#include <hldb/if_stmt.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class BreakTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "12.8--break.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("jump_tb", m_design->getAllModules());
  }

  static const hldb::Begin *getInitialBody() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const initial = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (initial == nullptr) return nullptr;
    return initial->getStmt<hldb::Begin>();
  }

  static const hldb::ForStmt *getForStmt() {
    const hldb::Begin *const body = getInitialBody();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) return nullptr;
    return any_cast<hldb::ForStmt>(body->getStmts()->at(0));
  }
};

TEST_F(BreakTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(BreakTest, ModuleHasNoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty()) << "no wire/net_type keyword is declared";
}

TEST_F(BreakTest, InitialBeginScopeHasVariableIWithNoInitialValue) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getVariables(), nullptr) << "'int i' should live in the enclosing Begin's scope";
  ASSERT_EQ(body->getVariables()->size(), 1u);
  const hldb::Variable *const i = hldb::findByName<hldb::Variable>("i", body->getVariables());
  ASSERT_NE(i, nullptr) << "Variable 'i' not found";
  EXPECT_EQ(i->getValue<hldb::Any>(), nullptr) << "'int i;' has no initializer";
}

TEST_F(BreakTest, FirstStmtInBeginIsForStmtWithNoLocalVariables) {
  const hldb::ForStmt *const forStmt = getForStmt();
  ASSERT_NE(forStmt, nullptr) << "the first statement in the initial body should resolve to ForStmt";
  EXPECT_TRUE(forStmt->getVariables() == nullptr || forStmt->getVariables()->empty())
      << "'i' was declared outside the loop, so ForStmt itself should have no local variables";
}

// ---------------------------------------------------------------------------
// for (i = 0; i < 256; i++)
// ---------------------------------------------------------------------------
TEST_F(BreakTest, ForInitStmtAssignsZeroToRefObjI) {
  const hldb::ForStmt *const forStmt = getForStmt();
  ASSERT_NE(forStmt, nullptr);
  ASSERT_NE(forStmt->getForInitStmts(), nullptr);
  ASSERT_EQ(forStmt->getForInitStmts()->size(), 1u);
  const hldb::Assignment *const init = any_cast<hldb::Assignment>(forStmt->getForInitStmts()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::RefObj *const lhs = init->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "for_initialization LHS should be RefObj 'i' (plain assignment, not a declaration)";
  EXPECT_EQ(lhs->getName(), "i");
  EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr);
  const hldb::Constant *const rhs = init->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "0");
}

TEST_F(BreakTest, ConditionIsILessThan256) {
  const hldb::ForStmt *const forStmt = getForStmt();
  ASSERT_NE(forStmt, nullptr);
  const hldb::Operation *const cond = forStmt->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiLtOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(cond->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "256");
}

TEST_F(BreakTest, ForStepIsPostIncrementOfI) {
  const hldb::ForStmt *const forStmt = getForStmt();
  ASSERT_NE(forStmt, nullptr);
  ASSERT_NE(forStmt->getForIncStmts(), nullptr);
  ASSERT_EQ(forStmt->getForIncStmts()->size(), 1u);
  const hldb::Operation *const step = any_cast<hldb::Operation>(forStmt->getForIncStmts()->at(0));
  ASSERT_NE(step, nullptr);
  EXPECT_EQ(step->getOpType(), vpiPostIncOp);
}

// ---------------------------------------------------------------------------
// begin if(i > 100) break; end
// ---------------------------------------------------------------------------
TEST_F(BreakTest, LoopBodyIsBeginWithIfBreak) {
  const hldb::ForStmt *const forStmt = getForStmt();
  ASSERT_NE(forStmt, nullptr);
  const hldb::Begin *const loopBody = forStmt->getStmt<hldb::Begin>();
  ASSERT_NE(loopBody, nullptr) << "for-loop body should be a Begin (explicit begin-end in source)";
  ASSERT_NE(loopBody->getStmts(), nullptr);
  ASSERT_EQ(loopBody->getStmts()->size(), 1u);
  const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(loopBody->getStmts()->at(0));
  ASSERT_NE(ifStmt, nullptr) << "loop body statement should be a plain IfStmt (no else)";

  const hldb::Operation *const cond = ifStmt->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiGtOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "i");
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(cond->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "100");

  const hldb::BreakStmt *const brk = ifStmt->getStmt<hldb::BreakStmt>();
  ASSERT_NE(brk, nullptr) << "if-body should be a plain BreakStmt";
}

// ---------------------------------------------------------------------------
// $display(":assert:(%d == 101)", i);  -- after the loop
// ---------------------------------------------------------------------------
TEST_F(BreakTest, DisplayAfterLoopReferencesI) {
  const hldb::Begin *const body = getInitialBody();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_GE(body->getStmts()->size(), 2u);
  const hldb::SysTaskCall *const display = any_cast<hldb::SysTaskCall>(body->getStmts()->at(1));
  ASSERT_NE(display, nullptr);
  EXPECT_EQ(display->getName(), "$display");
  ASSERT_NE(display->getArguments(), nullptr);
  ASSERT_EQ(display->getArguments()->size(), 2u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(display->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getDecompile(), "\":assert:(%d == 101)\"");
  const hldb::RefObj *const iArg = any_cast<hldb::RefObj>(display->getArguments()->at(1));
  ASSERT_NE(iArg, nullptr);
  EXPECT_EQ(iArg->getName(), "i");
  EXPECT_NE(iArg->getActual<hldb::Variable>(), nullptr);
}

TEST_F(BreakTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
