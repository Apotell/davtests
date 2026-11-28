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

// Test for tests/Google/chapter-23/23.3.2--mixed-connections-illegal.sv
//
// THE SOURCE
//
//   19: module m (input a, b);
//   20: endmodule
//   21:
//   22: module top;
//   23:   wire x, y;
//   24:   m u1 (x, .b(y));
//   25: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "Positional and named module port connections shall not be mixed in the
// same module instantiation; connections to the ports of a particular
// module instance shall be all by order or all by name." (23.3.2). Syntax
// 23-6: list_of_port_connections is either all ordered_port_connection or
// all named_port_connection. Line 24 connects x by order and b by name.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   LegalLinesAreAccepted            no fatal, syntax or error event on
//                                    lines 19-23
//   IllegalCodeIsAnError             PaSyntaxError at 24:11: Syntax 23-6
//                                    has no list that mixes ordered and
//                                    named port connections
//   NoErrorAfterTheIllegalCode       none on line 25 or later
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The event's wording. The LRM requires an error, not a particular
//     message.

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
constexpr uint32_t kLegalLastLine = 23;
constexpr uint32_t kIllegalFirstLine = 24;
constexpr uint32_t kIllegalLastLine = 24;
constexpr uint32_t kFirstLineAfter = 25;

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

class MixedConnectionsIllegalTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "mixed_connections_illegal.hlc"}); }
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

TEST_F(MixedConnectionsIllegalTest, LegalLinesAreAccepted) {
  EXPECT_EQ(eventsOnLines(kLegalFirstLine, kLegalLastLine), "");
}

// Syntax 23-6: a list_of_port_connections is all ordered or all named, so "(x, .b(y))" is a syntax
// error, reported at the "." of the named connection (24:11; a syntax error's column counts from 0).
TEST_F(MixedConnectionsIllegalTest, IllegalCodeIsAnError) {
  EXPECT_NE(findEvent(EventType::PaSyntaxError, kIllegalFirstLine, 11), nullptr)
      << "errors reported on line 24:" << eventsOnLines(kIllegalFirstLine, kIllegalLastLine);
}

TEST_F(MixedConnectionsIllegalTest, NoErrorAfterTheIllegalCode) {
  EXPECT_EQ(eventsOnLines(kFirstLineAfter, std::numeric_limits<uint32_t>::max()), "");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
