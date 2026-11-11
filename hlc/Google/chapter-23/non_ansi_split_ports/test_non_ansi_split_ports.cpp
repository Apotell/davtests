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

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.2.1--non-ansi-split-ports.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

int64_t integerProperty(const hldb::Any *object, int32_t property) {
  const hldb::Any::vpi_property_value_t value = object->getVpiPropertyValue(property);
  if (const int64_t *const number = std::get_if<int64_t>(&value)) return *number;
  return vpiUndefined;
}

size_t relationSize(const hldb::Any *object, int32_t relation) {
  const hldb::Any::get_by_vpi_type_return_t result = object->getByVpiType(relation);
  const std::vector<const hldb::Any *> *const objects = std::get<2>(result);
  if (objects != nullptr) return objects->size();
  return (std::get<1>(result) != nullptr) ? 1 : 0;
}

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

class NonAnsiSplitPortsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "non_ansi_split_ports.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static std::string allErrors() {
    if ((m_session == nullptr) || (m_session->getEventListener() == nullptr)) return "no error container";
    std::string description;
    for (const Event &error : m_session->getEventListener()->getEvents()) {
      if (isErrorClass(error)) description += describeError(error);
    }
    return description;
  }

  static const hldb::PortCollection *findPorts() {
    if ((m_design == nullptr) || (m_design->getAllModules() == nullptr)) return nullptr;
    for (const hldb::Module *module : *m_design->getAllModules()) {
      if (endsWith(module->getFile(), kSourceFileName) && (module->getName() == "split_ports")) {
        return module->getPorts();
      }
    }
    return nullptr;
  }
};

TEST_F(NonAnsiSplitPortsTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(NonAnsiSplitPortsTest, TwoUnnamedPorts) {
  const hldb::PortCollection *const ports = findPorts();
  ASSERT_NE(ports, nullptr);
  ASSERT_EQ(ports->size(), 2u);
  for (size_t index = 0; index < ports->size(); ++index) {
    const hldb::Port *const port = (*ports)[index];
    EXPECT_EQ(integerProperty(port, vpiPortIndex), static_cast<int64_t>(index));
    EXPECT_EQ(port->getName(), "") << index;
    EXPECT_EQ(integerProperty(port, vpiExplicitName), 0) << index;
  }
}

TEST_F(NonAnsiSplitPortsTest, BothPortsAreInputs) {
  const hldb::PortCollection *const ports = findPorts();
  ASSERT_NE(ports, nullptr);
  ASSERT_EQ(ports->size(), 2u);
  for (size_t index = 0; index < ports->size(); ++index) {
    EXPECT_EQ(integerProperty((*ports)[index], vpiDirection), static_cast<int64_t>(vpiInput)) << index;
  }
}

TEST_F(NonAnsiSplitPortsTest, BothPortsAreFourBits) {
  const hldb::PortCollection *const ports = findPorts();
  ASSERT_NE(ports, nullptr);
  ASSERT_EQ(ports->size(), 2u);
  for (size_t index = 0; index < ports->size(); ++index) {
    EXPECT_EQ(integerProperty((*ports)[index], vpiSize), 4) << index;
  }
}

TEST_F(NonAnsiSplitPortsTest, PortExpressionsAreLowConns) {
  const hldb::PortCollection *const ports = findPorts();
  ASSERT_NE(ports, nullptr);
  ASSERT_EQ(ports->size(), 2u);
  for (size_t index = 0; index < ports->size(); ++index) {
    EXPECT_EQ(relationSize((*ports)[index], vpiLowConn), 1u) << index;
    EXPECT_EQ(relationSize((*ports)[index], vpiHighConn), 0u) << index;
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
