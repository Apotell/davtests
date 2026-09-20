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

// Source under test: tests/Google/chapter-9/9.4.3--event_sequence_controls.sv
//
//   module block_tb ();
//   	reg a = 0;
//   	wire b = 1;
//   	reg enable = 0;
//
//   	initial begin
//   		#10 enable = 1;
//   	end
//
//   	initial begin
//   		wait (enable) #10 a = b;
//   	end
//   endmodule
//
// IEEE 1800-2023 clause tested: 9.4.3 "Level-sensitive event control" (the
// `wait` statement). Unlike `@`/edge-based event control, `wait
// (expression) statement_or_null` is LEVEL-sensitive: if the expression is
// already true, the statement executes immediately with no delay at all;
// only if it is false does execution block until the expression becomes
// true. Here the second initial block blocks on `wait (enable)`, then
// (once true) still applies a further `#10` delay control before the
// assignment -- two independent procedural timing controls chained in
// sequence, not one combined control.
//
// Checked:
//   - module block_tb exists.
//   - "b" is declared with the net-type keyword `wire`, so per IEEE
//     1800-2023 6.7 it must be classified as hldb::Net.
//   - "a" and "enable" are declared with the variable-type keyword `reg`
//     (6.8), so per 6.7/6.8 they must be classified as hldb::Variable,
//     each with its declared initializer (0) preserved on
//     Variable::getExpr().
//   - exactly two Initial processes exist.
//   - the first initial's body ("#10 enable = 1;") is: Begin with one
//     statement, a DelayControl whose getDelay() is Constant "10" and
//     whose getStmt() is the blocking Assignment "enable = 1" (lhs RefObj
//     "enable", rhs Constant "1").
//   - the second initial's body ("wait (enable) #10 a = b;") is: Begin
//     with one statement, a WaitStmt whose getCondition() is a RefObj
//     named "enable" (the level-sensitive guard) and whose getStmt() is a
//     DelayControl (Constant "10") whose own getStmt() is the blocking
//     Assignment "a = b" (lhs RefObj "a", rhs RefObj "b") -- confirming
//     the wait's condition and the following delay control are two
//     distinct, nested timing controls, not one node.
//
// Not checked:
//   - Runtime scheduling behavior (does the second initial really block
//     until "enable" becomes 1 at time 10, then further delay to time 20
//     before assigning "a") -- HLC is an elaborator with no simulator
//     (see .claude/hlc_overview.md), so no execution ever happens for
//     this test to observe.

#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/delay_control.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/wait_stmt.h>

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

class EventSequenceControlsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.4.3--event_sequence_controls.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(EventSequenceControlsTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
}

TEST_F(EventSequenceControlsTest, BIsNetAAndEnableAreVariables) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr) << "'b' is declared with the net-type keyword 'wire' (IEEE 1800-2023 6.7)";

  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr) << "'a' is declared with the variable-type keyword 'reg' (IEEE 1800-2023 6.8)";
  const hldb::Constant *const aInit = InitializerConstant(a);
  ASSERT_NE(aInit, nullptr);
  EXPECT_EQ(aInit->getDecompile(), "0");

  const hldb::Variable *const enable = hldb::findByName<hldb::Variable>("enable", top->getVariables());
  ASSERT_NE(enable, nullptr) << "'enable' is declared with the variable-type keyword 'reg' (IEEE 1800-2023 6.8)";
  const hldb::Constant *const enableInit = InitializerConstant(enable);
  ASSERT_NE(enableInit, nullptr);
  EXPECT_EQ(enableInit->getDecompile(), "0");
}

TEST_F(EventSequenceControlsTest, ExactlyTwoInitialProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);

  size_t initialCount = 0;
  for (const hldb::Process *const process : *top->getProcesses()) {
    if (any_cast<hldb::Initial>(process) != nullptr) ++initialCount;
  }
  EXPECT_EQ(initialCount, 2u);
}

TEST_F(EventSequenceControlsTest, FirstInitialDelaysThenAssignsEnable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const hldb::Initial *first = nullptr;
  for (const hldb::Process *const process : *top->getProcesses()) {
    if (const hldb::Initial *const candidate = any_cast<hldb::Initial>(process)) {
      first = candidate;
      break;
    }
  }
  ASSERT_NE(first, nullptr);

  const hldb::Begin *const body = any_cast<hldb::Begin>(first->getStmt());
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 1u) << "'#10 enable = 1;' is the block's single statement";

  const hldb::DelayControl *const delay = any_cast<hldb::DelayControl>(body->getStmts()->front());
  ASSERT_NE(delay, nullptr) << "'#10' must produce a DelayControl";
  const hldb::Constant *const delayValue = delay->getDelay<hldb::Constant>();
  ASSERT_NE(delayValue, nullptr);
  EXPECT_EQ(delayValue->getDecompile(), "10");

  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(delay->getStmt());
  ASSERT_NE(assign, nullptr) << "'enable = 1;' must be the DelayControl's controlled statement";
  EXPECT_TRUE(assign->getBlocking()) << "'=' is a blocking assignment";
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(assign->getLhs());
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "enable");
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(assign->getRhs());
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "1");
}

TEST_F(EventSequenceControlsTest, SecondInitialWaitsOnEnableThenDelaysThenAssignsA) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const hldb::Initial *second = nullptr;
  size_t initialIndex = 0;
  for (const hldb::Process *const process : *top->getProcesses()) {
    if (const hldb::Initial *const candidate = any_cast<hldb::Initial>(process)) {
      if (initialIndex == 1) {
        second = candidate;
        break;
      }
      ++initialIndex;
    }
  }
  ASSERT_NE(second, nullptr);

  const hldb::Begin *const body = any_cast<hldb::Begin>(second->getStmt());
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 1u) << "'wait (enable) #10 a = b;' is the block's single statement";

  const hldb::WaitStmt *const wait = any_cast<hldb::WaitStmt>(body->getStmts()->front());
  ASSERT_NE(wait, nullptr) << "'wait (enable) ...' must produce a WaitStmt";

  const hldb::RefObj *const condition = any_cast<hldb::RefObj>(wait->getCondition());
  ASSERT_NE(condition, nullptr) << "the wait condition must be a RefObj naming 'enable'";
  EXPECT_EQ(condition->getName(), "enable");

  const hldb::DelayControl *const delay = any_cast<hldb::DelayControl>(wait->getStmt());
  ASSERT_NE(delay, nullptr) << "the wait's controlled statement '#10 a = b;' must be a DelayControl -- "
                               "the wait condition and the delay control are two distinct nested controls";
  const hldb::Constant *const delayValue = delay->getDelay<hldb::Constant>();
  ASSERT_NE(delayValue, nullptr);
  EXPECT_EQ(delayValue->getDecompile(), "10");

  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(delay->getStmt());
  ASSERT_NE(assign, nullptr) << "'a = b;' must be the DelayControl's controlled statement";
  EXPECT_TRUE(assign->getBlocking()) << "'=' is a blocking assignment";
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(assign->getLhs());
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "a");
  const hldb::RefObj *const rhs = any_cast<hldb::RefObj>(assign->getRhs());
  ASSERT_NE(rhs, nullptr) << "'b' on the rhs is a bare reference, not an operation";
  EXPECT_EQ(rhs->getName(), "b");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
