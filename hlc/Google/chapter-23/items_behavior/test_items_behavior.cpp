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
// a module (37.5) reaches them through the relations alias stmt, cont assign
// and process; HLDB gives them as Module::getAliases(), getContAssigns() and
// getProcesses().
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error event
//   OneAliasStatement          getAliases() holds one object
//   OneContinuousAssign        getContAssigns() holds one object
//   SixProcessesInOrder        getProcesses() holds an Initial, a FinalStmt,
//                              then four Always objects
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - What the processes do. Their statements belong to Clauses 9 and 10.
//   - Which always form each Always is (always, always_comb, always_ff,
//     always_latch). HLDB gives it only as the integer code returned by
//     Always::getAlwaysType() and has no named values of its own for it, so it
//     can be checked once HLDB provides such names.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/alias_stmt.h>
#include <hldb/any_type.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/process_stmt.h>

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
  EXPECT_EQ(sizeOf(module->getAliases()), 1u);
}

TEST_F(ItemsBehaviorTest, OneContinuousAssign) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(sizeOf(module->getContAssigns()), 1u);
}

TEST_F(ItemsBehaviorTest, SixProcessesInOrder) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  const hldb::ProcessCollection *const processes = module->getProcesses();
  ASSERT_NE(processes, nullptr);
  ASSERT_EQ(processes->size(), 6u);
  EXPECT_EQ(typeName((*processes)[0]), hldb::AnyTypeName(hldb::AnyType::Initial));
  EXPECT_EQ(typeName((*processes)[1]), hldb::AnyTypeName(hldb::AnyType::FinalStmt));
  for (size_t index = 2; index < processes->size(); ++index) {
    EXPECT_EQ(typeName((*processes)[index]), hldb::AnyTypeName(hldb::AnyType::Always)) << index;
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
