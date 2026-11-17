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

// Test for tests/Google/chapter-23/23.2.3--parameterized-localparam.sv
//
// THE SOURCE
//
//   19: module generic_decoder
//   20:   #(num_code_bits = 3, localparam num_out_bits = 1 << num_code_bits)
//   21:   (input [num_code_bits-1:0] A, output reg [num_out_bits-1:0] Y);
//   22: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// This is Example 3 of 23.2.3, "Parameterized module header with local
// parameters using ANSI style header".
//   - The module is named generic_decoder (23.2).
//   - num_code_bits is a parameter (parameter_port_list ::= #
//     ( list_of_param_assignments ...), Syntax 23-1) with the value 3;
//     num_out_bits is declared with localparam, so vpiLocalParam is TRUE
//     (vpi_user.h: "TRUE when a param is declared as a localparam"), and its
//     value expression is 1 << num_code_bits (vpiExpr, 37.28 detail 3).
//   - A is an input net sized [num_code_bits-1:0]; Y is an "output reg", an
//     output with an explicit data type, hence a variable (23.2.2.3), sized
//     [num_out_bits-1:0]. Both ranges are built from the parameters.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors          no fatal, syntax or error diagnostic
//   ModuleIsNamedGenericDecoder    vpiName generic_decoder
//   NumCodeBitsIsAParameter        not local, value 3
//   NumOutBitsIsALocalParameter    vpiLocalParam TRUE, value is a left shift
//                                  (vpiLShiftOp) of num_code_bits
//   ADependsOnNumCodeBits          A is an input net whose range's left bound
//                                  refers to num_code_bits
//   YDependsOnNumOutBits           Y is an output variable whose range's left
//                                  bound refers to num_out_bits
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The evaluated values (num_out_bits = 8, widths 3 and 8). Evaluating
//     constant expressions belongs to elaboration (Clause 11 and 23.10).

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
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.3--parameterized-localparam.sv";

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

class ParameterizedLocalparamTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "parameterized_localparam.hlc"}); }
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

TEST_F(ParameterizedLocalparamTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(ParameterizedLocalparamTest, ModuleIsNamedGenericDecoder) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getName(), "generic_decoder");
  EXPECT_EQ(stringProperty(module, vpiName), "generic_decoder");
}

TEST_F(ParameterizedLocalparamTest, NumCodeBitsIsAParameter) {
  const hldb::Parameter *const parameter = any_cast<hldb::Parameter>(findParameter("num_code_bits"));
  ASSERT_NE(parameter, nullptr);
  EXPECT_EQ(integerProperty(parameter, vpiLocalParam), 0);
  const hldb::Any *const value = relatedObject(parameter, vpiExpr);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(stringProperty(value, vpiDecompile), "3");
}

TEST_F(ParameterizedLocalparamTest, NumOutBitsIsALocalParameter) {
  const hldb::Parameter *const parameter = any_cast<hldb::Parameter>(findParameter("num_out_bits"));
  ASSERT_NE(parameter, nullptr);
  EXPECT_EQ(integerProperty(parameter, vpiLocalParam), 1);
  const hldb::Operation *const value = any_cast<hldb::Operation>(relatedObject(parameter, vpiExpr));
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(integerProperty(value, vpiOpType), static_cast<int64_t>(vpiLShiftOp));
  EXPECT_TRUE(refersTo(value, findParameter("num_code_bits")));
}

TEST_F(ParameterizedLocalparamTest, ADependsOnNumCodeBits) {
  const hldb::Port *const port = findPort("A");
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(integerProperty(port, vpiDirection), static_cast<int64_t>(vpiInput));
  const hldb::Any *const declared = lowConnActual(port);
  ASSERT_NE(declared, nullptr);
  EXPECT_NE(any_cast<hldb::Net>(declared), nullptr);
  const hldb::Range *const range = firstRange(typespecOf(declared));
  ASSERT_NE(range, nullptr);
  EXPECT_TRUE(refersTo(range->getLeftExpr(), findParameter("num_code_bits")));
}

TEST_F(ParameterizedLocalparamTest, YDependsOnNumOutBits) {
  const hldb::Port *const port = findPort("Y");
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(integerProperty(port, vpiDirection), static_cast<int64_t>(vpiOutput));
  const hldb::Any *const declared = lowConnActual(port);
  ASSERT_NE(declared, nullptr);
  EXPECT_NE(any_cast<hldb::Variable>(declared), nullptr);
  const hldb::Range *const range = firstRange(typespecOf(declared));
  ASSERT_NE(range, nullptr);
  EXPECT_TRUE(refersTo(range->getLeftExpr(), findParameter("num_out_bits")));
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
