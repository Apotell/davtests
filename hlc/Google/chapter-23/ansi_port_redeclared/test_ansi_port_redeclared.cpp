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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.2--ansi-port-redeclared.sv
//
// THE SOURCE
//
//   19: module m(input [7:0] a);
//   20:   wire [7:0] a;
//   21: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "Each port declaration provides the complete information about the port.
// ... The port identifier shall not be redeclared, in part or in full,
// inside the module body." (23.2.2.2). The LRM's ANSI version of Example 1
// repeats it: "It is illegal to redeclare any ports of the module in the
// body of the module." a is an ANSI port, so "wire [7:0] a;" on line 20 is
// an error, even though its range matches.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   AnsiHeaderIsAccepted          no fatal, syntax or error diagnostic on
//                                 line 19
//   BodyRedeclarationIsAnError    at least one such diagnostic on line 20
//   NoErrorAfterTheRedeclaration  none on line 21 or later
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
#include <string>

namespace hlc {
namespace {
constexpr uint32_t kHeaderLine = 19;
constexpr uint32_t kRedeclarationLine = 20;

uint32_t lineOf(const Event &error) { return error.getArguments().empty() ? 0 : error.getArguments().front().m_line; }

bool isErrorClass(const Event &error) {
  const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
  if (errorInfo == nullptr) return false;
  const EventSeverity severity = errorInfo->m_severity;
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}

std::string describeError(const Event &error) {
  const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
  std::string description = " [" + std::to_string(static_cast<int32_t>(error.getType()));
  description += " line " + std::to_string(lineOf(error));
  if (errorInfo != nullptr) description += ": " + std::string(errorInfo->m_lines[0].m_text);
  return description + "]";
}
}  // namespace

class AnsiPortRedeclaredTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ansi_port_redeclared.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static std::string errorsOnLines(uint32_t firstLine, uint32_t lastLine) {
    if ((m_session == nullptr) || (m_session->getEventListener() == nullptr)) return "no error container";
    std::string description;
    for (const Event &error : m_session->getEventListener()->getEvents()) {
      const uint32_t line = lineOf(error);
      if (isErrorClass(error) && (line >= firstLine) && (line <= lastLine)) description += describeError(error);
    }
    return description;
  }
};

TEST_F(AnsiPortRedeclaredTest, AnsiHeaderIsAccepted) { EXPECT_EQ(errorsOnLines(kHeaderLine, kHeaderLine), ""); }

TEST_F(AnsiPortRedeclaredTest, BodyRedeclarationIsAnError) {
  ASSERT_NE(m_session, nullptr);
  ASSERT_NE(m_session->getEventListener(), nullptr);
  EXPECT_NE(errorsOnLines(kRedeclarationLine, kRedeclarationLine), "");
}

TEST_F(AnsiPortRedeclaredTest, NoErrorAfterTheRedeclaration) {
  EXPECT_EQ(errorsOnLines(kRedeclarationLine + 1, std::numeric_limits<uint32_t>::max()), "");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
