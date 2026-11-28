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
//     num_out_bits is declared with localparam, so it is a local parameter
//     (Parameter::getLocalParam()), and its value expression is
//     1 << num_code_bits (Parameter::getExpr(), 37.28 detail 3).
//   - A is an input net sized [num_code_bits-1:0]; Y is an "output reg", an
//     output with an explicit data type, hence a variable (23.2.2.3), sized
//     [num_out_bits-1:0]. Both ranges are built from the parameters.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors          no fatal, syntax or error diagnostic
//   ModuleIsNamedGenericDecoder    getDefName() generic_decoder
//   NumCodeBitsIsAParameter        not local, value 3
//   NumOutBitsIsALocalParameter    getLocalParam() TRUE, value is a left shift
//                                  (Operation::getOpType() vpiLShiftOp) of
//                                  num_code_bits
//   ADependsOnNumCodeBits          A is a net whose range's left bound refers
//                                  to num_code_bits
//   YDependsOnNumOutBits           Y is a variable whose range's left bound
//                                  refers to num_out_bits
//   DirectionsFollowTheRules       Port::getDirection(): A vpiInput, Y
//                                  vpiOutput
//   AIsAWire                       Net::getNetType() of A is vpiWire
//   PortsAreUnsigned               getSigned() of the data types of A and Y is
//                                  false
//   WidthsWithTheDefaults          elaborated model only: getBitCount() of A
//                                  is 3 and of Y is 8 (num_out_bits = 1 << 3)

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/ExprEval.h>
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
#include <hldb/vpi_user.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kModuleName = "generic_decoder";

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

TEST_F(ParameterizedLocalparamTest, CompilesWithoutErrors) { EXPECT_EQ(allErrors(), ""); }

TEST_F(ParameterizedLocalparamTest, ModuleIsNamedGenericDecoder) {
  const hldb::Module *const module = findModule();
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->getDefName(), std::string_view("generic_decoder"));
}

TEST_F(ParameterizedLocalparamTest, NumCodeBitsIsAParameter) {
  const hldb::Parameter *const parameter = any_cast<hldb::Parameter>(findParameter("num_code_bits"));
  ASSERT_NE(parameter, nullptr);
  EXPECT_FALSE(parameter->getLocalParam());
  EXPECT_EQ(valueText(parameter), "3");
}

TEST_F(ParameterizedLocalparamTest, NumOutBitsIsALocalParameter) {
  const hldb::Parameter *const parameter = any_cast<hldb::Parameter>(findParameter("num_out_bits"));
  ASSERT_NE(parameter, nullptr);
  EXPECT_TRUE(parameter->getLocalParam());
  const hldb::Operation *const value = any_cast<hldb::Operation>(parameter->getExpr());
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getOpType(), vpiLShiftOp);
  EXPECT_TRUE(refersTo(value, findParameter("num_code_bits")));
}

TEST_F(ParameterizedLocalparamTest, ADependsOnNumCodeBits) {
  const hldb::Port *const port = findPort("A");
  ASSERT_NE(port, nullptr);
  const hldb::Any *const declared = lowConnActual(port);
  ASSERT_NE(declared, nullptr);
  EXPECT_NE(any_cast<hldb::Net>(declared), nullptr);
  const hldb::Range *const range = firstRange(hldb::getTypespec(declared));
  ASSERT_NE(range, nullptr);
  EXPECT_TRUE(refersTo(range->getLeftExpr(), findParameter("num_code_bits")));
}

TEST_F(ParameterizedLocalparamTest, YDependsOnNumOutBits) {
  const hldb::Port *const port = findPort("Y");
  ASSERT_NE(port, nullptr);
  const hldb::Any *const declared = lowConnActual(port);
  ASSERT_NE(declared, nullptr);
  EXPECT_NE(any_cast<hldb::Variable>(declared), nullptr);
  const hldb::Range *const range = firstRange(hldb::getTypespec(declared));
  ASSERT_NE(range, nullptr);
  EXPECT_TRUE(refersTo(range->getLeftExpr(), findParameter("num_out_bits")));
}

TEST_F(ParameterizedLocalparamTest, DirectionsFollowTheRules) {
  const std::vector<std::pair<std::string_view, int32_t>> expected = {{"A", vpiInput}, {"Y", vpiOutput}};
  for (const std::pair<std::string_view, int32_t> &entry : expected) {
    const hldb::Port *const port = findPort(entry.first);
    ASSERT_NE(port, nullptr) << entry.first;
    EXPECT_EQ(port->getDirection(), entry.second) << entry.first;
  }
}

// A has no port kind and an implicit data type, so it is a net of the default net type, wire (23.2.2.3).
TEST_F(ParameterizedLocalparamTest, AIsAWire) {
  for (std::string_view name : {"A"}) {
    const hldb::Port *const port = findPort(name);
    ASSERT_NE(port, nullptr) << name;
    const hldb::Net *const net = any_cast<hldb::Net>(lowConnActual(port));
    ASSERT_NE(net, nullptr) << name << " is not a net";
    EXPECT_EQ(net->getNetType(), vpiWire) << name;
  }
}

// [num_code_bits-1:0] and reg [num_out_bits-1:0] have no signed keyword, so they are unsigned (6.11.3).
TEST_F(ParameterizedLocalparamTest, PortsAreUnsigned) {
  for (std::string_view name : {"A", "Y"}) {
    const hldb::Port *const port = findPort(name);
    ASSERT_NE(port, nullptr) << name;
    const hldb::Any *const declared = lowConnActual(port);
    ASSERT_NE(declared, nullptr) << name;
    const hldb::Typespec *const typespec = hldb::getTypespec(declared);
    ASSERT_NE(typespec, nullptr) << name;
    EXPECT_FALSE(hldb::getSigned(typespec)) << name;
  }
}

// num_code_bits can be redefined per instance (23.10), so the widths are instance-model facts. Elaborated
// model only: with the default num_code_bits = 3, A is 3 bits and Y is num_out_bits = 1 << 3 = 8 bits wide.
TEST_F(ParameterizedLocalparamTest, WidthsWithTheDefaults) {
  ASSERT_NE(m_design, nullptr);
  if (m_design->getElaborated()) {
    const std::vector<std::pair<std::string_view, int64_t>> expected = {{"A", 3}, {"Y", 8}};
    for (const std::pair<std::string_view, int64_t> &entry : expected) {
      const hldb::Port *const port = findElaboratedPort(entry.first);
      ASSERT_NE(port, nullptr) << entry.first;
      EXPECT_EQ(widthOf(lowConnActual(port)), entry.second) << entry.first;
    }
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
