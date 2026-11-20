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

// Test for tests/Google/chapter-23/23.2.4--items-behavior.sv
//
// THE SOURCE
//
//   19: module m(input logic clk, input logic d);
//   20:   wire n1, n2;
//   21:   alias n1 = n2;
//   22:   wire w;
//   23:   assign w = d;
//   24:   logic a, b, c, e, f;
//   25:   initial a = 1'b0;
//   26:   final a = 1'b1;
//   27:   always @(posedge clk) b <= d;
//   28:   always_comb c = d;
//   29:   always_ff @(posedge clk) e <= d;
//   30:   always_latch if (clk) f <= d;
//   31: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "The module definition can contain zero or more module items." (23.2.4).
// Syntax 23-5 lists, among module_common_item, net_alias, continuous_assign,
// initial_construct, final_construct and always_construct. The object model of
// a module (37.5) reaches them through the relations alias stmt (vpiAliasStmt),
// cont assign (vpiContAssign) and process (vpiProcess). The four always forms
// are told apart by vpiAlwaysType: vpiAlways, vpiAlwaysComb, vpiAlwaysFF and
// vpiAlwaysLatch (sv_vpi_user.h).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error event
//   OneAliasStatement          vpiAliasStmt holds one object
//   OneContinuousAssign        vpiContAssign holds one object
//   SixProcessesInOrder        vpiProcess holds initial, final, then the four
//                              always constructs with their vpiAlwaysType
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - What the processes do. Their statements belong to Clauses 9 and 10.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>
#include <hldb/module.h>
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
constexpr std::string_view kSourceFileName = "23.2.4--items-behavior.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

int64_t integerProperty(const hldb::Any *object, int32_t property) {
  const hldb::Any::vpi_property_value_t value = object->getVpiPropertyValue(property);
  if (const int64_t *const number = std::get_if<int64_t>(&value)) return *number;
  return vpiUndefined;
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

class ItemsBehaviorTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "items_behavior.hlc"}); }
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

TEST_F(ItemsBehaviorTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(ItemsBehaviorTest, OneAliasStatement) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(relationObjects(module, vpiAliasStmt).size(), 1u);
}

TEST_F(ItemsBehaviorTest, OneContinuousAssign) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(relationObjects(module, vpiContAssign).size(), 1u);
}

TEST_F(ItemsBehaviorTest, SixProcessesInOrder) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  const std::vector<const hldb::Any *> processes = relationObjects(module, vpiProcess);
  ASSERT_EQ(processes.size(), 6u);
  EXPECT_EQ(processes[0]->getVpiType(), static_cast<uint32_t>(vpiInitial));
  EXPECT_EQ(processes[1]->getVpiType(), static_cast<uint32_t>(vpiFinal));
  const std::vector<int64_t> alwaysTypes = {vpiAlways, vpiAlwaysComb, vpiAlwaysFF, vpiAlwaysLatch};
  for (size_t index = 0; index < alwaysTypes.size(); ++index) {
    const hldb::Any *const process = processes[index + 2];
    EXPECT_EQ(process->getVpiType(), static_cast<uint32_t>(vpiAlways)) << index;
    EXPECT_EQ(integerProperty(process, vpiAlwaysType), alwaysTypes[index]) << index;
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
