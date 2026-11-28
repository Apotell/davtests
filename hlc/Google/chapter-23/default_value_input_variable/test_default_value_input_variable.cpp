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

// Test for tests/Google/chapter-23/23.2.2.4--default-value-input-variable.sv
//
// THE SOURCE
//
//   20: module m(input var logic [7:0] d = 8'h5A);
//   21: endmodule
//   22:
//   23: module top;
//   24:   m u();
//   25: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// The informal syntax of 23.2.2.4 is "[ input ] [ type ] port_identifier =
// constant_expression", and Syntax 23-4 allows "[ variable_port_header ]
// port_identifier { variable_dimension } [ = constant_expression ]". So d,
// an input port of the variable kind (var, 23.2.2.3) with the constant
// default 8'h5A, is legal, and so is the instance u that omits it: "When the
// module is instantiated, input ports with default values can be omitted from
// the instantiation, and the compiler shall insert the corresponding default
// values." (23.2.2.4).
//
// THE MODEL
//
// HLC's graph is a definition model: it must hold everything the source
// says, so that an instance model can be built from it. For this source that
// means the definition m knows its port d, d's direction, and d's default
// 8'h5A, and the instance u of top is bound to m with no connections. The
// default is kept on the variable d declares (Variable::getValue()). Inserting
// the default into u.d is an instance-model step; that check is its own test,
// skipped until HLC builds an elaborated model (m_design->getElaborated()).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors         no fatal, syntax or error diagnostic
//   PortDIsAVariable              m has one port d, whose low connection
//                                 (Port::getLowConn()) resolves to a variable
//   PortDIsAnInput                Port::getDirection() of d is vpiInput
//   PortDIsUnsigned               getSigned() of the data type of d
//                                 (logic [7:0]) is false
//   PortDKnowsItsDefault          Variable::getValue() of d is the constant
//                                 8'h5A
//   InstanceUOmitsThePort         top has the instance u of m with no port
//                                 connections
//   InstanceUReceivesTheDefault   elaborated model only: u.d is connected to
//                                 the inserted default 8'h5A
//
// SKIPPED (GTEST_SKIP; the real assertion stays below the skip)
//
//   InstanceUReceivesTheDefault: HLC builds a definition model only, so there
//   is no instance u whose port d could be connected to the default.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The type of the constant 8'h5A itself; 5.7.1 is not under test here.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace hlc {
namespace {
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

class DefaultValueInputVariableTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "default_value_input_variable.hlc"}); }
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

  // The definition named 'name', or nullptr.
  static const hldb::Module *findDefinition(std::string_view name) {
    if (m_design == nullptr) return nullptr;
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  // The single port of the definition m, or nullptr.
  static const hldb::Port *findPortD() {
    const hldb::Module *const module = findDefinition("m");
    if ((module == nullptr) || (module->getPorts() == nullptr) || (module->getPorts()->size() != 1)) return nullptr;
    return module->getPorts()->front();
  }

  // The variable port d declares, or nullptr.
  static const hldb::Variable *findVariableD() {
    const hldb::Port *const port = findPortD();
    return (port == nullptr) ? nullptr : any_cast<hldb::Variable>(hldb::getActual(port->getLowConn()));
  }
};

TEST_F(DefaultValueInputVariableTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(DefaultValueInputVariableTest, PortDIsAVariable) {
  const hldb::Port *const port = findPortD();
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), std::string_view("d"));
  ASSERT_NE(port->getLowConn(), nullptr);
  const hldb::RefObj *const lowConn = any_cast<hldb::RefObj>(port->getLowConn());
  ASSERT_NE(lowConn, nullptr);
  EXPECT_NE(lowConn->getActual<hldb::Variable>(), nullptr);
}

TEST_F(DefaultValueInputVariableTest, PortDIsAnInput) {
  const hldb::Port *const port = findPortD();
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getDirection(), vpiInput);
}

// logic [7:0] is unsigned (6.11.3).
TEST_F(DefaultValueInputVariableTest, PortDIsUnsigned) {
  const hldb::Variable *const variable = findVariableD();
  ASSERT_NE(variable, nullptr);
  const hldb::Typespec *const typespec = hldb::getTypespec(variable);
  ASSERT_NE(typespec, nullptr);
  EXPECT_FALSE(hldb::getSigned(typespec));
}

// "A module declaration may specify a default value for each singular input port." (23.2.2.4)
TEST_F(DefaultValueInputVariableTest, PortDKnowsItsDefault) {
  const hldb::Variable *const variable = findVariableD();
  ASSERT_NE(variable, nullptr);
  const hldb::Constant *const value = variable->getValue<hldb::Constant>();
  ASSERT_NE(value, nullptr) << "the default 8'h5A of d is missing";
  EXPECT_EQ(value->getDecompile(), std::string_view("8'h5A"));
}

// "input ports with default values can be omitted from the instantiation" (23.2.2.4)
TEST_F(DefaultValueInputVariableTest, InstanceUOmitsThePort) {
  const hldb::Module *const top = findDefinition("top");
  ASSERT_NE(top, nullptr);
  const hldb::RefInstance *const instance = hldb::findByName<hldb::RefInstance>("u", top->getRefInstances());
  ASSERT_NE(instance, nullptr);
  ASSERT_NE(instance->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const definition = instance->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(definition, nullptr);
  EXPECT_EQ(definition->getDefName(), std::string_view("m"));
  EXPECT_TRUE((instance->getArguments() == nullptr) || instance->getArguments()->empty()) << "u connects no port";
}

// "the compiler shall insert the corresponding default values." (23.2.2.4) In the instance model, u.d is
// connected to the default.
TEST_F(DefaultValueInputVariableTest, InstanceUReceivesTheDefault) {
  ASSERT_NE(m_design, nullptr);
  if (!m_design->getElaborated()) {
    GTEST_SKIP() << "HLC builds a definition model only, with no instance u whose port d could be connected to "
                    "the default (IEEE 1800-2023 23.2.2.4: the compiler shall insert the default value of an "
                    "omitted input port)";
  }
  const hldb::Module *const top = findDefinition("top");
  ASSERT_NE(top, nullptr);
  const hldb::Module *const u = hldb::findByName<hldb::Module>("u", top->getModules());
  ASSERT_NE(u, nullptr);
  const hldb::Port *const port = hldb::findByName<hldb::Port>("d", u->getPorts());
  ASSERT_NE(port, nullptr);
  const hldb::Constant *const highConn = any_cast<hldb::Constant>(port->getHighConn());
  ASSERT_NE(highConn, nullptr) << "u.d is not connected to the inserted default";
  EXPECT_EQ(highConn->getDecompile(), std::string_view("8'h5A"));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
