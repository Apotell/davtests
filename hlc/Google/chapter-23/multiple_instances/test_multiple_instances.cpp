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
// holds its child instances in the relation module (vpiModule, 37.5), and
// each one has a vpiDefName (37.10).
//
// The test elaborates the compiled design with hldb::Elaborator, because
// instances exist only after elaboration (23.3).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors          no fatal, syntax or error event
//   DesignIsElaborated             the design is marked elaborated
//   TopIsTheOnlyTopLevelInstance   the top-level instances are exactly top
//   TopHoldsFf1Ff2Ff3              top's instances are ff1, ff2 and ff3, in
//                                  source order
//   AllThreeAreOfModuleFfnand      each of them has vpiDefName ffnand
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The port connections. Connecting by name is 23.3.2.2 (next batch).

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
constexpr std::string_view kSourceFileName = "23.3.2--multiple-instances.sv";

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

class MultipleInstancesTest : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "multiple_instances.hlc"});
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
};

TEST_F(MultipleInstancesTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(MultipleInstancesTest, DesignIsElaborated) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_TRUE(m_design->getElaborated());
}

TEST_F(MultipleInstancesTest, TopIsTheOnlyTopLevelInstance) { EXPECT_EQ(namesOf(topInstances()), "top"); }

TEST_F(MultipleInstancesTest, TopHoldsFf1Ff2Ff3) {
  const hldb::Module *const top = findNamed(topInstances(), "top");
  ASSERT_NE(top, nullptr) << "no top-level instance top";
  EXPECT_EQ(namesOf(toVector(top->getModules())), "ff1, ff2, ff3");
}

TEST_F(MultipleInstancesTest, AllThreeAreOfModuleFfnand) {
  const hldb::Module *const top = findNamed(topInstances(), "top");
  ASSERT_NE(top, nullptr) << "no top-level instance top";
  const std::vector<const hldb::Module *> children = toVector(top->getModules());
  ASSERT_EQ(children.size(), 3u) << "instances in top: " << namesOf(children);
  for (const hldb::Module *child : children) {
    EXPECT_EQ(child->getDefName(), "ffnand") << "instance " << child->getName();
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
