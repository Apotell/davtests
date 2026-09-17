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

// Source under test: tests/Google/chapter-9/9.4.2--event_control_sim.sv
//
//   module top();
//      event e;
//      int i = 0;
//      initial begin
//         #5;
//         i++;
//         $display(":assert: (1 == %d)", i);
//         $display(":assert: (5 == %d)", $time);
//         #5;
//         i++;
//         $display(":assert: (2 == %d)", i);
//         $display(":assert: (10 == %d)", $time);
//         #2;
//         ->e;
//         $display(":assert: (2 == %d)", i);
//         $display(":assert: (12 == %d)", $time);
//         #3;
//         $display(":assert: (3 == %d)", i);
//         $display(":assert: (15 == %d)", $time);
//         $finish;
//      end
//      always @ (e) begin
//         i++;
//      end
//   endmodule
//
// IEEE 1800-2023 clauses tested:
//   - 9.4.2 "Event control": "always @(e)" waits for the named event "e"
//     (declared per 6.8's event data type) to be triggered.
//   - 9.4.2 / 9.4.3: "->e;" is an event_trigger statement that triggers
//     the named event "e", which is what wakes the "always @(e)" block.
//   - 9.4.1: "#5;"/"#2;"/"#3;" are delay_control statements between the
//     event-control-relevant statements.
//
// Checked:
//   - module top exists.
//   - "e" is declared with the `event` keyword (6.8), producing a
//     NamedEvent on the module (Scope::getNamedEvents()).
//   - "i" is declared with the variable-type keyword `int` (6.8's
//     integer_atom_type), so it must be classified as hldb::Variable
//     (module->getVariables()), with declared initializer 0 preserved on
//     Variable::getExpr().
//   - an Initial process exists for the top-level "initial begin ... end",
//     and its top-level Begin block's statement list is non-empty.
//   - somewhere in that flat statement list there is an EventStmt whose
//     named-event operand resolves back to the same NamedEvent "e" (the
//     "->e;" trigger).
//   - the statement list's DelayControl nodes carry the exact delay text
//     "5", "5", "2", "3" in that declared order (getVpiDelay()), matching
//     the four "#5;"/"#5;"/"#2;"/"#3;" delays in the source, and a
//     SysTaskCall named "$finish" is present.
//   - exactly one Always process exists, getAlwaysType() == vpiAlways,
//     and its EventControl condition resolves back to the same NamedEvent
//     "e" (either directly or via a RefObj to it).
//
// Not checked:
//   - The internal structure of the standalone "i++;" increment
//     statements -- IEEE 1800-2023 models "++"/"--" used as a statement
//     via inc_or_dec_expression, a different grammar production from the
//     assignment_statement that hldb::Assignment models, and no header in
//     this build documents a dedicated wrapper class for it; guessing its
//     shape was avoided per this project's "never guess method names/
//     shapes" rule.
//   - Every runtime ":assert:" value in the source (i's value and $time
//     at each step) -- HLC is an elaborator with no simulator (see
//     .claude/hlc_overview.md), so no post-elaboration/runtime value can
//     be observed on the UHDM graph. A real (currently-failing) assertion
//     for the final value of "i" is kept below rather than skipped
//     outright, per this project's not-checked-skip-test pattern.

#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/delay_control.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/event_stmt.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/named_event.h>
#include <hldb/ref_obj.h>
#include <hldb/scope.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>

#include <string>
#include <vector>

namespace hlc {
class EventControlSimTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "9.4.2--event_control_sim.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(EventControlSimTest, ModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
}

TEST_F(EventControlSimTest, NamedEventEExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  ASSERT_NE(top->getNamedEvents(), nullptr);
  const hldb::NamedEvent *const e = hldb::findByName<hldb::NamedEvent>("e", top->getNamedEvents());
  ASSERT_NE(e, nullptr) << "'event e;' must produce a NamedEvent (IEEE 1800-2023 6.8)";
}

TEST_F(EventControlSimTest, IIsVariableWithZeroInitializer) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const hldb::Variable *const i = hldb::findByName<hldb::Variable>("i", top->getVariables());
  ASSERT_NE(i, nullptr) << "'int i' is a variable-type keyword (IEEE 1800-2023 6.8 integer_atom_type)";

  const hldb::Constant *const initValue = i->getValue<hldb::Constant>();
  ASSERT_NE(initValue, nullptr);
  EXPECT_EQ(initValue->getDecompile(), std::string_view("0"));
}

TEST_F(EventControlSimTest, EventTriggerStatementTargetsE) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::NamedEvent *const e = hldb::findByName<hldb::NamedEvent>("e", top->getNamedEvents());
  ASSERT_NE(e, nullptr);

  const hldb::Initial *initial = nullptr;
  for (const hldb::Process *const process : *top->getProcesses()) {
    if (const hldb::Initial *const candidate = any_cast<hldb::Initial>(process)) {
      initial = candidate;
      break;
    }
  }
  ASSERT_NE(initial, nullptr);

  const hldb::Begin *const body = any_cast<hldb::Begin>(initial->getStmt());
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_FALSE(body->getStmts()->empty());

  const hldb::EventStmt *trigger = nullptr;
  for (const hldb::Any *const stmt : *body->getStmts()) {
    if (const hldb::EventStmt *const candidate = any_cast<hldb::EventStmt>(stmt)) {
      trigger = candidate;
      break;
    }
  }
  ASSERT_NE(trigger, nullptr) << "'->e;' must produce an EventStmt";

  const hldb::Any *const namedEvent = trigger->getNamedEvent();
  ASSERT_NE(namedEvent, nullptr);
  if (const hldb::NamedEvent *const direct = any_cast<hldb::NamedEvent>(namedEvent)) {
    EXPECT_EQ(direct, e);
  } else {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(namedEvent);
    ASSERT_NE(ref, nullptr) << "EventStmt::getNamedEvent() must be either a NamedEvent or a RefObj to one";
    EXPECT_EQ(ref->getName(), "e");
    EXPECT_EQ(ref->getActual<hldb::NamedEvent>(), e);
  }
}

TEST_F(EventControlSimTest, DelayControlsAndFinishArePresentInOrder) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  const hldb::Initial *initial = nullptr;
  for (const hldb::Process *const process : *top->getProcesses()) {
    if (const hldb::Initial *const candidate = any_cast<hldb::Initial>(process)) {
      initial = candidate;
      break;
    }
  }
  ASSERT_NE(initial, nullptr);
  const hldb::Begin *const body = any_cast<hldb::Begin>(initial->getStmt());
  ASSERT_NE(body, nullptr);

  std::vector<std::string> delays;
  bool sawFinish = false;
  for (const hldb::Any *const stmt : *body->getStmts()) {
    if (const hldb::DelayControl *const delay = any_cast<hldb::DelayControl>(stmt)) {
      delays.emplace_back(delay->getVpiDelay());
    } else if (const hldb::SysTaskCall *const call = any_cast<hldb::SysTaskCall>(stmt)) {
      if (call->getName() == "$finish") sawFinish = true;
    }
  }

  const std::vector<std::string> expected = {"5", "5", "2", "3"};
  EXPECT_EQ(delays, expected);
  EXPECT_TRUE(sawFinish) << "'$finish;' must produce a SysTaskCall named \"$finish\"";
}

TEST_F(EventControlSimTest, AlwaysBlockIsSensitiveToE) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::NamedEvent *const e = hldb::findByName<hldb::NamedEvent>("e", top->getNamedEvents());
  ASSERT_NE(e, nullptr);

  const hldb::Always *always = nullptr;
  for (const hldb::Process *const process : *top->getProcesses()) {
    if (const hldb::Always *const candidate = any_cast<hldb::Always>(process)) {
      ASSERT_EQ(always, nullptr) << "expected exactly one always block in top";
      always = candidate;
    }
  }
  ASSERT_NE(always, nullptr);
  EXPECT_EQ(always->getAlwaysType(), vpiAlways);

  const hldb::EventControl *const eventControl = any_cast<hldb::EventControl>(always->getStmt());
  ASSERT_NE(eventControl, nullptr) << "'always @(e)' must produce an EventControl";

  const hldb::Any *const condition = eventControl->getCondition();
  ASSERT_NE(condition, nullptr);
  if (const hldb::NamedEvent *const direct = any_cast<hldb::NamedEvent>(condition)) {
    EXPECT_EQ(direct, e);
  } else {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(condition);
    ASSERT_NE(ref, nullptr) << "EventControl::getCondition() must be either a NamedEvent or a RefObj to one";
    EXPECT_EQ(ref->getName(), "e");
    EXPECT_EQ(ref->getActual<hldb::NamedEvent>(), e);
  }
}

TEST_F(EventControlSimTest, IFinalValueShouldBeThreeButHlcCannotSimulate) {
  GTEST_SKIP() << "HLC has no simulator (see .claude/hlc_overview.md); the source's runtime "
                  ":assert: checks (i reaching 1, 2, then 3, and $time reaching 5, 10, 12, 15) "
                  "cannot be observed on the compiled UHDM graph.";

  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const i = hldb::findByName<hldb::Variable>("i", top->getVariables());
  ASSERT_NE(i, nullptr);
  const hldb::Constant *const value = i->getValue<hldb::Constant>();
  ASSERT_NE(value, nullptr) << "post-simulation value of 'i' is never populated at compile time";
  EXPECT_EQ(value->getDecompile(), "3") << "final simulated value of 'i' per the source's own :assert: checks";
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
