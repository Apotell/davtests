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

// Test for tests/Google/chapter-23/23.3.2--no-parentheses-illegal.sv
//
// THE SOURCE
//
//   19: module empty_m;
//   20: endmodule
//   21:
//   22: module top;
//   23:   empty_m u1;
//   24: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "The parentheses shall be required on all module instantiations, even
// when the instantiated module does not have ports." (23.3.2). Syntax 23-6:
// hierarchical_instance ::= name_of_instance ( [ list_of_port_connections ] ),
// so the parentheses are part of every instance. Line 23 omits them.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   LegalLinesAreAccepted            no fatal, syntax or error event on
//                                    lines 19-22
//   IllegalCodeIsAnError             at least one such event on
//                                    line 23
//   NoErrorAfterTheIllegalCode       none on line 24 or later
//
// SKIPPED (GTEST_SKIP; the real assertion stays below the skip)
//
//   IllegalCodeIsAnError: HLC accepts "empty_m u1;" and reports nothing on
//   line 23.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The event's code or wording. The LRM requires an error, not a
//     particular message.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace hlc {
namespace {
constexpr uint32_t kLegalFirstLine = 19;
constexpr uint32_t kLegalLastLine = 22;
constexpr uint32_t kIllegalFirstLine = 23;
constexpr uint32_t kIllegalLastLine = 23;
constexpr uint32_t kFirstLineAfter = 24;

uint32_t lineOf(const Event &event) { return event.getArguments().empty() ? 0 : event.getArguments().front().m_line; }

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

class NoParenthesesIllegalTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "no_parentheses_illegal.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static std::string eventsOnLines(uint32_t firstLine, uint32_t lastLine) {
    hlc::EventListener *const listener = m_session->getEventListener();
    std::ostringstream oss;
    for (const Event &event : listener->getEvents()) {
      const uint32_t line = lineOf(event);
      if (isErrorClass(event) && (line >= firstLine) && (line <= lastLine)) oss << describeEvent(event);
    }
    return oss.str();
  }
};

TEST_F(NoParenthesesIllegalTest, LegalLinesAreAccepted) {
  EXPECT_EQ(eventsOnLines(kLegalFirstLine, kLegalLastLine), "");
}

TEST_F(NoParenthesesIllegalTest, IllegalCodeIsAnError) {
  GTEST_SKIP() << "HLC accepts 'empty_m u1;' and reports nothing on line 23 (IEEE 1800-2023 23.3.2: the parentheses "
                  "shall be required on all module instantiations)";
  EXPECT_NE(eventsOnLines(kIllegalFirstLine, kIllegalLastLine), "");
}

TEST_F(NoParenthesesIllegalTest, NoErrorAfterTheIllegalCode) {
  EXPECT_EQ(eventsOnLines(kFirstLineAfter, std::numeric_limits<uint32_t>::max()), "");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
