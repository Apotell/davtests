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

// Test for tests/Google/chapter-23/23.3.1--top-level-multiple.sv
//
// THE SOURCE
//
//   19: module leaf;
//   20: endmodule
//   21:
//   22: module mid;
//   23:   leaf l1 ();
//   24: endmodule
//   25:
//   26: module top_a;
//   27:   mid m1 ();
//   28: endmodule
//   29:
//   30: module top_b;
//   31: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "Top-level modules are modules that are included in the SystemVerilog
// source text, but do not appear in any module instantiation statement"
// (23.3.1). "A top-level module is implicitly instantiated once, and its
// instance name is the same as the module name." (23.3.1). leaf and mid
// are instantiated, so the top-level modules are top_a and top_b.
// "Explicit module instantiation creates a hierarchical instance of a
// module." (23.3.2). In the object model a module holds its child instances
// in the relation module (vpiModule, 37.5), top-level modules are reached
// by vpi_iterate() with a NULL reference (37.5 detail 1), vpiTopModule is
// TRUE only for them (37.5), and vpiFullName gives an instance's
// hierarchical name (37.10).
//
// The test elaborates the compiled design with hldb::Elaborator, because
// instances exist only after elaboration (23.3).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors           no fatal, syntax or error event
//   DesignIsElaborated              the design is marked elaborated
//   TopLevelInstancesAreTopAAndTopB the top-level instances are exactly
//                                   top_a and top_b
//   TopAHoldsInstanceM1OfMid        top_a holds m1, an instance of mid
//   M1IsNotATopModule               m1's vpiTopModule is FALSE
//   M1HoldsInstanceL1OfLeaf         m1 holds l1, an instance of leaf
//   L1FullNameIsTopAM1L1            l1's vpiFullName is top_a.m1.l1
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Nothing else: the modules are empty apart from their instances.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Database.h>
#include <hldb/Elaborator.h>
#include <hldb/design.h>
#include <hldb/module.h>

#include <algorithm>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.3.1--top-level-multiple.sv";

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

// The names of 'objects', comma-separated, sorted when 'sorted' is true.
template <typename T>
std::string namesOf(const std::vector<const T *> &objects, bool sorted) {
  std::vector<std::string> names;
  for (const T *object : objects) names.emplace_back(object->getName());
  if (sorted) std::sort(names.begin(), names.end());
  std::string result;
  for (const std::string &name : names) {
    if (!result.empty()) result += ", ";
    result += name;
  }
  return result;
}
}  // namespace

class TopLevelMultipleTest : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "top_level_multiple.hlc"});
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

  // Instance m1 inside top-level instance top_a, or nullptr.
  static const hldb::Module *findM1() {
    const hldb::Module *const topA = findNamed(topInstances(), "top_a");
    return (topA == nullptr) ? nullptr : findNamed(toVector(topA->getModules()), "m1");
  }
};

TEST_F(TopLevelMultipleTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(TopLevelMultipleTest, DesignIsElaborated) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_TRUE(m_design->getElaborated());
}

TEST_F(TopLevelMultipleTest, TopLevelInstancesAreTopAAndTopB) {
  EXPECT_EQ(namesOf(topInstances(), true), "top_a, top_b");
}

TEST_F(TopLevelMultipleTest, TopAHoldsInstanceM1OfMid) {
  const hldb::Module *const topA = findNamed(topInstances(), "top_a");
  ASSERT_NE(topA, nullptr) << "top-level instances: " << namesOf(topInstances(), true);
  const std::vector<const hldb::Module *> children = toVector(topA->getModules());
  const hldb::Module *const m1 = findNamed(children, "m1");
  ASSERT_NE(m1, nullptr) << "instances in top_a: " << namesOf(children, false);
  EXPECT_EQ(m1->getDefName(), "mid");
}

TEST_F(TopLevelMultipleTest, M1IsNotATopModule) {
  const hldb::Module *const m1 = findM1();
  ASSERT_NE(m1, nullptr);
  EXPECT_FALSE(m1->getTopModule());
}

TEST_F(TopLevelMultipleTest, M1HoldsInstanceL1OfLeaf) {
  const hldb::Module *const m1 = findM1();
  ASSERT_NE(m1, nullptr);
  const std::vector<const hldb::Module *> children = toVector(m1->getModules());
  const hldb::Module *const l1 = findNamed(children, "l1");
  ASSERT_NE(l1, nullptr) << "instances in m1: " << namesOf(children, false);
  EXPECT_EQ(l1->getDefName(), "leaf");
}

TEST_F(TopLevelMultipleTest, L1FullNameIsTopAM1L1) {
  const hldb::Module *const m1 = findM1();
  ASSERT_NE(m1, nullptr);
  const hldb::Module *const l1 = findNamed(toVector(m1->getModules()), "l1");
  ASSERT_NE(l1, nullptr);
  EXPECT_EQ(l1->getFullName(), "top_a.m1.l1");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
