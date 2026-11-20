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
// instance (37.10) reaches specparams through spec param (vpiSpecParam); a
// module (37.5) reaches path delays through mod path (vpiModPath), and a mod
// path reaches its delay expressions through vpiDelay (37.39).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error event
//   SpecparamsTRiseAndTFall    vpiSpecParam holds tRise (module body) and
//                              tFall (specify block)
//   OneModulePath              vpiModPath holds one path, a => b
//   PathDelayUsesTheSpecparams the path's two vpiDelay expressions resolve to
//                              the specparams tRise and tFall
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The delay values 5 and 6 as timing. Timing annotation is Clause 30 and
//     simulation.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/sv_vpi_user.h>
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
constexpr std::string_view kSourceFileName = "23.2.4--items-specify.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

// The objects 'object' has in the VPI relation 'relation', as vpi_iterate()
// or vpi_handle() would return them (37.3).
std::vector<const hldb::Any *> relationObjects(const hldb::Any *object, int32_t relation) {
  const hldb::Any::get_by_vpi_type_return_t result = object->getByVpiType(relation);
  if (const std::vector<const hldb::Any *> *const objects = std::get<2>(result)) return *objects;
  if (const hldb::Any *const single = std::get<1>(result)) return {single};
  return {};
}

// The object named 'name' among 'objects', or nullptr.
const hldb::Any *findNamed(const std::vector<const hldb::Any *> &objects, std::string_view name) {
  for (const hldb::Any *object : objects) {
    if (object->getName() == name) return object;
  }
  return nullptr;
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
  const std::vector<const hldb::Any *> specparams = relationObjects(module, vpiSpecParam);
  EXPECT_NE(findNamed(specparams, "tRise"), nullptr);
  EXPECT_NE(findNamed(specparams, "tFall"), nullptr);
}

TEST_F(ItemsSpecifyTest, OneModulePath) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(relationObjects(module, vpiModPath).size(), 1u);
}

TEST_F(ItemsSpecifyTest, PathDelayUsesTheSpecparams) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  const std::vector<const hldb::Any *> paths = relationObjects(module, vpiModPath);
  ASSERT_EQ(paths.size(), 1u);
  const std::vector<const hldb::Any *> delays = relationObjects(paths.front(), vpiDelay);
  ASSERT_EQ(delays.size(), 2u);
  const std::vector<std::string_view> names = {"tRise", "tFall"};
  for (size_t index = 0; index < names.size(); ++index) {
    const hldb::RefObj *const reference = any_cast<hldb::RefObj>(delays[index]);
    ASSERT_NE(reference, nullptr) << names[index];
    const hldb::Any *const actual = reference->getActual();
    ASSERT_NE(actual, nullptr) << names[index] << " is not bound";
    EXPECT_EQ(actual->getVpiType(), static_cast<uint32_t>(vpiSpecParam)) << names[index];
    EXPECT_EQ(actual->getName(), names[index]);
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
