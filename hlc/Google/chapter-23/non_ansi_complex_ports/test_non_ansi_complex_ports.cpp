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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.1--non-ansi-complex-ports.sv
//
// THE SOURCE
//
//   20: module complex_ports({c, d}, .e(f));
//   21:   input c, d;
//   22:   input f;
//   23: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// This is Example 2 of 23.2.2.1 with input declarations added. Its
// annotations: "Nets {c,d} receive the first port bits. Name 'f' is declared
// inside the module. Name 'e' is defined outside the module. Cannot use
// named port connections of first port."
//   - The first port is an implicit port whose port_expression is the
//     concatenation {c, d}. Only a "simple identifier or escaped identifier
//     ... shall be used as the port name" (23.2.2.1), so it has no name:
//     "If a port is explicitly named, then the explicit name shall be
//     returned. If not, and a name exists, then that name shall be returned.
//     Otherwise, NULL shall be returned." (37.14 detail 8).
//   - The second port is the explicit port e, whose expression is f.
//   - c, d and f are inputs, so both ports are inputs, and c, d and f are
//     the module's nets.
//   - The port expressions are the low connections (37.14 detail 4); the
//     module is never instantiated, so no port has a high connection
//     (37.14 detail 10).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors       no fatal, syntax or error diagnostic
//   FirstPortHasNoName          port 0: no name, vpiExplicitName FALSE
//   SecondPortIsExplicitE       port 1: named e, vpiExplicitName TRUE
//   BothPortsAreInputs          vpiDirection vpiInput for both
//   NetsCDAndFAreDeclared       the module holds the nets c, d and f
//   PortExpressionsAreLowConns  each port has a vpiLowConn, no vpiHighConn
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The shape of {c, d}. That is the expression model, outside 23.2.2.1.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.2.1--non-ansi-complex-ports.sv";

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

bool hasNetNamed(const hldb::NetCollection *nets, std::string_view name) {
  if (nets == nullptr) return false;
  for (const hldb::Net *net : *nets) {
    if (net->getName() == name) return true;
  }
  return false;
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

class NonAnsiComplexPortsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "non_ansi_complex_ports.hlc"}); }
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

  static const hldb::Module *findModule() {
    if ((m_design == nullptr) || (m_design->getAllModules() == nullptr)) return nullptr;
    for (const hldb::Module *module : *m_design->getAllModules()) {
      if (endsWith(module->getFile(), kSourceFileName) && (module->getName() == "complex_ports")) return module;
    }
    return nullptr;
  }

  // The port at position 'index' of the list of ports, or nullptr.
  static const hldb::Port *findPort(size_t index) {
    const hldb::Module *const module = findModule();
    if ((module == nullptr) || (module->getPorts() == nullptr) || (module->getPorts()->size() != 2)) return nullptr;
    return (*module->getPorts())[index];
  }
};

TEST_F(NonAnsiComplexPortsTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(NonAnsiComplexPortsTest, FirstPortHasNoName) {
  const hldb::Port *const port = findPort(0);
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(integerProperty(port, vpiPortIndex), 0);
  EXPECT_EQ(port->getName(), "");
  EXPECT_EQ(integerProperty(port, vpiExplicitName), 0);
}

TEST_F(NonAnsiComplexPortsTest, SecondPortIsExplicitE) {
  const hldb::Port *const port = findPort(1);
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(integerProperty(port, vpiPortIndex), 1);
  EXPECT_EQ(port->getName(), "e");
  EXPECT_EQ(integerProperty(port, vpiExplicitName), 1);
}

TEST_F(NonAnsiComplexPortsTest, BothPortsAreInputs) {
  for (size_t index = 0; index < 2; ++index) {
    const hldb::Port *const port = findPort(index);
    ASSERT_NE(port, nullptr) << index;
    EXPECT_EQ(integerProperty(port, vpiDirection), static_cast<int64_t>(vpiInput)) << index;
  }
}

TEST_F(NonAnsiComplexPortsTest, NetsCDAndFAreDeclared) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  for (std::string_view name : {"c", "d", "f"}) {
    EXPECT_TRUE(hasNetNamed(module->getNets(), name)) << name;
  }
}

TEST_F(NonAnsiComplexPortsTest, PortExpressionsAreLowConns) {
  for (size_t index = 0; index < 2; ++index) {
    const hldb::Port *const port = findPort(index);
    ASSERT_NE(port, nullptr) << index;
    EXPECT_EQ(relationSize(port, vpiLowConn), 1u) << index;
    EXPECT_EQ(relationSize(port, vpiHighConn), 0u) << index;
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
