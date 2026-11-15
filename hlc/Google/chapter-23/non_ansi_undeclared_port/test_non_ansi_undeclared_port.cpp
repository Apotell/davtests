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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.1--non-ansi-undeclared-port.sv
//
// THE SOURCE
//
//   21: module m(a, b);
//   22:   input a;
//   23: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "Each port_identifier in a port_expression in the list of ports for the
// module declaration shall also be declared in the body of the module as one
// of the following port declarations: input, output, inout (bidirectional),
// ref, or as an interface port (see Clause 25)." (23.2.2.1). b is in the list
// of ports but has no port declaration, so module m (lines 21-23) is in error.
// The LRM does not say which line carries the error.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   IllegalCodeIsAnError             CompUndeclaredPort for b on line 21,
//                                    where b is listed in the header
//   NoErrorAfterTheIllegalCode       none on line 24 or later
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The diagnostic's wording. The LRM requires an error, not a
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
constexpr uint32_t kIllegalFirstLine = 21;
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

class NonAnsiUndeclaredPortTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "non_ansi_undeclared_port.hlc"}); }
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

TEST_F(NonAnsiUndeclaredPortTest, IllegalCodeIsAnError) {
  ASSERT_NE(m_session, nullptr);
  ASSERT_NE(m_session->getEventListener(), nullptr);
  EXPECT_NE(findEvent(EventType::CompUndeclaredPort, "b", kIllegalFirstLine), nullptr);
}

TEST_F(NonAnsiUndeclaredPortTest, NoErrorAfterTheIllegalCode) {
  EXPECT_EQ(eventsOnLines(kFirstLineAfter, std::numeric_limits<uint32_t>::max()), "");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
