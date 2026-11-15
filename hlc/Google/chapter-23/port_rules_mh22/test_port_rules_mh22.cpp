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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.3--mh22-explicit-port-inheritance.sv
//
// THE SOURCE
//
//   21: module mh22(input wire integer p_a, .p_b(s_b), p_c);
//   22:   logic [5:0] s_b;
//   23: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "Explicit port declarations shall inherit only the port direction from the
// preceding port (if not explicitly specified), but not other properties. The
// data type of the port is the self-determined data type of the expression."
// "A port declaration that immediately follows an explicit port declaration
// shall inherit only the port direction (if not explicitly specified) from
// the explicit port declaration, but not other properties. The port kind and
// data type of such a port shall be determined using the same rules as for
// the first port in the port list." (23.2.2.3). For this exact example the
// LRM says: "port p_a is fully declared. p_b is an explicitly named port that
// inherits only the direction input from port p_a. Its data type is that of
// s_b. Port p_c inherits only the direction from p_b, and defaults to the net
// port kind and to the logic data type."
//   - p_a: input, a wire net of type integer (32 bits, 6.11).
//   - p_b: input, explicitly named (vpiExplicitName, 37.14); its expression
//     s_b is its low connection (37.14 detail 4), so its type is logic [5:0]
//     and its vpiSize is 6. p_b names a port only and declares nothing
//     ("port identifiers exist in their own name space", 23.2.2.2).
//   - p_c: input, a wire net (the default net type, 22.8) of type logic, 1
//     bit; it does not inherit integer from p_a.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors       no fatal, syntax or error diagnostic
//   PortsInDeclarationOrder     p_a, p_b, p_c with vpiPortIndex 0, 1, 2
//   AllThreeAreInputs           vpiDirection vpiInput for all three
//   PortAIsAWireIntegerNet      net, vpiWire, integer typespec, vpiSize 32
//   PortBTakesTheTypeOfSB       explicitly named, vpiLowConn resolves to the
//                               variable s_b, port vpiSize 6, and no net or
//                               variable is named p_b
//   PortCIsAWireLogicNet        net, vpiWire, logic typespec, vpiSize 1
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Instances and connections; they are 23.3.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

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
constexpr std::string_view kSourceFileName = "23.2.2.3--mh22-explicit-port-inheritance.sv";

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

// The object a port's low connection refers to, or nullptr.
const hldb::Any *lowConnActual(const hldb::Port *port) {
  const hldb::Any *const connection = relatedObject(port, vpiLowConn);
  if (connection == nullptr) return nullptr;
  const hldb::RefObj *const reference = any_cast<hldb::RefObj>(connection);
  return (reference == nullptr) ? nullptr : reference->getActual();
}

template <typename T>
bool hasObjectNamed(const std::vector<T *> *collection, std::string_view name) {
  if (collection == nullptr) return false;
  for (const T *object : *collection) {
    if (object->getName() == name) return true;
  }
  return false;
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

class PortRulesMh22Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "port_rules_mh22.hlc"}); }
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

  static const hldb::Module *findModule() {
    if ((m_design == nullptr) || (m_design->getAllModules() == nullptr)) return nullptr;
    for (const hldb::Module *module : *m_design->getAllModules()) {
      if (endsWith(module->getFile(), kSourceFileName) && (module->getName() == "mh22")) return module;
    }
    return nullptr;
  }

  // The port at position 'index' of the list of ports, or nullptr.
  static const hldb::Port *findPort(size_t index) {
    const hldb::Module *const module = findModule();
    if ((module == nullptr) || (module->getPorts() == nullptr) || (module->getPorts()->size() != 3)) return nullptr;
    return (*module->getPorts())[index];
  }

  // Asserts that the port at 'index' declares a wire net of the given typespec
  // type and width.
  static void expectWireNet(size_t index, uint32_t dataType, int64_t size) {
    const hldb::Port *const port = findPort(index);
    ASSERT_NE(port, nullptr);
    const hldb::Any *const declared = lowConnActual(port);
    ASSERT_NE(declared, nullptr);
    EXPECT_NE(any_cast<hldb::Net>(declared), nullptr);
    EXPECT_EQ(integerProperty(declared, vpiNetType), static_cast<int64_t>(vpiWire));
    const hldb::Any *const typespec = actualTypespec(declared);
    ASSERT_NE(typespec, nullptr);
    EXPECT_EQ(typespec->getVpiType(), dataType);
    EXPECT_EQ(integerProperty(declared, vpiSize), size);
  }
};

TEST_F(PortRulesMh22Test, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(PortRulesMh22Test, PortsInDeclarationOrder) {
  const std::vector<std::string_view> names = {"p_a", "p_b", "p_c"};
  for (size_t index = 0; index < names.size(); ++index) {
    const hldb::Port *const port = findPort(index);
    ASSERT_NE(port, nullptr) << index;
    EXPECT_EQ(port->getName(), names[index]);
    EXPECT_EQ(integerProperty(port, vpiPortIndex), static_cast<int64_t>(index));
  }
}

TEST_F(PortRulesMh22Test, AllThreeAreInputs) {
  for (size_t index = 0; index < 3; ++index) {
    const hldb::Port *const port = findPort(index);
    ASSERT_NE(port, nullptr) << index;
    EXPECT_EQ(integerProperty(port, vpiDirection), static_cast<int64_t>(vpiInput)) << index;
  }
}

TEST_F(PortRulesMh22Test, PortAIsAWireIntegerNet) { expectWireNet(0, vpiIntegerTypespec, 32); }

TEST_F(PortRulesMh22Test, PortBTakesTheTypeOfSB) {
  const hldb::Module *const module = findModule();
  const hldb::Port *const port = findPort(1);
  ASSERT_NE(module, nullptr);
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(integerProperty(port, vpiExplicitName), 1);

  const hldb::Any *const declared = lowConnActual(port);
  ASSERT_NE(declared, nullptr);
  const hldb::Variable *const variable = any_cast<hldb::Variable>(declared);
  ASSERT_NE(variable, nullptr);
  EXPECT_EQ(variable->getName(), "s_b");

  EXPECT_EQ(integerProperty(port, vpiSize), 6);
  EXPECT_FALSE(hasObjectNamed(module->getNets(), "p_b"));
  EXPECT_FALSE(hasObjectNamed(module->getVariables(), "p_b"));
}

TEST_F(PortRulesMh22Test, PortCIsAWireLogicNet) { expectWireNet(2, vpiLogicTypespec, 1); }
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
