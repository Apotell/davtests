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

// Test for tests/Google/chapter-23/23.2.3--parameterized-non-ansi.sv
//
// THE SOURCE
//
//   20: module generic_fifo(clk, read, write, reset, in, out, full, empty);
//   21:   parameter MSB=3, LSB=0, DEPTH=4;
//   22:   input [MSB:LSB] in;
//   23:   input clk, read, write, reset;
//   24:   output [MSB:LSB] out;
//   25:   output full, empty;
//   26:   wire [MSB:LSB] in;
//   27:   wire clk, read, write, reset;
//   28:   logic [MSB:LSB] out;
//   29:   logic full, empty;
//   30: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "Port declarations can be based on parameter declarations." (23.2.3). This
// is Example 1 of 23.2.3, "Parameterized module declaration using non-ANSI
// style module header", with its "..." removed and in added to the list of
// ports: the printed list omits in although the example declares in as an
// input port, and every port declaration must belong to a port of the list
// (23.2.2.1).
//   - "The identifier following the keyword module shall be the name of the
//     module being defined." (23.2): generic_fifo.
//   - MSB, LSB and DEPTH are parameters ("these parameters can be
//     redefined"), not local parameters, with the values 3, 0 and 4; a
//     parameter's value expression is its vpiExpr (37.28 detail 3).
//   - in and out are declared [MSB:LSB]: their packed range is built from the
//     parameters MSB and LSB.
//   - clk, read, write, reset and in are inputs declared as wire nets; out,
//     full and empty are outputs declared as logic variables (23.2.2.1).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors       no fatal, syntax or error diagnostic
//   ModuleIsNamedGenericFifo    vpiName generic_fifo
//   ParametersMsbLsbDepth       three parameters, not local, values 3, 0, 4
//   PortsInDeclarationOrder     clk, read, write, reset, in, out, full, empty
//                               with their directions
//   PortKindsAreAsDeclared      nets for the inputs, variables for the outputs
//   InAndOutAreSizedByMsbAndLsb the range of in and of out is [MSB:LSB]
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Redefining the parameters per instance. That is 23.10 and needs
//     instances.
//   - The widths in bits. They depend on the parameter values an instance
//     gives; this file has no instance.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.3--parameterized-non-ansi.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

int64_t integerProperty(const hldb::Any *object, int32_t property) {
  const hldb::Any::vpi_property_value_t value = object->getVpiPropertyValue(property);
  if (const int64_t *const number = std::get_if<int64_t>(&value)) return *number;
  return vpiUndefined;
}

std::string stringProperty(const hldb::Any *object, int32_t property) {
  const hldb::Any::vpi_property_value_t value = object->getVpiPropertyValue(property);
  if (const char *const *const text = std::get_if<const char *>(&value)) {
    return (*text == nullptr) ? std::string("<null>") : std::string(*text);
  }
  return "<not a string>";
}

const hldb::Any *relatedObject(const hldb::Any *object, int32_t relation) {
  const hldb::Any::get_by_vpi_type_return_t result = object->getByVpiType(relation);
  if (const hldb::Any *const single = std::get<1>(result)) return single;
  const std::vector<const hldb::Any *> *const objects = std::get<2>(result);
  return ((objects != nullptr) && (objects->size() == 1)) ? objects->front() : nullptr;
}

// The typespec of an object, seen through HLDB's reference typespec.
const hldb::Any *typespecOf(const hldb::Any *object) {
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

// True when the expression is, or contains as an operand, a reference that
// resolves to 'target'.
bool refersTo(const hldb::Any *expression, const hldb::Any *target) {
  if ((expression == nullptr) || (target == nullptr)) return false;
  if (const hldb::RefObj *const reference = any_cast<hldb::RefObj>(expression)) {
    return reference->getActual() == target;
  }
  if (const hldb::Operation *const operation = any_cast<hldb::Operation>(expression)) {
    if (operation->getOperands() == nullptr) return false;
    for (const hldb::Any *operand : *operation->getOperands()) {
      if (refersTo(operand, target)) return true;
    }
  }
  return false;
}

// The first packed range of a logic typespec, or nullptr.
const hldb::Range *firstRange(const hldb::Any *typespec) {
  const hldb::LogicTypespec *const logic = any_cast<hldb::LogicTypespec>(typespec);
  if ((logic == nullptr) || (logic->getRanges() == nullptr) || logic->getRanges()->empty()) return nullptr;
  return logic->getRanges()->front();
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

class ParameterizedNonAnsiTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "parameterized_non_ansi.hlc"}); }
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

  // The only module defined in the file under test, or nullptr.
  static const hldb::Module *findModule() {
    if ((m_design == nullptr) || (m_design->getAllModules() == nullptr)) return nullptr;
    for (const hldb::Module *module : *m_design->getAllModules()) {
      if (endsWith(module->getFile(), kSourceFileName)) return module;
    }
    return nullptr;
  }

  // The parameter (value or type) named 'name' of the module, or nullptr.
  static const hldb::Any *findParameter(std::string_view name) {
    const hldb::Module *const module = findModule();
    if ((module == nullptr) || (module->getParameters() == nullptr)) return nullptr;
    for (const hldb::Any *parameter : *module->getParameters()) {
      if (parameter->getName() == name) return parameter;
    }
    return nullptr;
  }

  // The port named 'name' of the module, or nullptr.
  static const hldb::Port *findPort(std::string_view name) {
    const hldb::Module *const module = findModule();
    if ((module == nullptr) || (module->getPorts() == nullptr)) return nullptr;
    for (const hldb::Port *port : *module->getPorts()) {
      if (port->getName() == name) return port;
    }
    return nullptr;
  }
};

TEST_F(ParameterizedNonAnsiTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(ParameterizedNonAnsiTest, ModuleIsNamedGenericFifo) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getName(), "generic_fifo");
  EXPECT_EQ(stringProperty(module, vpiName), "generic_fifo");
}

TEST_F(ParameterizedNonAnsiTest, ParametersMsbLsbDepth) {
  const std::vector<std::pair<std::string_view, std::string_view>> expected = {
      {"MSB", "3"}, {"LSB", "0"}, {"DEPTH", "4"}};
  for (const std::pair<std::string_view, std::string_view> &entry : expected) {
    const hldb::Parameter *const parameter = any_cast<hldb::Parameter>(findParameter(entry.first));
    ASSERT_NE(parameter, nullptr) << entry.first;
    EXPECT_EQ(integerProperty(parameter, vpiLocalParam), 0) << entry.first;
    const hldb::Any *const value = relatedObject(parameter, vpiExpr);
    ASSERT_NE(value, nullptr) << entry.first;
    EXPECT_EQ(stringProperty(value, vpiDecompile), entry.second) << entry.first;
  }
}

TEST_F(ParameterizedNonAnsiTest, PortsInDeclarationOrder) {
  const std::vector<std::pair<std::string_view, int64_t>> expected = {
      {"clk", vpiInput}, {"read", vpiInput}, {"write", vpiInput}, {"reset", vpiInput},
      {"in", vpiInput},  {"out", vpiOutput}, {"full", vpiOutput}, {"empty", vpiOutput}};
  for (size_t index = 0; index < expected.size(); ++index) {
    const hldb::Port *const port = findPort(expected[index].first);
    ASSERT_NE(port, nullptr) << expected[index].first;
    EXPECT_EQ(integerProperty(port, vpiPortIndex), static_cast<int64_t>(index)) << expected[index].first;
    EXPECT_EQ(integerProperty(port, vpiDirection), expected[index].second) << expected[index].first;
  }
}

TEST_F(ParameterizedNonAnsiTest, PortKindsAreAsDeclared) {
  for (std::string_view name : {"clk", "read", "write", "reset", "in"}) {
    const hldb::Port *const port = findPort(name);
    ASSERT_NE(port, nullptr) << name;
    EXPECT_NE(any_cast<hldb::Net>(lowConnActual(port)), nullptr) << name << " is not a net";
  }
  for (std::string_view name : {"out", "full", "empty"}) {
    const hldb::Port *const port = findPort(name);
    ASSERT_NE(port, nullptr) << name;
    EXPECT_NE(any_cast<hldb::Variable>(lowConnActual(port)), nullptr) << name << " is not a variable";
  }
}

TEST_F(ParameterizedNonAnsiTest, InAndOutAreSizedByMsbAndLsb) {
  const hldb::Any *const msb = findParameter("MSB");
  const hldb::Any *const lsb = findParameter("LSB");
  ASSERT_NE(msb, nullptr);
  ASSERT_NE(lsb, nullptr);
  for (std::string_view name : {"in", "out"}) {
    const hldb::Port *const port = findPort(name);
    ASSERT_NE(port, nullptr) << name;
    const hldb::Any *const declared = lowConnActual(port);
    ASSERT_NE(declared, nullptr) << name;
    const hldb::Range *const range = firstRange(typespecOf(declared));
    ASSERT_NE(range, nullptr) << name;
    EXPECT_TRUE(refersTo(range->getLeftExpr(), msb)) << name;
    EXPECT_TRUE(refersTo(range->getRightExpr(), lsb)) << name;
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
