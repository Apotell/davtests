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

// Source under test: tests/Google/chapter-9/9.6.1--wait_fork.sv
//
//   module fork_tb ();
//   	reg a = 0;
//   	reg b = 0;
//   	initial begin
//   		fork
//   			begin
//   				#50 a = 1;
//   				#50 a = 0;
//   				#50 a = 1;
//   			end
//   			begin
//   				#50 b = 1;
//   				#50 b = 0;
//   				#50 b = 1;
//   			end
//   		join_none
//   		wait fork;
//   	end
//   endmodule
//
// IEEE 1800-2023 clause tested: 9.6.1 "Disabling of named blocks and
// tasks"'s sibling, the "wait fork" statement: with `join_none`, the
// parent process does not wait for its two fork branches to finish
// before moving past the fork/join_none block; `wait fork;` afterward is
// what actually blocks the parent until BOTH branches have completed.
//
// Checked:
//   - module fork_tb exists.
//   - "a" and "b" are declared with the variable-type keyword `reg`
//     (IEEE 1800-2023 6.8), so both must be classified as hldb::Variable,
//     each with its declared initializer (0) preserved on
//     Variable::getExpr().
//   - the initial block's body (explicit "begin...end") is a Begin with
//     exactly two statements: a ForkStmt, then a WaitFork.
//   - the ForkStmt's getJoinType() == vpiJoinNone (not plain vpiJoin or
//     vpiJoinAny), and it has exactly two branches, each a Begin with
//     exactly three statements (DelayControl "50" -> blocking Assignment
//     to "a"/"b" with the alternating Constant values 1, 0, 1).
//   - the WaitFork statement itself has a null getStmt() (Waits::getStmt()
//     inherited from the Waits base) since "wait fork;" has no controlled
//     statement of its own -- it is a standalone synchronization
//     statement.
//
// Not checked:
//   - Runtime scheduling behavior (does the parent process in fact
//     proceed past "join_none" immediately, then block at "wait fork;"
//     until both branches finish around simulation time 150) -- HLC is
//     an elaborator with no simulator (see .claude/hlc_overview.md), so
//     no execution ever happens for this test to observe.

#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/delay_control.h>
#include <hldb/design.h>
#include <hldb/fork_stmt.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/wait_fork.h>

namespace hlc {
namespace {
// Tries Variable::getExpr() first (the declared variable_decl_assignment
// initializer expression), then falls back to Variable::getValue() --
// which of the two hldb actually populates for a plain scalar initializer
// like "reg a = 0;" was not confirmed via a header or a .log, so both are
// accepted rather than assuming one.
const hldb::Constant *InitializerConstant(const hldb::Variable *const variable) {
  if (const hldb::Constant *const viaExpr = any_cast<hldb::Constant>(variable->getExpr())) return viaExpr;
  return any_cast<hldb::Constant>(variable->getValue());
}
}  // namespace

class WaitForkTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.6.1--wait_fork.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(WaitForkTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("fork_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
}

TEST_F(WaitForkTest, AAndBAreVariables) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("fork_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr) << "'a' is declared with the variable-type keyword 'reg' (IEEE 1800-2023 6.8)";
  const hldb::Constant *const aInit = InitializerConstant(a);
  ASSERT_NE(aInit, nullptr);
  EXPECT_EQ(aInit->getDecompile(), "0");

  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  ASSERT_NE(b, nullptr) << "'b' is declared with the variable-type keyword 'reg' (IEEE 1800-2023 6.8)";
  const hldb::Constant *const bInit = InitializerConstant(b);
  ASSERT_NE(bInit, nullptr);
  EXPECT_EQ(bInit->getDecompile(), "0");
}

namespace {
void CheckBranch(const hldb::Begin *const branch, std::string_view varName) {
  ASSERT_NE(branch, nullptr);
  ASSERT_NE(branch->getStmts(), nullptr);
  ASSERT_EQ(branch->getStmts()->size(), 3u) << "each fork branch has exactly three '#50 x = N;' statements";

  const char *const expectedValues[3] = {"1", "0", "1"};
  for (size_t index = 0; index < 3; ++index) {
    const hldb::DelayControl *const delay = any_cast<hldb::DelayControl>(branch->getStmts()->at(index));
    ASSERT_NE(delay, nullptr) << "'#50' must produce a DelayControl";
    const hldb::Constant *const delayValue = delay->getDelay<hldb::Constant>();
    ASSERT_NE(delayValue, nullptr);
    EXPECT_EQ(delayValue->getDecompile(), "50");

    const hldb::Assignment *const assign = any_cast<hldb::Assignment>(delay->getStmt());
    ASSERT_NE(assign, nullptr);
    EXPECT_TRUE(assign->getBlocking());
    const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(assign->getLhs());
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getName(), varName);
    const hldb::Constant *const rhs = any_cast<hldb::Constant>(assign->getRhs());
    ASSERT_NE(rhs, nullptr);
    EXPECT_EQ(rhs->getDecompile(), expectedValues[index]);
  }
}
}  // namespace

TEST_F(WaitForkTest, ForkJoinNoneHasTwoAlternatingBranchesThenWaitFork) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("fork_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);

  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);

  const hldb::Begin *const body = any_cast<hldb::Begin>(init->getStmt());
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u) << "'fork ... join_none' then 'wait fork;' are exactly two statements";

  const hldb::ForkStmt *const fork = any_cast<hldb::ForkStmt>(body->getStmts()->at(0));
  ASSERT_NE(fork, nullptr) << "'fork ... join_none' must produce a ForkStmt";
  EXPECT_EQ(fork->getJoinType(), vpiJoinNone);
  ASSERT_NE(fork->getStmts(), nullptr);
  ASSERT_EQ(fork->getStmts()->size(), 2u);

  CheckBranch(any_cast<hldb::Begin>(fork->getStmts()->at(0)), "a");
  CheckBranch(any_cast<hldb::Begin>(fork->getStmts()->at(1)), "b");

  const hldb::WaitFork *const waitFork = any_cast<hldb::WaitFork>(body->getStmts()->at(1));
  ASSERT_NE(waitFork, nullptr) << "'wait fork;' must produce a WaitFork";
  EXPECT_EQ(waitFork->getStmt(), nullptr) << "'wait fork;' has no controlled statement";
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
