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

// Test for tests/Google/chapter-23/23.2.4--items-clocking-assertions.sv
//
// THE SOURCE
//
//   20: module m(input logic clk, input logic rst, input logic req, input logic gnt);
//   21:   default clocking cb @(posedge clk);
//   22:   endclocking
//   23:   default disable iff (rst);
//   24:   property p_req_gnt;
//   25:     req |=> gnt;
//   26:   endproperty
//   27:   a_req_gnt: assert property (p_req_gnt);
//   28: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "The module definition can contain zero or more module items." (23.2.4).
// Syntax 23-5: module_or_generate_item_declaration ::= ... |
// clocking_declaration | default clocking clocking_identifier ; | default
// disable iff expression_or_dist ;, and module_common_item ::= ... |
// assertion_item. The object model of a module (37.5) reaches them through
// the relations clocking block, default clocking and default disable iff;
// an instance (37.10) reaches assertions through the relation assertion.
// HLDB gives them as Module::getClockingBlocks(), getDefaultClocking(),
// getDefaultDisableIff() and Instance::getAssertions().
//   - cb is a clocking block and, being declared "default clocking", it is
//     the module's default clocking block.
//   - The default disable condition is rst.
//   - a_req_gnt is one concurrent assertion (an assert property).
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors       no fatal, syntax or error event
//   ClockingBlockCbIsTheDefault getClockingBlocks() holds cb, and
//                               getDefaultClocking() is cb
//   DefaultDisableIffIsRst      getDefaultDisableIff() refers to the net rst
//   OneAssertion                getAssertions() holds one Assert
//
// SKIPPED (GTEST_SKIP; the real assertion stays below the skip)
//
//   OneAssertion: HLC gives a_req_gnt only through getConcurrentAssertions();
//   the module's assertion relation (37.10), getAssertions(), is empty.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The property p_req_gnt itself. Properties are Clause 16.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/assert_stmt.h>
#include <hldb/clocking_block.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>

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

class ItemsClockingAssertionsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "items_clocking_assertions.hlc"}); }
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

TEST_F(ItemsClockingAssertionsTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(ItemsClockingAssertionsTest, ClockingBlockCbIsTheDefault) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  const hldb::ClockingBlock *const clockingBlock =
      hldb::findByName<hldb::ClockingBlock>("cb", module->getClockingBlocks());
  ASSERT_NE(clockingBlock, nullptr);
  EXPECT_EQ(module->getDefaultClocking(), clockingBlock);
}

TEST_F(ItemsClockingAssertionsTest, DefaultDisableIffIsRst) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  const hldb::RefObj *const reference = any_cast<hldb::RefObj>(module->getDefaultDisableIff());
  ASSERT_NE(reference, nullptr);
  EXPECT_EQ(reference->getActual(), hldb::findByName<hldb::Net>("rst", module->getNets()));
}

TEST_F(ItemsClockingAssertionsTest, OneAssertion) {
  GTEST_SKIP() << "HLC gives a_req_gnt only through getConcurrentAssertions(); the module's assertion relation, "
                  "getAssertions(), is empty (IEEE 1800-2023 37.10: a module reaches its assertions through "
                  "assertion)";
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  const hldb::AnyCollection *const assertions = module->getAssertions();
  ASSERT_EQ(sizeOf(assertions), 1u);
  EXPECT_EQ(typeName(assertions->front()), hldb::AnyTypeName(hldb::AnyType::Assert));
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
