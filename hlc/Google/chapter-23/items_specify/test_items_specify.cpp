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

// Test for tests/Google/chapter-23/23.2.4--items-specify.sv
//
// THE SOURCE
//
//   19: module m(input a, output b);
//   20:   specparam tRise = 5;
//   21:   specify
//   22:     specparam tFall = 6;
//   23:     (a => b) = (tRise, tFall);
//   24:   endspecify
//   25:   assign b = a;
//   26: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "The module definition can contain zero or more module items." (23.2.4).
// Syntax 23-5: non_port_module_item ::= ... | specify_block | {
// attribute_instance } specparam_declaration | .... "Specify parameters (also
// called specparams) are permitted both within the specify block (see Clause
// 30) and in the main module body", and they "can appear in any expression
// that is not assigned to a parameter" (6.20.5). The object model of an
// instance (37.10) reaches specparams through the relation spec param; a
// module (37.5) reaches path delays through the relation mod path, and a mod
// path reaches its delay expressions through the relation delay (37.39). HLDB
// gives them as Instance::getSpecParams(), Module::getModPaths() and
// ModPath::getDelays().
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error event
//   SpecparamsTRiseAndTFall    getSpecParams() holds tRise (module body) and
//                              tFall (specify block)
//   OneModulePath              getModPaths() holds one path, a => b
//   PathDelayUsesTheSpecparams the path's two getDelays() expressions resolve
//                              to the specparams tRise and tFall
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The delay values 5 and 6 as timing. Timing annotation is Clause 30 and
//     simulation.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/any_type.h>
#include <hldb/design.h>
#include <hldb/mod_path.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/spec_param.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "23.2.4--items-specify.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

// The object named 'name' in 'collection', or nullptr.
template <typename T>
const T *findNamed(const std::vector<T *> *collection, std::string_view name) {
  if (collection == nullptr) return nullptr;
  for (const T *object : *collection) {
    if (object->getName() == name) return object;
  }
  return nullptr;
}

// The number of objects in 'collection'; zero for a null collection.
template <typename T>
size_t sizeOf(const std::vector<T *> *collection) {
  return (collection == nullptr) ? 0 : collection->size();
}

// The type name of 'object' (getAnyType()), as hldb::AnyTypeName() gives it.
std::string_view typeName(const hldb::Any *object) { return hldb::AnyTypeName(object->getAnyType()); }

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
}  // namespace

class ItemsSpecifyTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "items_specify.hlc"}); }
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

  // The module named 'name' defined in the file under test, or nullptr.
  static const hldb::Module *findModule(std::string_view name) {
    if ((m_design == nullptr) || (m_design->getAllModules() == nullptr)) return nullptr;
    for (const hldb::Module *module : *m_design->getAllModules()) {
      if (endsWith(module->getFile(), kSourceFileName) && (module->getName() == name)) return module;
    }
    return nullptr;
  }
};

TEST_F(ItemsSpecifyTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(ItemsSpecifyTest, SpecparamsTRiseAndTFall) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  EXPECT_NE(findNamed(module->getSpecParams(), "tRise"), nullptr);
  EXPECT_NE(findNamed(module->getSpecParams(), "tFall"), nullptr);
}

TEST_F(ItemsSpecifyTest, OneModulePath) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(sizeOf(module->getModPaths()), 1u);
}

TEST_F(ItemsSpecifyTest, PathDelayUsesTheSpecparams) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  ASSERT_EQ(sizeOf(module->getModPaths()), 1u);
  const hldb::ExprCollection *const delays = module->getModPaths()->front()->getDelays();
  ASSERT_EQ(sizeOf(delays), 2u);
  const std::vector<std::string_view> names = {"tRise", "tFall"};
  for (size_t index = 0; index < names.size(); ++index) {
    const hldb::RefObj *const reference = any_cast<hldb::RefObj>((*delays)[index]);
    ASSERT_NE(reference, nullptr) << names[index];
    const hldb::Any *const actual = reference->getActual();
    ASSERT_NE(actual, nullptr) << names[index] << " is not bound";
    EXPECT_EQ(typeName(actual), hldb::AnyTypeName(hldb::AnyType::SpecParam)) << names[index];
    EXPECT_EQ(actual->getName(), names[index]);
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
