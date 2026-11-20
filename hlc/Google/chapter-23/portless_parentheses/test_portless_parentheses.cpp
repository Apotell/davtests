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
// (vpiModule, 37.5), and an instance has a vpiDefName (37.10).
//
// The test elaborates the compiled design with hldb::Elaborator, because
// instances exist only after elaboration (23.3).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors          no fatal, syntax or error event
//   DesignIsElaborated             the design is marked elaborated
//   TopIsTheOnlyTopLevelInstance   the top-level instances are exactly top
//   TopHoldsU1OfModuleEmptyM       top holds u1, whose vpiDefName is empty_m
//   U1HasNoPorts                   u1 has no ports
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The illegal forms (no parentheses, connections to a module without
//     ports). They have their own tests, no_parentheses_illegal and
//     ports_on_portless_illegal.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Database.h>
#include <hldb/Elaborator.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/port.h>

#include <algorithm>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.3.2--portless-parentheses.sv";

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
}  // namespace

class PortlessParenthesesTest : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "portless_parentheses.hlc"});
    elaborate();
  }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Instances exist only after elaboration (23.3), which Compile() does not
  // run, so the test runs HLDB's elaborator on the compiled database.
  static void elaborate() {
    if (m_session == nullptr) return;
    hldb::Database &database = m_session->getDatabase();
    hldb::Elaborator elaborator(database);
    elaborator.listen(database);
  }

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

  // Instance u1 inside top-level instance top, or nullptr.
  static const hldb::Module *findU1() {
    const hldb::Module *const top = findNamed(topInstances(), "top");
    return (top == nullptr) ? nullptr : findNamed(toVector(top->getModules()), "u1");
  }
};

TEST_F(PortlessParenthesesTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(PortlessParenthesesTest, DesignIsElaborated) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_TRUE(m_design->getElaborated());
}

TEST_F(PortlessParenthesesTest, TopIsTheOnlyTopLevelInstance) { EXPECT_EQ(namesOf(topInstances()), "top"); }

TEST_F(PortlessParenthesesTest, TopHoldsU1OfModuleEmptyM) {
  const hldb::Module *const u1 = findU1();
  ASSERT_NE(u1, nullptr) << "no instance top.u1";
  EXPECT_EQ(u1->getDefName(), "empty_m");
}

TEST_F(PortlessParenthesesTest, U1HasNoPorts) {
  const hldb::Module *const u1 = findU1();
  ASSERT_NE(u1, nullptr) << "no instance top.u1";
  EXPECT_EQ(namesOf(toVector(u1->getPorts())), "");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
