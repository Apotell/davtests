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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.2--ansi-shared-attributes.sv
//
// THE SOURCE
//
//   20: module test(input [7:0] a,
//   21:             input signed [7:0] b, c, d,
//   22:             output [7:0] e,
//   23:             output var signed [7:0] f, g,
//   24:             output signed [7:0] h);
//   25: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// 23.2.2.2 gives this as the ANSI form of 23.2.2.1 Example 1: "Multiple
// ports that share all attributes can be declared together." "Every
// attribute of the declaration shall be in the one declaration."
//   - The ports are a to h in that order, port indices 0 to 7 (37.14
//     detail 9).
//   - a to d are inputs, e to h outputs.
//   - The keyword var makes f and g variables; the others have no port kind
//     and an implicit data type, so they default to nets (23.2.2.3).
//   - b, c, d share "signed", as do f and g; h is signed; a and e are not.
//     Nets and variables report this through vpiSigned (vpi_user.h).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error diagnostic
//   EightPortsInOrder          a to h, vpiPortIndex 0 to 7
//   DirectionsAreShared        a-d vpiInput, e-h vpiOutput
//   FAndGAreVariablesOthersNets  vpiLowConn resolves to a variable for f
//                              and g, to a net for the others
//   SignednessIsShared         vpiSigned TRUE except for a and e
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - vpiSize. Every port is [7:0]; port size is covered by
//     23.2.2.2--ansi-explicit-named.sv.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.2.2--ansi-shared-attributes.sv";

struct ExpectedPort {
  std::string_view m_name;
  int32_t m_direction;
  bool m_variable;
  bool m_signed;
};

const std::vector<ExpectedPort> kExpectedPorts = {
    {"a", vpiInput, false, false}, {"b", vpiInput, false, true},   {"c", vpiInput, false, true},
    {"d", vpiInput, false, true},  {"e", vpiOutput, false, false}, {"f", vpiOutput, true, true},
    {"g", vpiOutput, true, true},  {"h", vpiOutput, false, true},
};

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

uint32_t lineOf(const Event &event) { return event.getArguments().empty() ? 0 : event.getArguments().front().m_line; }

bool isErrorClass(const Event &event) {
  const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), event.getType());
  if (errorInfo == nullptr) return false;
  const EventSeverity severity = errorInfo->m_severity;
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}
}  // namespace

class AnsiSharedAttributesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ansi_shared_attributes.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static std::string allEvents() {
    hlc::EventListener *const listener = m_session->getEventListener();
    std::ostringstream oss;
    for (const hldb::Event &event : listener->getEvents()) {
      if (isErrorClass(event)) {
        std::vector<std::string> msgs = hldb::renderEvent(event);
        std::copy(msgs.cbegin(), msgs.cend(), std::ostream_iterator<std::string>(oss));
      }
    }
    return oss.str();
  }

  static const hldb::PortCollection *findPorts() {
    if ((m_design == nullptr) || (m_design->getAllModules() == nullptr)) return nullptr;
    for (const hldb::Module *module : *m_design->getAllModules()) {
      if (endsWith(module->getFile(), kSourceFileName)) return module->getPorts();
    }
    return nullptr;
  }
};

TEST_F(AnsiSharedAttributesTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(AnsiSharedAttributesTest, EightPortsInOrder) {
  const hldb::PortCollection *const ports = findPorts();
  ASSERT_NE(ports, nullptr);
  EXPECT_EQ(ports->size(), kExpectedPorts.size());
  for (size_t index = 0; index < kExpectedPorts.size(); ++index) {
    const hldb::Port *const port = hldb::findByName<hldb::Port>(kExpectedPorts[index].m_name, ports);
    ASSERT_NE(port, nullptr) << kExpectedPorts[index].m_name;
    EXPECT_EQ(port->getPortIndex(), static_cast<int32_t>(index)) << kExpectedPorts[index].m_name;
  }
}

TEST_F(AnsiSharedAttributesTest, DirectionsAreShared) {
  for (const ExpectedPort &expected : kExpectedPorts) {
    const hldb::Port *const port = hldb::findByName<hldb::Port>(expected.m_name, findPorts());
    ASSERT_NE(port, nullptr) << expected.m_name;
    EXPECT_EQ(port->getDirection(), expected.m_direction) << expected.m_name;
  }
}

TEST_F(AnsiSharedAttributesTest, FAndGAreVariablesOthersNets) {
  for (const ExpectedPort &expected : kExpectedPorts) {
    const hldb::Port *const port = hldb::findByName<hldb::Port>(expected.m_name, findPorts());
    ASSERT_NE(port, nullptr) << expected.m_name;
    const hldb::Any *const actual = hldb::getActual(port->getLowConn());
    ASSERT_NE(actual, nullptr) << expected.m_name;
    if (expected.m_variable) {
      EXPECT_NE(any_cast<hldb::Variable>(actual), nullptr) << expected.m_name;
    } else {
      EXPECT_NE(any_cast<hldb::Net>(actual), nullptr) << expected.m_name;
    }
  }
}

TEST_F(AnsiSharedAttributesTest, SignednessIsShared) {
  for (const ExpectedPort &expected : kExpectedPorts) {
    const hldb::Port *const port = hldb::findByName<hldb::Port>(expected.m_name, findPorts());
    ASSERT_NE(port, nullptr) << expected.m_name;
    const hldb::Any *const actual = hldb::getActual(port->getLowConn());
    ASSERT_NE(actual, nullptr) << expected.m_name;
    const hldb::Typespec *const ts = hldb::getTypespec(actual);
    ASSERT_NE(ts, nullptr);
    EXPECT_EQ(hldb::getSigned(ts), expected.m_signed) << expected.m_name;
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
