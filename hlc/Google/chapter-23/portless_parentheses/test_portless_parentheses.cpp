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

// Test for tests/Google/chapter-23/23.3.2--portless-parentheses.sv
//
// THE SOURCE
//
//   19: module empty_m;
//   20: endmodule
//   21:
//   22: module top;
//   23:   empty_m u1 ();
//   24: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "The parentheses shall be required on all module instantiations, even
// when the instantiated module does not have ports." (23.3.2). Syntax 23-6:
// hierarchical_instance ::= name_of_instance ( [ list_of_port_connections ] ),
// so u1 () on line 23 is a complete instantiation of empty_m with no
// connections. A module holds its child instances in the relation module
// (Module::getModules(), 37.5), and an instance has a definition name
// (getDefName(), 37.10).
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
//   TopInstantiatesU1OfEmptyM      top's unelaborated instance u1 is bound to
//                                  the definition empty_m
//   U1HasNoConnections             u1 has no port connections
//   TopIsTheOnlyTopLevelInstance   elaborated model only: the top-level
//                                  instances are exactly top
//   TopHoldsU1OfModuleEmptyM       elaborated model only: top holds u1, an
//                                  instance of empty_m
//   U1HasNoPorts                   elaborated model only: u1 has no ports

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

class PortlessParenthesesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "portless_parentheses.hlc"}); }
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

TEST_F(PortlessParenthesesTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

// "empty_m u1 ();" names the module empty_m.
TEST_F(PortlessParenthesesTest, TopInstantiatesU1OfEmptyM) {
  const hldb::RefInstance *const u1 = findRefInstance("top", "u1");
  ASSERT_NE(u1, nullptr);
  EXPECT_EQ(boundDefinition(u1), std::string_view("empty_m"));
}

// "()" holds no port connections.
TEST_F(PortlessParenthesesTest, U1HasNoConnections) {
  const hldb::RefInstance *const u1 = findRefInstance("top", "u1");
  ASSERT_NE(u1, nullptr);
  EXPECT_EQ(connectionsOf(u1), "");
}

TEST_F(PortlessParenthesesTest, TopIsTheOnlyTopLevelInstance) {
  ASSERT_NE(m_design, nullptr);
  if (m_design->getElaborated()) {
    EXPECT_EQ(namesOf(topInstances()), "top");
  }
}

TEST_F(PortlessParenthesesTest, TopHoldsU1OfModuleEmptyM) {
  ASSERT_NE(m_design, nullptr);
  if (m_design->getElaborated()) {
    const hldb::Module *const u1 = findU1();
    ASSERT_NE(u1, nullptr) << "no instance top.u1";
    EXPECT_EQ(u1->getDefName(), std::string_view("empty_m"));
  }
}

TEST_F(PortlessParenthesesTest, U1HasNoPorts) {
  ASSERT_NE(m_design, nullptr);
  if (m_design->getElaborated()) {
    const hldb::Module *const u1 = findU1();
    ASSERT_NE(u1, nullptr) << "no instance top.u1";
    EXPECT_EQ(namesOf(toVector(u1->getPorts())), "");
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
