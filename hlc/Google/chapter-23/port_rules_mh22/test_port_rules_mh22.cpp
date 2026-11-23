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
//   - p_b: input, explicitly named (37.14); its expression s_b is its low
//     connection (37.14 detail 4), so its type is logic [5:0] and its size
//     is 6. p_b names a port only and declares nothing
//     ("port identifiers exist in their own name space", 23.2.2.2).
//   - p_c: input, a wire net (the default net type, 22.8) of type logic, 1
//     bit; it does not inherit integer from p_a.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors       no fatal, syntax or error diagnostic
//   PortsInDeclarationOrder     p_a, p_b, p_c with getPortIndex() 0, 1, 2
//   PortAIsAnIntegerNet         net, integer typespec, getBitCount() 32
//   PortBTakesTheTypeOfSB       getExplicitName(), getLowConn() resolves to
//                               the variable s_b, the port's typespec is 6
//                               bits wide, and no net or variable is named p_b
//   PortCIsALogicNet            net, logic typespec, getBitCount() 1
//
// The packed width of an object or typespec (37.14 and 37.17) is the bit
// count hldb::ExprEval::getBitCount() computes for it.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Instances and connections; they are 23.3.
//   - The direction input of the three ports and the net type wire of p_a
//     and p_c. HLDB gives them only as the integer codes returned by
//     Port::getDirection() and Net::getNetType() and has no named values of
//     its own for them, so they can be checked once HLDB provides such names.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/ExprEval.h>
#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/typespec.h>
#include <hldb/variable.h>

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

// The packed width in bits of an object or typespec, as HLDB's
// expression evaluator computes it, or -1 when it cannot.
int64_t widthOf(const hldb::Any *object) {
  const hldb::ExprEval evaluator(nullptr, true);
  uint64_t bits = 0;
  if (!evaluator.getBitCount(object, object, true, &bits, true)) return -1;
  return static_cast<int64_t>(bits);
}

// The object a port's low connection refers to, or nullptr.
const hldb::Any *lowConnActual(const hldb::Port *port) { return hldb::getActual(port->getLowConn()); }

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

  // Asserts that the port at 'index' declares a net of the given typespec
  // type and width.
  static void expectNet(size_t index, hldb::AnyType dataType, int64_t size) {
    const hldb::Port *const port = findPort(index);
    ASSERT_NE(port, nullptr);
    const hldb::Any *const declared = lowConnActual(port);
    ASSERT_NE(declared, nullptr);
    const hldb::Net *const net = any_cast<hldb::Net>(declared);
    ASSERT_NE(net, nullptr) << "not a net";
    const hldb::Typespec *const typespec = hldb::getTypespec(net);
    ASSERT_NE(typespec, nullptr);
    EXPECT_EQ(hldb::AnyTypeName(typespec->getAnyType()), hldb::AnyTypeName(dataType));
    EXPECT_EQ(widthOf(net), size);
  }
};

TEST_F(PortRulesMh22Test, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(PortRulesMh22Test, PortsInDeclarationOrder) {
  const std::vector<std::string_view> names = {"p_a", "p_b", "p_c"};
  for (size_t index = 0; index < names.size(); ++index) {
    const hldb::Port *const port = findPort(index);
    ASSERT_NE(port, nullptr) << index;
    EXPECT_EQ(port->getName(), names[index]);
    EXPECT_EQ(port->getPortIndex(), static_cast<int32_t>(index));
  }
}

TEST_F(PortRulesMh22Test, PortAIsAnIntegerNet) { expectNet(0, hldb::AnyType::IntegerTypespec, 32); }

TEST_F(PortRulesMh22Test, PortBTakesTheTypeOfSB) {
  const hldb::Module *const module = findModule();
  const hldb::Port *const port = findPort(1);
  ASSERT_NE(module, nullptr);
  ASSERT_NE(port, nullptr);
  EXPECT_TRUE(port->getExplicitName());

  const hldb::Any *const declared = lowConnActual(port);
  ASSERT_NE(declared, nullptr);
  const hldb::Variable *const variable = any_cast<hldb::Variable>(declared);
  ASSERT_NE(variable, nullptr);
  EXPECT_EQ(variable->getName(), "s_b");

  const hldb::Typespec *const portTypespec = hldb::getTypespec(port);
  ASSERT_NE(portTypespec, nullptr) << "port p_b has no typespec";
  EXPECT_EQ(widthOf(portTypespec), 6);
  EXPECT_FALSE(hasObjectNamed(module->getNets(), "p_b"));
  EXPECT_FALSE(hasObjectNamed(module->getVariables(), "p_b"));
}

TEST_F(PortRulesMh22Test, PortCIsALogicNet) { expectNet(2, hldb::AnyType::LogicTypespec, 1); }
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
