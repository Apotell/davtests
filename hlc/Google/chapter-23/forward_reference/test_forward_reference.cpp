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
// Instances exist only after elaboration (23.3). The HLC install of
// 2026-11-23 no longer gives tests an elaborator (hldb::Elaborator was
// removed), so the test checks the design as Compile() leaves it; the
// instance checks fail until HLC builds the instance tree itself.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors          no fatal, syntax or error event
//   DesignIsElaborated             the design is marked elaborated
//   TopIsTheOnlyTopLevelInstance   the top-level instances are exactly top
//   TopHoldsU1OfModuleLater        top holds u1, whose getDefName() is later
//   U1HasPortI                     u1 has the port i of module later
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The connection of a to port i. Ordered connections are 23.3.2.1 (next
//     batch).

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

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
constexpr std::string_view kSourceFileName = "23.3.2--forward-reference.sv";

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

TEST_F(ForwardReferenceTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(ForwardReferenceTest, DesignIsElaborated) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_TRUE(m_design->getElaborated());
}

TEST_F(ForwardReferenceTest, TopIsTheOnlyTopLevelInstance) { EXPECT_EQ(namesOf(topInstances()), "top"); }

TEST_F(ForwardReferenceTest, TopHoldsU1OfModuleLater) {
  const hldb::Module *const u1 = findU1();
  ASSERT_NE(u1, nullptr) << "no instance top.u1";
  EXPECT_EQ(u1->getDefName(), "later");
}

TEST_F(ForwardReferenceTest, U1HasPortI) {
  const hldb::Module *const u1 = findU1();
  ASSERT_NE(u1, nullptr) << "no instance top.u1";
  EXPECT_EQ(namesOf(toVector(u1->getPorts())), "i");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
