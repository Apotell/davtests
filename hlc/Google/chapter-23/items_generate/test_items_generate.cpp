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

// Test for tests/Google/chapter-23/23.2.4--items-generate.sv
//
// THE SOURCE
//
//   20: module m;
//   21:   generate
//   22:     for (genvar i = 0; i < 2; i++) begin : g_loop
//   23:       wire w;
//   24:     end
//   25:   endgenerate
//   26:   if (1) begin : g_if
//   27:     wire x;
//   28:   end
//   29:   case (1)
//   30:     1: begin : g_case
//   31:       wire y;
//   32:     end
//   33:   endcase
//   34: endmodule
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// "The module definition can contain zero or more module items." (23.2.4).
// Syntax 23-5: non_port_module_item ::= generate_region | ..., and
// module_common_item ::= ... | loop_generate_construct |
// conditional_generate_construct. The file uses a generate region holding a
// loop generate construct, an if generate construct and a case generate
// construct, all legal module items.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors      no fatal, syntax or error event
//   ModuleMIsDeclared          the module m is defined
//   ThreeGenerateConstructs    getGenStmts() holds three objects: one
//                              GenRegion, one GenIf and one GenCase
//   LoopGenerateGLoop          the GenFor in the region declares the genvar
//                              i in its initialization; its condition and
//                              increment refer to i; its block g_loop
//                              declares the net w
//   IfGenerateGIf              condition 1; its block g_if declares x
//   CaseGenerateGCase          condition 1, one item labeled 1; its block
//                              g_case declares y
//   GenerateBlocksAfterElaboration  elaborated model only: the gen scope
//                              array g_loop holds two gen scopes, g_if and
//                              g_case one each
//
// SKIPPED (GTEST_SKIP; the real assertion stays below the skip)
//
//   GenerateBlocksAfterElaboration: HLC builds a definition model only, so
//   no generate construct of m is unrolled or selected.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - That i is a genvar (37.12, vpiGenVar). HLDB has no genvar class, so
//     only i's name and the references to it are checked.
//   - The order of getGenStmts(). The object model does not define one.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/case_item.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_block.h>
#include <hldb/gen_case.h>
#include <hldb/gen_for.h>
#include <hldb/gen_if.h>
#include <hldb/gen_region.h>
#include <hldb/gen_scope.h>
#include <hldb/gen_scope_array.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hlc {
namespace {
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

class ItemsGenerateTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "items_generate.hlc"}); }
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

  // The number of generate statements of m (getGenStmts()) of the class 'type'.
  static size_t countGenStmts(hldb::AnyType type) {
    const hldb::Module *const module = findModule("m");
    if ((module == nullptr) || (module->getGenStmts() == nullptr)) return 0;
    return static_cast<size_t>(std::count_if(module->getGenStmts()->cbegin(), module->getGenStmts()->cend(),
                                             [type](const hldb::Any *stmt) { return stmt->getAnyType() == type; }));
  }

  // The first generate statement of m (getGenStmts()) of the class 'type', or nullptr.
  static const hldb::Any *findGenStmt(hldb::AnyType type) {
    const hldb::Module *const module = findModule("m");
    if ((module == nullptr) || (module->getGenStmts() == nullptr)) return nullptr;
    for (const hldb::Any *stmt : *module->getGenStmts()) {
      if (stmt->getAnyType() == type) return stmt;
    }
    return nullptr;
  }

  // The loop generate construct inside the generate region of m, or nullptr.
  static const hldb::GenFor *findLoop() {
    const hldb::GenRegion *const region = any_cast<hldb::GenRegion>(findGenStmt(hldb::AnyType::GenRegion));
    if ((region == nullptr) || (region->getStmts() == nullptr) || region->getStmts()->empty()) return nullptr;
    return any_cast<hldb::GenFor>(region->getStmts()->front());
  }
};

TEST_F(ItemsGenerateTest, CompilesWithoutErrors) { EXPECT_EQ(allEvents(), ""); }

TEST_F(ItemsGenerateTest, ModuleMIsDeclared) { EXPECT_NE(findModule("m"), nullptr); }

// The three generate constructs of m (Syntax 23-5; 27.2): one generate region, one if and one case generate
// construct.
TEST_F(ItemsGenerateTest, ThreeGenerateConstructs) {
  const hldb::Module *const module = findModule("m");
  ASSERT_NE(module, nullptr);
  ASSERT_NE(module->getGenStmts(), nullptr);
  EXPECT_EQ(module->getGenStmts()->size(), 3u);
  EXPECT_EQ(countGenStmts(hldb::AnyType::GenRegion), 1u) << "generate ... endgenerate";
  EXPECT_EQ(countGenStmts(hldb::AnyType::GenIf), 1u) << "if (1)";
  EXPECT_EQ(countGenStmts(hldb::AnyType::GenCase), 1u) << "case (1)";
}

// "for (genvar i = 0; i < 2; i++) begin : g_loop" (27.4): the loop declares the genvar i, and its initialization,
// condition and increment all refer to it. The genvar is reached through the initialization, so the check does
// not depend on how the genvar is modeled: the object model has a genvar class of its own (37.12, vpiGenVar).
TEST_F(ItemsGenerateTest, LoopGenerateGLoop) {
  const hldb::GenFor *const loop = findLoop();
  ASSERT_NE(loop, nullptr);
  ASSERT_NE(loop->getForInitStmts(), nullptr);
  ASSERT_EQ(loop->getForInitStmts()->size(), 1u);
  const hldb::Assignment *const init = any_cast<hldb::Assignment>(loop->getForInitStmts()->front());
  ASSERT_NE(init, nullptr);
  const hldb::Any *genvar = init->getLhs();
  if (const hldb::RefObj *const reference = any_cast<hldb::RefObj>(genvar)) genvar = reference->getActual();
  ASSERT_NE(genvar, nullptr) << "the genvar i is missing";
  EXPECT_EQ(genvar->getName(), std::string_view("i"));
  EXPECT_TRUE(refersTo(loop->getCondition(), genvar)) << "i < 2";
  ASSERT_NE(loop->getForIncStmts(), nullptr);
  ASSERT_EQ(loop->getForIncStmts()->size(), 1u);
  EXPECT_TRUE(refersTo(loop->getForIncStmts()->front(), genvar)) << "i++";
  const hldb::GenBlock *const block = loop->getStmt<hldb::GenBlock>();
  ASSERT_NE(block, nullptr);
  EXPECT_EQ(block->getName(), std::string_view("g_loop"));
  EXPECT_NE(hldb::findByName<hldb::Net>("w", block->getNets()), nullptr);
}

// "if (1) begin : g_if" (27.5).
TEST_F(ItemsGenerateTest, IfGenerateGIf) {
  const hldb::GenIf *const genIf = any_cast<hldb::GenIf>(findGenStmt(hldb::AnyType::GenIf));
  ASSERT_NE(genIf, nullptr);
  const hldb::Constant *const condition = genIf->getCondition<hldb::Constant>();
  ASSERT_NE(condition, nullptr);
  EXPECT_EQ(condition->getDecompile(), std::string_view("1"));
  const hldb::GenBlock *const block = genIf->getStmt<hldb::GenBlock>();
  ASSERT_NE(block, nullptr);
  EXPECT_EQ(block->getName(), std::string_view("g_if"));
  EXPECT_NE(hldb::findByName<hldb::Net>("x", block->getNets()), nullptr);
}

// "case (1) 1: begin : g_case" (27.5).
TEST_F(ItemsGenerateTest, CaseGenerateGCase) {
  const hldb::GenCase *const genCase = any_cast<hldb::GenCase>(findGenStmt(hldb::AnyType::GenCase));
  ASSERT_NE(genCase, nullptr);
  const hldb::Constant *const condition = genCase->getCondition<hldb::Constant>();
  ASSERT_NE(condition, nullptr);
  EXPECT_EQ(condition->getDecompile(), std::string_view("1"));
  ASSERT_NE(genCase->getCaseItems(), nullptr);
  ASSERT_EQ(genCase->getCaseItems()->size(), 1u);
  const hldb::CaseItem *const item = genCase->getCaseItems()->front();
  ASSERT_NE(item->getExprs(), nullptr);
  ASSERT_EQ(item->getExprs()->size(), 1u);
  const hldb::Constant *const label = any_cast<hldb::Constant>(item->getExprs()->front());
  ASSERT_NE(label, nullptr);
  EXPECT_EQ(label->getDecompile(), std::string_view("1"));
  const hldb::GenBlock *const block = item->getStmt<hldb::GenBlock>();
  ASSERT_NE(block, nullptr);
  EXPECT_EQ(block->getName(), std::string_view("g_case"));
  EXPECT_NE(hldb::findByName<hldb::Net>("y", block->getNets()), nullptr);
}

// Elaborated model only: the loop unrolls into the two blocks g_loop[0] and g_loop[1] (27.4), and the true
// conditions select g_if and g_case (27.5). Each named generate block is a gen scope array whose size is "the
// number of elements in the array" (37.13 detail 1): g_loop holds two gen scopes, g_if and g_case one each.
TEST_F(ItemsGenerateTest, GenerateBlocksAfterElaboration) {
  ASSERT_NE(m_design, nullptr);
  if (!m_design->getElaborated()) {
    GTEST_SKIP() << "HLC builds a definition model only, with no instance of m whose generate constructs are "
                    "unrolled or selected (IEEE 1800-2023 27.4, 27.5, 37.13)";
  }
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("m", m_design->getTopModules());
  ASSERT_NE(top, nullptr);
  const std::vector<std::pair<std::string_view, size_t>> expected = {{"g_loop", 2}, {"g_if", 1}, {"g_case", 1}};
  for (const std::pair<std::string_view, size_t> &entry : expected) {
    const hldb::GenScopeArray *const array =
        hldb::findByName<hldb::GenScopeArray>(entry.first, top->getGenScopeArrays());
    ASSERT_NE(array, nullptr) << entry.first;
    ASSERT_NE(array->getGenScopes(), nullptr) << entry.first;
    EXPECT_EQ(array->getGenScopes()->size(), entry.second) << entry.first;
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
