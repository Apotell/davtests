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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.2--ansi-mixed-styles.sv
//
// THE SOURCE
//
//   20: module m(a, input b);
//   21:   input a;
//   22: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "Each module shall be declared either entirely with the list_of_ports
// syntax as described in 23.2.2.1 or entirely with the
// list_of_port_declarations syntax as described in this subclause."
// (23.2.2.2). The header lists a in the non-ANSI style and "input b" in the
// ANSI style, so module m (lines 20-22) is in error. The LRM does not say
// which line carries the error.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   IllegalCodeIsAnError             PaSyntaxError on lines 20-22. The
//                                    grammar rejects it: after an ANSI
//                                    header (a bare first port such as a
//                                    is legal ANSI, 23.2.2.3) the body
//                                    takes only non_port_module_items, so
//                                    "input a;" cannot be parsed (A.1.2)
//   NoErrorAfterTheIllegalCode       none on line 23 or later
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The diagnostic's wording, and on which of lines 20-22 the parser
//     stops. The LRM requires an error, not a particular message.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <cstdint>
#include <limits>
#include <sstream>
#include <string>

namespace hlc {
namespace {
constexpr uint32_t kIllegalFirstLine = 20;
constexpr uint32_t kIllegalLastLine = 22;
constexpr uint32_t kFirstLineAfter = 23;

uint32_t lineOf(const Event &event) { return event.getArguments().empty() ? 0 : event.getArguments().front().m_line; }

bool isErrorClass(const Event &event) {
  const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), event.getType());
  if (errorInfo == nullptr) return false;
  const EventSeverity severity = errorInfo->m_severity;
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}
}  // namespace

class AnsiMixedStylesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ansi_mixed_styles.hlc"}); }
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

TEST_F(AnsiMixedStylesTest, IllegalCodeIsAnError) {
  ASSERT_NE(m_session, nullptr);
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const Event *const event = findEvent(EventType::PaSyntaxError);
  ASSERT_NE(event, nullptr);
  EXPECT_GE(lineOf(*event), kIllegalFirstLine);
  EXPECT_LE(lineOf(*event), kIllegalLastLine);
}

TEST_F(AnsiMixedStylesTest, NoErrorAfterTheIllegalCode) {
  EXPECT_EQ(eventsOnLines(kFirstLineAfter, std::numeric_limits<uint32_t>::max()), "");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
