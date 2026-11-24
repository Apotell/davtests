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

// Test for tests/Google/chapter-23/23.3.1--top-level-ungenerated-instance.sv
//
// THE SOURCE
//
//   20: module child;
//   21: endmodule
//   22:
//   23: module top;
//   24:   if (0) begin : g
//   25:     child c1 ();
//   26:   end
//   27: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "Top-level modules are modules that are included in the SystemVerilog
// source text, but do not appear in any module instantiation statement, as
// described in 23.3.2. This applies even if the module instantiation appears
// in a generate block that is not itself instantiated (see 27.3)." (23.3.1).
// The condition of the if-generate on line 24 is 0, so block g is not
// instantiated, but child still appears in the instantiation statement on
// line 25. So top is the only top-level module, and there is no instance of
// child at all. Top-level modules are reached by Design::getTopModules()
// (37.5 detail 1).
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
//   ChildIsNotATopLevelInstance    no top-level instance is named child
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The generate block itself (whether g is instantiated). Conditional
//     generate constructs are 27.5, not 23.3.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

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
constexpr std::string_view kSourceFileName = "23.3.1--top-level-ungenerated-instance.sv";

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

// The names of 'objects', comma-separated, sorted.
template <typename T>
std::string namesOf(const std::vector<const T *> &objects) {
  std::vector<std::string> names;
  for (const T *object : objects) names.emplace_back(object->getName());
  std::sort(names.begin(), names.end());
  std::string result;
  for (const std::string &name : names) {
    if (!result.empty()) result += ", ";
    result += name;
  }
  return result;
}
}  // namespace

class TopLevelUngeneratedInstanceTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "top_level_ungenerated_instance.hlc"}); }
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
};

TEST_F(TopLevelUngeneratedInstanceTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(TopLevelUngeneratedInstanceTest, DesignIsElaborated) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_TRUE(m_design->getElaborated());
}

TEST_F(TopLevelUngeneratedInstanceTest, TopIsTheOnlyTopLevelInstance) { EXPECT_EQ(namesOf(topInstances()), "top"); }

TEST_F(TopLevelUngeneratedInstanceTest, ChildIsNotATopLevelInstance) {
  const std::vector<const hldb::Module *> tops = topInstances();
  // Without any top-level instance the check below would pass vacuously.
  ASSERT_FALSE(tops.empty()) << "no top-level instance at all";
  EXPECT_EQ(findNamed(tops, "child"), nullptr) << "top-level instances: " << namesOf(tops);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
