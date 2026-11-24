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
// Instances exist only after elaboration (23.3). The HLC install of
// 2026-11-23 no longer gives tests an elaborator (hldb::Elaborator was
// removed), so the test checks the design as Compile() leaves it; the
// instance checks fail until HLC builds the instance tree itself.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors          no fatal, syntax or error event
//   DesignIsElaborated             the design is marked elaborated
//   TopHoldsModuleArrayU           top holds a module array named u
//   ArrayUHasSize4                 u's range [3:0] spans 4 elements (bounds
//                                  read with ExprEval::getInt64())
//   ArrayUHoldsFourCellMs          u holds 4 instances, each of module cell_m
//   ElementsAreArrayMembers        each element's getArrayMember() is TRUE
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - How a and y are split over the four instances. Port connections to
//     arrays of instances are 23.3.3.5 (a later batch).

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/ExprEval.h>
#include <hldb/design.h>
#include <hldb/instance.h>
#include <hldb/module.h>
#include <hldb/module_array.h>
#include <hldb/range.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.3.2--instance-array.sv";

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

  // The top-level instances from the file under test (37.5 detail 1).
  static std::vector<const hldb::Module *> topInstances() {
    std::vector<const hldb::Module *> result;
    if (m_design == nullptr) return result;
    for (const hldb::Module *module : toVector(m_design->getTopModules())) {
      if (endsWith(module->getFile(), kSourceFileName)) result.emplace_back(module);
    }
    return result;
  }

  // Module array u inside top-level instance top, or nullptr.
  static const hldb::ModuleArray *findArrayU() {
    const hldb::Module *const top = findNamed(topInstances(), "top");
    return (top == nullptr) ? nullptr : findNamed(toVector(top->getModuleArrays()), "u");
  }
};

TEST_F(InstanceArrayTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(InstanceArrayTest, DesignIsElaborated) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_TRUE(m_design->getElaborated());
}

TEST_F(InstanceArrayTest, TopHoldsModuleArrayU) {
  const hldb::Module *const top = findNamed(topInstances(), "top");
  ASSERT_NE(top, nullptr) << "no top-level instance top";
  EXPECT_NE(findNamed(toVector(top->getModuleArrays()), "u"), nullptr);
}

TEST_F(InstanceArrayTest, ArrayUHasSize4) {
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

TEST_F(InstanceArrayTest, ArrayUHoldsFourCellMs) {
  const hldb::ModuleArray *const array = findArrayU();
  ASSERT_NE(array, nullptr) << "no module array top.u";
  const std::vector<const hldb::Instance *> elements = toVector(array->getInstances());
  ASSERT_EQ(elements.size(), 4u);
  for (const hldb::Instance *element : elements) EXPECT_EQ(element->getDefName(), "cell_m");
}

TEST_F(InstanceArrayTest, ElementsAreArrayMembers) {
  const hldb::ModuleArray *const array = findArrayU();
  ASSERT_NE(array, nullptr) << "no module array top.u";
  const std::vector<const hldb::Instance *> elements = toVector(array->getInstances());
  ASSERT_FALSE(elements.empty());
  for (const hldb::Instance *element : elements) EXPECT_TRUE(element->getArrayMember());
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
