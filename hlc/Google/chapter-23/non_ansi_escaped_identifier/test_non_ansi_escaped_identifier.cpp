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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.1--non-ansi-escaped-identifier.sv
//
// THE SOURCE
//
//   20: module m(\bus+index , c);
//   21:   input \bus+index ;
//   22:   input c;
//   23: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "The port reference for each port in the list_of_ports in the module
// header can be one of the following: -- A simple identifier or escaped
// identifier" (23.2.2.1), and for an implicit port whose expression is
// such an identifier, the identifier "shall be used as the port name".
// "The leading backslash character and the terminating white space are not
// considered to be part of an identifier." (5.6.1). So the first port and
// its net are both named bus+index, without the backslash or the trailing
// space, and the second port is c.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors       no fatal, syntax or error diagnostic
//   PortNamesDropTheBackslash   port 0 is named "bus+index", port 1 "c"
//   BothPortsAreInputs          vpiDirection vpiInput for both
//   EscapedPortConnectsToItsNet the vpiLowConn of port 0 resolves to the net
//                               named "bus+index"
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - vpiFullName. It is a computed property this suite does not assert.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.2.1--non-ansi-escaped-identifier.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

// True for an error-class event: one whose severity is fatal, syntax or error.
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

class NonAnsiEscapedIdentifierTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "non_ansi_escaped_identifier.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Every error-class event, described as by describeEvent(); empty when there is none.
  static std::string allEvents() {
    hlc::EventListener *const listener = m_session->getEventListener();
    std::ostringstream oss;
    for (const Event &event : listener->getEvents()) {
      if (isErrorClass(event)) oss << describeEvent(event);
    }
    return oss.str();
  }

  static const hldb::PortCollection *findPorts() {
    if ((m_design == nullptr) || (m_design->getAllModules() == nullptr)) return nullptr;
    for (const hldb::Module *module : *m_design->getAllModules()) {
      if (endsWith(module->getFile(), kSourceFileName) && (module->getPorts() != nullptr) &&
          (module->getPorts()->size() == 2)) {
        return module->getPorts();
      }
    }
    return nullptr;
  }
};

TEST_F(NonAnsiEscapedIdentifierTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(NonAnsiEscapedIdentifierTest, PortNamesDropTheBackslash) {
  const hldb::PortCollection *const ports = findPorts();
  ASSERT_NE(ports, nullptr);
  EXPECT_EQ((*ports)[0]->getName(), "bus+index");
  EXPECT_EQ((*ports)[1]->getName(), "c");
}

TEST_F(NonAnsiEscapedIdentifierTest, BothPortsAreInputs) {
  const hldb::PortCollection *const ports = findPorts();
  ASSERT_NE(ports, nullptr);
  for (const hldb::Port *port : *ports) {
    EXPECT_EQ(port->getDirection(), vpiInput) << port->getName();
  }
}

TEST_F(NonAnsiEscapedIdentifierTest, EscapedPortConnectsToItsNet) {
  const hldb::PortCollection *const ports = findPorts();
  ASSERT_NE(ports, nullptr);
  const hldb::Any *const connection = (*ports)[0]->getLowConn();
  ASSERT_NE(connection, nullptr);
  const hldb::RefObj *const lowConn = any_cast<hldb::RefObj>(connection);
  ASSERT_NE(lowConn, nullptr);
  const hldb::Net *const net = lowConn->getActual<hldb::Net>();
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getName(), "bus+index");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
