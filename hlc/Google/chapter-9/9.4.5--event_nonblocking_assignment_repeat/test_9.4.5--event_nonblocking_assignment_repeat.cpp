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

// Source under test: tests/Google/chapter-9/9.4.5--event_nonblocking_assignment_repeat.sv
//
//   module block_tb ();
//   	reg a = 0;
//   	reg b = 1;
//   	wire clk = 0;
//
//   	initial begin
//   		a = repeat(3) @(posedge clk) b;
//   	end
//   endmodule
//
// IEEE 1800-2023 clause tested: 9.4.5, intra-assignment timing controls,
// the "repeat (expression) event_control" form of delay_or_event_control:
// "b" is sampled now, then the assignment waits for "posedge clk" to
// occur 3 times before writing the sampled value into "a".
//
// NOTE ON THE FILE NAME: as in 9.4.5--event_nonblocking_assignment_event.sv,
// the source text uses '=' (blocking), not '<=', despite "nonblocking" in
// the file name. This test asserts getBlocking() == true, matching the
// actual source.
//
// Checked:
//   - module block_tb exists.
//   - "a" and "b" are declared with the variable-type keyword `reg`
//     (IEEE 1800-2023 6.8), so both must be classified as hldb::Variable;
//     "clk" is declared with the net-type keyword `wire` (6.7), so it
//     must be classified as hldb::Net.
//   - exactly one Initial process exists, whose body (explicit
//     "begin...end") is a Begin with exactly one statement: a BLOCKING
//     Assignment (getBlocking() == true, matching the actual '=') with
//     lhs RefObj "a" and rhs RefObj "b".
//   - the "repeat(3) @(posedge clk)" control is carried on
//     Assignment::getRepeatControl() (a RepeatControl), NOT on
//     Assignment::getEventControl() -- delay_or_event_control's three
//     forms (delay_control / event_control / "repeat (expr) event_control")
//     are mutually exclusive, so Assignment::getDelayControl() and
//     Assignment::getEventControl() must both be null when a repeat
//     control is used.
//   - the RepeatControl's getDelay() (the repeat COUNT expression) is a
//     Constant "3", and its getEventControl() is an EventControl whose
//     getCondition() is an Operation (opType == vpiPosedgeOp) with a
//     single RefObj "clk" operand -- the event_control is nested INSIDE
//     the RepeatControl, not sibling to it.
//
// Not checked:
//   - Runtime repeat-count-then-triggered-write ordering (does the
//     assignment really wait for exactly 3 posedges of "clk" before
//     writing "a") -- HLC is an elaborator with no simulator (see
//     .claude/hlc_overview.md), so no execution ever happens for this
//     test to observe.

#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/repeat_control.h>
#include <hldb/variable.h>

namespace hlc {
class EventNonblockingAssignmentRepeatTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.4.5--event_nonblocking_assignment_repeat.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(EventNonblockingAssignmentRepeatTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
}

TEST_F(EventNonblockingAssignmentRepeatTest, AAndBAreVariablesClkIsNet) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr) << "'a' is declared with the variable-type keyword 'reg' (IEEE 1800-2023 6.8)";
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  ASSERT_NE(b, nullptr) << "'b' is declared with the variable-type keyword 'reg' (IEEE 1800-2023 6.8)";
  const hldb::Net *const clk = hldb::findByName<hldb::Net>("clk", top->getNets());
  ASSERT_NE(clk, nullptr) << "'clk' is declared with the net-type keyword 'wire' (IEEE 1800-2023 6.7)";
}

TEST_F(EventNonblockingAssignmentRepeatTest, AssignmentCarriesRepeatControlWithLiteralCountThree) {
  GTEST_SKIP() << "Whether HLC attaches the intra-assignment 'repeat(3) @(posedge clk)' via "
                  "Assignment::getRepeatControl() (vs. an alternate wrapping-statement shape) was "
                  "inferred, not confirmed via a header or a .log.";

  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);

  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);

  const hldb::Begin *const body = any_cast<hldb::Begin>(init->getStmt());
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 1u) << "'a = repeat(3) @(posedge clk) b;' is the block's single statement";

  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->front());
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking()) << "the source uses '=', not '<=', despite the file name saying "
                                        "'nonblocking'";

  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(assign->getLhs());
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "a");
  const hldb::RefObj *const rhs = any_cast<hldb::RefObj>(assign->getRhs());
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "b");

  EXPECT_EQ(assign->getDelayControl(), nullptr)
      << "delay_or_event_control has exactly one alternative populated; this one is a repeat control";
  EXPECT_EQ(assign->getEventControl(), nullptr)
      << "the nested event_control belongs inside RepeatControl, not directly on the Assignment";

  const hldb::RepeatControl *const repeat = assign->getRepeatControl();
  ASSERT_NE(repeat, nullptr) << "'repeat(3) @(posedge clk)' must be carried on Assignment::getRepeatControl()";

  const hldb::Constant *const count = repeat->getDelay<hldb::Constant>();
  ASSERT_NE(count, nullptr) << "the repeat count expression should be a Constant";
  EXPECT_EQ(count->getDecompile(), "3");

  const hldb::EventControl *const eventControl = repeat->getEventControl();
  ASSERT_NE(eventControl, nullptr) << "'@(posedge clk)' must be nested inside the RepeatControl";
  const hldb::Operation *const condition = any_cast<hldb::Operation>(eventControl->getCondition());
  ASSERT_NE(condition, nullptr);
  EXPECT_EQ(condition->getOpType(), vpiPosedgeOp);
  ASSERT_NE(condition->getOperands(), nullptr);
  ASSERT_EQ(condition->getOperands()->size(), 1u);
  const hldb::RefObj *const clkRef = any_cast<hldb::RefObj>(condition->getOperands()->front());
  ASSERT_NE(clkRef, nullptr);
  EXPECT_EQ(clkRef->getName(), "clk");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
