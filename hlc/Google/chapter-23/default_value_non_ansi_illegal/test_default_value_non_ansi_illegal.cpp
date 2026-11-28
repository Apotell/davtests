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

// Test for tests/Google/chapter-23/23.2.2.4--default-value-non-ansi-illegal.sv
//
// THE SOURCE
//
//   19: module m(d);
//   20:   input [7:0] d = 8'h00;
//   21: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "Defaults can be specified only for input ports and only in ANSI style
// declarations." (23.2.2.4). The non-ANSI input_declaration of Syntax 23-3 has
// no "= constant_expression", so the default on line 20 is an error, while
// the header on line 19 is legal.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   LegalLinesAreAccepted            no fatal, syntax or error diagnostic on
//                                    line 19
//   IllegalCodeIsAnError             PaSyntaxError at 20:16, the "=" the
//                                    non-ANSI grammar does not allow
//   NoErrorAfterTheIllegalCode       none on line 21 or later
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The diagnostic's wording. The LRM requires an error, not a particular
//     message.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <cstdint>
#include <limits>
#include <string>

namespace hlc {
namespace {
constexpr uint32_t kLegalFirstLine = 19;
constexpr uint32_t kLegalLastLine = 19;
constexpr uint32_t kIllegalFirstLine = 20;
constexpr uint32_t kIllegalLastLine = 20;
constexpr uint32_t kFirstLineAfter = 21;

uint32_t lineOf(const Event &event) { return event.getArguments().empty() ? 0 : event.getArguments().front().m_line; }

bool isErrorClass(const Event &event) {
  const EventDefinition *const eventDefinition = hldb::findEventDefinition(hlc::getEventTable(), event.getType());
  if (eventDefinition == nullptr) return false;
  const EventSeverity severity = eventDefinition->m_severity;
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}

std::string describeEvent(const Event &event) {
  const EventDefinition *const eventDefinition = hldb::findEventDefinition(hlc::getEventTable(), event.getType());
  std::string description = " [" + std::to_string(static_cast<int32_t>(event.getType()));
  description += " line " + std::to_string(lineOf(event));
  if (eventDefinition != nullptr) description += ": " + std::string(eventDefinition->m_lines[0].m_text);
  return description + "]";
}
}  // namespace

class DefaultValueNonAnsiIllegalTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "default_value_non_ansi_illegal.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static std::string errorsOnLines(uint32_t firstLine, uint32_t lastLine) {
    if ((m_session == nullptr) || (m_session->getEventListener() == nullptr)) return "no event listener";
    std::string description;
    for (const Event &event : m_session->getEventListener()->getEvents()) {
      const uint32_t line = lineOf(event);
      if (isErrorClass(event) && (line >= firstLine) && (line <= lastLine)) description += describeEvent(event);
    }
    return description;
  }
};

TEST_F(DefaultValueNonAnsiIllegalTest, LegalLinesAreAccepted) {
  EXPECT_EQ(errorsOnLines(kLegalFirstLine, kLegalLastLine), "");
}

// The non-ANSI input_declaration (Syntax 23-3) has no "= constant_expression", so the "=" at 20:16 is a
// syntax error.
TEST_F(DefaultValueNonAnsiIllegalTest, IllegalCodeIsAnError) {
  EXPECT_NE(findEvent(EventType::PaSyntaxError, kIllegalFirstLine, 16), nullptr)
      << "errors reported on line 20:" << errorsOnLines(kIllegalFirstLine, kIllegalLastLine);
}

TEST_F(DefaultValueNonAnsiIllegalTest, NoErrorAfterTheIllegalCode) {
  EXPECT_EQ(errorsOnLines(kFirstLineAfter, std::numeric_limits<uint32_t>::max()), "");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
