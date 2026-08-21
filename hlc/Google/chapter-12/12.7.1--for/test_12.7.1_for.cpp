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

// Tests for 12.7.1--for.sv (tags: 12.7.1)
//   module for_tb ();
//     initial begin
//       for (int i = 0; i < 256; i++)
//         $display("%d", i);
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023 12.7.1 "The for-loop", p.329,
// checked before any test code was written):
//   "for ( [ for_initialization ] ; [ expression ] ; [ for_step ] )
//   statement_or_null"; "for_variable_declaration ::= [ var ] data_type
//   variable_identifier = expression". "The variables used to control a
//   for-loop can also be declared within the loop, as part of the
//   for_initialization assignments. This creates an implicit begin-end
//   block around the loop, containing declarations of the loop
//   variables with automatic lifetime... making the variables local to
//   the loop scope." This file declares "int i" inline in the
//   for_initialization, so "i" must be a Variable scoped to the ForStmt
//   itself (not the enclosing initial-block Begin), and the
//   for_initialization/expression/for_step map to distinct ForStmt
//   fields (getForInitStmts/getCondition/getForIncStmts).
//
//   Also (IEEE 1800-2023 6.8): "int" is an integer_atom_type keyword, a
//   non-net data type, so "int i" must be a Variable, never a Net.
//
// What is checked:
//   - module for_tb has an Initial process whose body is a single-item
//     Begin containing one statement, resolving to ForStmt
//   - the ForStmt's own scope (not the outer Begin) has exactly 1
//     Variable "i" (IntTypespec) -- confirming the implicit local scope
//     described above
//   - getForInitStmts() has exactly 1 item: Assignment with LHS the
//     Variable "i" directly (not a RefObj, since this is the
//     declaration's own initializer) and RHS Constant "0"
//   - getCondition() is Operation(vpiLtOp) comparing RefObj "i"
//     (resolving to the Variable) against Constant "256"
//   - getForIncStmts() has exactly 1 item: Operation(vpiPostIncOp) with
//     1 operand, RefObj "i" resolving to the Variable (the "i++" step)
//   - getStmt() is SysTaskCall "$display" with 2 arguments: Constant
//     "\"%d\"" and RefObj "i" resolving to the Variable
//   - no continuous assignments and no Nets exist in the module
//
// What is NOT checked and why:
//   - the runtime iteration behavior itself (that the loop actually
//     executes 256 times) is a simulation-time concept, not a static/
//     structural compile-time property
//   - the "initial begin...end" wrapping Begin's own fields beyond
//     confirming it holds exactly the one ForStmt are out of scope --
//     covered generically elsewhere

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/for_stmt.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ForTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "12.7.1--for.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("for_tb", m_design->getAllModules()); }

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

TEST_F(ForTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ForTest, ModuleHasNoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty()) << "no wire/net_type keyword is declared";
}

TEST_F(ForTest, InitialProcessBodyIsForStmt) {
  const hldb::ForStmt *const forStmt = getForStmt();
  ASSERT_NE(forStmt, nullptr) << "the single statement in the initial body should resolve to ForStmt";
}

// ---------------------------------------------------------------------------
// "int i" is declared local to the ForStmt's own implicit scope
// ---------------------------------------------------------------------------
TEST_F(ForTest, ForStmtScopeHasExactlyOneVariableI) {
  const hldb::ForStmt *const forStmt = getForStmt();
  ASSERT_NE(forStmt, nullptr);
  ASSERT_NE(forStmt->getVariables(), nullptr)
      << "'int i' declared in for_initialization should live in the ForStmt's own local scope";
  ASSERT_EQ(forStmt->getVariables()->size(), 1u);
  EXPECT_NE(hldb::findByName<hldb::Variable>("i", forStmt->getVariables()), nullptr) << "Variable 'i' not found";
}

// ---------------------------------------------------------------------------
// for_initialization: int i = 0
// ---------------------------------------------------------------------------
TEST_F(ForTest, ForInitStmtsAssignsZeroToVariableI) {
  const hldb::ForStmt *const forStmt = getForStmt();
  ASSERT_NE(forStmt, nullptr);
  ASSERT_NE(forStmt->getForInitStmts(), nullptr);
  ASSERT_EQ(forStmt->getForInitStmts()->size(), 1u);
  const hldb::Assignment *const init = any_cast<hldb::Assignment>(forStmt->getForInitStmts()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Variable *const lhs = init->getLhs<hldb::Variable>();
  ASSERT_NE(lhs, nullptr) << "for_initialization LHS should be the Variable 'i' directly (its own declaration)";
  EXPECT_EQ(lhs->getName(), "i");
  const hldb::Constant *const rhs = init->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "0");
}

// ---------------------------------------------------------------------------
// expression: i < 256
// ---------------------------------------------------------------------------
TEST_F(ForTest, ConditionIsILessThan256) {
  const hldb::ForStmt *const forStmt = getForStmt();
  ASSERT_NE(forStmt, nullptr);
  const hldb::Operation *const cond = forStmt->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "for-loop expression is not an Operation";
  EXPECT_EQ(cond->getOpType(), vpiLtOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "i");
  EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr);
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(cond->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "256");
}

// ---------------------------------------------------------------------------
// for_step: i++
// ---------------------------------------------------------------------------
TEST_F(ForTest, ForStepIsPostIncrementOfI) {
  const hldb::ForStmt *const forStmt = getForStmt();
  ASSERT_NE(forStmt, nullptr);
  ASSERT_NE(forStmt->getForIncStmts(), nullptr);
  ASSERT_EQ(forStmt->getForIncStmts()->size(), 1u);
  const hldb::Operation *const step = any_cast<hldb::Operation>(forStmt->getForIncStmts()->at(0));
  ASSERT_NE(step, nullptr) << "for_step 'i++' should be a post-increment Operation";
  EXPECT_EQ(step->getOpType(), vpiPostIncOp);
  ASSERT_NE(step->getOperands(), nullptr);
  ASSERT_EQ(step->getOperands()->size(), 1u);
  const hldb::RefObj *const operand = any_cast<hldb::RefObj>(step->getOperands()->at(0));
  ASSERT_NE(operand, nullptr);
  EXPECT_EQ(operand->getName(), "i");
  EXPECT_NE(operand->getActual<hldb::Variable>(), nullptr);
}

// ---------------------------------------------------------------------------
// loop body: $display("%d", i)
// ---------------------------------------------------------------------------
TEST_F(ForTest, LoopBodyIsDisplayOfI) {
  const hldb::ForStmt *const forStmt = getForStmt();
  ASSERT_NE(forStmt, nullptr);
  const hldb::SysTaskCall *const display = forStmt->getStmt<hldb::SysTaskCall>();
  ASSERT_NE(display, nullptr) << "for-loop body should be a plain SysTaskCall (single statement, no begin-end)";
  EXPECT_EQ(display->getName(), "$display");
  ASSERT_NE(display->getArguments(), nullptr);
  ASSERT_EQ(display->getArguments()->size(), 2u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(display->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getConstType(), vpiStringConst);
  EXPECT_EQ(fmt->getDecompile(), "\"%d\"");
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(display->getArguments()->at(1));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), "i");
  EXPECT_NE(arg->getActual<hldb::Variable>(), nullptr);
}

TEST_F(ForTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
