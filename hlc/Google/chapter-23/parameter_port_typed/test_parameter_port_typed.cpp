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

// Test for tests/Google/chapter-23/23.2.3--parameter-port-typed.sv
//
// THE SOURCE
//
//   20: module m #(int N = 4, logic [3:0] V = 4'hA) (input [N-1:0] a);
//   21: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// Syntax 23-1: "parameter_port_declaration ::= parameter_declaration |
// local_parameter_declaration | data_type list_of_param_assignments |
// type_parameter_declaration". N and V use the third alternative: a data type
// without the keyword parameter. Such a declaration is a parameter, not a
// local parameter (only local_parameter_declaration makes one).
//   - The module is named m (23.2).
//   - N has the type int and the value 4; V has the type logic [3:0] and the
//     value 4'hA (the parameter's expression, 37.28 detail 3).
//   - "Port declarations can be based on parameter declarations" (23.2.3):
//     a is an input whose range [N-1:0] is built from N.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error diagnostic
//   ModuleIsNamedM             getName() m
//   NIsAnIntParameter          not local, int typespec, value 4
//   VIsALogicParameter         not local, logic typespec, value 4'hA
//   PortAIsSizedByN            the left bound of a's range refers to N
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The evaluated width of a (4). It needs constant evaluation at
//     elaboration.
//   - The direction input of a. HLDB gives it only as the integer code
//     returned by Port::getDirection() and has no named values of its own for
//     directions, so it can be checked once HLDB provides such names.

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
#include <hldb/operation.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/typespec.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.3--parameter-port-typed.sv";

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

class ParameterPortTypedTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "parameter_port_typed.hlc"}); }
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

TEST_F(ParameterPortTypedTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(ParameterPortTypedTest, ModuleIsNamedM) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getName(), "m");
}

TEST_F(ParameterPortTypedTest, NIsAnIntParameter) {
  const hldb::Parameter *const parameter = any_cast<hldb::Parameter>(findParameter("N"));
  ASSERT_NE(parameter, nullptr);
  EXPECT_FALSE(parameter->getLocalParam());
  const hldb::Typespec *const typespec = hldb::getTypespec(parameter);
  ASSERT_NE(typespec, nullptr);
  EXPECT_EQ(hldb::AnyTypeName(typespec->getAnyType()), hldb::AnyTypeName(hldb::AnyType::IntTypespec));
  EXPECT_EQ(valueText(parameter), "4");
}

TEST_F(ParameterPortTypedTest, VIsALogicParameter) {
  const hldb::Parameter *const parameter = any_cast<hldb::Parameter>(findParameter("V"));
  ASSERT_NE(parameter, nullptr);
  EXPECT_FALSE(parameter->getLocalParam());
  const hldb::Typespec *const typespec = hldb::getTypespec(parameter);
  ASSERT_NE(typespec, nullptr);
  EXPECT_EQ(hldb::AnyTypeName(typespec->getAnyType()), hldb::AnyTypeName(hldb::AnyType::LogicTypespec));
  EXPECT_EQ(valueText(parameter), "4'hA");
}

TEST_F(ParameterPortTypedTest, PortAIsSizedByN) {
  const hldb::Port *const port = findPort("a");
  ASSERT_NE(port, nullptr);
  const hldb::Any *const declared = lowConnActual(port);
  ASSERT_NE(declared, nullptr);
  const hldb::Range *const range = firstRange(hldb::getTypespec(declared));
  ASSERT_NE(range, nullptr);
  EXPECT_TRUE(refersTo(range->getLeftExpr(), findParameter("N")));
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
