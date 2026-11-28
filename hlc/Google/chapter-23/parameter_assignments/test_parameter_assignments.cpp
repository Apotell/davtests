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

// Test for tests/Google/chapter-23/23.3.2--parameter-assignments.sv
//
// THE SOURCE
//
//   21: module m #(parameter int W = 1, parameter int D = 2) ();
//   22: endmodule
//   23:
//   24: module top;
//   25:   m u_default ();
//   26:   m #(4, 8) u_ordered ();
//   27:   m #(.D(16)) u_named ();
//   28:   m #(.W(), .D(32)) u_named_empty ();
//   29: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "Parameter assignments can be named or ordered." (23.3.2). Syntax 23-6:
// parameter_value_assignment ::= # ( [ list_of_parameter_value_assignments ] ),
// with ordered_parameter_assignment ::= param_expression and
// named_parameter_assignment ::= . parameter_identifier
// ( [ param_expression ] ). For assignment by name, "Only parameters that
// are assigned new values need to be specified." and when the expression is
// left out "the parameter retains its default value" (23.10.2.2). So:
//   u_default      W = 1, D = 2  (defaults)
//   u_ordered      W = 4, D = 8  (in declaration order)
//   u_named        W = 1, D = 16
//   u_named_empty  W = 1, D = 32 (.W() keeps the default)
// An instance holds its parameters in the relation parameters
// (getParameters(), 37.10), and a constant's text is
// Constant::getDecompile().
//
// THE MODEL
//
// HLC's graph is a definition model: it holds everything the source says,
// so that an instance model can be built from it. An instantiation is an
// unelaborated instance (Module::getRefInstances()) bound through its
// typespec to the definition it names, with its parameter and port
// connections bound to that definition's parameters and ports. Those
// bindings are checked unconditionally. The top-level instances and the
// instance tree exist only in an elaborated model (23.3); those tests skip
// unless m_design->getElaborated(), so they run once HLC elaborates.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors          no fatal, syntax or error event
//   InstancesAreBoundToM           top's four unelaborated instances are
//                                  bound to the definition m
//   UDefaultAssignsNothing         u_default has no parameter assignment
//   UOrderedAssignsByPosition      u_ordered's ordered assignments bind 4 to
//                                  W and 8 to D
//   UNamedAssignsDByName           u_named's named assignment binds 16 to D
//   UNamedEmptyKeepsTheDefaultOfW  u_named_empty's named assignments bind
//                                  nothing to W (.W()) and 32 to D
//   UDefaultHasW1D2                elaborated model only: u_default has W=1,
//                                  D=2
//   UOrderedHasW4D8                elaborated model only: W=4, D=8
//   UNamedHasW1D16                 elaborated model only: W=1, D=16
//   UNamedEmptyHasW1D32            elaborated model only: W=1, D=32
//
// SKIPPED (GTEST_SKIP; the real assertions stay below the skip)
//
//   UDefaultHasW1D2, UOrderedHasW4D8, UNamedHasW1D16, UNamedEmptyHasW1D32:
//   HLC builds a definition model only, so there are no instances whose
//   parameters hold the final values.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
// The reason the instance-model tests are skipped while HLC builds a definition model only.
constexpr std::string_view kNotElaborated =
    "HLC builds a definition model only, with no instances whose parameter values could be checked (IEEE "
    "1800-2023 23.3.2, 23.10.2)";

// True for an error-class event: fatal, syntax or error severity.
bool isErrorClass(const Event &event) {
  const EventDefinition *const eventDefinition = hldb::findEventDefinition(hlc::getEventTable(), event.getType());
  if (eventDefinition == nullptr) return false;
  const EventSeverity severity = eventDefinition->m_severity;
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}

// One event as hldb::renderEvent() prints it, one line per output line, so that a
// failing test shows what was reported and where.
std::string describeEvent(const Event &event) {
  std::ostringstream oss;
  const std::vector<std::string> lines = hldb::renderEvent(event);
  std::copy(lines.cbegin(), lines.cend(), std::ostream_iterator<std::string>(oss, "\n"));
  return oss.str();
}

// The objects in 'collection'; none for a null collection.
template <typename T>
std::vector<const T *> toVector(const std::vector<T *> *collection) {
  std::vector<const T *> result;
  if (collection == nullptr) return result;
  for (const T *object : *collection) result.emplace_back(object);
  return result;
}

// The source text (Constant::getDecompile()) of the value of parameter 'name'
// in 'instance', or a placeholder that says what is missing.
std::string parameterValue(const hldb::Module *instance, std::string_view name) {
  const hldb::Any *const object = hldb::findByName<hldb::Any>(name, instance->getParameters());
  if (object == nullptr) return "<no parameter>";
  const hldb::Parameter *const parameter = any_cast<hldb::Parameter>(object);
  if (parameter == nullptr) return "<not a parameter>";
  if (parameter->getExpr() == nullptr) return "<no value>";
  const hldb::Constant *const constant = any_cast<hldb::Constant>(parameter->getExpr());
  if (constant == nullptr) return "<" + std::string(hldb::AnyTypeName(parameter->getExpr()->getAnyType())) + ">";
  return std::string(constant->getDecompile());
}
// The name of the definition an unelaborated instance is bound to (the module its typespec names), or "" when it
// is not bound to a module.
std::string_view boundDefinition(const hldb::RefInstance *instance) {
  if ((instance == nullptr) || (instance->getTypespec() == nullptr)) return "";
  const hldb::ModuleTypespec *const definition = instance->getTypespec()->getActual<hldb::ModuleTypespec>();
  return (definition == nullptr) ? std::string_view() : definition->getDefName();
}

// The parameter value assignments of an unelaborated instance as text, in order: "<parameter>=<value>" for an
// ordered assignment and ".<parameter>=<value>" for a named one, where <parameter> is the parameter the
// assignment is bound to and <value> the constant assigned ("" for an empty assignment such as ".W()").
std::string parameterAssignmentsOf(const hldb::RefInstance *instance) {
  if ((instance == nullptr) || (instance->getTypespec() == nullptr)) return "<no instance>";
  const hldb::ModuleTypespec *const definition = instance->getTypespec()->getActual<hldb::ModuleTypespec>();
  if (definition == nullptr) return "<not bound to a module>";
  std::string result;
  if (definition->getParamAssigns() == nullptr) return result;
  for (const hldb::ParamAssign *assign : *definition->getParamAssigns()) {
    const hldb::Parameter *const parameter = any_cast<hldb::Parameter>(hldb::getActual(assign->getLhs()));
    std::string value;
    if (const hldb::Constant *const constant = assign->getRhs<hldb::Constant>()) {
      value = std::string(constant->getDecompile());
    } else if (assign->getRhs() != nullptr) {
      value = "<not a constant>";
    }
    if (!result.empty()) result += ", ";
    result += std::string(assign->getConnByName() ? "." : "") +
              ((parameter == nullptr) ? std::string("<unbound>") : std::string(parameter->getName())) + "=" + value;
  }
  return result;
}

}  // namespace

class ParameterAssignmentsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "parameter_assignments.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static std::string allEvents() {
    hlc::EventListener *const listener = m_session->getEventListener();
    std::ostringstream oss;
    for (const Event &event : listener->getEvents()) {
      if (isErrorClass(event)) oss << describeEvent(event);
    }
    return oss.str();
  }

  // The definition of the module named 'name', or nullptr.
  static const hldb::Module *findModule(std::string_view name) {
    if (m_design == nullptr) return nullptr;
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  // The unelaborated instance 'name' that the definition 'parent' declares, or nullptr.
  static const hldb::RefInstance *findRefInstance(std::string_view parent, std::string_view name) {
    const hldb::Module *const module = findModule(parent);
    return (module == nullptr) ? nullptr : hldb::findByName<hldb::RefInstance>(name, module->getRefInstances());
  }

  // The top-level instances (37.5 detail 1); there are some only in an elaborated model.
  static std::vector<const hldb::Module *> topInstances() {
    std::vector<const hldb::Module *> result;
    if ((m_design == nullptr) || (m_design->getTopModules() == nullptr)) return result;
    for (const hldb::Module *module : *m_design->getTopModules()) result.emplace_back(module);
    return result;
  }

  // "W=<value>, D=<value>" for instance 'name' inside top-level instance
  // top, or a placeholder that says what is missing.
  static std::string parametersOf(std::string_view name) {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
    if (top == nullptr) return "<no top-level instance top>";
    const hldb::Module *const instance = hldb::findByName<hldb::Module>(name, top->getModules());
    if (instance == nullptr) return "<no instance top." + std::string(name) + ">";
    return "W=" + parameterValue(instance, "W") + ", D=" + parameterValue(instance, "D");
  }
};

TEST_F(ParameterAssignmentsTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

// All four instantiations name the module m.
TEST_F(ParameterAssignmentsTest, InstancesAreBoundToM) {
  for (std::string_view name : {"u_default", "u_ordered", "u_named", "u_named_empty"}) {
    const hldb::RefInstance *const instance = findRefInstance("top", name);
    ASSERT_NE(instance, nullptr) << name;
    EXPECT_EQ(boundDefinition(instance), std::string_view("m")) << name;
  }
}

// "m u_default ();" overrides no parameter.
TEST_F(ParameterAssignmentsTest, UDefaultAssignsNothing) {
  EXPECT_EQ(parameterAssignmentsOf(findRefInstance("top", "u_default")), "");
}

// "#(4, 8)" assigns by position (23.10.2.1): 4 to the first parameter W and 8 to D.
TEST_F(ParameterAssignmentsTest, UOrderedAssignsByPosition) {
  EXPECT_EQ(parameterAssignmentsOf(findRefInstance("top", "u_ordered")), "W=4, D=8");
}

// "#(.D(16))" assigns D by name (23.10.2.2).
TEST_F(ParameterAssignmentsTest, UNamedAssignsDByName) {
  EXPECT_EQ(parameterAssignmentsOf(findRefInstance("top", "u_named")), ".D=16");
}

// "#(.W(), .D(32))": the empty named assignment ".W()" keeps W's default (23.10.2.2).
TEST_F(ParameterAssignmentsTest, UNamedEmptyKeepsTheDefaultOfW) {
  EXPECT_EQ(parameterAssignmentsOf(findRefInstance("top", "u_named_empty")), ".W=, .D=32");
}

TEST_F(ParameterAssignmentsTest, UDefaultHasW1D2) {
  ASSERT_NE(m_design, nullptr);
  if (!m_design->getElaborated()) GTEST_SKIP() << kNotElaborated;
  EXPECT_EQ(parametersOf("u_default"), "W=1, D=2");
}

TEST_F(ParameterAssignmentsTest, UOrderedHasW4D8) {
  ASSERT_NE(m_design, nullptr);
  if (!m_design->getElaborated()) GTEST_SKIP() << kNotElaborated;
  EXPECT_EQ(parametersOf("u_ordered"), "W=4, D=8");
}

TEST_F(ParameterAssignmentsTest, UNamedHasW1D16) {
  ASSERT_NE(m_design, nullptr);
  if (!m_design->getElaborated()) GTEST_SKIP() << kNotElaborated;
  EXPECT_EQ(parametersOf("u_named"), "W=1, D=16");
}

TEST_F(ParameterAssignmentsTest, UNamedEmptyHasW1D32) {
  ASSERT_NE(m_design, nullptr);
  if (!m_design->getElaborated()) GTEST_SKIP() << kNotElaborated;
  EXPECT_EQ(parametersOf("u_named_empty"), "W=1, D=32");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
