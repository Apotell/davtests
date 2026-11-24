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
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors         no fatal, syntax or error diagnostic
//   PortDIsAVariable              m has one port d, whose low connection
//                                 (Port::getLowConn()) resolves to a variable
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - That u.d is connected to the inserted default 8'h5A. Insertion happens
//     when the module is instantiated, so it needs an elaborated design,
//     which this test flow does not produce; it is deferred to the 23.3
//     batches.
//   - The direction input of d. HLDB gives it only as the integer code
//     returned by Port::getDirection() and has no named values of its own for
//     directions, so it can be checked once HLDB provides such names.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.2.4--default-value-input-variable.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
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

  // The single port of the definition m, or nullptr.
  static const hldb::Port *findPortD() {
    if ((m_design == nullptr) || (m_design->getAllModules() == nullptr)) return nullptr;
    for (const hldb::Module *module : *m_design->getAllModules()) {
      if (!endsWith(module->getFile(), kSourceFileName) || (module->getName() != "m")) continue;
      if ((module->getPorts() == nullptr) || (module->getPorts()->size() != 1)) return nullptr;
      return module->getPorts()->front();
    }
    return nullptr;
  }
};

TEST_F(DefaultValueInputVariableTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(DefaultValueInputVariableTest, PortDIsAVariable) {
  const hldb::Port *const port = findPortD();
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), "d");
  ASSERT_NE(port->getLowConn(), nullptr);
  const hldb::RefObj *const lowConn = any_cast<hldb::RefObj>(port->getLowConn());
  ASSERT_NE(lowConn, nullptr);
  EXPECT_NE(lowConn->getActual<hldb::Variable>(), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
