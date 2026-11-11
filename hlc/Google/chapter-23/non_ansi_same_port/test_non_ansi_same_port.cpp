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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.1--non-ansi-same-port.sv
//
// THE SOURCE
//
//   20: module same_port(.a(i), .b(i));
//   21:   inout i;
//   22: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// This is Example 4 of 23.2.2.1: "Two ports with different names connected
// to same internal net". Its annotations: "Name 'i' is declared inside the
// module as an inout port. Names 'a' and 'b' are defined for port
// connections."
//   - a and b are explicit ports (vpiExplicitName TRUE, 37.14), indices 0
//     and 1 (37.14 detail 9).
//   - Both expressions are i, an inout, so both ports are inout.
//   - i has no net declaration, so it is the module's one implicit net; a
//     and b name ports only and declare no net (23.2.2.1: "Names 'a' and 'b'
//     are defined for port connections").
//   - Each port's low connection (37.14 detail 4) refers to that net i.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors       no fatal, syntax or error diagnostic
//   TwoExplicitPortsAAndB       a and b, indices 0 and 1, explicitly named
//   BothPortsAreInout           vpiDirection vpiInout for both
//   OneNetI                     the module holds exactly one net, i
//   BothPortsConnectToNetI      each port's vpiLowConn resolves to net i, and
//                               neither has a vpiHighConn (37.14 detail 10)
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - How an instance connects a and b. That is 23.3.

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

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.2.1--non-ansi-same-port.sv";

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

const hldb::Any *relatedObject(const hldb::Any *object, int32_t relation) {
  const hldb::Any::get_by_vpi_type_return_t result = object->getByVpiType(relation);
  if (const hldb::Any *const single = std::get<1>(result)) return single;
  const std::vector<const hldb::Any *> *const objects = std::get<2>(result);
  return ((objects != nullptr) && (objects->size() == 1)) ? objects->front() : nullptr;
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

class NonAnsiSamePortTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "non_ansi_same_port.hlc"}); }
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
      if (endsWith(module->getFile(), kSourceFileName) && (module->getName() == "same_port")) return module;
    }
    return nullptr;
  }

  static const hldb::PortCollection *findPorts() {
    const hldb::Module *const module = findModule();
    if ((module == nullptr) || (module->getPorts() == nullptr) || (module->getPorts()->size() != 2)) return nullptr;
    return module->getPorts();
  }
};

TEST_F(NonAnsiSamePortTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(NonAnsiSamePortTest, TwoExplicitPortsAAndB) {
  const hldb::PortCollection *const ports = findPorts();
  ASSERT_NE(ports, nullptr);
  const std::vector<std::string_view> names = {"a", "b"};
  for (size_t index = 0; index < names.size(); ++index) {
    const hldb::Port *const port = (*ports)[index];
    EXPECT_EQ(port->getName(), names[index]);
    EXPECT_EQ(integerProperty(port, vpiPortIndex), static_cast<int64_t>(index)) << names[index];
    EXPECT_EQ(integerProperty(port, vpiExplicitName), 1) << names[index];
  }
}

TEST_F(NonAnsiSamePortTest, BothPortsAreInout) {
  const hldb::PortCollection *const ports = findPorts();
  ASSERT_NE(ports, nullptr);
  for (const hldb::Port *port : *ports) {
    EXPECT_EQ(integerProperty(port, vpiDirection), static_cast<int64_t>(vpiInout)) << port->getName();
  }
}

TEST_F(NonAnsiSamePortTest, OneNetI) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  ASSERT_NE(module->getNets(), nullptr);
  ASSERT_EQ(module->getNets()->size(), 1u);
  EXPECT_EQ(module->getNets()->front()->getName(), "i");
}

TEST_F(NonAnsiSamePortTest, BothPortsConnectToNetI) {
  const hldb::Module *const module = findModule();
  const hldb::PortCollection *const ports = findPorts();
  ASSERT_NE(module, nullptr);
  ASSERT_NE(ports, nullptr);
  ASSERT_NE(module->getNets(), nullptr);
  ASSERT_EQ(module->getNets()->size(), 1u);
  for (const hldb::Port *port : *ports) {
    EXPECT_EQ(relationSize(port, vpiHighConn), 0u) << port->getName();
    const hldb::Any *const connection = relatedObject(port, vpiLowConn);
    ASSERT_NE(connection, nullptr) << port->getName();
    const hldb::RefObj *const lowConn = any_cast<hldb::RefObj>(connection);
    ASSERT_NE(lowConn, nullptr) << port->getName();
    EXPECT_EQ(lowConn->getActual(), module->getNets()->front()) << port->getName();
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
