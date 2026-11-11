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

// Test for tests/Google/chapter-23/chapter-23_testsuit-1/23.2.2.1--non-ansi-port-directions.sv
//
// THE SOURCE
//
//   20: module m(a, b, c, d);
//   21:   input a;
//   22:   output b;
//   23:   inout c;
//   24:   ref int d;
//   25: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// Each port_identifier of the list of ports "shall also be declared in the
// body of the module as one of the following port declarations: input,
// output, inout (bidirectional), ref, or as an interface port" (23.2.2.1),
// and Syntax 23-3 gives "ref_declaration ::= ref variable_port_type
// list_of_variable_identifiers". So:
//   - a, b and c have the directions input, output and inout. None of them
//     has a net or variable declaration, so each is a net: "Nets connected
//     to ports without an explicit net declaration ..." (23.2.2.1).
//   - d has the direction ref (vpiRef, sv_vpi_user.h) and is an int
//     variable; "A ref port is always a variable." (23.2.2.3).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors       no fatal, syntax or error diagnostic
//   DirectionsAreAsDeclared     vpiInput, vpiOutput, vpiInout, vpiRef
//   InputOutputInoutAreNets     the vpiLowConn of a, b and c resolve to nets
//   RefPortDIsAVariable         the vpiLowConn of d resolves to a variable
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The interface port declaration. 23.2.2.1--non-ansi-interface-port.sv
//     covers it, since an interface port must be connected (23.3.3.4).

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
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.2.1--non-ansi-port-directions.sv";

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

// The object a port's low connection refers to, or nullptr.
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

class NonAnsiPortDirectionsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "non_ansi_port_directions.hlc"}); }
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

  static const hldb::Port *findPort(std::string_view name) {
    if ((m_design == nullptr) || (m_design->getAllModules() == nullptr)) return nullptr;
    for (const hldb::Module *module : *m_design->getAllModules()) {
      if (endsWith(module->getFile(), kSourceFileName)) return findPortByName(module->getPorts(), name);
    }
    return nullptr;
  }
};

TEST_F(NonAnsiPortDirectionsTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(NonAnsiPortDirectionsTest, DirectionsAreAsDeclared) {
  const std::vector<std::pair<std::string_view, int64_t>> expected = {
      {"a", vpiInput}, {"b", vpiOutput}, {"c", vpiInout}, {"d", vpiRef}};
  for (const std::pair<std::string_view, int64_t> &entry : expected) {
    const hldb::Port *const port = findPort(entry.first);
    ASSERT_NE(port, nullptr) << entry.first;
    EXPECT_EQ(integerProperty(port, vpiDirection), entry.second) << entry.first;
  }
}

TEST_F(NonAnsiPortDirectionsTest, InputOutputInoutAreNets) {
  for (std::string_view name : {"a", "b", "c"}) {
    const hldb::Port *const port = findPort(name);
    ASSERT_NE(port, nullptr) << name;
    const hldb::Any *const actual = lowConnActual(port);
    ASSERT_NE(actual, nullptr) << name;
    EXPECT_NE(any_cast<hldb::Net>(actual), nullptr) << name;
  }
}

TEST_F(NonAnsiPortDirectionsTest, RefPortDIsAVariable) {
  const hldb::Port *const port = findPort("d");
  ASSERT_NE(port, nullptr);
  const hldb::Any *const actual = lowConnActual(port);
  ASSERT_NE(actual, nullptr);
  EXPECT_NE(any_cast<hldb::Variable>(actual), nullptr);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
