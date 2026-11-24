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

// Test for tests/Google/chapter-23/23.2.3--parameterized-ansi.sv
//
// THE SOURCE
//
//   19: module generic_fifo
//   20:   #(parameter MSB=3, LSB=0, DEPTH=4)
//   21:   (input wire [MSB:LSB] in, reset,
//   22:    input wire [MSB:LSB] clk, read, write,
//   23:    output logic out,
//   24:    output logic full, empty);
//   25: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "Port declarations can be based on parameter declarations." (23.2.3). This
// is Example 2 of 23.2.3, "Parameterized module declaration using ANSI style
// module header", with its "..." removed.
//   - The module is named generic_fifo (23.2); the parameter port list does
//     not change the name.
//   - MSB, LSB and DEPTH are parameters, not local, with the values 3, 0 and
//     4 (the parameter's expression, 37.28 detail 3).
//   - reset has no direction, kind or type of its own, so it inherits "input
//     wire [MSB:LSB]" from in (23.2.2.3); read and write inherit the same
//     from clk. So in, reset, clk, read and write are input wire nets whose
//     packed range is [MSB:LSB].
//   - out, full and empty are "output logic": an output with an explicit data
//     type is a variable (23.2.2.3), and logic without a range is 1 bit.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors       no fatal, syntax or error diagnostic
//   ModuleIsNamedGenericFifo    getName() generic_fifo
//   ParametersMsbLsbDepth       three parameters, not local, values 3, 0, 4
//   PortsInDeclarationOrder     in, reset, clk, read, write, out, full, empty
//                               with getPortIndex() 0 to 7
//   InputsAreNetsSizedByMsbAndLsb   the five inputs are nets with range
//                               [MSB:LSB]
//   OutputsAreScalarVariables   out, full, empty are variables with no range
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Redefining the parameters per instance (23.10); there is no instance.
//   - The port directions (input, output) and the net type wire of the
//     inputs. HLDB gives them only as the integer codes returned by
//     Port::getDirection() and Net::getNetType() and has no named values of
//     its own for them, so they can be checked once HLDB provides such names.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/constant.h>
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
#include <hldb/typespec.h>
#include <hldb/variable.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.3--parameterized-ansi.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

// The source text of a parameter's value (Parameter::getExpr()) when it is a
// constant, or a placeholder that says what it is instead.
std::string valueText(const hldb::Parameter *parameter) {
  const hldb::Expr *const value = parameter->getExpr();
  if (value == nullptr) return "<no value>";
  const hldb::Constant *const constant = any_cast<hldb::Constant>(value);
  if (constant == nullptr) return "<" + std::string(hldb::AnyTypeName(value->getAnyType())) + ">";
  return std::string(constant->getDecompile());
}

// The net or variable a port's low connection refers to, or nullptr.
const hldb::Any *lowConnActual(const hldb::Port *port) { return hldb::getActual(port->getLowConn()); }

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

class ParameterizedAnsiTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "parameterized_ansi.hlc"}); }
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

TEST_F(ParameterizedAnsiTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(ParameterizedAnsiTest, ModuleIsNamedGenericFifo) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getName(), "generic_fifo");
}

TEST_F(ParameterizedAnsiTest, ParametersMsbLsbDepth) {
  const std::vector<std::pair<std::string_view, std::string_view>> expected = {
      {"MSB", "3"}, {"LSB", "0"}, {"DEPTH", "4"}};
  for (const std::pair<std::string_view, std::string_view> &entry : expected) {
    const hldb::Parameter *const parameter = any_cast<hldb::Parameter>(findParameter(entry.first));
    ASSERT_NE(parameter, nullptr) << entry.first;
    EXPECT_FALSE(parameter->getLocalParam()) << entry.first;
    EXPECT_EQ(valueText(parameter), entry.second) << entry.first;
  }
}

TEST_F(ParameterizedAnsiTest, PortsInDeclarationOrder) {
  const std::vector<std::string_view> expected = {"in", "reset", "clk", "read", "write", "out", "full", "empty"};
  for (size_t index = 0; index < expected.size(); ++index) {
    const hldb::Port *const port = findPort(expected[index]);
    ASSERT_NE(port, nullptr) << expected[index];
    EXPECT_EQ(port->getPortIndex(), static_cast<int32_t>(index)) << expected[index];
  }
}

TEST_F(ParameterizedAnsiTest, InputsAreNetsSizedByMsbAndLsb) {
  const hldb::Any *const msb = findParameter("MSB");
  const hldb::Any *const lsb = findParameter("LSB");
  ASSERT_NE(msb, nullptr);
  ASSERT_NE(lsb, nullptr);
  for (std::string_view name : {"in", "reset", "clk", "read", "write"}) {
    const hldb::Port *const port = findPort(name);
    ASSERT_NE(port, nullptr) << name;
    const hldb::Any *const declared = lowConnActual(port);
    ASSERT_NE(declared, nullptr) << name;
    EXPECT_NE(any_cast<hldb::Net>(declared), nullptr) << name << " is not a net";
    const hldb::Range *const range = firstRange(hldb::getTypespec(declared));
    ASSERT_NE(range, nullptr) << name;
    EXPECT_TRUE(refersTo(range->getLeftExpr(), msb)) << name;
    EXPECT_TRUE(refersTo(range->getRightExpr(), lsb)) << name;
  }
}

TEST_F(ParameterizedAnsiTest, OutputsAreScalarVariables) {
  for (std::string_view name : {"out", "full", "empty"}) {
    const hldb::Port *const port = findPort(name);
    ASSERT_NE(port, nullptr) << name;
    const hldb::Any *const declared = lowConnActual(port);
    ASSERT_NE(declared, nullptr) << name;
    EXPECT_NE(any_cast<hldb::Variable>(declared), nullptr) << name << " is not a variable";
    const hldb::Typespec *const typespec = hldb::getTypespec(declared);
    ASSERT_NE(typespec, nullptr) << name;
    EXPECT_EQ(hldb::AnyTypeName(typespec->getAnyType()), hldb::AnyTypeName(hldb::AnyType::LogicTypespec)) << name;
    EXPECT_EQ(firstRange(typespec), nullptr) << name;
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
