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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.1--non-ansi-duplicate-port-name.sv
//
// THE SOURCE
//
//   19: module m(.p(a), .p(b));
//   20:   input a, b;
//   21: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "Once a port has been defined, there shall not be another port definition
// with this same name." (23.2.2.1). Both explicit ports in the header are
// named p, so the header on line 19 is an error, although the port
// expressions a and b are distinct and declared on line 20.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   LegalLinesAreAccepted            no fatal, syntax or error diagnostic on
//                                    line 20
//   IllegalCodeIsAnError             CompDuplicatePortName for p on
//                                    line 19
//   NoErrorAfterTheIllegalCode       none on line 21 or later
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
constexpr uint32_t kLegalFirstLine = 20;
constexpr uint32_t kLegalLastLine = 20;
constexpr uint32_t kIllegalFirstLine = 19;
constexpr uint32_t kFirstLineAfter = 21;

// The line an event points at, or 0 when it carries no location.
uint32_t lineOf(const Event &event) { return event.getArguments().empty() ? 0 : event.getArguments().front().m_line; }

// True for an error-class event: one whose severity is fatal, syntax or error.
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

class NonAnsiDuplicatePortNameTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "non_ansi_duplicate_port_name.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Every error-class event on lines firstLine..lastLine, described as by describeEvent();
  // empty when there is none.
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

TEST_F(NonAnsiDuplicatePortNameTest, LegalLinesAreAccepted) {
  EXPECT_EQ(eventsOnLines(kLegalFirstLine, kLegalLastLine), "");
}

TEST_F(NonAnsiDuplicatePortNameTest, IllegalCodeIsAnError) {
  ASSERT_NE(m_session, nullptr);
  ASSERT_NE(m_session->getEventListener(), nullptr);
  EXPECT_NE(findEvent(EventType::CompDuplicatePortName, "p", kIllegalFirstLine), nullptr);
}

TEST_F(NonAnsiDuplicatePortNameTest, NoErrorAfterTheIllegalCode) {
  EXPECT_EQ(eventsOnLines(kFirstLineAfter, std::numeric_limits<uint32_t>::max()), "");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
