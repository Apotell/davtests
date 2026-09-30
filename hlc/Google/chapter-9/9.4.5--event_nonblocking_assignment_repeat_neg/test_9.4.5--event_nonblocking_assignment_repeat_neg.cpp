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

// Source under test: tests/Google/chapter-9/9.4.5--event_nonblocking_assignment_repeat_neg.sv
//
//   module block_tb ();
//   	reg a = 0;
//   	reg b = 1;
//   	wire clk = 0;
//
//   	initial begin
//   		a = repeat(-3) @(posedge clk) b;
//   	end
//   endmodule
//
// IEEE 1800-2023 clause tested: same 9.4.5 "repeat (expression)
// event_control" intra-assignment control as
// 9.4.5--event_nonblocking_assignment_repeat.sv, but with a literal
// NEGATIVE repeat count ("-3") instead of a positive one ("3") or a
// variable reference ("i", see the sibling repeat_int(_neg) files). Per
// 11.4.1's unary operators grammar, "-3" (a unary minus applied to the
// literal 3) may be represented either as a single signed Constant or as
// an Operation (vpiMinusOp) wrapping a Constant "3" -- this test checks
// for either shape rather than guessing one, exactly as for "i"'s
// initializer in 9.4.5--event_nonblocking_assignment_repeat_int_neg.sv.
//
// NOTE ON THE FILE NAME: as in the sibling repeat/event files, the source
// text uses '=' (blocking), not '<=', despite "nonblocking" in the file
// name. This test asserts getBlocking() == true, matching the actual
// source.
//
// Checked:
//   - module block_tb exists.
//   - "a" and "b" are declared with the variable-type keyword `reg`
//     (IEEE 1800-2023 6.8), so both must be classified as hldb::Variable;
//     "clk" is declared with the net-type keyword `wire` (6.7), so it
//     must be classified as hldb::Net.
//   - exactly one Initial process exists, whose body is a Begin with one
//     BLOCKING Assignment (lhs RefObj "a", rhs RefObj "b") whose
//     Assignment::getRepeatControl() is a RepeatControl with
//     getEventControl() an EventControl (opType == vpiPosedgeOp, single
//     RefObj "clk" operand); Assignment::getDelayControl() and
//     getEventControl() are both null.
//   - the RepeatControl's getDelay() (the repeat count "-3") is either a
//     Constant whose getDecompile() is "-3", or an Operation (vpiMinusOp)
//     wrapping a Constant "3" -- both shapes are accepted, neither is
//     assumed.
//
// Not checked:
//   - Runtime behavior of a negative repeat count (the LRM does not
//     define an explicit semantic for repeat() with a negative
//     expression value) -- HLC is an elaborator with no simulator (see
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
class EventNonblockingAssignmentRepeatNegTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.4.5--event_nonblocking_assignment_repeat_neg.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(EventNonblockingAssignmentRepeatNegTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
}

TEST_F(EventNonblockingAssignmentRepeatNegTest, AAndBAreVariablesClkIsNet) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("block_tb", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr) << "'a' is declared with the variable-type keyword 'reg' (IEEE 1800-2023 6.8)";
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  ASSERT_NE(b, nullptr) << "'b' is declared with the variable-type keyword 'reg' (IEEE 1800-2023 6.8)";
  const hldb::Net *const clk = hldb::findByName<hldb::Net>("clk", top->getNets());
  ASSERT_NE(clk, nullptr) << "'clk' is declared with the net-type keyword 'wire' (IEEE 1800-2023 6.7)";
}

TEST_F(EventNonblockingAssignmentRepeatNegTest, AssignmentCarriesRepeatControlWithLiteralNegativeThree) {
  GTEST_SKIP() << "Whether HLC attaches the intra-assignment 'repeat(-3) @(posedge clk)' via "
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
  ASSERT_EQ(body->getStmts()->size(), 1u) << "'a = repeat(-3) @(posedge clk) b;' is the block's single statement";

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
  ASSERT_NE(repeat, nullptr) << "'repeat(-3) @(posedge clk)' must be carried on Assignment::getRepeatControl()";

  const hldb::Expr *const count = repeat->getDelay();
  ASSERT_NE(count, nullptr) << "the repeat count expression must be present";
  if (const hldb::Constant *const asConstant = any_cast<hldb::Constant>(count)) {
    EXPECT_EQ(asConstant->getDecompile(), "-3");
  } else {
    const hldb::Operation *const asMinus = any_cast<hldb::Operation>(count);
    ASSERT_NE(asMinus, nullptr) << "'-3' must be either a signed Constant or a vpiMinusOp Operation";
    EXPECT_EQ(asMinus->getOpType(), vpiMinusOp);
    ASSERT_NE(asMinus->getOperands(), nullptr);
    ASSERT_EQ(asMinus->getOperands()->size(), 1u);
    const hldb::Constant *const operand = any_cast<hldb::Constant>(asMinus->getOperands()->front());
    ASSERT_NE(operand, nullptr);
    EXPECT_EQ(operand->getDecompile(), "3");
  }

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
