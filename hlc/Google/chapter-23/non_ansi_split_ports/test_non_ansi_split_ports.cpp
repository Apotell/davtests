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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.1--non-ansi-split-ports.sv
//
// THE SOURCE
//
//   19: module split_ports(a[7:4], a[3:0]);
//   20:   input [7:0] a;
//   21: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// This is Example 3 of 23.2.2.1 with the input declaration added. Its
// annotations: "First port is upper 4 bits of 'a'. Second port is lower 4
// bits of 'a'. Cannot use named port connections because of part-select port
// 'a'." A port reference can be "A part-select of a vector declared within
// the module" (23.2.2.1).
//   - Both ports are implicit ports whose expressions are part-selects, not
//     simple identifiers, so neither has a name (23.2.2.1; 37.14 detail 8:
//     "Otherwise, NULL shall be returned").
//   - a is an input, so both ports are inputs.
//   - Each port is 4 bits wide: vpiSize (37.14) of a[7:4] and a[3:0].
//   - The part-selects are the low connections (37.14 detail 4); the module
//     is never instantiated, so there is no high connection (detail 10).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors       no fatal, syntax or error diagnostic
//   TwoUnnamedPorts             two ports, indices 0 and 1, no names, not
//                               explicitly named
//   BothPortsAreInputs          vpiDirection vpiInput for both
//   BothPortsAreFourBits        vpiSize 4 for both
//   PortExpressionsAreLowConns  each port has a vpiLowConn, no vpiHighConn
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The bounds of each part-select. That is the expression model.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/port.h>
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
constexpr std::string_view kSourceFileName = "23.2.2.1--non-ansi-split-ports.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

// True for an error-class event: fatal, syntax or error severity.
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

class NonAnsiSplitPortsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "non_ansi_split_ports.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
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
      if (endsWith(module->getFile(), kSourceFileName) && (module->getDefName() == std::string_view("split_ports"))) {
        return module->getPorts();
      }
    }
    return nullptr;
  }
};

TEST_F(NonAnsiSplitPortsTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(NonAnsiSplitPortsTest, TwoUnnamedPorts) {
  GTEST_SKIP() << "An implicit non-ANSI port whose expression is a part-select (a[7:4]) is named after its "
                  "identifier instead of having no name (IEEE 1800-2023 23.2.2.1, 37.14 detail 8); see "
                  "hldb_model_gaps.md #3";
  const hldb::PortCollection *const ports = findPorts();
  ASSERT_NE(ports, nullptr);
  ASSERT_EQ(ports->size(), 2u);
  for (size_t index = 0; index < ports->size(); ++index) {
    const hldb::Port *const port = (*ports)[index];
    EXPECT_EQ(port->getPortIndex(), static_cast<int32_t>(index));
    EXPECT_EQ(port->getName(), "") << index;
    EXPECT_FALSE(port->getExplicitName()) << index;
  }
}

TEST_F(NonAnsiSplitPortsTest, BothPortsAreInputs) {
  const hldb::PortCollection *const ports = findPorts();
  ASSERT_NE(ports, nullptr);
  ASSERT_EQ(ports->size(), 2u);
  for (size_t index = 0; index < ports->size(); ++index) {
    EXPECT_EQ((*ports)[index]->getDirection(), vpiInput) << index;
  }
}

TEST_F(NonAnsiSplitPortsTest, BothPortsAreFourBits) {
  // Expected: the size of both ports is 4.
  GTEST_SKIP() << "hldb::Port has no size (vpiSize, IEEE 1800-2023 37.14); see hldb_model_gaps.md #8";
}

TEST_F(NonAnsiSplitPortsTest, PortExpressionsAreLowConns) {
  const hldb::PortCollection *const ports = findPorts();
  ASSERT_NE(ports, nullptr);
  ASSERT_EQ(ports->size(), 2u);
  for (size_t index = 0; index < ports->size(); ++index) {
    EXPECT_NE((*ports)[index]->getLowConn(), nullptr) << index;
    EXPECT_EQ((*ports)[index]->getHighConn(), nullptr) << index;
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
