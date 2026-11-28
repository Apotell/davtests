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

// Test for tests/Google/chapter-23/23.2.3--parameter-port-type-parameter.sv
//
// THE SOURCE
//
//   19: module m #(parameter type T = logic [3:0]) (input T a);
//   20: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// Syntax 23-1: "parameter_port_declaration ::= ... |
// type_parameter_declaration". "Parameter types can be redefined for each
// instance of a module" (23.2.3). T is a type parameter (an
// hldb::TypeParameter object) whose default is logic [3:0], and "Port declarations
// can be based on parameter declarations" (23.2.3): the data type of port a is
// T.
//   - The module is named m (23.2).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error diagnostic
//   ModuleIsNamedM             getDefName() m
//   TIsATypeParameter          the module's parameter T is an
//                              hldb::TypeParameter
//   PortAHasTypeT              the typespec of a's net is T
//   PortAIsAnInput             Port::getDirection() of a is vpiInput
//   PortAIsAWire               a is a net with Net::getNetType() vpiWire
//   PortAWithTheDefaultT       elaborated model only: with the default T, a
//                              is logic, unsigned and 4 bits wide
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Redefining T per instance (23.10); there is no instance.

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
#include <hldb/type_parameter.h>
#include <hldb/typespec.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kModuleName = "m";

// The net or variable a port's low connection refers to, or nullptr.
const hldb::Any *lowConnActual(const hldb::Port *port) { return hldb::getActual(port->getLowConn()); }

// The packed width in bits of a net or variable (37.16, 37.17), as HLDB's expression evaluator computes it
// from the object's typespec, or -1 when it cannot.
int64_t widthOf(const hldb::Any *object) {
  if (object == nullptr) return -1;
  const hldb::ExprEval evaluator(nullptr, true);
  uint64_t bits = 0;
  if (!evaluator.getBitCount(object, object, true, &bits, true)) return -1;
  return static_cast<int64_t>(bits);
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

class ParameterPortTypeParameterTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "parameter_port_type_parameter.hlc"}); }
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

  // The definition kModuleName, or nullptr.
  static const hldb::Module *findModule() {
    if (m_design == nullptr) return nullptr;
    return hldb::findByDefName<hldb::Module>(kModuleName, m_design->getAllModules());
  }

  // The parameter (value or type) named 'name' of the module, or nullptr.
  static const hldb::Any *findParameter(std::string_view name) {
    const hldb::Module *const module = findModule();
    return (module == nullptr) ? nullptr : hldb::findByName<hldb::Any>(name, module->getParameters());
  }

  // The port named 'name' of the module, or nullptr.
  static const hldb::Port *findPort(std::string_view name) {
    const hldb::Module *const module = findModule();
    return (module == nullptr) ? nullptr : hldb::findByName<hldb::Port>(name, module->getPorts());
  }

  // The port named 'name' of the top-level instance of kModuleName in the elaborated model, or nullptr.
  static const hldb::Port *findElaboratedPort(std::string_view name) {
    if (m_design == nullptr) return nullptr;
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>(kModuleName, m_design->getTopModules());
    return (top == nullptr) ? nullptr : hldb::findByName<hldb::Port>(name, top->getPorts());
  }
};

TEST_F(ParameterPortTypeParameterTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(ParameterPortTypeParameterTest, ModuleIsNamedM) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getDefName(), std::string_view("m"));
}

TEST_F(ParameterPortTypeParameterTest, TIsATypeParameter) {
  const hldb::Any *const parameter = findParameter("T");
  ASSERT_NE(parameter, nullptr);
  EXPECT_NE(any_cast<hldb::TypeParameter>(parameter), nullptr) << hldb::AnyTypeName(parameter->getAnyType());
}

TEST_F(ParameterPortTypeParameterTest, PortAHasTypeT) {
  const hldb::Port *const port = findPort("a");
  ASSERT_NE(port, nullptr);
  const hldb::Any *const declared = lowConnActual(port);
  ASSERT_NE(declared, nullptr);
  const hldb::Any *const typespec = hldb::getTypespec(declared);
  ASSERT_NE(typespec, nullptr);
  EXPECT_EQ(typespec, findParameter("T"));
}

TEST_F(ParameterPortTypeParameterTest, PortAIsAnInput) {
  const hldb::Port *const port = findPort("a");
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getDirection(), vpiInput);
}

// a is an input with no port kind, so it is a net of the default net type, wire (23.2.2.3).
TEST_F(ParameterPortTypeParameterTest, PortAIsAWire) {
  const hldb::Port *const port = findPort("a");
  ASSERT_NE(port, nullptr);
  const hldb::Net *const net = any_cast<hldb::Net>(lowConnActual(port));
  ASSERT_NE(net, nullptr) << "a is not a net";
  EXPECT_EQ(net->getNetType(), vpiWire);
}

// "Parameter types can be redefined for each instance of a module" (23.2.3), so the data type a ends up with is
// an instance-model fact. Elaborated model only: with the default T, a is logic [3:0].
TEST_F(ParameterPortTypeParameterTest, PortAWithTheDefaultT) {
  ASSERT_NE(m_design, nullptr);
  if (m_design->getElaborated()) {
    const hldb::Port *const port = findElaboratedPort("a");
    ASSERT_NE(port, nullptr);
    const hldb::Any *const declared = lowConnActual(port);
    ASSERT_NE(declared, nullptr);
    const hldb::Typespec *const typespec = hldb::getTypespec(declared);
    ASSERT_NE(typespec, nullptr);
    EXPECT_EQ(hldb::AnyTypeName(typespec->getAnyType()), hldb::AnyTypeName(hldb::AnyType::LogicTypespec));
    EXPECT_FALSE(hldb::getSigned(typespec));
    EXPECT_EQ(widthOf(declared), 4);
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
