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

// Tests for the IEEE 1800-2023 Clause 12 (Procedural programming statements)
// error scenarios catalogued in sv_error_catalog_Latest.xlsx (rows 378, 380,
// 384, 392).
//
// Scope, fixture layout and test shapes follow the Clause 3 file in this same
// suite; see hlc/ErrorCatalog/chapter-3 for the rationale.
//
// Behaviour observed while writing this file (hlc.exe -d db over the fixture):
// all four modules compile. Row 384's mixed for-loop initialization is
// rejected by the grammar; the parser recovers locally, so r392_m after it is
// still built. Rows 378, 380 and 392 produce no diagnostic. The Linter's
// single orphan-node complaint is a consequence of the row 384 recovery, not a
// diagnostic for any of these rules, and is not asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter12ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "12--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 378: one default per case statement (12.5) -------------------------

TEST_F(Chapter12ErrorRulesTest, Row378_MultipleDefaultCaseItemsAreRejected) {
  // catalog row 378 | 12.5 | COMP
  // "The default statement shall be optional. Use of multiple default
  // statements in one case statement shall be illegal." r378_m has two, on
  // lines 16 and 17; line 17 is the offending repeat.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 12.5 makes a second default case_item "
                  "in the same case statement illegal";
  EXPECT_NE(findError(ErrorDefinition::COMP_DUPLICATE_DECLARATION, "r378_m", 17, 7), nullptr)
      << "a case statement may have at most one default item (IEEE 1800-2023 12.5)";
}

// --- row 380: leaf patterns must be integral (12.6) -------------------------

TEST_F(Chapter12ErrorRulesTest, Row380_NonIntegralConstantLeafPatternIsRejected) {
  // catalog row 380 | 12.6 | COMP
  // "A constant expression used as a leaf pattern shall be of integral type."
  // The pattern on line 28 matches the real member R against the real literal
  // 1.5.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 12.6 requires a constant expression "
                  "used as a leaf pattern to be of integral type";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_PATTERN, "R", 28, 16), nullptr)
      << "a real constant is not a legal leaf pattern (IEEE 1800-2023 12.6)";
}

// --- row 384: for-loop control variables (12.7.1) ---------------------------

TEST_F(Chapter12ErrorRulesTest, Row384_MixedLocalAndNonLocalLoopVariablesAreRejected) {
  // catalog row 384 | 12.7.1 | COMP
  // "Either all or none of the control variables shall be locally declared."
  // Line 40 assigns to the module-level x while declaring y locally in the
  // same initialization list.
  //
  // HLC rejects this in the grammar: the for_initialization production admits
  // either a list of variable_assignments or a list of local declarations, not
  // a mixture, so the local declaration after the assignment is a syntax
  // error. The parser recovers locally and r392_m is still compiled.
  EXPECT_NE(findError(ErrorDefinition::PA_SYNTAX_ERROR, 40, 20), nullptr)
      << "for-loop control variables must be all local or none (IEEE 1800-2023 12.7.1)";
}

// --- row 392: a nonvoid function must return a value (12.8) -----------------

TEST_F(Chapter12ErrorRulesTest, Row392_NonvoidFunctionReturnWithoutExpressionIsRejected) {
  // catalog row 392 | 12.8 | COMP
  // "In a function, the return statement shall specify an expression of the
  // correct type" -- only a void function, a task or a block may return
  // bare. f is declared to return int, and line 48 returns nothing.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 12.8 requires a return statement in a "
                  "value-returning function to carry an expression of the correct type";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_RETURN_VALUE, "f", 48, 5), nullptr)
      << "a nonvoid function cannot return without an expression (IEEE 1800-2023 12.8)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
