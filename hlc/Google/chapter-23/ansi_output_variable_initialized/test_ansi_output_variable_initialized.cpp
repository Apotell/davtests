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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.2--ansi-output-variable-initialized.sv
//
// THE SOURCE
//
//   20: module m(output var logic x = 1'b1);
//   21: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "ansi_port_declaration ::= ... | [ variable_port_header ] port_identifier
// { variable_dimension } [ = constant_expression ]" (Syntax 23-4), and its
// footnote 2: "It shall be illegal to initialize a port that is not a
// variable output port". x is declared with the keyword var, so it is a
// variable output port, and its initializer "= 1'b1" is legal.
// 23.2.2.2--ansi-output-net-initialized.sv is the illegal net counterpart.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error diagnostic
//   PortXIsAnOutputVariable    one port x, vpiOutput, whose vpiLowConn
//                              resolves to a variable
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The initial value itself. Variable initialization is 6.8, not the
//     port declaration.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.2.2--ansi-output-variable-initialized.sv";

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

class AnsiOutputVariableInitializedTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ansi_output_variable_initialized.hlc"}); }
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
};

TEST_F(AnsiOutputVariableInitializedTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(AnsiOutputVariableInitializedTest, PortXIsAnOutputVariable) {
  ASSERT_NE(m_design, nullptr);
  ASSERT_NE(m_design->getAllModules(), nullptr);
  const hldb::Port *port = nullptr;
  for (const hldb::Module *module : *m_design->getAllModules()) {
    if (endsWith(module->getFile(), kSourceFileName) && (module->getPorts() != nullptr) &&
        (module->getPorts()->size() == 1)) {
      port = module->getPorts()->front();
    }
  }
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), "x");
  EXPECT_EQ(integerProperty(port, vpiDirection), static_cast<int64_t>(vpiOutput));

  const hldb::Any *const connection = relatedObject(port, vpiLowConn);
  ASSERT_NE(connection, nullptr);
  const hldb::RefObj *const lowConn = any_cast<hldb::RefObj>(connection);
  ASSERT_NE(lowConn, nullptr);
  EXPECT_NE(lowConn->getActual<hldb::Variable>(), nullptr);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
