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

// Test for tests/Google/chapter-23/23.2.3--parameter-port-list-empty.sv
//
// THE SOURCE
//
//   19: module m #() (input a);
//   20: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// Syntax 23-1 (23.2.3 refers to it for parameterized headers):
// "parameter_port_list ::= # ( list_of_param_assignments { ,
// parameter_port_declaration } ) | # ( parameter_port_declaration { ,
// parameter_port_declaration } ) | #( )". The last alternative is an empty
// parameter port list, so module m (23.2) declares no parameter, and its one
// port a is an input (23.2.2.2).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error diagnostic
//   ModuleIsNamedM             getDefName() m
//   NoParameters               the module has no parameter
//   PortAIsTheOnlyPort         one port, named a
//   PortAIsAnInput             Port::getDirection() of a is vpiInput
//   PortAIsAWire               a is a net with Net::getNetType() vpiWire
//   PortAIsUnsigned            getSigned() of a's data type is false
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Instantiating m with #(). That is 23.10.2.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/ref_typespec.h>
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

class ParameterPortListEmptyTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "parameter_port_list_empty.hlc"}); }
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
};

TEST_F(ParameterPortListEmptyTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(ParameterPortListEmptyTest, ModuleIsNamedM) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getDefName(), std::string_view("m"));
}

TEST_F(ParameterPortListEmptyTest, NoParameters) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  const size_t count = (module->getParameters() == nullptr) ? 0 : module->getParameters()->size();
  EXPECT_EQ(count, 0u);
}

TEST_F(ParameterPortListEmptyTest, PortAIsTheOnlyPort) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  ASSERT_NE(module->getPorts(), nullptr);
  ASSERT_EQ(module->getPorts()->size(), 1u);
  EXPECT_NE(findPort("a"), nullptr);
}

TEST_F(ParameterPortListEmptyTest, PortAIsAnInput) {
  const hldb::Port *const port = findPort("a");
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getDirection(), vpiInput);
}

// a has no port kind and no data type, so it is a net of the default net type, wire (23.2.2.3).
TEST_F(ParameterPortListEmptyTest, PortAIsAWire) {
  const hldb::Port *const port = findPort("a");
  ASSERT_NE(port, nullptr);
  const hldb::Net *const net = any_cast<hldb::Net>(lowConnActual(port));
  ASSERT_NE(net, nullptr) << "a is not a net";
  EXPECT_EQ(net->getNetType(), vpiWire);
}

// The data type of a defaults to logic, which is unsigned (23.2.2.3, 6.11.3).
TEST_F(ParameterPortListEmptyTest, PortAIsUnsigned) {
  const hldb::Port *const port = findPort("a");
  ASSERT_NE(port, nullptr);
  const hldb::Any *const declared = lowConnActual(port);
  ASSERT_NE(declared, nullptr);
  const hldb::Typespec *const typespec = hldb::getTypespec(declared);
  ASSERT_NE(typespec, nullptr);
  EXPECT_FALSE(hldb::getSigned(typespec));
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
