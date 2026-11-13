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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.1--non-ansi-range-mismatch.sv
//
// THE SOURCE
//
//   19: module m(a);
//   20:   input [7:0] a;
//   21:   wire [3:0] a;
//   22: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "If a port declaration does not include a net or variable type, then the
// port can be again declared in a net or variable declaration. If the net or
// variable is declared as a vector, the range specification between the two
// declarations of a port shall be identical." (23.2.2.1). a is declared [7:0]
// as a port and [3:0] as a net, so the pair of declarations on lines 20-21 is
// an error. The LRM does not say which of the two lines carries it.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   LegalLinesAreAccepted            no fatal, syntax or error diagnostic on
//                                    line 19
//   IllegalCodeIsAnError             an error for a on lines 20-21
//                                    (skipped: HLC has no event for it)
//   NoErrorAfterTheIllegalCode       none on line 22 or later
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The diagnostic's code or wording. The LRM requires an error, not a
//     particular message.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <cstdint>
#include <limits>
#include <sstream>
#include <string>

namespace hlc {
namespace {
constexpr uint32_t kLegalFirstLine = 19;
constexpr uint32_t kLegalLastLine = 19;
constexpr uint32_t kFirstLineAfter = 22;

uint32_t lineOf(const Event &event) { return event.getArguments().empty() ? 0 : event.getArguments().front().m_line; }

bool isErrorClass(const Event &event) {
  const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), event.getType());
  if (errorInfo == nullptr) return false;
  const EventSeverity severity = errorInfo->m_severity;
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}
}  // namespace

class NonAnsiRangeMismatchTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "non_ansi_range_mismatch.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static std::string eventsOnLines(uint32_t firstLine, uint32_t lastLine) {
    hlc::EventListener *const listener = m_session->getEventListener();
    std::ostringstream oss;
    for (const Event &event : listener->getEvents()) {
      const uint32_t line = lineOf(event);
      if (isErrorClass(event) && (line >= firstLine) && (line <= lastLine)) {
        std::vector<std::string> msgs = hldb::renderEvent(event);
        std::copy(msgs.cbegin(), msgs.cend(), std::ostream_iterator<std::string>(oss));
      }
    }
    return oss.str();
  }
};

TEST_F(NonAnsiRangeMismatchTest, LegalLinesAreAccepted) {
  EXPECT_EQ(eventsOnLines(kLegalFirstLine, kLegalLastLine), "");
}

TEST_F(NonAnsiRangeMismatchTest, IllegalCodeIsAnError) {
  // Expected: an error for a on lines 20-21, saying the ranges [7:0] and [3:0] of its two
  // declarations differ.
  GTEST_SKIP() << "HLC has no event for a port whose net/variable declaration has a different range "
                  "(IEEE 1800-2023 23.2.2.1); not reported yet";
}

TEST_F(NonAnsiRangeMismatchTest, NoErrorAfterTheIllegalCode) {
  EXPECT_EQ(eventsOnLines(kFirstLineAfter, std::numeric_limits<uint32_t>::max()), "");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
