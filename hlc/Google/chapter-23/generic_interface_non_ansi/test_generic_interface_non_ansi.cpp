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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.2--generic-interface-non-ansi.sv
//
// THE SOURCE
//
//   19: module m(d);
//   20:   interface d;
//   21: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "Generic interface ports (see 25.3.3) cannot be declared using the non-ANSI
// style list_of_ports syntax (see 23.2.2.1). Generic interface ports can only
// be declared using the ANSI style list_of_port_declarations syntax."
// (23.2.2.2). Line 20 tries to declare d as a generic interface port in the
// body of a non-ANSI module, so module m (lines 19-21) is in error.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   IllegalCodeIsAnError             PaSyntaxError on lines 19-21. The
//                                    grammar has no non-ANSI port
//                                    declaration for a generic interface
//                                    (port_declaration, A.1.3), so
//                                    "interface d;" cannot be parsed as one
//   NoErrorAfterTheIllegalCode       none on line 22 or later
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The diagnostic's wording, and on which of lines 19-21 the parser
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
constexpr uint32_t kIllegalFirstLine = 19;
constexpr uint32_t kIllegalLastLine = 21;
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

class GenericInterfaceNonAnsiTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "generic_interface_non_ansi.hlc"}); }
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

TEST_F(GenericInterfaceNonAnsiTest, IllegalCodeIsAnError) {
  ASSERT_NE(m_session, nullptr);
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const Event *const event = findEvent(EventType::PaSyntaxError);
  ASSERT_NE(event, nullptr);
  EXPECT_GE(lineOf(*event), kIllegalFirstLine);
  EXPECT_LE(lineOf(*event), kIllegalLastLine);
}

TEST_F(GenericInterfaceNonAnsiTest, NoErrorAfterTheIllegalCode) {
  EXPECT_EQ(eventsOnLines(kFirstLineAfter, std::numeric_limits<uint32_t>::max()), "");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
