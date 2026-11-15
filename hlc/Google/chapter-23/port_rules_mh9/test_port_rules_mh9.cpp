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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.3--mh9-output-var.sv
//
// THE SOURCE
//
//   18: module mh9(output var x);
//   19: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// For the first port in an ANSI style port list (23.2.2.3): "If the
// direction is omitted, it shall default to inout. If the data type is
// omitted, it shall default to logic ... If the port kind is omitted: For
// input and inout ports, the port shall default to a net of default net type.
// ... For output ports, ... If the data type is omitted or declared with the
// implicit_data_type syntax, the port kind shall default to a net of default
// net type. If the data type is declared with the explicit data_type syntax,
// the port kind shall default to variable. A ref port is always a variable."
// The default net type is wire (22.8); integer is 32 bits (6.11).
// The LRM annotates this example:
//   "module mh9 (output var x); // output var logic x"
//
// How the test observes it: each port's vpiLowConn (37.14 detail 4) refers
// to the net or variable the port declares. Its port kind is the object
// itself (a net with the vpiNetType of its net type, or a variable), its
// data type is its typespec (an array typespec for unpacked dimensions), and
// its packed width is vpiSize (37.17).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors         no fatal, syntax or error diagnostic
//   PortsInDeclarationOrder       port names and vpiPortIndex 0, 1, ...
//   DirectionsFollowTheRules      vpiDirection of each port
//   PortKindsFollowTheRules       net (with its vpiNetType) or variable
//   DataTypesFollowTheRules       typespec of each net or variable
//   PackedWidthsFollowTheRules    vpiSize of each net or variable
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Instances and connections. The rules of 23.2.2.3 fix kind, data type
//     and direction from the declaration alone; connections are 23.3.
//   - The port objects' own vpiSize. 23.2.2.2--ansi-explicit-named.sv covers
//     it.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/array_typespec.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.2.3--mh9-output-var.sv";
constexpr std::string_view kModuleName = "mh9";

// Port kind marker for a variable; a net is identified by its vpiNetType.
constexpr int64_t kVariable = 0;
// Interconnect ports "have no data type" (23.2.2.3).
constexpr uint32_t kNoDataType = 0;
constexpr int64_t kUnchecked = -1;

struct ExpectedPort {
  std::string_view m_name;
  int64_t m_direction;
  int64_t m_kind;
  uint32_t m_dataType;
  int64_t m_size;
  int64_t m_signed;
  bool m_unpacked;
};

const std::vector<ExpectedPort> kExpectedPorts = {
    {"x", vpiOutput, kVariable, vpiLogicTypespec, 1, kUnchecked, false},
};

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

int64_t integerProperty(const hldb::Any *object, int32_t property) {
  const hldb::Any::vpi_property_value_t value = object->getVpiPropertyValue(property);
  if (const int64_t *const number = std::get_if<int64_t>(&value)) return *number;
  return vpiUndefined;
}

const hldb::Any *relatedObject(const hldb::Any *object, int32_t relation) {
  const hldb::Any::get_by_vpi_type_return_t result = object->getByVpiType(relation);
  if (const hldb::Any *const single = std::get<1>(result)) return single;
  const std::vector<const hldb::Any *> *const objects = std::get<2>(result);
  return ((objects != nullptr) && (objects->size() == 1)) ? objects->front() : nullptr;
}

// The typespec of an object, seen through HLDB's reference typespec.
const hldb::Any *actualTypespec(const hldb::Any *object) {
  const hldb::Any *const typespec = relatedObject(object, vpiTypespec);
  if (typespec == nullptr) return nullptr;
  if (const hldb::RefTypespec *const reference = any_cast<hldb::RefTypespec>(typespec)) {
    return reference->getActual();
  }
  return typespec;
}

// The net or variable a port's low connection refers to, or nullptr.
const hldb::Any *lowConnActual(const hldb::Port *port) {
  const hldb::Any *const connection = relatedObject(port, vpiLowConn);
  if (connection == nullptr) return nullptr;
  const hldb::RefObj *const reference = any_cast<hldb::RefObj>(connection);
  return (reference == nullptr) ? nullptr : reference->getActual();
}

const hldb::Port *findPortByName(const hldb::PortCollection *ports, std::string_view name) {
  if (ports == nullptr) return nullptr;
  for (const hldb::Port *port : *ports) {
    if (port->getName() == name) return port;
  }
  return nullptr;
}

uint32_t lineOf(const Event &event) { return event.getArguments().empty() ? 0 : event.getArguments().front().m_line; }

bool isErrorClass(const Event &event) {
  const EventDefinition *const eventDefinition = hldb::findEventDefinition(hlc::getEventTable(), event.getType());
  if (eventDefinition == nullptr) return false;
  const EventSeverity severity = eventDefinition->m_severity;
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}

std::string describeEvent(const Event &event) {
  const EventDefinition *const eventDefinition = hldb::findEventDefinition(hlc::getEventTable(), event.getType());
  std::string description = " [" + std::to_string(static_cast<int32_t>(event.getType()));
  description += " line " + std::to_string(lineOf(event));
  if (eventDefinition != nullptr) description += ": " + std::string(eventDefinition->m_lines[0].m_text);
  return description + "]";
}
}  // namespace

class PortRulesMh9Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "port_rules_mh9.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static std::string allErrors() {
    if ((m_session == nullptr) || (m_session->getEventListener() == nullptr)) return "no event listener";
    std::string description;
    for (const Event &event : m_session->getEventListener()->getEvents()) {
      if (isErrorClass(event)) description += describeEvent(event);
    }
    return description;
  }

  static const hldb::PortCollection *findPorts() {
    if ((m_design == nullptr) || (m_design->getAllModules() == nullptr)) return nullptr;
    for (const hldb::Module *module : *m_design->getAllModules()) {
      if (endsWith(module->getFile(), kSourceFileName) && (module->getName() == kModuleName)) {
        return module->getPorts();
      }
    }
    return nullptr;
  }

  // The net or variable declared by the port named 'name', or nullptr.
  static const hldb::Any *findDeclared(std::string_view name) {
    const hldb::Port *const port = findPortByName(findPorts(), name);
    return (port == nullptr) ? nullptr : lowConnActual(port);
  }
};

TEST_F(PortRulesMh9Test, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(PortRulesMh9Test, PortsInDeclarationOrder) {
  const hldb::PortCollection *const ports = findPorts();
  ASSERT_NE(ports, nullptr);
  ASSERT_EQ(ports->size(), kExpectedPorts.size());
  for (size_t index = 0; index < kExpectedPorts.size(); ++index) {
    EXPECT_EQ((*ports)[index]->getName(), kExpectedPorts[index].m_name);
    EXPECT_EQ(integerProperty((*ports)[index], vpiPortIndex), static_cast<int64_t>(index));
  }
}

TEST_F(PortRulesMh9Test, DirectionsFollowTheRules) {
  for (const ExpectedPort &expected : kExpectedPorts) {
    const hldb::Port *const port = findPortByName(findPorts(), expected.m_name);
    ASSERT_NE(port, nullptr) << expected.m_name;
    EXPECT_EQ(integerProperty(port, vpiDirection), expected.m_direction) << expected.m_name;
  }
}

TEST_F(PortRulesMh9Test, PortKindsFollowTheRules) {
  for (const ExpectedPort &expected : kExpectedPorts) {
    const hldb::Any *const declared = findDeclared(expected.m_name);
    ASSERT_NE(declared, nullptr) << expected.m_name;
    if (expected.m_kind == kVariable) {
      EXPECT_NE(any_cast<hldb::Variable>(declared), nullptr) << expected.m_name << " is not a variable";
    } else {
      EXPECT_NE(any_cast<hldb::Net>(declared), nullptr) << expected.m_name << " is not a net";
      EXPECT_EQ(integerProperty(declared, vpiNetType), expected.m_kind) << expected.m_name;
    }
  }
}

TEST_F(PortRulesMh9Test, DataTypesFollowTheRules) {
  for (const ExpectedPort &expected : kExpectedPorts) {
    if (expected.m_dataType == kNoDataType) continue;
    const hldb::Any *const declared = findDeclared(expected.m_name);
    ASSERT_NE(declared, nullptr) << expected.m_name;
    const hldb::Any *const typespec = actualTypespec(declared);
    ASSERT_NE(typespec, nullptr) << expected.m_name;
    if (expected.m_unpacked) {
      ASSERT_EQ(typespec->getVpiType(), static_cast<uint32_t>(vpiArrayTypespec)) << expected.m_name;
      const hldb::RefTypespec *const element = any_cast<hldb::ArrayTypespec>(typespec)->getElemTypespec();
      ASSERT_NE(element, nullptr) << expected.m_name;
      ASSERT_NE(element->getActual(), nullptr) << expected.m_name;
      EXPECT_EQ(element->getActual()->getVpiType(), expected.m_dataType) << expected.m_name;
    } else {
      EXPECT_EQ(typespec->getVpiType(), expected.m_dataType) << expected.m_name;
    }
  }
}

TEST_F(PortRulesMh9Test, PackedWidthsFollowTheRules) {
  for (const ExpectedPort &expected : kExpectedPorts) {
    if (expected.m_size == kUnchecked) continue;
    const hldb::Any *const declared = findDeclared(expected.m_name);
    ASSERT_NE(declared, nullptr) << expected.m_name;
    EXPECT_EQ(integerProperty(declared, vpiSize), expected.m_size) << expected.m_name;
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
