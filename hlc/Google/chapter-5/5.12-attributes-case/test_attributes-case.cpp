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

// Validates three forms of case-statement attribute annotation in UHDM:
//
//   (* full_case, parallel_case *)   case (a) ...   endcase
//   (* full_case = 1 *)
//   (* parallel_case = 1 *)          case (a) ...   endcase
//   (* full_case, parallel_case = 0 *) case (a) ...   endcase
//
// Attribute attachment rules (confirmed from UHDM dump):
//   - "flag" attribute (no = value):     getValue() returns nullptr
//   - "value" attribute (= expr):        getValue() returns Constant
//   - parallel_case bare/valued attrs attach to the parent Begin scope;
//     full_case attrs attach to the CaseStmt directly.
//   - All three case statements have vpiCaseType == 1 (exact / plain "case").

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/assignment.h>
#include <uhdm/attribute.h>
#include <uhdm/begin.h>
#include <uhdm/case_item.h>
#include <uhdm/case_stmt.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/initial.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/ref_obj.h>

namespace SURELOG {

class AttributesCase : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.12-attributes-case.hlc"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

// Helpers ----------------------------------------------------------------

static const uhdm::Module *getTop(const uhdm::Design *d) {
  return uhdm::findByName<uhdm::Module>("work@top", d->getAllModules());
}

static const uhdm::Begin *getInitialBegin(const uhdm::Module *top) {
  if (!top->getProcesses()) return nullptr;
  for (const uhdm::Process *const p : *top->getProcesses()) {
    if (const uhdm::Initial *const i = any_cast<uhdm::Initial>(p))
      return i->getStmt<uhdm::Begin>();
  }
  return nullptr;
}

// Returns the Nth CaseStmt from the Begin block (0-based, skips non-CaseStmt stmts).
static const uhdm::CaseStmt *getNthCase(const uhdm::Begin *blk, size_t n) {
  if (!blk || !blk->getStmts()) return nullptr;
  size_t idx = 0;
  for (const uhdm::Any *const s : *blk->getStmts()) {
    if (const uhdm::CaseStmt *const cs = any_cast<uhdm::CaseStmt>(s)) {
      if (idx++ == n) return cs;
    }
  }
  return nullptr;
}

// Returns the named Attribute from a CaseStmt, or nullptr if absent.
static const uhdm::Attribute *findAttr(const uhdm::CaseStmt *cs,
                                        std::string_view name) {
  if (!cs->getAttributes()) return nullptr;
  for (const uhdm::Attribute *const a : *cs->getAttributes()) {
    if (a->getName() == name) return a;
  }
  return nullptr;
}

// ---------------------------------------------------------------------------
// Module and nets
// ---------------------------------------------------------------------------
TEST_F(AttributesCase, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr);
}

TEST_F(AttributesCase, NetsAAndBExist) {
  const uhdm::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);

  bool hasA = false, hasB = false;
  for (const uhdm::Net *const n : *top->getNets()) {
    if (n->getName() == "a") hasA = true;
    if (n->getName() == "b") hasB = true;
  }
  EXPECT_TRUE(hasA) << "net 'a' not found";
  EXPECT_TRUE(hasB) << "net 'b' not found";
}

// ---------------------------------------------------------------------------
// Begin block contains exactly 3 CaseStmt nodes
// ---------------------------------------------------------------------------
TEST_F(AttributesCase, InitialBlockHasThreeCaseStatements) {
  const uhdm::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);

  const uhdm::Begin *const blk = getInitialBegin(top);
  ASSERT_NE(blk, nullptr) << "no Initial/Begin block found";
  ASSERT_NE(blk->getStmts(), nullptr);

  size_t count = 0;
  for (const uhdm::Any *const s : *blk->getStmts())
    if (any_cast<uhdm::CaseStmt>(s)) ++count;
  EXPECT_EQ(count, 3u);
}

// ---------------------------------------------------------------------------
// All three cases use the "exact" (plain case) keyword
// ---------------------------------------------------------------------------
TEST_F(AttributesCase, AllCasesAreExactType) {
  const uhdm::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  const uhdm::Begin *const blk = getInitialBegin(top);
  ASSERT_NE(blk, nullptr);

  for (size_t i = 0; i < 3u; ++i) {
    const uhdm::CaseStmt *const cs = getNthCase(blk, i);
    ASSERT_NE(cs, nullptr) << "case[" << i << "] not found";
    EXPECT_EQ(cs->getCaseType(), 1) << "case[" << i << "] should be exact (1)";
  }
}

// ---------------------------------------------------------------------------
// All three cases have 3 items and switch on net 'a'
// ---------------------------------------------------------------------------
TEST_F(AttributesCase, AllCasesHaveThreeItems) {
  const uhdm::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  const uhdm::Begin *const blk = getInitialBegin(top);
  ASSERT_NE(blk, nullptr);

  for (size_t i = 0; i < 3u; ++i) {
    const uhdm::CaseStmt *const cs = getNthCase(blk, i);
    ASSERT_NE(cs, nullptr) << "case[" << i << "] not found";
    ASSERT_NE(cs->getCaseItems(), nullptr) << "case[" << i << "] has no items";
    EXPECT_EQ(cs->getCaseItems()->size(), 3u) << "case[" << i << "] should have 3 items";
  }
}

TEST_F(AttributesCase, AllCasesConditionIsNetA) {
  const uhdm::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  const uhdm::Begin *const blk = getInitialBegin(top);
  ASSERT_NE(blk, nullptr);

  for (size_t i = 0; i < 3u; ++i) {
    const uhdm::CaseStmt *const cs = getNthCase(blk, i);
    ASSERT_NE(cs, nullptr) << "case[" << i << "] not found";
    const uhdm::RefObj *const cond = cs->getCondition<uhdm::RefObj>();
    ASSERT_NE(cond, nullptr) << "case[" << i << "] condition is not a RefObj";
    EXPECT_EQ(cond->getName(), "a") << "case[" << i << "] condition should be 'a'";
  }
}

// ---------------------------------------------------------------------------
// CaseItem structure for the first case statement
// Item 0: 2'b00 → b = 0
// Item 1: 2'b01, 2'b10 → b = 1
// Item 2: default → b = 0
// ---------------------------------------------------------------------------
TEST_F(AttributesCase, FirstCaseItem0HasOneExpr) {
  const uhdm::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  const uhdm::CaseStmt *const cs = getNthCase(getInitialBegin(top), 0);
  ASSERT_NE(cs, nullptr);
  ASSERT_EQ(cs->getCaseItems()->size(), 3u);

  const uhdm::CaseItem *const item0 = (*cs->getCaseItems())[0];
  ASSERT_NE(item0, nullptr);
  ASSERT_NE(item0->getExprs(), nullptr);
  EXPECT_EQ(item0->getExprs()->size(), 1u) << "item 0 (2'b00) should have 1 expression";
}

TEST_F(AttributesCase, FirstCaseItem1HasTwoExprs) {
  const uhdm::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  const uhdm::CaseStmt *const cs = getNthCase(getInitialBegin(top), 0);
  ASSERT_NE(cs, nullptr);
  ASSERT_EQ(cs->getCaseItems()->size(), 3u);

  const uhdm::CaseItem *const item1 = (*cs->getCaseItems())[1];
  ASSERT_NE(item1, nullptr);
  ASSERT_NE(item1->getExprs(), nullptr);
  EXPECT_EQ(item1->getExprs()->size(), 2u) << "item 1 (2'b01, 2'b10) should have 2 expressions";
}

TEST_F(AttributesCase, FirstCaseDefaultItemHasNoExprs) {
  const uhdm::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  const uhdm::CaseStmt *const cs = getNthCase(getInitialBegin(top), 0);
  ASSERT_NE(cs, nullptr);
  ASSERT_EQ(cs->getCaseItems()->size(), 3u);

  const uhdm::CaseItem *const def = (*cs->getCaseItems())[2];
  ASSERT_NE(def, nullptr);
  // default item has no match expressions
  EXPECT_TRUE(!def->getExprs() || def->getExprs()->empty())
      << "default item should have no match expressions";
}

// ---------------------------------------------------------------------------
// Attribute: (* full_case, parallel_case *) — case 1
//   full_case → attached to CaseStmt, no value (flag)
// ---------------------------------------------------------------------------
TEST_F(AttributesCase, FirstCaseHasFullCaseFlagAttribute) {
  const uhdm::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  const uhdm::CaseStmt *const cs = getNthCase(getInitialBegin(top), 0);
  ASSERT_NE(cs, nullptr);

  const uhdm::Attribute *const attr = findAttr(cs, "full_case");
  ASSERT_NE(attr, nullptr) << "case 1 should have a 'full_case' attribute";
  EXPECT_EQ(attr->getValue(), nullptr) << "flag attribute 'full_case' should have no value";
}

// ---------------------------------------------------------------------------
// Attribute: (* full_case = 1 *) (* parallel_case = 1 *) — case 2
//   Both attached to the CaseStmt with value = 1
// ---------------------------------------------------------------------------
TEST_F(AttributesCase, SecondCaseHasTwoAttributes) {
  const uhdm::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  const uhdm::CaseStmt *const cs = getNthCase(getInitialBegin(top), 1);
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getAttributes(), nullptr);
  EXPECT_EQ(cs->getAttributes()->size(), 2u)
      << "case 2 should have 2 attributes (full_case=1, parallel_case=1)";
}

TEST_F(AttributesCase, SecondCaseFullCaseValueIsOne) {
  const uhdm::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  const uhdm::CaseStmt *const cs = getNthCase(getInitialBegin(top), 1);
  ASSERT_NE(cs, nullptr);

  const uhdm::Attribute *const attr = findAttr(cs, "full_case");
  ASSERT_NE(attr, nullptr) << "case 2 should have 'full_case' attribute";
  const uhdm::Constant *const val = attr->getValue<uhdm::Constant>();
  ASSERT_NE(val, nullptr) << "full_case attribute should have a Constant value";
  EXPECT_EQ(val->getDecompile(), "1");
}

TEST_F(AttributesCase, SecondCaseParallelCaseValueIsOne) {
  const uhdm::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  const uhdm::CaseStmt *const cs = getNthCase(getInitialBegin(top), 1);
  ASSERT_NE(cs, nullptr);

  const uhdm::Attribute *const attr = findAttr(cs, "parallel_case");
  ASSERT_NE(attr, nullptr) << "case 2 should have 'parallel_case' attribute";
  const uhdm::Constant *const val = attr->getValue<uhdm::Constant>();
  ASSERT_NE(val, nullptr) << "parallel_case attribute should have a Constant value";
  EXPECT_EQ(val->getDecompile(), "1");
}

// ---------------------------------------------------------------------------
// Attribute: (* full_case, parallel_case = 0 *) — case 3
//   full_case → attached to CaseStmt, no value (flag)
// ---------------------------------------------------------------------------
TEST_F(AttributesCase, ThirdCaseHasFullCaseFlagAttribute) {
  const uhdm::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  const uhdm::CaseStmt *const cs = getNthCase(getInitialBegin(top), 2);
  ASSERT_NE(cs, nullptr);

  const uhdm::Attribute *const attr = findAttr(cs, "full_case");
  ASSERT_NE(attr, nullptr) << "case 3 should have a 'full_case' attribute";
  EXPECT_EQ(attr->getValue(), nullptr) << "flag attribute 'full_case' should have no value";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
