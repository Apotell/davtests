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

// Test for tests/Google/chapter-23/23.3.2--wildcard-twice-illegal.sv
//
// THE SOURCE
//
//   19: module m (input a, b);
//   20: endmodule
//   21:
//   22: module top;
//   23:   wire a, b;
//   24:   m u1 (.*, .*);
//   25: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// Syntax 23-6, footnote 34: "The .* token pair shall appear at most once in a
// list of port connections." 23.3.2.4 repeats it: "The .* token pair may
// appear at most once in the port list." Line 24 uses .* twice.
//
// The test elaborates the compiled design with hldb::Elaborator, because
// some instantiation rules can only be checked during elaboration (23.3).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   LegalLinesAreAccepted            no fatal, syntax or error event on
//                                    lines 19-23
//   IllegalCodeIsAnError             at least one such event on
//                                    line 24
//   NoErrorAfterTheIllegalCode       none on line 25 or later
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The event's code or wording. The LRM requires an error, not a
//     particular message.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Database.h>
#include <hldb/Elaborator.h>

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

class WildcardTwiceIllegalTest : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "wildcard_twice_illegal.hlc"});
    elaborate();
  }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Instances exist only after elaboration (23.3), which Compile() does not
  // run, so the test runs HLDB's elaborator on the compiled database.
  static void elaborate() {
    if (m_session == nullptr) return;
    hldb::Database &database = m_session->getDatabase();
    hldb::Elaborator elaborator(database);
    elaborator.listen(database);
  }

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

TEST_F(WildcardTwiceIllegalTest, LegalLinesAreAccepted) {
  EXPECT_EQ(eventsOnLines(kLegalFirstLine, kLegalLastLine), "");
}

TEST_F(WildcardTwiceIllegalTest, IllegalCodeIsAnError) {
  EXPECT_NE(eventsOnLines(kIllegalFirstLine, kIllegalLastLine), "");
}

TEST_F(WildcardTwiceIllegalTest, NoErrorAfterTheIllegalCode) {
  EXPECT_EQ(eventsOnLines(kFirstLineAfter, std::numeric_limits<uint32_t>::max()), "");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
