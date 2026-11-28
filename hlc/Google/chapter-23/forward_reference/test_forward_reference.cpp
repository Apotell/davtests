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

// Test for tests/Google/chapter-23/23.3.2--forward-reference.sv
//
// THE SOURCE
//
//   19: module top;
//   20:   wire a;
//   21:   later u1 (a);
//   22: endmodule
//   23:
//   24: module later (input i);
//   25: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "The module name can be a module previously declared or one declared
// later." (23.3.2). So u1 on line 21 is an instance of the module later,
// declared on line 24, and later is not a top-level module because it
// appears in a module instantiation statement (23.3.1). A module holds its
// child instances in the relation module (Module::getModules(), 37.5); an
// instance has a definition name (getDefName(), 37.10) and the ports of its
// definition (37.5, relation port).
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
//   TopInstantiatesU1OfLater       top's unelaborated instance u1 is bound to
//                                  the definition later, declared after top
//   OrderedConnectionBindsI        u1's one connection is ordered, binds to
//                                  later's first port i and refers to the net
//                                  a
//   TopIsTheOnlyTopLevelInstance   elaborated model only: the top-level
//                                  instances are exactly top
//   TopHoldsU1OfModuleLater        elaborated model only: top holds u1, an
//                                  instance of later
//   U1HasPortI                     elaborated model only: u1's ports are i

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

class ForwardReferenceTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "forward_reference.hlc"}); }
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

  // Instance u1 inside top-level instance top, or nullptr.
  static const hldb::Module *findU1() {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
    return (top == nullptr) ? nullptr : hldb::findByName<hldb::Module>("u1", top->getModules());
  }
};

TEST_F(ForwardReferenceTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

// "later u1 (a);" names the module later, which is declared after top.
TEST_F(ForwardReferenceTest, TopInstantiatesU1OfLater) {
  const hldb::RefInstance *const u1 = findRefInstance("top", "u1");
  ASSERT_NE(u1, nullptr);
  EXPECT_EQ(boundDefinition(u1), std::string_view("later"));
}

// "(a)" is an ordered port connection (23.3.2.1): it binds to later's first port, i.
TEST_F(ForwardReferenceTest, OrderedConnectionBindsI) { EXPECT_EQ(connectionsOf(findRefInstance("top", "u1")), "i:a"); }

TEST_F(ForwardReferenceTest, TopIsTheOnlyTopLevelInstance) {
  ASSERT_NE(m_design, nullptr);
  if (m_design->getElaborated()) {
    EXPECT_EQ(namesOf(topInstances()), "top");
  }
}

TEST_F(ForwardReferenceTest, TopHoldsU1OfModuleLater) {
  ASSERT_NE(m_design, nullptr);
  if (m_design->getElaborated()) {
    const hldb::Module *const u1 = findU1();
    ASSERT_NE(u1, nullptr) << "no instance top.u1";
    EXPECT_EQ(u1->getDefName(), std::string_view("later"));
  }
}

TEST_F(ForwardReferenceTest, U1HasPortI) {
  ASSERT_NE(m_design, nullptr);
  if (m_design->getElaborated()) {
    const hldb::Module *const u1 = findU1();
    ASSERT_NE(u1, nullptr) << "no instance top.u1";
    EXPECT_EQ(namesOf(toVector(u1->getPorts())), "i");
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
