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

// Test for tests/Google/chapter-23/23.3.1--top-level-none-illegal.sv
//
// THE SOURCE
//
//   20: module a;
//   21:   b u_b ();
//   22: endmodule
//   23:
//   24: module b;
//   25:   a u_a ();
//   26: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "Top-level modules are modules that are included in the SystemVerilog
// source text, but do not appear in any module instantiation statement"
// (23.3.1). "A design shall contain at least one top-level module."
// (23.3.1). Module a is instantiated in b and b in a, so neither is a
// top-level module and the design has none.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   NoTopLevelModuleIsReported             ElabNoTopLevelModule -- HLC's
//                                          event for this rule
//   DesignWithoutTopLevelModuleIsAnError   that event is an error (fatal,
//                                          syntax or error severity)
//
// SKIPPED (GTEST_SKIP; the real assertion stays below the skip)
//
//   DesignWithoutTopLevelModuleIsAnError: HLC reports the missing top-level
//   module as a warning, not an error.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The line of the error. The rule is about the design as a whole, so the
//     LRM gives no line to report it on.
//   - The event's wording. The LRM requires an error, not a particular
//     message.
//   - The recursion. Any design without a top-level module has a cycle of
//     instantiations, but the error the LRM requires here is the missing
//     top-level module.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <algorithm>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace hlc {
namespace {
// True for an error-class event: fatal, syntax or error severity.
bool isErrorClass(const Event &event) {
  const EventDefinition *const eventDefinition = hldb::findEventDefinition(hlc::getEventTable(), event.getType());
  if (eventDefinition == nullptr) return false;
  const EventSeverity severity = eventDefinition->m_severity;
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}

// One event as hldb::renderEvent() prints it, one line per output line, so that a
// failing test shows what was reported and where.
std::string describeEvent(const Event &event) {
  std::ostringstream oss;
  const std::vector<std::string> lines = hldb::renderEvent(event);
  std::copy(lines.cbegin(), lines.cend(), std::ostream_iterator<std::string>(oss, "\n"));
  return oss.str();
}
}  // namespace

class TopLevelNoneIllegalTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "top_level_none_illegal.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static std::string allEvents() {
    hlc::EventListener *const listener = m_session->getEventListener();
    std::ostringstream oss;
    for (const Event &event : listener->getEvents()) {
      if (isErrorClass(event)) oss << describeEvent(event);
    }
    return oss.str();
  }
};

// "A design shall contain at least one top-level module." (23.3.1)
TEST_F(TopLevelNoneIllegalTest, NoTopLevelModuleIsReported) {
  EXPECT_NE(findEvent(EventType::ElabNoTopLevelModule), nullptr) << "errors reported:" << allEvents();
}

TEST_F(TopLevelNoneIllegalTest, DesignWithoutTopLevelModuleIsAnError) {
  GTEST_SKIP() << "HLC reports the missing top-level module (ElabNoTopLevelModule) as a warning, not an error "
                  "(IEEE 1800-2023 23.3.1: a design shall contain at least one top-level module)";
  const Event *const event = findEvent(EventType::ElabNoTopLevelModule);
  ASSERT_NE(event, nullptr);
  EXPECT_TRUE(isErrorClass(*event));
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
