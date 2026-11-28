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
//   AliasBindsN1AndN2          the alias's two sides resolve to the nets n1
//                              and n2
//   OneContinuousAssign        getContAssigns() holds one object
//   AssignBindsWToD            the assign's left side resolves to the net w,
//                              its right side to the net d
//   SixProcesses               getProcesses() holds one Initial, one
//                              FinalStmt and four Always objects
//   AlwaysKinds                Always::getAlwaysType() of the four is
//                              vpiAlways, vpiAlwaysComb, vpiAlwaysFF and
//                              vpiAlwaysLatch, one each
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - What the processes do. Their statements belong to Clauses 9 and 10.
//   - The order of getProcesses(). The object model does not define one.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/alias_stmt.h>
#include <hldb/always.h>
#include <hldb/any_type.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/process_stmt.h>
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
// The number of objects in 'collection'; zero for a null collection.
template <typename T>
size_t sizeOf(const std::vector<T *> *collection) {
  return (collection == nullptr) ? 0 : collection->size();
}

// True when 'expression' is 'target', a reference that resolves to it, or an operation with such an operand.
bool refersTo(const hldb::Any *expression, const hldb::Any *target) {
  if ((expression == nullptr) || (target == nullptr)) return false;
  if (expression == target) return true;
  if (const hldb::RefObj *const reference = any_cast<hldb::RefObj>(expression)) {
    return reference->getActual() == target;
  }
  if (const hldb::Operation *const operation = any_cast<hldb::Operation>(expression)) {
    if (operation->getOperands() == nullptr) return false;
    for (const hldb::Any *operand : *operation->getOperands()) {
      if (refersTo(operand, target)) return true;
    }
  }
  return false;
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

  // The definition of the module named 'name', or nullptr.
  static const hldb::Module *findModule(std::string_view name) {
    if (m_design == nullptr) return nullptr;
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }
};

TEST_F(ItemsBehaviorTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(ItemsBehaviorTest, OneAliasStatement) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(sizeOf(module->getAliases()), 1u);
}

// "alias n1 = n2;" (10.11) binds the two nets n1 and n2.
TEST_F(ItemsBehaviorTest, AliasBindsN1AndN2) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  ASSERT_EQ(sizeOf(module->getAliases()), 1u);
  const hldb::Alias *const alias = module->getAliases()->front();
  const hldb::Net *const n1 = hldb::findByName<hldb::Net>("n1", module->getNets());
  const hldb::Net *const n2 = hldb::findByName<hldb::Net>("n2", module->getNets());
  ASSERT_NE(n1, nullptr);
  ASSERT_NE(n2, nullptr);
  EXPECT_TRUE(refersTo(alias->getLhs(), n1)) << "n1";
  ASSERT_EQ(sizeOf(alias->getRhs()), 1u);
  EXPECT_TRUE(refersTo(alias->getRhs()->front(), n2)) << "n2";
}

TEST_F(ItemsBehaviorTest, OneContinuousAssign) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(sizeOf(module->getContAssigns()), 1u);
}

// "assign w = d;" (10.3) drives the net w from the port d's net.
TEST_F(ItemsBehaviorTest, AssignBindsWToD) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  ASSERT_EQ(sizeOf(module->getContAssigns()), 1u);
  const hldb::ContAssign *const assign = module->getContAssigns()->front();
  EXPECT_TRUE(refersTo(assign->getLhs(), hldb::findByName<hldb::Net>("w", module->getNets()))) << "w";
  EXPECT_TRUE(refersTo(assign->getRhs(), hldb::findByName<hldb::Net>("d", module->getNets()))) << "d";
}

// One initial, one final and four always constructs (9.2). The object model does not define the order of the
// processes, so they are counted by class.
TEST_F(ItemsBehaviorTest, SixProcesses) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  const hldb::ProcessCollection *const processes = module->getProcesses();
  ASSERT_NE(processes, nullptr);
  ASSERT_EQ(processes->size(), 6u);
  const auto count = [processes](hldb::AnyType type) {
    return std::count_if(processes->cbegin(), processes->cend(),
                         [type](const hldb::Any *process) { return process->getAnyType() == type; });
  };
  EXPECT_EQ(count(hldb::AnyType::Initial), 1);
  EXPECT_EQ(count(hldb::AnyType::FinalStmt), 1);
  EXPECT_EQ(count(hldb::AnyType::Always), 4);
}

// The four always constructs are an always, an always_comb, an always_ff and an always_latch (9.2.2), one each.
TEST_F(ItemsBehaviorTest, AlwaysKinds) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  const hldb::ProcessCollection *const processes = module->getProcesses();
  ASSERT_NE(processes, nullptr);
  std::vector<int32_t> kinds;
  for (const hldb::Any *process : *processes) {
    if (const hldb::Always *const always = any_cast<hldb::Always>(process)) kinds.emplace_back(always->getAlwaysType());
  }
  std::sort(kinds.begin(), kinds.end());
  std::vector<int32_t> expected = {vpiAlways, vpiAlwaysComb, vpiAlwaysFF, vpiAlwaysLatch};
  std::sort(expected.begin(), expected.end());
  EXPECT_EQ(kinds, expected);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
