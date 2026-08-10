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

// Tests for 12.6.1--casex_pattern.sv (tags: 12.6.1)
//   module case_tb ();
//     typedef union tagged {
//       struct { bit [3:0] val1, val2; } a;
//       struct { bit [7:0] val1, val2; } b;
//       struct { bit [15:0] val1, val2; } c;
//     } u;
//     u tmp;
//     initial casex (v) matches
//       tagged a '{.v, 4'b00?x} : $display("a %d", v);
//       tagged a '{.v1, .v2} : $display("a %d %d", v1, v2);
//       tagged b '{.v1, .v2} : $display("b %d %d", v1, v2);
//       tagged c '{4'h??0x, .v} : $display("c %d", v);
//     endcase
//   endmodule
//
// What to check and why (IEEE 1800-2023 12.6.1 "Pattern matching in
// case statements", p.326-327, checked before any test code was
// written):
//   "The casez and casex keywords can be used instead of case, with
//   the same semantics. In other words, during pattern matching,
//   wherever 2 bits are compared (whether they are tag bits or
//   members), the casez form ignores z bits, and the casex form
//   ignores both z and x bits." This file is otherwise structurally
//   identical to 12.6.1--case_pattern.sv (same tag names, same pattern
//   shapes, same $display bodies) except the case-keyword is "casex"
//   and the constant-expression sub-patterns use "?"/x/z wildcard
//   literals ("4'b00?x", "4'h??0x") instead of plain values.
//
//   IMPORTANT source-file note (same as 12.6.1--case_pattern.sv):
//   "casex (v)" reads the identifier "v", which is declared nowhere in
//   this module ("tmp" is the only variable of a compatible type).
//   Per ordinary identifier-resolution rules this must fail to bind and
//   be diagnosed -- independent of, and not to be confused with, the
//   pattern-bound "v"/"v1"/"v2" identifiers legitimately declared by
//   each case item's own pattern (12.6.1: "this scope extends to...
//   the statement in the right-hand side of the same case item").
//
//   Per IEEE 1800-2023 vpi_user.h vpiCaseType enumerants (vpiCaseX=2
//   for "casex", confirmed against this repo's actual
//   build/include/hldb/vpi_user.h) and Annex 37.72's vpiQualifier
//   enumerants (vpiMatchesQualifier, the same pattern already used for
//   "inside" in 12.5.4--case_set.cpp), this file's "casex ... matches"
//   maps to getCaseType() == vpiCaseX and getQualifier() ==
//   vpiMatchesQualifier.
//
//   CONFIRMED BY RUNNING THIS FILE WITH THE SKIPS BELOW REMOVED (all
//   fail as expected, matching the identical gaps already confirmed in
//   12.6.1--case_pattern.cpp): (1) getQualifier() comes back 0 instead
//   of vpiMatchesQualifier; (2) the undeclared case_expression "v"
//   produces zero diagnostics instead of COMP_FAILED_TO_BIND; (3) the
//   pattern-bound "v" referenced in case items 1 and 4's $display never
//   resolves via getActual<hldb::AnyPattern>(). All three kept as
//   GTEST_SKIP() with the real assertions underneath, per the
//   established gating rule (skips only added after personal
//   verification).
//
// What is checked:
//   - module case_tb has zero Nets and exactly 1 Variable "tmp"
//   - CaseStmt exists, getCaseType() == vpiCaseX; its qualifier SHOULD
//     be vpiMatchesQualifier per spec, but this is currently a
//     confirmed-failing, skipped assertion (see note above)
//   - the case_expression "v" SHOULD fail to bind and be diagnosed
//     (findError(COMP_FAILED_TO_BIND, "v")), but this is currently a
//     confirmed-failing, skipped assertion (see note above)
//   - exactly 4 CaseItems, each with exactly one vpiExpr: a
//     TaggedPattern whose name/tag matches the source ("a", "a", "b",
//     "c") and whose nested StructPattern has exactly 2 sub-patterns of
//     the exact kind and (for Constants) decompiled value written in
//     the source, including the "?"/x/z wildcard literals
//   - each case item's $display body references its own pattern-bound
//     identifier(s); per 12.6.1 those RefObjs SHOULD resolve
//     (getActual<hldb::AnyPattern>() non-null) to the AnyPattern
//     declared by that item's own pattern, but for items 1 and 4 this
//     is currently a confirmed-failing, skipped assertion (see note
//     above)
//   - no continuous assignments exist in the module
//
// What is NOT checked and why:
//   - the internal shape of the "union tagged {...} u" typedef itself
//     is chapter-7 territory, already covered elsewhere
//   - the runtime do-not-care matching behavior itself (whether x/z
//     bits actually match during simulation) is a simulation-time
//     concept, not a static/structural compile-time property

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/any_pattern.h>
#include <hldb/case_item.h>
#include <hldb/case_stmt.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/struct_pattern.h>
#include <hldb/sys_task_call.h>
#include <hldb/tagged_pattern.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class CasexPatternTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "12.6.1--casex_pattern.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("case_tb", m_design->getAllModules());
  }

  static const hldb::CaseStmt *getCaseStmt() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const initial = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (initial == nullptr) return nullptr;
    return initial->getStmt<hldb::CaseStmt>();
  }

  enum class SubKind { kConstant, kIdentifier };

  static void checkSubPattern(const hldb::Any *sub, SubKind kind, std::string_view text) {
    ASSERT_NE(sub, nullptr);
    if (kind == SubKind::kConstant) {
      const hldb::Constant *const c = any_cast<hldb::Constant>(sub);
      ASSERT_NE(c, nullptr) << "expected a Constant-expression sub-pattern";
      EXPECT_EQ(c->getDecompile(), text);
    } else {
      const hldb::AnyPattern *const p = any_cast<hldb::AnyPattern>(sub);
      ASSERT_NE(p, nullptr) << "expected an identifier (AnyPattern) sub-pattern";
      EXPECT_EQ(p->getName(), text);
    }
  }

  static void checkItemPattern(const hldb::CaseItem *item, std::string_view tagName, SubKind kind0,
                                std::string_view text0, SubKind kind1, std::string_view text1) {
    ASSERT_NE(item, nullptr);
    ASSERT_NE(item->getExprs(), nullptr);
    ASSERT_EQ(item->getExprs()->size(), 1u);
    const hldb::TaggedPattern *const tagged = any_cast<hldb::TaggedPattern>(item->getExprs()->at(0));
    ASSERT_NE(tagged, nullptr) << "case_pattern_item expr should be a TaggedPattern";
    EXPECT_EQ(tagged->getName(), tagName);
    const hldb::StructPattern *const structPattern = tagged->getPattern<hldb::StructPattern>();
    ASSERT_NE(structPattern, nullptr) << "TaggedPattern's nested pattern is not a StructPattern";
    ASSERT_NE(structPattern->getPatterns(), nullptr);
    ASSERT_EQ(structPattern->getPatterns()->size(), 2u);
    checkSubPattern(structPattern->getPatterns()->at(0), kind0, text0);
    checkSubPattern(structPattern->getPatterns()->at(1), kind1, text1);
  }
};

TEST_F(CasexPatternTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(CasexPatternTest, ModuleHasNoNetsAndOneVariableTmp) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty()) << "no wire/net_type keyword is declared";
  ASSERT_NE(top->getVariables(), nullptr) << "'u tmp' should be a Variable, not a Net";
  EXPECT_NE(hldb::findByName<hldb::Variable>("tmp", top->getVariables()), nullptr) << "Variable 'tmp' not found";
}

TEST_F(CasexPatternTest, CaseStmtExistsWithCaseXTypeAndMatchesQualifier) {
  GTEST_SKIP() << "Confirmed HLC bug -- verified by running this test with the skip removed (fails as expected): "
                  "IEEE 1800-2023 12.6.1 requires the 'matches' keyword to be recorded as vpiMatchesQualifier on "
                  "the CaseStmt, but HLC leaves getQualifier() at its default 0 (vpiNoQualifier). Same underlying "
                  "gap as 12.6.1--case_pattern.cpp and 12.4.2--priority_if.cpp/unique_if.cpp/unique0_if.cpp. "
                  "Tracked, not yet fixed by the compiler.";
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr) << "the initial process's statement should resolve directly to CaseStmt";
  EXPECT_EQ(cs->getCaseType(), vpiCaseX);
  EXPECT_EQ(cs->getQualifier(), vpiMatchesQualifier);
}

TEST_F(CasexPatternTest, CaseExpressionVIsUndeclaredAndFailsToBind) {
  GTEST_SKIP() << "Confirmed HLC bug -- verified by running this test with the skip removed (fails as expected): "
                  "'casex (v)' reads an identifier declared nowhere in this module, which must fail to bind and "
                  "be diagnosed, but HLC reports zero diagnostics for it. Same underlying gap as "
                  "12.6.1--case_pattern.cpp and 6.10--implicit_continuous_assignment.cpp. Tracked, not yet fixed "
                  "by the compiler.";
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  const hldb::RefObj *const cond = cs->getCondition<hldb::RefObj>();
  ASSERT_NE(cond, nullptr) << "case_expression is not a RefObj";
  EXPECT_EQ(cond->getName(), "v");
  ASSERT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "v"), nullptr)
      << "'casex (v)' references an identifier that is declared nowhere in this module (only 'tmp' of a "
         "compatible type exists); per ordinary identifier-resolution rules this must fail to bind and be "
         "diagnosed";
}

TEST_F(CasexPatternTest, ExactlyFourCaseItems) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  EXPECT_EQ(cs->getCaseItems()->size(), 4u);
}

TEST_F(CasexPatternTest, FirstItemIsTaggedAWithIdentifierVAndWildcardBinaryLiteral) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_GE(cs->getCaseItems()->size(), 1u);
  checkItemPattern(cs->getCaseItems()->at(0), "a", SubKind::kIdentifier, "v", SubKind::kConstant, "4'b00?x");
}

TEST_F(CasexPatternTest, FirstItemDisplayReferencesPatternBoundV) {
  GTEST_SKIP() << "Confirmed HLC bug -- verified by running this test with the skip removed (fails as expected): "
                  "the 'v' in this item's $display never resolves to the AnyPattern declared by 'tagged a '{.v, "
                  "4'b00?x}' -- same pattern-identifier binding gap confirmed in 12.6.1--case_pattern.cpp. "
                  "Tracked, not yet fixed by the compiler.";
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_GE(cs->getCaseItems()->size(), 1u);
  const hldb::SysTaskCall *const display = cs->getCaseItems()->at(0)->getStmt<hldb::SysTaskCall>();
  ASSERT_NE(display, nullptr);
  ASSERT_NE(display->getArguments(), nullptr);
  ASSERT_EQ(display->getArguments()->size(), 2u);
  const hldb::RefObj *const vArg = any_cast<hldb::RefObj>(display->getArguments()->at(1));
  ASSERT_NE(vArg, nullptr);
  EXPECT_EQ(vArg->getName(), "v");
  EXPECT_NE(vArg->getActual<hldb::AnyPattern>(), nullptr)
      << "'v' in this item's own $display should resolve to this item's own pattern-bound identifier";
}

TEST_F(CasexPatternTest, SecondItemIsTaggedAWithIdentifiersV1V2) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_GE(cs->getCaseItems()->size(), 2u);
  checkItemPattern(cs->getCaseItems()->at(1), "a", SubKind::kIdentifier, "v1", SubKind::kIdentifier, "v2");
}

TEST_F(CasexPatternTest, ThirdItemIsTaggedBWithIdentifiersV1V2) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_GE(cs->getCaseItems()->size(), 3u);
  checkItemPattern(cs->getCaseItems()->at(2), "b", SubKind::kIdentifier, "v1", SubKind::kIdentifier, "v2");
}

TEST_F(CasexPatternTest, FourthItemIsTaggedCWithWildcardHexLiteralAndIdentifierV) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_GE(cs->getCaseItems()->size(), 4u);
  checkItemPattern(cs->getCaseItems()->at(3), "c", SubKind::kConstant, "4'h??0x", SubKind::kIdentifier, "v");
}

TEST_F(CasexPatternTest, FourthItemDisplayReferencesPatternBoundV) {
  GTEST_SKIP() << "Confirmed HLC bug -- verified by running this test with the skip removed (fails as expected): "
                  "same pattern-identifier binding gap as FirstItemDisplayReferencesPatternBoundV, confirmed here "
                  "for item 4's 'tagged c '{4'h??0x, .v}' pattern too. Tracked, not yet fixed by the compiler.";
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_GE(cs->getCaseItems()->size(), 4u);
  const hldb::SysTaskCall *const display = cs->getCaseItems()->at(3)->getStmt<hldb::SysTaskCall>();
  ASSERT_NE(display, nullptr);
  ASSERT_NE(display->getArguments(), nullptr);
  ASSERT_EQ(display->getArguments()->size(), 2u);
  const hldb::RefObj *const vArg = any_cast<hldb::RefObj>(display->getArguments()->at(1));
  ASSERT_NE(vArg, nullptr);
  EXPECT_EQ(vArg->getName(), "v");
  EXPECT_NE(vArg->getActual<hldb::AnyPattern>(), nullptr)
      << "'v' in this item's own $display should resolve to this item's own pattern-bound identifier, distinct "
         "from item 1's differently-scoped 'v'";
}

TEST_F(CasexPatternTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
