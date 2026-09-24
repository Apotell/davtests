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

// Tests for the IEEE 1800-2023 Clause 17 (Checkers) error scenarios catalogued
// in sv_error_catalog_Latest.xlsx / docs/error_catalog.xml (rows 573, 580,
// 582, 590, 595, 596, 597).
//
// Scope, fixture layout and test shapes follow the Clause 3 file in this same
// suite; see hlc/ErrorCatalog/chapter-3 for the rationale.
//
// Behaviour observed while writing this file (hlc.exe -d db over the fixture):
// r573_sub compiles, and the instantiation inside the checker is rejected by
// the grammar with a single syntax error at 13:2.
//
// Rows 580, 582, 596, 597 were merged in from a sibling snapshot of this same
// catalog file. All four are semantic (COMP/LINT) violations that parse
// cleanly. Rows 580, 582 and 597 are not yet implemented in HLC's Linter and
// are marked GTEST_SKIP() accordingly -- see each test's own comment.
//
// Rows 590 and 595 were added later: row 590's automatic-lifetime checker
// variable compiles with no diagnostic at all. Row 595's "rand" qualifier on
// a function formal is rejected by the grammar itself -- a PA_SYNTAX_ERROR
// fires at 80:22, the same style of grammar-level gap already documented for
// row 573 above -- so its own catalogued COMP_ILLEGAL_QUALIFIER code never
// gets a chance to run either.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter17ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "17--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 573: nothing instantiable inside a checker (17.2) ------------------

TEST_F(Chapter17ErrorRulesTest, Row573_ModuleInstantiationInsideACheckerIsRejected) {
  // catalog row 573 | 17.2 | COMP
  // "Modules, interfaces, and programs shall not be instantiated inside
  // checkers." The instance on line 13 is inside checker r573_c.
  //
  // HLC enforces this in the grammar: checker_or_generate_item has no
  // instantiation alternative, so the instance is a syntax error rather than a
  // semantic one. The rule is honoured either way, which is what this asserts;
  // the catalog's COMP_ILLEGAL_INSTANTIATION code may therefore be
  // unnecessary for this row. Recorded here rather than acted on.
  EXPECT_NE(findError(ErrorDefinition::PA_SYNTAX_ERROR, 13, 2), nullptr)
      << "a module cannot be instantiated inside a checker (IEEE 1800-2023 17.2)";
}

// --- row 580: $ as an actual requires an untyped formal used only as a -----
// --- delay-range upper bound (17.3) -----------------------------------------

TEST_F(Chapter17ErrorRulesTest, Row580_DollarActualRequiresUntypedFormalUsedAsDelayBound) {
  // catalog row 580 | 17.3 | LINT
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 17.3)";
  // If $ is an actual input argument to a checker instance, the
  // corresponding formal shall be untyped and each of its references shall
  // either be an upper bound in a cycle_delay_const_range_expression or
  // itself be an actual argument in an instance of a named sequence or
  // property, in a checker instance, or as a default argument to a nested
  // checker. r580_inst passes $ to "hi", a typed (logic) formal that is used
  // as a plain assertion operand, not a delay-range bound.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r580_inst"), nullptr)
      << "$ passed to a typed formal used outside a delay-range bound must be diagnosed (IEEE 1800-2023 17.3)";
}

// --- row 582: checker actual output argument must be an lvalue (17.3) ------

TEST_F(Chapter17ErrorRulesTest, Row582_CheckerOutputArgumentMustBeLvalue) {
  // catalog row 582 | 17.3 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 17.3)";
  // Each checker actual output argument shall be a variable_lvalue or a
  // net_lvalue. r582_inst's second actual, "r582_y & r582_z", is a plain
  // expression, not an lvalue.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_ASSIGNMENT_LHS, "r582_inst"), nullptr)
      << "a checker actual output argument that is not an lvalue must be diagnosed (IEEE 1800-2023 17.3)";
}

// --- row 596: functions in checker variable assignments cannot have output,-
// --- inout or ref arguments (17.8) ------------------------------------------

TEST_F(Chapter17ErrorRulesTest, Row596_FunctionInCheckerAssignmentCannotHaveOutputArgument) {
  // catalog row 596 | 17.8 | COMP
  // Functions appearing in expressions on the right-hand side of checker
  // variable assignments shall not contain output, inout, or ref arguments
  // (const ref is allowed). r596_f has an output argument and is called from
  // "z <= f(t)" inside checker r596_c.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r596_f"), nullptr)
      << "a function with an output argument used in a checker variable assignment must be diagnosed "
         "(IEEE 1800-2023 17.8)";
}

// --- row 597: functions in checker variable assignments must be automatic --
// --- and side-effect free (17.8) --------------------------------------------

TEST_F(Chapter17ErrorRulesTest, Row597_FunctionInCheckerAssignmentMustBeSideEffectFree) {
  // catalog row 597 | 17.8 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 17.8)";
  // Functions called in expressions on the right-hand side of checker
  // variable assignments shall be automatic (or preserve no state
  // information) and have no side effects. r597_f retains state via a
  // static local variable and is called from "z <= f(a)" inside checker
  // r597_c.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_SIDE_EFFECT, "r597_f"), nullptr)
      << "a stateful/side-effecting function used in a checker variable assignment must be diagnosed "
         "(IEEE 1800-2023 17.8)";
}

// --- row 590: checker variables must have static lifetime (17.7) -----------

TEST_F(Chapter17ErrorRulesTest, Row590_AutomaticLifetimeCheckerVariableIsRejected) {
  // catalog row 590 | 17.7 | COMP
  // r590_c's "automatic bit r590_v;" on line 73 declares a checker variable
  // with an explicit automatic lifetime.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_AUTOMATIC_LIFETIME, "r590_v", 73, 17), nullptr)
      << "a checker variable must have a static lifetime (IEEE 1800-2023 17.7)";
}

// --- row 595: checker function formals/internals cannot be rand (17.8) -----

TEST_F(Chapter17ErrorRulesTest, Row595_CheckerFunctionFormalAndInternalCannotBeRand) {
  // catalog row 595 | 17.8 | COMP
  GTEST_SKIP() << "blocked on a grammar change (rand/randc is rejected on any function formal "
                  "everywhere, not just in checkers -- a corpus-wide-impact fix); see "
                  ".claude/instructions/error_catalog_known_gaps.md item 2";
  // The formal arguments and internal variables of functions used in
  // checkers shall not be declared as free (rand) variables. r595_f declares
  // both a rand formal (line 80) and a rand internal variable (line 81).
  //
  // HLC's grammar already rejects "rand" as a function-formal qualifier
  // outright -- a PA_SYNTAX_ERROR fires at 80:22 -- the same shape as row
  // 573's grammar-level rejection above, so the catalog's own designated
  // code below never gets a chance to run; the assertions still target that
  // code, per project convention.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_QUALIFIER, "r595_p", 80, 32), nullptr)
      << "a checker function's formal argument cannot be rand (IEEE 1800-2023 17.8)";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_QUALIFIER, "r595_local_v", 81, 14), nullptr)
      << "a checker function's internal variable cannot be rand (IEEE 1800-2023 17.8)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
