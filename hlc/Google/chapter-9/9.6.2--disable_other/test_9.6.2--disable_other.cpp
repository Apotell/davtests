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

// Source under test: tests/Google/chapter-9/9.6.2--disable_other.sv
//
//   module fork_tb ();
//   	reg a = 0;
//   	reg b = 0;
//   	reg c = 0;
//   	initial fork
//   		begin: block
//   			#10 a = 1;
//   			#10 b = 1;
//   		end
//   		#15 disable block;
//   	join
//   endmodule
//
// IEEE 1800-2023 clause tested: 9.6.2 "Disabling of named blocks and
// tasks", the "disable OTHER task/block" form: unlike
// 9.6.2--disable.sv (where a block disables itself from inside), here a
// SIBLING fork branch disables "block" from outside it, after its own
// "#15" delay -- a purely structural, cross-branch name reference.
//
// Checked:
//   - module fork_tb exists.
//   - "a", "b", and "c" are declared with the variable-type keyword `reg`
//     (IEEE 1800-2023 6.8), so all three must be classified as
//     hldb::Variable, each with its declared initializer (0) preserved on
//     Variable::getExpr() ("c" is declared but otherwise unused in this
//     source).
//   - the initial statement (no begin/end at the initial level) binds a
//     ForkStmt directly, with getJoinType() == vpiJoin and exactly two
//     branches.
//   - the first branch is a Begin named "block" (from "begin: block")
//     containing exactly two statements: DelayControl "10" -> blocking
//     Assignment "a = 1;", then DelayControl "10" -> blocking Assignment
//     "b = 1;".
//   - the second branch is NOT wrapped in begin/end -- it is a bare
//     DelayControl "15" whose controlled statement is a Disable naming
//     "block" via a RefObj.
//
// Not checked:
//   - Whether the Disable's RefObj actually resolves (getActual()) back
//     to the sibling fork branch's named Begin -- cross-branch name
//     resolution is not assumed (see the same caveat in
//     9.6.2--disable.sv).
//   - Runtime behavior (does "disable block;" at simulation time 15
//     really terminate "block" mid-way through its own #10+#10 delay
//     chain) -- HLC is an elaborator with no simulator (see
//     .claude/hlc_overview.md), so no execution ever happens for this
//     test to observe.

#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/delay_control.h>
#include <hldb/design.h>
#include <hldb/disable.h>
#include <hldb/fork_stmt.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>

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

class DisableOtherTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.6.2--disable_other.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(DisableOtherTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("fork_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
}

TEST_F(DisableOtherTest, AAndBAndCAreVariables) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("fork_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  for (const char *const name : {"a", "b", "c"}) {
    const hldb::Variable *const variable = hldb::findByName<hldb::Variable>(name, top->getVariables());
    ASSERT_NE(variable, nullptr) << name << " is declared with the variable-type keyword 'reg' (IEEE 1800-2023 6.8)";
    const hldb::Constant *const init = InitializerConstant(variable);
    ASSERT_NE(init, nullptr);
    EXPECT_EQ(init->getDecompile(), "0");
  }
}

TEST_F(DisableOtherTest, FirstBranchIsNamedBlockWithTwoDelayedAssignments) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("fork_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);

  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);

  const hldb::ForkStmt *const fork = any_cast<hldb::ForkStmt>(init->getStmt());
  ASSERT_NE(fork, nullptr) << "'initial fork ... join' must bind a ForkStmt directly";
  EXPECT_EQ(fork->getJoinType(), vpiJoin);
  ASSERT_NE(fork->getStmts(), nullptr);
  ASSERT_EQ(fork->getStmts()->size(), 2u);

  const hldb::Begin *const block = any_cast<hldb::Begin>(fork->getStmts()->at(0));
  ASSERT_NE(block, nullptr) << "'begin: block ... end' must produce a Begin";
  EXPECT_EQ(block->getName(), "block");
  ASSERT_NE(block->getStmts(), nullptr);
  ASSERT_EQ(block->getStmts()->size(), 2u) << "'#10 a = 1;' and '#10 b = 1;' are exactly two statements";

  const hldb::DelayControl *const delayA = any_cast<hldb::DelayControl>(block->getStmts()->at(0));
  ASSERT_NE(delayA, nullptr);
  EXPECT_EQ(delayA->getDelay<hldb::Constant>()->getDecompile(), "10");
  const hldb::Assignment *const assignA = any_cast<hldb::Assignment>(delayA->getStmt());
  ASSERT_NE(assignA, nullptr);
  EXPECT_TRUE(assignA->getBlocking());
  EXPECT_EQ(any_cast<hldb::RefObj>(assignA->getLhs())->getName(), "a");

  const hldb::DelayControl *const delayB = any_cast<hldb::DelayControl>(block->getStmts()->at(1));
  ASSERT_NE(delayB, nullptr);
  EXPECT_EQ(delayB->getDelay<hldb::Constant>()->getDecompile(), "10");
  const hldb::Assignment *const assignB = any_cast<hldb::Assignment>(delayB->getStmt());
  ASSERT_NE(assignB, nullptr);
  EXPECT_TRUE(assignB->getBlocking());
  EXPECT_EQ(any_cast<hldb::RefObj>(assignB->getLhs())->getName(), "b");
}

TEST_F(DisableOtherTest, SecondBranchIsBareDelayedDisableOfBlock) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("fork_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);
  const hldb::ForkStmt *const fork = any_cast<hldb::ForkStmt>(init->getStmt());
  ASSERT_NE(fork, nullptr);
  ASSERT_EQ(fork->getStmts()->size(), 2u);

  const hldb::DelayControl *const delay = any_cast<hldb::DelayControl>(fork->getStmts()->at(1));
  ASSERT_NE(delay, nullptr) << "'#15 disable block;' (not wrapped in begin/end) must be a bare DelayControl";
  const hldb::Constant *const delayValue = delay->getDelay<hldb::Constant>();
  ASSERT_NE(delayValue, nullptr);
  EXPECT_EQ(delayValue->getDecompile(), "15");

  const hldb::Disable *const disable = any_cast<hldb::Disable>(delay->getStmt());
  ASSERT_NE(disable, nullptr) << "'disable block;' must be the DelayControl's controlled statement";
  const hldb::RefObj *const disableRef = any_cast<hldb::RefObj>(disable->getExpr());
  ASSERT_NE(disableRef, nullptr);
  EXPECT_EQ(disableRef->getName(), "block");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
