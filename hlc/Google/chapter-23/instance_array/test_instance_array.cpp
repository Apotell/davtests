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

// Test for tests/Google/chapter-23/23.3.2--instance-array.sv
//
// THE SOURCE
//
//   19: module cell_m (input i, output o);
//   20: endmodule
//   21:
//   22: module top;
//   23:   wire [3:0] a, y;
//   24:   cell_m u [3:0] (.i(a), .o(y));
//   25: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "The instantiations of modules can contain a range specification. This
// allows an array of instances to be created. The array of instances is
// described in 28.3.5" (23.3.2). Syntax 23-6: name_of_instance ::=
// instance_identifier { unpacked_dimension }. So u [3:0] on line 24 is an
// array of four instances of cell_m. In the object model a module holds its
// module arrays in the relation module array (Module::getModuleArrays(),
// 37.5); a module array has a name, a size and the relation instance to its
// elements (37.11); each element has getDefName() and getArrayMember() TRUE
// (37.10).
//
// THE MODEL
//
// HLC's graph is a definition model: it holds everything the source says,
// so that an instance model can be built from it. An instantiation is an
// unelaborated instance (Module::getRefInstances()) bound through its
// typespec to the definition it names, with its parameter and port
// connections bound to that definition's parameters and ports. Those
// bindings are checked unconditionally. The top-level instances and the
// instance tree exist only in an elaborated model (23.3); those checks are
// gated on m_design->getElaborated(), so they run once HLC elaborates.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors          no fatal, syntax or error event
//   TopInstantiatesArrayUOfCellM   top's unelaborated instance u is an array
//                                  with the range [3:0] whose elements are
//                                  bound to the definition cell_m
//   NamedConnectionsBindIAndO      u's named connections bind to cell_m's
//                                  ports i and o and refer to the nets a and
//                                  y
//   TopHoldsModuleArrayU           elaborated model only: top holds the
//                                  module array u
//   ArrayUHasSize4                 elaborated model only: u's range spans 4
//   ArrayUHoldsFourCellMs          elaborated model only: u holds four
//                                  instances of cell_m
//   ElementsAreArrayMembers        elaborated model only: each of them is an
//                                  array member
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Which bits of a and y each element connects to (23.3.3.5). Those
//     connections exist only in an elaborated model; the bindings above are
//     what the definition model has to hold to build them.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/ExprEval.h>
#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/instance.h>
#include <hldb/module.h>
#include <hldb/module_array.h>
#include <hldb/module_typespec.h>
#include <hldb/named_argument.h>
#include <hldb/port.h>
#include <hldb/range.h>
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

// The port connections of an unelaborated instance as text, in order: ".<port>(<actual>)" for a named connection
// and "<port>:<actual>" for an ordered one, where <port> is the port the connection is bound to and <actual> the
// object it refers to ("" for an empty connection such as ".q()").
std::string connectionsOf(const hldb::RefInstance *instance) {
  std::string result;
  if ((instance == nullptr) || (instance->getArguments() == nullptr)) return result;
  for (const hldb::NamedArgument *argument : *instance->getArguments()) {
    const hldb::Port *const port = any_cast<hldb::Port>(hldb::getActual(argument->getLowConn()));
    const std::string portName = (port == nullptr) ? std::string("<unbound>") : std::string(port->getName());
    std::string actual;
    if (const hldb::Any *const highConn = argument->getHighConn()) {
      const hldb::Any *const target = hldb::getActual(highConn);
      actual = (target == nullptr) ? std::string("<unbound>") : std::string(target->getName());
    }
    if (!result.empty()) result += ", ";
    if (argument->getWildcardConn()) {
      result += ".*";
    } else if (argument->getConnByName()) {
      result += "." + portName + "(" + actual + ")";
    } else {
      result += portName + ":" + actual;
    }
  }
  return result;
}

}  // namespace

class InstanceArrayTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "instance_array.hlc"}); }
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

  // Module array u inside top-level instance top, or nullptr.
  static const hldb::ModuleArray *findArrayU() {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
    return (top == nullptr) ? nullptr : hldb::findByName<hldb::ModuleArray>("u", top->getModuleArrays());
  }
};

TEST_F(InstanceArrayTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

// "cell_m u [3:0] (...)" (23.3.2): u is an array of instances of cell_m with the range [3:0].
TEST_F(InstanceArrayTest, TopInstantiatesArrayUOfCellM) {
  const hldb::Module *const top = findModule("top");
  ASSERT_NE(top, nullptr);
  const hldb::RefInstance *const u = hldb::findByName<hldb::RefInstance>("u", top->getRefInstances());
  ASSERT_NE(u, nullptr);
  ASSERT_NE(u->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const array = u->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(array, nullptr) << "u is not an instance array";
  const hldb::Range *const range = array->getRange();
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const left = range->getLeftExpr<hldb::Constant>();
  const hldb::Constant *const right = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(left->getDecompile(), std::string_view("3"));
  EXPECT_EQ(right->getDecompile(), std::string_view("0"));
  ASSERT_NE(array->getElemTypespec(), nullptr);
  const hldb::ModuleTypespec *const element = array->getElemTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(element, nullptr) << "the elements of u are not bound to a module";
  EXPECT_EQ(element->getDefName(), std::string_view("cell_m"));
}

// ".i(a), .o(y)" (23.3.2.2) bind to cell_m's ports i and o.
TEST_F(InstanceArrayTest, NamedConnectionsBindIAndO) {
  const hldb::Module *const top = findModule("top");
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(connectionsOf(hldb::findByName<hldb::RefInstance>("u", top->getRefInstances())), ".i(a), .o(y)");
}

TEST_F(InstanceArrayTest, TopHoldsModuleArrayU) {
  ASSERT_NE(m_design, nullptr);
  if (m_design->getElaborated()) {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
    ASSERT_NE(top, nullptr) << "no top-level instance top";
    EXPECT_NE(hldb::findByName<hldb::ModuleArray>("u", top->getModuleArrays()), nullptr);
  }
}

TEST_F(InstanceArrayTest, ArrayUHasSize4) {
  ASSERT_NE(m_design, nullptr);
  if (m_design->getElaborated()) {
    const hldb::ModuleArray *const array = findArrayU();
    ASSERT_NE(array, nullptr) << "no module array top.u";
    ASSERT_NE(array->getRanges(), nullptr);
    ASSERT_EQ(array->getRanges()->size(), 1u);
    const hldb::Range *const range = array->getRanges()->front();
    const hldb::ExprEval evaluator(nullptr, true);
    int64_t left = 0;
    int64_t right = 0;
    ASSERT_TRUE(evaluator.getInt64(range->getLeftExpr(), &left));
    ASSERT_TRUE(evaluator.getInt64(range->getRightExpr(), &right));
    EXPECT_EQ(((left >= right) ? (left - right) : (right - left)) + 1, 4);
  }
}

TEST_F(InstanceArrayTest, ArrayUHoldsFourCellMs) {
  ASSERT_NE(m_design, nullptr);
  if (m_design->getElaborated()) {
    const hldb::ModuleArray *const array = findArrayU();
    ASSERT_NE(array, nullptr) << "no module array top.u";
    const std::vector<const hldb::Instance *> elements = toVector(array->getInstances());
    ASSERT_EQ(elements.size(), 4u);
    for (const hldb::Instance *element : elements) EXPECT_EQ(element->getDefName(), std::string_view("cell_m"));
  }
}

TEST_F(InstanceArrayTest, ElementsAreArrayMembers) {
  ASSERT_NE(m_design, nullptr);
  if (m_design->getElaborated()) {
    const hldb::ModuleArray *const array = findArrayU();
    ASSERT_NE(array, nullptr) << "no module array top.u";
    const std::vector<const hldb::Instance *> elements = toVector(array->getInstances());
    ASSERT_FALSE(elements.empty());
    for (const hldb::Instance *element : elements) EXPECT_TRUE(element->getArrayMember());
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
