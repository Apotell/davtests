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

// Test for tests/Google/chapter-23/23.2.4--timeunits-mismatch-illegal.sv
//
// THE SOURCE
//
//   20: module m;
//   21:   timeunit 1ns;
//   22:   logic x;
//   23:   timeunit 1ps;
//   24: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// Syntax 23-5, footnote 3: "A timeunits_declaration shall be legal as a
// non_port_module_item ... only if it repeats and matches a previous
// timeunits_declaration within the same time scope." Line 21 is the module's
// own timeunits_declaration (Syntax 23-1). Line 23, "timeunit 1ps;", does not
// match "timeunit 1ns;", so it is an error; lines 20-22 are legal.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   LegalLinesAreAccepted            no fatal, syntax or error event on
//                                    lines 20-22
//   IllegalCodeIsAnError             CompIllegalTimescale for m on line 23
//                                    -- HLC's event for this rule
//   NoErrorAfterTheIllegalCode       none on line 24 or later
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
constexpr uint32_t kLegalFirstLine = 20;
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

class TimeunitsMismatchIllegalTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "timeunits_mismatch_illegal.hlc"}); }
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

TEST_F(TimeunitsMismatchIllegalTest, LegalLinesAreAccepted) {
  EXPECT_EQ(eventsOnLines(kLegalFirstLine, kLegalLastLine), "");
}

// "timeunit 1ps;" does not repeat and match "timeunit 1ns;" (Syntax 23-5 footnote 3).
TEST_F(TimeunitsMismatchIllegalTest, IllegalCodeIsAnError) {
  EXPECT_NE(findEvent(EventType::CompIllegalTimescale, "m", kIllegalFirstLine), nullptr)
      << "errors reported on line 23:" << eventsOnLines(kIllegalFirstLine, kIllegalLastLine);
}

TEST_F(TimeunitsMismatchIllegalTest, NoErrorAfterTheIllegalCode) {
  EXPECT_EQ(eventsOnLines(kFirstLineAfter, std::numeric_limits<uint32_t>::max()), "");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
