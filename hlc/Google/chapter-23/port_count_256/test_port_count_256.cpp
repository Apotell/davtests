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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2--port-count-256.sv
//
// THE SOURCE
//
//   19: module m(input p0, p1, p2, p3, p4, p5, p6, p7,
//   20:          p8, p9, p10, p11, p12, p13, p14, p15,
//   ...          (eight ports per line)
//   50:          p248, p249, p250, p251, p252, p253, p254, p255);
//   51: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "Implementations may limit the maximum number of ports in a module
// definition, but the limit shall be at least 256." (23.2.2). m has exactly
// 256 ports, the smallest count every implementation must accept. Ports p1
// to p255 inherit "input" from p0 (23.2.2.3), and "The first port has a port
// index of zero" (37.14 detail 9), so port pN has index N.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error diagnostic
//   ModuleHas256Ports          exactly 256 ports
//   PortsAreInDeclarationOrder port pN has vpiPortIndex N and vpiInput
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - More than 256 ports. The LRM lets an implementation stop at 256.

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
constexpr std::string_view kSourceFileName = "23.2.2--port-count-256.sv";
constexpr size_t kPortCount = 256;

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

class PortCount256Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "port_count_256.hlc"}); }
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

  static const hldb::Module *findModule() {
    if ((m_design == nullptr) || (m_design->getAllModules() == nullptr)) return nullptr;
    for (const hldb::Module *module : *m_design->getAllModules()) {
      if (endsWith(module->getFile(), kSourceFileName) && (module->getName() == "m")) return module;
    }
    return nullptr;
  }
};

TEST_F(PortCount256Test, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(PortCount256Test, ModuleHas256Ports) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  ASSERT_NE(module->getPorts(), nullptr);
  EXPECT_EQ(module->getPorts()->size(), kPortCount);
}

TEST_F(PortCount256Test, PortsAreInDeclarationOrder) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  ASSERT_NE(module->getPorts(), nullptr);
  ASSERT_EQ(module->getPorts()->size(), kPortCount);
  for (size_t index = 0; index < kPortCount; ++index) {
    const hldb::Port *const port = (*module->getPorts())[index];
    EXPECT_EQ(port->getName(), "p" + std::to_string(index));
    EXPECT_EQ(port->getPortIndex(), static_cast<int32_t>(index)) << port->getName();
    EXPECT_EQ(port->getDirection(), vpiInput) << port->getName();
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
