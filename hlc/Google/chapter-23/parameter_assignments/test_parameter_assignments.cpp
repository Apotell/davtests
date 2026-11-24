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
// Instances exist only after elaboration (23.3). The HLC install of
// 2026-11-23 no longer gives tests an elaborator (hldb::Elaborator was
// removed), so the test checks the design as Compile() leaves it; the
// instance checks fail until HLC builds the instance tree itself.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors          no fatal, syntax or error event
//   DesignIsElaborated             the design is marked elaborated
//   UDefaultHasW1D2                u_default's parameters are W=1, D=2
//   UOrderedHasW4D8                u_ordered's parameters are W=4, D=8
//   UNamedHasW1D16                 u_named's parameters are W=1, D=16
//   UNamedEmptyHasW1D32            u_named_empty's parameters are W=1, D=32
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Assigning the same parameter twice ("Once a parameter is assigned a
//     value, there shall not be another assignment to this parameter name").
//     Parameter overriding is 23.10, a later batch.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/any_type.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/parameter.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.3.2--parameter-assignments.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

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

// The object named 'name' among 'objects', or nullptr.
template <typename T>
const T *findNamed(const std::vector<const T *> &objects, std::string_view name) {
  for (const T *object : objects) {
    if (object->getName() == name) return object;
  }
  return nullptr;
}

// The source text (Constant::getDecompile()) of the value of parameter 'name'
// in 'instance', or a placeholder that says what is missing.
std::string parameterValue(const hldb::Module *instance, std::string_view name) {
  const hldb::Any *const object = findNamed(toVector(instance->getParameters()), name);
  if (object == nullptr) return "<no parameter>";
  const hldb::Parameter *const parameter = any_cast<hldb::Parameter>(object);
  if (parameter == nullptr) return "<not a parameter>";
  if (parameter->getExpr() == nullptr) return "<no value>";
  const hldb::Constant *const constant = any_cast<hldb::Constant>(parameter->getExpr());
  if (constant == nullptr) return "<" + std::string(hldb::AnyTypeName(parameter->getExpr()->getAnyType())) + ">";
  return std::string(constant->getDecompile());
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

  // The top-level instances from the file under test (37.5 detail 1).
  static std::vector<const hldb::Module *> topInstances() {
    std::vector<const hldb::Module *> result;
    if (m_design == nullptr) return result;
    for (const hldb::Module *module : toVector(m_design->getTopModules())) {
      if (endsWith(module->getFile(), kSourceFileName)) result.emplace_back(module);
    }
    return result;
  }

  // "W=<value>, D=<value>" for instance 'name' inside top-level instance
  // top, or a placeholder that says what is missing.
  static std::string parametersOf(std::string_view name) {
    const hldb::Module *const top = findNamed(topInstances(), "top");
    if (top == nullptr) return "<no top-level instance top>";
    const hldb::Module *const instance = findNamed(toVector(top->getModules()), name);
    if (instance == nullptr) return "<no instance top." + std::string(name) + ">";
    return "W=" + parameterValue(instance, "W") + ", D=" + parameterValue(instance, "D");
  }
};

TEST_F(ParameterAssignmentsTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(ParameterAssignmentsTest, DesignIsElaborated) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_TRUE(m_design->getElaborated());
}

TEST_F(ParameterAssignmentsTest, UDefaultHasW1D2) { EXPECT_EQ(parametersOf("u_default"), "W=1, D=2"); }

TEST_F(ParameterAssignmentsTest, UOrderedHasW4D8) { EXPECT_EQ(parametersOf("u_ordered"), "W=4, D=8"); }

TEST_F(ParameterAssignmentsTest, UNamedHasW1D16) { EXPECT_EQ(parametersOf("u_named"), "W=1, D=16"); }

TEST_F(ParameterAssignmentsTest, UNamedEmptyHasW1D32) { EXPECT_EQ(parametersOf("u_named_empty"), "W=1, D=32"); }
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
