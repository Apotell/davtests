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

// Test for tests/Google/chapter-23/23.3.2--multiple-instances.sv
//
// THE SOURCE
//
//   21: module ffnand (output q, qbar, input clear, preset);
//   22: endmodule
//   23:
//   24: module top;
//   25:   wire out1, out2, out3, in1, in2;
//   26:   ffnand ff1 (.q(), .qbar(out1), .clear(in1), .preset(in2)),
//   27:          ff2 (.q(), .qbar(out2), .clear(in2), .preset(in1)),
//   28:          ff3 (.q(out3), .qbar(), .clear(in1), .preset(in2));
//   29: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "Hierarchical instantiation allows more than one instance of the same
// module." (23.3.2). "One or more module instances (identical copies of a
// module) can be specified in a single module instantiation statement."
// (23.3.2). Syntax 23-6: module_instantiation ::= module_identifier
// [ parameter_value_assignment ] hierarchical_instance { ,
// hierarchical_instance } ;. The fixture is the LRM's ffnand example; as
// printed in the LRM it ends ff2 with ';' instead of ',' and connects .q()
// twice in ff2, which 23.3.2.2 forbids, so the fixture fixes both. A module
// holds its child instances in the relation module (Module::getModules(),
// 37.5), and each one has a definition name (getDefName(), 37.10).
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
//   TopInstantiatesFf1Ff2Ff3       top's unelaborated instances are ff1, ff2
//                                  and ff3, in source order, each bound to
//                                  the definition ffnand
//   NamedConnectionsAreBound       each instance's four named connections
//                                  bind, in order, to the ports q, qbar,
//                                  clear and preset and refer to top's nets;
//                                  an empty .q() or .qbar() has no actual
//   TopIsTheOnlyTopLevelInstance   elaborated model only: the top-level
//                                  instances are exactly top
//   TopHoldsFf1Ff2Ff3              elaborated model only: top's instances
//                                  are ff1, ff2 and ff3, in source order
//   AllThreeAreOfModuleFfnand      elaborated model only: each of them has
//                                  getDefName() ffnand
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Which net each instance port ends up connected to. Connections exist
//     only in an elaborated model; the bindings above are what the
//     definition model has to hold to build them.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/named_argument.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>

#include <algorithm>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
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

// The names of 'objects' in their order, comma-separated.
template <typename T>
std::string namesOf(const std::vector<const T *> &objects) {
  std::string result;
  for (const T *object : objects) {
    if (!result.empty()) result += ", ";
    result += std::string(object->getName());
  }
  return result;
}
// The name of the definition an unelaborated instance is bound to (the module its typespec names), or "" when it
// is not bound to a module.
std::string_view boundDefinition(const hldb::RefInstance *instance) {
  if ((instance == nullptr) || (instance->getTypespec() == nullptr)) return "";
  const hldb::ModuleTypespec *const definition = instance->getTypespec()->getActual<hldb::ModuleTypespec>();
  return (definition == nullptr) ? std::string_view() : definition->getDefName();
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

class MultipleInstancesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "multiple_instances.hlc"}); }
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
};

TEST_F(MultipleInstancesTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

// "ffnand ff1 (...), ff2 (...), ff3 (...);" (Syntax 23-6): three instances of ffnand in one statement.
TEST_F(MultipleInstancesTest, TopInstantiatesFf1Ff2Ff3) {
  const hldb::Module *const top = findModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  std::string names;
  for (const hldb::RefInstance *instance : *top->getRefInstances()) {
    names += (names.empty() ? "" : ", ") + std::string(instance->getName());
    EXPECT_EQ(boundDefinition(instance), std::string_view("ffnand")) << instance->getName();
  }
  EXPECT_EQ(names, "ff1, ff2, ff3");
}

// Named port connections (23.3.2.2) bind each connection to the port it names; ".q()" leaves q unconnected.
TEST_F(MultipleInstancesTest, NamedConnectionsAreBound) {
  const std::pair<std::string_view, std::string_view> expected[] = {
      {"ff1", ".q(), .qbar(out1), .clear(in1), .preset(in2)"},
      {"ff2", ".q(), .qbar(out2), .clear(in2), .preset(in1)"},
      {"ff3", ".q(out3), .qbar(), .clear(in1), .preset(in2)"}};
  for (const std::pair<std::string_view, std::string_view> &entry : expected) {
    const hldb::RefInstance *const instance = findRefInstance("top", entry.first);
    ASSERT_NE(instance, nullptr) << entry.first;
    EXPECT_EQ(connectionsOf(instance), entry.second) << entry.first;
  }
}

TEST_F(MultipleInstancesTest, TopIsTheOnlyTopLevelInstance) {
  ASSERT_NE(m_design, nullptr);
  if (m_design->getElaborated()) {
    EXPECT_EQ(namesOf(topInstances()), "top");
  }
}

TEST_F(MultipleInstancesTest, TopHoldsFf1Ff2Ff3) {
  ASSERT_NE(m_design, nullptr);
  if (m_design->getElaborated()) {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
    ASSERT_NE(top, nullptr) << "no top-level instance top";
    EXPECT_EQ(namesOf(toVector(top->getModules())), "ff1, ff2, ff3");
  }
}

TEST_F(MultipleInstancesTest, AllThreeAreOfModuleFfnand) {
  ASSERT_NE(m_design, nullptr);
  if (m_design->getElaborated()) {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
    ASSERT_NE(top, nullptr) << "no top-level instance top";
    const std::vector<const hldb::Module *> children = toVector(top->getModules());
    ASSERT_EQ(children.size(), 3u) << "instances in top: " << namesOf(children);
    for (const hldb::Module *child : children) {
      EXPECT_EQ(child->getDefName(), std::string_view("ffnand")) << "instance " << child->getName();
    }
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
