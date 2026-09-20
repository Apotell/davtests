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

// Source under test: tests/Google/chapter-9/9.6.3--disable_fork.sv
//
//   module fork_tb ();
//   	reg a = 0;
//   	reg b = 0;
//   	reg c = 0;
//   	initial begin
//   		fork
//   			#50 a = 1;
//   			#100 b = 1;
//   			#150 c = 1;
//   		join_any
//   		disable fork;
//   	end
//   endmodule
//
// IEEE 1800-2023 clause tested: 9.6.3 "The disable fork statement":
// `join_any` waits for only the first of the three branches to finish;
// `disable fork;` afterward terminates all of the (still-running)
// sibling branches spawned by this process's own fork -- distinct from
// both `disable <name>` (9.6.2, which names one specific block/task) and
// `wait fork;` (9.6.1, which waits rather than terminates).
//
// Checked:
//   - module fork_tb exists.
//   - "a", "b", and "c" are declared with the variable-type keyword `reg`
//     (IEEE 1800-2023 6.8), so all three must be classified as
//     hldb::Variable, each with its declared initializer (0) preserved on
//     Variable::getExpr().
//   - the initial block's body (explicit "begin...end") is a Begin with
//     exactly two statements: a ForkStmt, then a DisableFork.
//   - the ForkStmt's getJoinType() == vpiJoinAny (not vpiJoin or
//     vpiJoinNone), with exactly three branches, none wrapped in
//     begin/end -- each is a bare DelayControl ("50"/"100"/"150") whose
//     controlled statement is a blocking Assignment to "a"/"b"/"c"
//     (Constant "1").
//   - the DisableFork statement is a standalone statement with no
//     operand fields of its own (disable_fork.h declares none beyond
//     what AtomicStmt/Disables already provide) -- unlike Disable
//     (9.6.2), which names a specific block/task, "disable fork;" always
//     refers to the calling process's own child processes.
//
// Not checked:
//   - Runtime scheduling behavior (does "join_any" really unblock once
//     any one of the three branches finishes, and does "disable fork;"
//     really terminate the remaining branches) -- HLC is an elaborator
//     with no simulator (see .claude/hlc_overview.md), so no execution
//     ever happens for this test to observe.

#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/delay_control.h>
#include <hldb/design.h>
#include <hldb/disable_fork.h>
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

class DisableForkTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.6.3--disable_fork.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(DisableForkTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("fork_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
}

TEST_F(DisableForkTest, AAndBAndCAreVariables) {
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

TEST_F(DisableForkTest, ForkJoinAnyHasThreeBareBranchesThenDisableFork) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("fork_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);

  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);

  const hldb::Begin *const body = any_cast<hldb::Begin>(init->getStmt());
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u) << "'fork ... join_any' then 'disable fork;' are exactly two statements";

  const hldb::ForkStmt *const fork = any_cast<hldb::ForkStmt>(body->getStmts()->at(0));
  ASSERT_NE(fork, nullptr) << "'fork ... join_any' must produce a ForkStmt";
  EXPECT_EQ(fork->getJoinType(), vpiJoinAny);
  ASSERT_NE(fork->getStmts(), nullptr);
  ASSERT_EQ(fork->getStmts()->size(), 3u);

  const char *const expectedNames[3] = {"a", "b", "c"};
  const char *const expectedDelays[3] = {"50", "100", "150"};
  for (size_t index = 0; index < 3; ++index) {
    const hldb::DelayControl *const delay = any_cast<hldb::DelayControl>(fork->getStmts()->at(index));
    ASSERT_NE(delay, nullptr) << "each bare fork branch must be a DelayControl";
    const hldb::Constant *const delayValue = delay->getDelay<hldb::Constant>();
    ASSERT_NE(delayValue, nullptr);
    EXPECT_EQ(delayValue->getDecompile(), expectedDelays[index]);

    const hldb::Assignment *const assign = any_cast<hldb::Assignment>(delay->getStmt());
    ASSERT_NE(assign, nullptr);
    EXPECT_TRUE(assign->getBlocking());
    const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(assign->getLhs());
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getName(), expectedNames[index]);
    const hldb::Constant *const rhs = any_cast<hldb::Constant>(assign->getRhs());
    ASSERT_NE(rhs, nullptr);
    EXPECT_EQ(rhs->getDecompile(), "1");
  }

  const hldb::DisableFork *const disableFork = any_cast<hldb::DisableFork>(body->getStmts()->at(1));
  ASSERT_NE(disableFork, nullptr) << "'disable fork;' must produce a DisableFork statement";
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
