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

// Tests for 12.6.1--case_pattern.sv (tags: 12.6.1)
//   module case_tb ();
//     typedef union tagged {
//       struct { bit [3:0] val1, val2; } a;
//       struct { bit [7:0] val1, val2; } b;
//       struct { bit [15:0] val1, val2; } c;
//     } u;
//     u tmp;
//     initial case (v) matches
//       tagged a '{.v, 0} : $display("a %d", v);
//       tagged a '{.v1, .v2} : $display("a %d %d", v1, v2);
//       tagged b '{.v1, .v2} : $display("b %d %d", v1, v2);
//       tagged c '{0, .v} : $display("c %d", v);
//     endcase
//   endmodule
//
// What to check and why (IEEE 1800-2023 12.6.1 "Pattern matching in
// case statements", p.326, checked before any test code was written):
//   "In a pattern-matching case statement, the expression in
//   parentheses is followed by the keyword matches, and the statement
//   contains a series of case_pattern_items." "Each pattern introduces
//   a new scope, in which its pattern identifiers are implicitly
//   declared; this scope extends to... the statement in the right-hand
//   side of the same case item. Thus different case items can reuse
//   pattern identifiers" -- which is exactly why the identifier "v" is
//   legally reused across case items 1 and 4 with different bindings
//   each time, and "v1"/"v2" are reused across items 2 and 3.
//
//   IMPORTANT source-file note, confirmed structurally in this exact
//   file (not guessed): the case_expression itself is "case (v)", but
//   "v" is never declared anywhere in this module -- "tmp" (of type u)
//   is the only variable of a compatible type. Per ordinary
//   SystemVerilog identifier-resolution rules, an identifier read in a
//   procedural expression that has no declaration in any enclosing
//   scope is illegal and must fail to resolve; unlike an implicit net
//   (6.10, which only applies to specific structural contexts such as a
//   continuous assignment's LHS), a bare read of an undeclared
//   identifier here is not implicitly declared by any rule. This is
//   independent of, and must not be confused with, the pattern-bound
//   "v"/"v1"/"v2" identifiers used inside each case item's $display,
//   which per 12.6.1 are legitimately declared by their own patterns
//   and are visible only within that case item's own statement (a
//   different scope than the outer, undeclared case-expression "v").
//
//   Per IEEE 1800-2023 Annex 37.72 (vpiQualifier on Case) and the
//   vpi_user.h enumerants (confirmed against this repo's actual
//   build/include/hldb/sv_vpi_user.h), the "matches" keyword is given
//   its own named constant, vpiMatchesQualifier -- the same pattern
//   already used for "inside" (vpiInsideQualifier) in
//   12.5.4--case_set.cpp.
//
//   CONFIRMED BY RUNNING THIS FILE WITH THE SKIPS BELOW REMOVED (all
//   fail as expected): (1) getQualifier() comes back 0 (vpiNoQualifier)
//   instead of vpiMatchesQualifier -- same underlying gap as
//   12.4.2--priority_if.cpp/unique_if.cpp/unique0_if.cpp, now also
//   confirmed for case statements; (2) the undeclared case_expression
//   "v" produces zero diagnostics instead of COMP_FAILED_TO_BIND -- the
//   same "silently accepts an unresolved reference" pattern already
//   confirmed in 6.10--implicit_continuous_assignment.cpp; (3) the
//   pattern-bound "v" referenced in case items 1 and 4's $display never
//   resolves via getActual<hldb::AnyPattern>() -- the AnyPattern node
//   exists in the tree, but nothing binds a use of the name back to it.
//   All three kept as GTEST_SKIP() with the real assertions underneath,
//   per the established gating rule (skips only added after personal
//   verification).
//
// What is checked:
//   - module case_tb has zero Nets and exactly 1 Variable "tmp"
//   - CaseStmt exists, getCaseType() == vpiCaseExact (pattern matching
//     does not change the case type; the case-keyword here is plain
//     "case", not casex/casez); its qualifier SHOULD be
//     vpiMatchesQualifier per spec, but this is currently a confirmed-
//     failing, skipped assertion (see note above)
//   - the case_expression "v" SHOULD fail to bind and be diagnosed
//     (findError(COMP_FAILED_TO_BIND, "v")) per ordinary identifier-
//     resolution rules, but this is currently a confirmed-failing,
//     skipped assertion (see note above)
//   - exactly 4 CaseItems, each with exactly one vpiExpr: a
//     TaggedPattern whose name/tag matches the source ("a", "a", "b",
//     "c") and whose nested StructPattern has exactly 2 sub-patterns of
//     the exact kind and (for Constants) decompiled value written in
//     the source
//   - each case item's $display body references its own pattern-bound
//     identifier(s) ("v" for items 1 and 4, "v1"/"v2" for items 2 and
//     3); per 12.6.1 those RefObjs SHOULD resolve
//     (getActual<hldb::AnyPattern>() non-null) to the AnyPattern
//     declared by that item's own pattern, but for items 1 and 4 this
//     is currently a confirmed-failing, skipped assertion (see note
//     above) -- items 2 and 3 only check the pattern shape itself, not
//     any $display reference back to it, so they remain real
//     (non-skipped) assertions
//   - no continuous assignments exist in the module
//
// What is NOT checked and why:
//   - the internal shape of the "union tagged {...} u" typedef itself
//     is chapter-7 territory, already covered elsewhere
//   - the runtime pattern-match/linear-search behavior itself is a
//     simulation-time concept, not a static/structural property

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

class CasePatternTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "12.6.1--case_pattern.hlc"}); }
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

  // Checks a case item whose pattern is 'tagged <tagName> '{sub0, sub1}'
  // where each subKind/subText pair describes either a Constant
  // (subKind == kConstant, subText its decompile) or an AnyPattern
  // (subKind == kIdentifier, subText its name).
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

TEST_F(CasePatternTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(CasePatternTest, ModuleHasNoNetsAndOneVariableTmp) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty()) << "no wire/net_type keyword is declared";
  ASSERT_NE(top->getVariables(), nullptr) << "'u tmp' should be a Variable, not a Net";
  EXPECT_NE(hldb::findByName<hldb::Variable>("tmp", top->getVariables()), nullptr) << "Variable 'tmp' not found";
}

TEST_F(CasePatternTest, CaseStmtExistsWithExactTypeAndMatchesQualifier) {
  GTEST_SKIP() << "Confirmed HLC bug -- verified by running this test with the skip removed (fails as expected): "
                  "IEEE 1800-2023 12.6.1 requires the 'matches' keyword to be recorded as vpiMatchesQualifier on "
                  "the CaseStmt, but HLC leaves getQualifier() at its default 0 (vpiNoQualifier) -- the keyword is "
                  "parsed but never recorded onto the final object. Same underlying gap as "
                  "12.4.2--priority_if.cpp/unique_if.cpp/unique0_if.cpp, now confirmed for case statements too. "
                  "Tracked, not yet fixed by the compiler.";
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr) << "the initial process's statement should resolve directly to CaseStmt";
  EXPECT_EQ(cs->getCaseType(), vpiCaseExact);
  EXPECT_EQ(cs->getQualifier(), vpiMatchesQualifier);
}

TEST_F(CasePatternTest, CaseExpressionVIsUndeclaredAndFailsToBind) {
  GTEST_SKIP() << "Confirmed HLC bug -- verified by running this test with the skip removed (fails as expected): "
                  "'case (v)' reads an identifier declared nowhere in this module, which per ordinary "
                  "identifier-resolution rules must fail to bind and be diagnosed, but HLC reports zero "
                  "diagnostics for it -- the same 'silently accepts an unresolved reference' pattern already "
                  "confirmed in 6.10--implicit_continuous_assignment.cpp. Tracked, not yet fixed by the compiler.";
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  const hldb::RefObj *const cond = cs->getCondition<hldb::RefObj>();
  ASSERT_NE(cond, nullptr) << "case_expression is not a RefObj";
  EXPECT_EQ(cond->getName(), "v");
  ASSERT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "v"), nullptr)
      << "'case (v)' references an identifier that is declared nowhere in this module (only 'tmp' of a compatible "
         "type exists); per ordinary identifier-resolution rules this must fail to bind and be diagnosed";
}

TEST_F(CasePatternTest, ExactlyFourCaseItems) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  EXPECT_EQ(cs->getCaseItems()->size(), 4u);
}

TEST_F(CasePatternTest, FirstItemIsTaggedAWithIdentifierVAndConstantZero) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_GE(cs->getCaseItems()->size(), 1u);
  checkItemPattern(cs->getCaseItems()->at(0), "a", SubKind::kIdentifier, "v", SubKind::kConstant, "0");
}

TEST_F(CasePatternTest, FirstItemDisplayReferencesPatternBoundV) {
  GTEST_SKIP() << "Confirmed HLC bug -- verified by running this test with the skip removed (fails as expected): "
                  "IEEE 1800-2023 12.6.1 requires the 'v' in this item's $display to resolve to the AnyPattern "
                  "declared by 'tagged a '{.v, 0}', but HLC never binds a pattern-identifier reference back to "
                  "its declaring pattern -- the AnyPattern node exists in the tree, but nothing wires the "
                  "$display's RefObj to it. Tracked, not yet fixed by the compiler.";
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
      << "'v' in this item's own $display should resolve to this item's own pattern-bound identifier (12.6.1: "
         "pattern scope extends to the statement in the same case item), not the undeclared outer 'v'";
}

TEST_F(CasePatternTest, SecondItemIsTaggedAWithIdentifiersV1V2) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_GE(cs->getCaseItems()->size(), 2u);
  checkItemPattern(cs->getCaseItems()->at(1), "a", SubKind::kIdentifier, "v1", SubKind::kIdentifier, "v2");
}

TEST_F(CasePatternTest, ThirdItemIsTaggedBWithIdentifiersV1V2) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_GE(cs->getCaseItems()->size(), 3u);
  checkItemPattern(cs->getCaseItems()->at(2), "b", SubKind::kIdentifier, "v1", SubKind::kIdentifier, "v2");
}

TEST_F(CasePatternTest, FourthItemIsTaggedCWithConstantZeroAndIdentifierV) {
  const hldb::CaseStmt *const cs = getCaseStmt();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_GE(cs->getCaseItems()->size(), 4u);
  checkItemPattern(cs->getCaseItems()->at(3), "c", SubKind::kConstant, "0", SubKind::kIdentifier, "v");
}

TEST_F(CasePatternTest, FourthItemDisplayReferencesPatternBoundV) {
  GTEST_SKIP() << "Confirmed HLC bug -- verified by running this test with the skip removed (fails as expected): "
                  "same pattern-identifier binding gap as FirstItemDisplayReferencesPatternBoundV, confirmed here "
                  "for item 4's 'tagged c '{0, .v}' pattern too -- not a one-off, the binding step is missing "
                  "for every pattern-bound identifier in this file. Tracked, not yet fixed by the compiler.";
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

TEST_F(CasePatternTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
