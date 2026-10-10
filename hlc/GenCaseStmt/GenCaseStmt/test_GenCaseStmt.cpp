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

// Tests for tests/GenCaseStmt/dut.sv:
//
//   module GOOD();
//   endmodule
//
//   module top();
//     parameter bit unsigned x_1b0 = 1'b0;
//     parameter bit signed x_1sb0 = 1'sb0;
//     parameter bit signed x_1sb1 = 1'sb1;
//     parameter logic signed [1:0] x_2sb11 = 2'sb11;
//
//     case (x_2sb11)
//       2'b?1: BAD u2();
//       default: GOOD u3();
//     endcase
//
//     // Mix signed and unsigned
//     case (x_2sb11)
//       x_1b0:  BAD u1();
//       x_1sb0: BAD u2();
//       default: GOOD u3();
//     endcase
//
//     case (x_2sb11)
//       x_1sb0:  BAD u1();
//       x_1sb1: GOOD u2();
//       default: BAD u3();
//     endcase
//   endmodule
//
// GenCaseStmt.hlc compiles at "-d db -d ast" (no "-d inst"), so, exactly
// like tests/GenCase, none of the three case_generate_constructs below are
// actually elaborated -- no item is resolved as "selected" at this compile
// phase, since Sec 27.5's item-vs-condition comparison (Sec 11.8.1's
// same-size, same-signedness 4-state identity comparison) is elaboration's
// job. This file therefore checks only the unelaborated parse-time shape
// (three GenCase constructs, their conditions, and each item's match
// expression), not which branch wins -- an elaborated ("-d inst") run of
// this same source would be needed to assert item selection, and no such
// run exists for this test.
//
// -- rules under test ---------------------------------------------------
//
// IEEE 1800-2023 Sec 5.7.1 "Integer literal constants": '2'b?1' is a
// literal containing a '?' digit, which Sec 5.7.1 defines as equivalent to
// 'z' inside a literal -- i.e. it is a literal value with a high-impedance
// bit, not a wildcard-match pattern (wildcard matching is a 'casez'/'casex'
// procedural-statement concept, Sec 12.5.3/12.5.4, which does not apply to
// case_generate_item, Sec 27.5's item comparison always uses plain
// identity equality, Sec 11.4.5 "===").

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/case_item.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_case.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

#include <vector>

namespace hlc {

class GenCaseStmtTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "GenCaseStmt.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  // Returns all GenCase statements directly in m's getGenStmts(), in source
  // order.
  static std::vector<const hldb::GenCase *> findAllGenCase(const hldb::Module *m) {
    std::vector<const hldb::GenCase *> result;
    if (m == nullptr || m->getGenStmts() == nullptr) return result;
    for (const hldb::Any *const stmt : *m->getGenStmts()) {
      if (const hldb::GenCase *const gc = any_cast<hldb::GenCase>(stmt)) result.emplace_back(gc);
    }
    return result;
  }
};

// ---------------------------------------------------------------------------
// Module existence
// ---------------------------------------------------------------------------

TEST_F(GenCaseStmtTest, ModulesExist) {
  EXPECT_NE(getTop(), nullptr) << "module 'top' not found";
  EXPECT_NE(hldb::findByDefName<hldb::Module>("GOOD", m_design->getAllModules()), nullptr)
      << "module 'GOOD' not found";
}

TEST_F(GenCaseStmtTest, ModuleHasExactlyThreeGenCaseConstructs) {
  const std::vector<const hldb::GenCase *> cases = findAllGenCase(getTop());
  ASSERT_EQ(cases.size(), 3u) << "'top' has three separate case_generate_constructs";
}

// Every GenCase's condition is a bare reference to 'x_2sb11'.
TEST_F(GenCaseStmtTest, AllThreeGenCasesConditionOnX2sb11) {
  const std::vector<const hldb::GenCase *> cases = findAllGenCase(getTop());
  ASSERT_EQ(cases.size(), 3u);
  for (size_t i = 0u; i < cases.size(); ++i) {
    const hldb::RefObj *const cond = cases[i]->getCondition<hldb::RefObj>();
    ASSERT_NE(cases[i]->getCondition(), nullptr) << "case " << i;
    ASSERT_NE(cond, nullptr) << "case " << i << ": condition must be a RefObj";
    EXPECT_EQ(cond->getName(), std::string_view{"x_2sb11"}) << "case " << i;
  }
}

// ---------------------------------------------------------------------------
// First case: 'case (x_2sb11) 2'b?1: BAD u2(); default: GOOD u3(); endcase'
// -- Sec 5.7.1, Sec 27.5.
// ---------------------------------------------------------------------------

TEST_F(GenCaseStmtTest, FirstCaseHasTwoItemsFirstBeingLiteralWithZOrXBit) {
  const std::vector<const hldb::GenCase *> cases = findAllGenCase(getTop());
  ASSERT_EQ(cases.size(), 3u);
  const hldb::GenCase *const gc = cases[0];
  ASSERT_NE(gc->getCaseItems(), nullptr);
  ASSERT_EQ(gc->getCaseItems()->size(), 2u) << "'2'b?1:' item plus 'default:' item";

  const hldb::CaseItem *const first = gc->getCaseItems()->at(0);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(first->getExprs(), nullptr);
  ASSERT_EQ(first->getExprs()->size(), 1u);
  const hldb::Constant *const literal = any_cast<hldb::Constant>(first->getExprs()->at(0));
  ASSERT_NE(literal, nullptr) << "'2'b?1' item expression must be a Constant";
  EXPECT_EQ(literal->getSize(), 2) << "'2'b?1' is a 2-bit literal";
  // Sec 5.7.1: '?' is equivalent to 'z' inside a literal, so this constant
  // must decompile with a non-0/1 digit somewhere in its value text -- it
  // is not silently folded down to a plain 2-bit binary literal.
  const std::string decompile = std::string(literal->getDecompile());
  const bool hasIndeterminateDigit =
      decompile.find('z') != std::string::npos || decompile.find('Z') != std::string::npos ||
      decompile.find('?') != std::string::npos || decompile.find('x') != std::string::npos ||
      decompile.find('X') != std::string::npos;
  EXPECT_TRUE(hasIndeterminateDigit) << "Sec 5.7.1: '2'b?1' decompiled as '" << decompile
                                      << "' -- must retain the '?'(z) bit, not collapse it to a definite value";
}

// ---------------------------------------------------------------------------
// Second case: 'x_1b0:' / 'x_1sb0:' / 'default:' -- Sec 27.5.
// ---------------------------------------------------------------------------

TEST_F(GenCaseStmtTest, SecondCaseHasThreeItemsWithExpectedExprNames) {
  const std::vector<const hldb::GenCase *> cases = findAllGenCase(getTop());
  ASSERT_EQ(cases.size(), 3u);
  const hldb::GenCase *const gc = cases[1];
  ASSERT_NE(gc->getCaseItems(), nullptr);
  ASSERT_EQ(gc->getCaseItems()->size(), 3u) << "'x_1b0:', 'x_1sb0:', 'default:'";

  const hldb::CaseItem *const first = gc->getCaseItems()->at(0);
  const hldb::CaseItem *const second = gc->getCaseItems()->at(1);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  ASSERT_NE(first->getExprs(), nullptr);
  ASSERT_EQ(first->getExprs()->size(), 1u);
  ASSERT_NE(second->getExprs(), nullptr);
  ASSERT_EQ(second->getExprs()->size(), 1u);

  const hldb::RefObj *const firstExpr = any_cast<hldb::RefObj>(first->getExprs()->at(0));
  const hldb::RefObj *const secondExpr = any_cast<hldb::RefObj>(second->getExprs()->at(0));
  ASSERT_NE(firstExpr, nullptr);
  ASSERT_NE(secondExpr, nullptr);
  EXPECT_EQ(firstExpr->getName(), std::string_view{"x_1b0"});
  EXPECT_EQ(secondExpr->getName(), std::string_view{"x_1sb0"});

  const hldb::CaseItem *const third = gc->getCaseItems()->at(2);
  ASSERT_NE(third, nullptr);
  EXPECT_TRUE(third->getExprs() == nullptr || third->getExprs()->empty())
      << "Sec 27.5: 'default:' carries no match expression of its own";
}

// ---------------------------------------------------------------------------
// Third case: 'x_1sb0:' / 'x_1sb1:' / 'default:' -- Sec 27.5.
// ---------------------------------------------------------------------------

TEST_F(GenCaseStmtTest, ThirdCaseHasThreeItemsWithExpectedExprNames) {
  const std::vector<const hldb::GenCase *> cases = findAllGenCase(getTop());
  ASSERT_EQ(cases.size(), 3u);
  const hldb::GenCase *const gc = cases[2];
  ASSERT_NE(gc->getCaseItems(), nullptr);
  ASSERT_EQ(gc->getCaseItems()->size(), 3u) << "'x_1sb0:', 'x_1sb1:', 'default:'";

  const hldb::CaseItem *const first = gc->getCaseItems()->at(0);
  const hldb::CaseItem *const second = gc->getCaseItems()->at(1);
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  ASSERT_NE(first->getExprs(), nullptr);
  ASSERT_EQ(first->getExprs()->size(), 1u);
  ASSERT_NE(second->getExprs(), nullptr);
  ASSERT_EQ(second->getExprs()->size(), 1u);

  const hldb::RefObj *const firstExpr = any_cast<hldb::RefObj>(first->getExprs()->at(0));
  const hldb::RefObj *const secondExpr = any_cast<hldb::RefObj>(second->getExprs()->at(0));
  ASSERT_NE(firstExpr, nullptr);
  ASSERT_NE(secondExpr, nullptr);
  EXPECT_EQ(firstExpr->getName(), std::string_view{"x_1sb0"});
  EXPECT_EQ(secondExpr->getName(), std::string_view{"x_1sb1"});

  const hldb::CaseItem *const third = gc->getCaseItems()->at(2);
  ASSERT_NE(third, nullptr);
  EXPECT_TRUE(third->getExprs() == nullptr || third->getExprs()->empty())
      << "Sec 27.5: 'default:' carries no match expression of its own";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
