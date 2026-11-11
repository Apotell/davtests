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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.1--non-ansi-duplicate-port-direction.sv
//
// THE SOURCE
//
//   21: module m(aport);
//   22:   input aport;
//   23:   output aport;
//   24: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "Once a name is used in a port declaration, it shall not be declared again
// in another port declaration or in a data type declaration." (23.2.2.1). The
// LRM's own example marks these lines:
//
//   input aport;   // First declaration - okay
//   output aport;  // Error - multiple declaration, port declaration
//
// So line 23 is an error and line 22 is not; changing the direction does not
// make the second declaration legal.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   LegalLinesAreAccepted            no fatal, syntax or error diagnostic on
//                                    lines 21-22
//   IllegalCodeIsAnError             at least one such diagnostic on
//                                    line 23
//   NoErrorAfterTheIllegalCode       none on line 24 or later
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
constexpr uint32_t kLegalFirstLine = 21;
constexpr uint32_t kLegalLastLine = 22;
constexpr uint32_t kIllegalFirstLine = 23;
constexpr uint32_t kIllegalLastLine = 23;
constexpr uint32_t kFirstLineAfter = 24;

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

class NonAnsiDuplicatePortDirectionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "non_ansi_duplicate_port_direction.hlc"}); }
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

TEST_F(NonAnsiDuplicatePortDirectionTest, LegalLinesAreAccepted) {
  EXPECT_EQ(errorsOnLines(kLegalFirstLine, kLegalLastLine), "");
}

TEST_F(NonAnsiDuplicatePortDirectionTest, IllegalCodeIsAnError) {
  ASSERT_NE(m_session, nullptr);
  ASSERT_NE(m_session->getEventListener(), nullptr);
  EXPECT_NE(errorsOnLines(kIllegalFirstLine, kIllegalLastLine), "");
}

TEST_F(NonAnsiDuplicatePortDirectionTest, NoErrorAfterTheIllegalCode) {
  EXPECT_EQ(errorsOnLines(kFirstLineAfter, std::numeric_limits<uint32_t>::max()), "");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
