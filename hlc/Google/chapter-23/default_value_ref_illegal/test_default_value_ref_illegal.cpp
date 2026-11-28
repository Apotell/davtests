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

// Test for tests/Google/chapter-23/23.2.2.4--default-value-ref-illegal.sv
//
// THE SOURCE
//
//   19: module m(ref logic [7:0] r = 8'h00);
//   20: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "Defaults can be specified only for input ports and only in ANSI style
// declarations." (23.2.2.4), and Syntax 23-4 footnote 2: "It shall be illegal
// ... to specify a default value for a port that is not an input port." r is
// a ref port with the default 8'h00, so line 19 is an error.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   IllegalCodeIsAnError             CompIllegalPortInitializer for port r at
//                                    19:30 (the default) -- HLC's event for
//                                    this rule
//   NoErrorAfterTheIllegalCode       no fatal, syntax or error diagnostic on
//                                    line 20 or later
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

class DefaultValueRefIllegalTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "default_value_ref_illegal.hlc"}); }
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

// "It shall be illegal ... to specify a default value for a port that is not an input port." (Syntax 23-4
// footnote 2). Reported at the default (19:30) naming port r.
TEST_F(DefaultValueRefIllegalTest, IllegalCodeIsAnError) {
  EXPECT_NE(findEvent(EventType::CompIllegalPortInitializer, "r", kIllegalFirstLine, 30), nullptr)
      << "errors reported on line 19:" << errorsOnLines(kIllegalFirstLine, kIllegalLastLine);
}

TEST_F(DefaultValueRefIllegalTest, NoErrorAfterTheIllegalCode) {
  EXPECT_EQ(errorsOnLines(kFirstLineAfter, std::numeric_limits<uint32_t>::max()), "");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
