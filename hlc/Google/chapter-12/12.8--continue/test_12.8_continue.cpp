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

// Tests for 12.8--continue.sv (tags: 12.8)
//   module jump_tb ();
//     initial begin
//       for (int i = 0; i < 256; i++)begin
//         if(i < 255)
//           continue;
//         $display(":assert:(%d == 255)", i);
//       end
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023 12.8 "Jump statements", p.334,
// checked before any test code was written):
//   "jump_statement ::= return [ expression ] ; | break ; | continue ;
//   ... The continue statement jumps to the end of the loop and
//   executes the loop control if present." Here "int i = 0" is
//   declared inline in the for_initialization (matching
//   12.7.1--for.cpp's shape), so "i" is local to the ForStmt's own
//   scope. Inside the loop body, "if(i < 255) continue;" skips the
//   trailing $display for every iteration except the last, without a
//   trailing else -- so it is a plain IfStmt whose body is a
//   ContinueStmt.
//
//   Also (IEEE 1800-2023 6.8): "int" is an integer_atom_type keyword, a
//   non-net data type, so "i" must be a Variable, never a Net.
//
// What is checked:
//   - module jump_tb has zero Nets
//   - the initial process's single statement resolves to ForStmt; its
//     own scope has exactly 1 Variable "i" (IntTypespec), matching the
//     inline-declaration shape from 12.7.1--for.cpp
//   - ForStmt.getForInitStmts() has 1 item: Assignment with LHS the
//     Variable "i" directly (its own declaration) and RHS Constant "0"
//   - ForStmt.getCondition() is Operation(vpiLtOp) comparing RefObj "i"
//     against Constant "256"; getForIncStmts() has 1 item,
//     Operation(vpiPostIncOp) on RefObj "i"
//   - ForStmt.getStmt() is a Begin (explicit begin-end in source) with
//     exactly 2 statements:
//     1) IfStmt whose condition is Operation(vpiLtOp) comparing RefObj
//        "i" against Constant "255", and whose body is a plain
//        ContinueStmt (AnyType::ContinueStmt, no else)
//     2) SysTaskCall "$display" with 2 arguments: Constant
//        "\":assert:(%d == 255)\"" and RefObj "i"
//
// What is NOT checked and why:
//   - the runtime value that "i" actually holds on the one iteration
//     where $display executes (the ":assert:" comment implies a
//     simulation-time expectation of 255) is a simulation-time concept
//   - the "continue skips to the end of the loop and re-runs the loop
//     control" runtime behavior itself is likewise a simulation-time
//     concept, not a static/structural compile-time property

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/continue_stmt.h>
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

class ContinueTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "12.8--continue.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("jump_tb", m_design->getAllModules());
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

TEST_F(ContinueTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ContinueTest, ModuleHasNoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty()) << "no wire/net_type keyword is declared";
}

TEST_F(ContinueTest, ForStmtScopeHasExactlyOneVariableI) {
  const hldb::ForStmt *const forStmt = getForStmt();
  ASSERT_NE(forStmt, nullptr) << "the single statement in the initial body should resolve to ForStmt";
  ASSERT_NE(forStmt->getVariables(), nullptr) << "'int i' declared inline should live in ForStmt's own scope";
  ASSERT_EQ(forStmt->getVariables()->size(), 1u);
  EXPECT_NE(hldb::findByName<hldb::Variable>("i", forStmt->getVariables()), nullptr) << "Variable 'i' not found";
}

// ---------------------------------------------------------------------------
// for (int i = 0; i < 256; i++)
// ---------------------------------------------------------------------------
TEST_F(ContinueTest, ForInitStmtDeclaresIAsZero) {
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

TEST_F(ContinueTest, ConditionIsILessThan256) {
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

TEST_F(ContinueTest, ForStepIsPostIncrementOfI) {
  const hldb::ForStmt *const forStmt = getForStmt();
  ASSERT_NE(forStmt, nullptr);
  ASSERT_NE(forStmt->getForIncStmts(), nullptr);
  ASSERT_EQ(forStmt->getForIncStmts()->size(), 1u);
  const hldb::Operation *const step = any_cast<hldb::Operation>(forStmt->getForIncStmts()->at(0));
  ASSERT_NE(step, nullptr);
  EXPECT_EQ(step->getOpType(), vpiPostIncOp);
}

// ---------------------------------------------------------------------------
// begin if(i < 255) continue; $display(...); end
// ---------------------------------------------------------------------------
TEST_F(ContinueTest, LoopBodyIsBeginWithIfContinueThenDisplay) {
  const hldb::ForStmt *const forStmt = getForStmt();
  ASSERT_NE(forStmt, nullptr);
  const hldb::Begin *const loopBody = forStmt->getStmt<hldb::Begin>();
  ASSERT_NE(loopBody, nullptr) << "for-loop body should be a Begin (explicit begin-end in source)";
  ASSERT_NE(loopBody->getStmts(), nullptr);
  ASSERT_EQ(loopBody->getStmts()->size(), 2u);

  const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(loopBody->getStmts()->at(0));
  ASSERT_NE(ifStmt, nullptr) << "first loop-body statement should be a plain IfStmt (no else)";
  const hldb::Operation *const cond = ifStmt->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiLtOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "i");
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(cond->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "255");
  const hldb::ContinueStmt *const cont = ifStmt->getStmt<hldb::ContinueStmt>();
  ASSERT_NE(cont, nullptr) << "if-body should be a plain ContinueStmt";

  const hldb::SysTaskCall *const display = any_cast<hldb::SysTaskCall>(loopBody->getStmts()->at(1));
  ASSERT_NE(display, nullptr);
  EXPECT_EQ(display->getName(), "$display");
  ASSERT_NE(display->getArguments(), nullptr);
  ASSERT_EQ(display->getArguments()->size(), 2u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(display->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getDecompile(), "\":assert:(%d == 255)\"");
  const hldb::RefObj *const iArg = any_cast<hldb::RefObj>(display->getArguments()->at(1));
  ASSERT_NE(iArg, nullptr);
  EXPECT_EQ(iArg->getName(), "i");
}

TEST_F(ContinueTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
