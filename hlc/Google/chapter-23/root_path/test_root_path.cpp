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

// Test for tests/Google/chapter-23/23.3.1--root-path.sv
//
// THE SOURCE
//
//   20: module leaf;
//   21:   logic x;
//   22: endmodule
//   23:
//   24: module B;
//   25:   leaf C ();
//   26: endmodule
//   27:
//   28: module A;
//   29:   B B ();
//   30: endmodule
//   31:
//   32: module A2;
//   33:   B B ();
//   34: endmodule
//   35:
//   36: module top;
//   37:   logic l, r;
//   38:   A2 A ();
//   39:   assign l = A.B.C.x;
//   40:   assign r = $root.A.B.C.x;
//   41: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "The name $root is used to unambiguously refer to a top-level instance or
// to an instance path starting from the root of the instantiation tree."
// (23.3.1). "A.B.C can mean the local A.B.C or the top-level A.B.C
// (assuming there is an instance A that contains an instance B at both the
// top level and in the current module). The ambiguity is resolved by giving
// priority to the local scope and thereby preventing access to the
// top-level path. $root allows explicit access to the top level in those
// cases" (23.3.1). Module A is never instantiated, so it is a top-level
// instance named A; top also holds a local instance named A (of A2). So on
// line 39 A.B.C.x is the x in top.A.B.C, and on line 40 $root.A.B.C.x is
// the x in the top-level A.B.C. Each instance has its own x, and vpiFullName
// gives an object's hierarchical name (37.10). The references are reached
// through the continuous assignments of the top instance (37.5, relation
// cont assign).
//
// The test elaborates the compiled design with hldb::Elaborator, because
// instances exist only after elaboration (23.3).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors          no fatal, syntax or error event
//   DesignIsElaborated             the design is marked elaborated
//   TopLevelInstancesAreAAndTop    the top-level instances are exactly A
//                                  and top
//   LocalPathIsTopABCX             line 39's reference is top.A.B.C.x
//   RootPathIsABCX                 line 40's reference is A.B.C.x
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The prefix objects of the paths. The LRM gives no object model for
//     them; the object a path refers to is what 23.3.1 defines.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Database.h>
#include <hldb/Elaborator.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.3.1--root-path.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

std::string stringProperty(const hldb::Any *object, int32_t property) {
  const hldb::Any::vpi_property_value_t value = object->getVpiPropertyValue(property);
  if (const char *const *const text = std::get_if<const char *>(&value)) {
    return (*text == nullptr) ? std::string("<null>") : std::string(*text);
  }
  return "<not a string>";
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

class RootPathTest : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "root_path.hlc"});
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

  // The vpiFullName of the object that the right-hand side of the top
  // instance's 'assign <lhsName> = ...' refers to, or a placeholder that
  // says what is missing.
  static std::string rhsTargetFullName(std::string_view lhsName) {
    const hldb::Module *const top = findNamed(topInstances(), "top");
    if (top == nullptr) return "<no top-level instance top>";
    for (const hldb::ContAssign *assign : toVector(top->getContAssigns())) {
      if ((assign->getLhs() == nullptr) || (assign->getLhs()->getName() != lhsName)) continue;
      const hldb::RefObj *const reference = assign->getRhs<hldb::RefObj>();
      if (reference == nullptr) return "<right-hand side is not a reference>";
      if (reference->getActual() == nullptr) return "<reference is not bound>";
      return stringProperty(reference->getActual(), vpiFullName);
    }
    return "<no continuous assignment to " + std::string(lhsName) + ">";
  }
};

TEST_F(RootPathTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(RootPathTest, DesignIsElaborated) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_TRUE(m_design->getElaborated());
}

TEST_F(RootPathTest, TopLevelInstancesAreAAndTop) { EXPECT_EQ(namesOf(topInstances()), "A, top"); }

TEST_F(RootPathTest, LocalPathIsTopABCX) { EXPECT_EQ(rhsTargetFullName("l"), "top.A.B.C.x"); }

TEST_F(RootPathTest, RootPathIsABCX) { EXPECT_EQ(rhsTargetFullName("r"), "A.B.C.x"); }
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
