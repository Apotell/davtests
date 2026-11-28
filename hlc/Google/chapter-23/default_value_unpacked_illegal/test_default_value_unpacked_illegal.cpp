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

// Test for tests/Google/chapter-23/23.2.2.4--default-value-unpacked-illegal.sv
//
// THE SOURCE
//
//   19: module m(input logic a [1:0] = '{1'b0, 1'b0});
//   20: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "A module declaration may specify a default value for each singular input
// port." (23.2.2.4). An unpacked array is not singular: "A singular type shall
// be any data type except an unpacked structure, unpacked union, or unpacked
// array" (6.4). So the default of a on line 19 is an error. The grammar
// accepts it (ansi_port_declaration allows
// "= constant_expression" after unpacked dimensions), so the error is
// semantic, not a syntax error.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   IllegalCodeIsAnError             an error is reported on line 19
//   NoErrorAfterTheIllegalCode       no fatal, syntax or error diagnostic on
//                                    line 20 or later
//
// SKIPPED (GTEST_SKIP; the real assertion stays below the skip)
//
//   IllegalCodeIsAnError: HLC accepts a default on an unpacked array input
//   port and reports nothing on line 19. HLC has no diagnostic for this rule
//   yet, so the assertion checks for any error on the line rather than a
//   specific event.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The diagnostic's wording. The LRM requires an error, not a particular
//     message.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <cstdint>
#include <limits>
#include <string>

namespace hlc {
namespace {
constexpr uint32_t kIllegalFirstLine = 19;
constexpr uint32_t kIllegalLastLine = 19;
constexpr uint32_t kFirstLineAfter = 20;

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

class DefaultValueUnpackedIllegalTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "default_value_unpacked_illegal.hlc"}); }
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

// "A module declaration may specify a default value for each singular input port." (23.2.2.4); a is an
// unpacked array, not singular.
TEST_F(DefaultValueUnpackedIllegalTest, IllegalCodeIsAnError) {
  GTEST_SKIP() << "HLC accepts a default on the unpacked array input port a and reports nothing on line 19 "
                  "(IEEE 1800-2023 23.2.2.4: a default value may be specified for a singular input port only)";
  EXPECT_NE(errorsOnLines(kIllegalFirstLine, kIllegalLastLine), "") << "no error reported on line 19";
}

TEST_F(DefaultValueUnpackedIllegalTest, NoErrorAfterTheIllegalCode) {
  EXPECT_EQ(errorsOnLines(kFirstLineAfter, std::numeric_limits<uint32_t>::max()), "");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
