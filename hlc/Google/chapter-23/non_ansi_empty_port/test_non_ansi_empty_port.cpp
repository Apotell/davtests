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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.1--non-ansi-empty-port.sv
//
// THE SOURCE
//
//   20: module m(a, , .c());
//   21:   input a;
//   22: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "port ::= [ port_expression ] | . port_identifier ( [ port_expression ] )"
// (Syntax 23-2), and "The port expression is optional because ports can be
// defined that do not connect to anything internal to the module."
// (23.2.2.1). So m has three ports:
//   - port 0, a, an input connected to the net a;
//   - port 1, with no expression and no name: a null port, which has no
//     name (37.14 detail 8: "Otherwise, NULL shall be returned"), no low
//     connection (detail 10: "vpiLowConn shall return NULL if the ... port
//     is a null port") and size 0 (detail 11: "vpiSize for a null port
//     shall return 0");
//   - port 2, the explicit port c with no expression, which therefore
//     connects to nothing inside the module.
// "The first port has a port index of zero." (37.14 detail 9)
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors       no fatal, syntax or error diagnostic
//   ThreePortsInOrder           three ports, vpiPortIndex 0, 1, 2
//   PortAIsAnInput              port 0 is a, vpiInput, with a vpiLowConn
//   EmptyPortIsANullPort        port 1: no name, no vpiLowConn
//   EmptyPortHasSizeZero        port 1: vpiSize 0 (skipped, model gap)
//   PortCConnectsToNothing      port 2: named c, explicitly named, no
//                               vpiLowConn
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The direction of ports 1 and 2. Nothing declares one, and the LRM
//     gives none.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/port.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.2.1--non-ansi-empty-port.sv";

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

class NonAnsiEmptyPortTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "non_ansi_empty_port.hlc"}); }
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

  // The port at position 'index' of the list of ports, or nullptr.
  static const hldb::Port *findPort(size_t index) {
    if ((m_design == nullptr) || (m_design->getAllModules() == nullptr)) return nullptr;
    for (const hldb::Module *module : *m_design->getAllModules()) {
      if (endsWith(module->getFile(), kSourceFileName) && (module->getPorts() != nullptr) &&
          (module->getPorts()->size() == 3)) {
        return (*module->getPorts())[index];
      }
    }
    return nullptr;
  }
};

TEST_F(NonAnsiEmptyPortTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(NonAnsiEmptyPortTest, ThreePortsInOrder) {
  for (size_t index = 0; index < 3; ++index) {
    const hldb::Port *const port = findPort(index);
    ASSERT_NE(port, nullptr) << index;
    EXPECT_EQ(port->getPortIndex(), static_cast<int32_t>(index));
  }
}

TEST_F(NonAnsiEmptyPortTest, PortAIsAnInput) {
  const hldb::Port *const port = findPort(0);
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), "a");
  EXPECT_EQ(port->getDirection(), vpiInput);
  EXPECT_NE(port->getLowConn(), nullptr);
}

TEST_F(NonAnsiEmptyPortTest, EmptyPortIsANullPort) {
  const hldb::Port *const port = findPort(1);
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), "");
  EXPECT_EQ(port->getLowConn(), nullptr);
}

TEST_F(NonAnsiEmptyPortTest, EmptyPortHasSizeZero) {
  // Expected (37.14 detail 11): the size of port 1 is 0.
  GTEST_SKIP() << "hldb::Port has no size (vpiSize, IEEE 1800-2023 37.14); see hldb_model_gaps.md #8";
}

TEST_F(NonAnsiEmptyPortTest, PortCConnectsToNothing) {
  const hldb::Port *const port = findPort(2);
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), "c");
  EXPECT_TRUE(port->getExplicitName());
  EXPECT_EQ(port->getLowConn(), nullptr);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
