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

// Tests for the IEEE 1800-2023 Clause 18 (Constrained random value generation)
// error scenarios catalogued in sv_error_catalog_Latest.xlsx.
//
// Scope, fixture layout and test shapes follow the Clause 3 file in this same
// suite; see hlc/ErrorCatalog/chapter-3 for the rationale.
//
// Fixture (compiled in one run by 18--error_rules.hlc):
//   18--error_rules.sv  rows 608, 613, 614, 615, 616, 629, 630, 632, 633,
//                        634, 636, 637, 640, 641, 645, 646, 647, 650, 652,
//                        659, 663, 665, 666
//
// Rows 608 and 663 were the original two scenarios in this file; behaviour
// observed while writing those two (hlc.exe -d db over the fixture): r663_m
// compiles and r608_c parses. Two diagnostics appear, neither of them the
// required one and neither asserted below: a CP5849 "Feature not yet
// implemented" warning for the randsequence, and a Linter null-actual error
// because the aa.size method call in the constraint never binds. The latter
// is a binding gap that happens to sit on top of the row 608 construct; it
// would keep passing after aa.size started binding correctly, at which point
// the real rule would go unchecked.
//
// The remaining 21 scenarios (rows 613, 614, 615, 616, 629, 630, 632, 633,
// 634, 636, 637, 640, 641, 645, 646, 647, 650, 652, 659, 665, 666) were
// merged into this same fixture and verified independently (in isolation
// from rows 608/663) with hlc.exe: every one of those top-level class/module
// design elements compiles, and 5 of them (rows 613, 615, 630, 637, 652)
// raise a generic PA_SYNTAX_ERROR -- HLC's grammar already rejects those five
// constructs outright (e.g. "solve x() before y;", a dist expression nested
// inside another expression, "default :=" in a distribution, a function call
// as a foreach array identifier, and an identifier_list after "with" on a
// scope randomize), before the catalog's own designated semantic
// ErrorDefinition code would ever have a chance to run. None of those five
// syntax errors swallows or corrupts any other scenario in the file (every
// design element after each one still compiles).
//
// Per project convention, every assertion below targets the catalog's own
// designated ErrorDefinition code (not the PA_SYNTAX_ERROR that happens to
// fire today for those five rows), since that is the diagnostic IEEE
// 1800-2023 actually requires for the rule being tested.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter18ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 608: associative array size is not randomizable (18.4) -------------

TEST_F(Chapter18ErrorRulesTest, Row608_ConstrainingAssociativeArraySizeIsRejected) {
  // catalog row 608 | 18.4 | COMP
  // Dynamic arrays and queues may have their size constrained; associative
  // arrays may not -- their size and index values are not randomizable. The
  // constraint on line 12 constrains aa.size for an associative array keyed by
  // string.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 18.4 makes the size and index values "
                  "of an associative array non-randomizable, so constraining aa.size is illegal";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "aa", 12, 18), nullptr)
      << "the size of an associative array cannot be constrained (IEEE 1800-2023 18.4)";
}

// --- row 613: parentheses on a constraint_primary require an array method -
// --- call, e.g. size() (18.5) -----------------------------------------------

TEST_F(Chapter18ErrorRulesTest, Row613_ParenthesesOnConstraintPrimaryRequireArrayMethod) {
  // catalog row 613 | 18.5 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 18.5)";
  // In a constraint_primary, parentheses are allowed only when the
  // constraint_primary is an array built-in method such as size().
  // r613_c's "solve r613_x() before r613_y;" applies parentheses to a plain
  // variable, not an array method. HLC's grammar already rejects this input
  // outright (a PA_SYNTAX_ERROR fires at compile time), so the catalog's own
  // designated code below never gets a chance to run; the assertion still
  // targets that code, per project convention.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "r613_c"), nullptr)
      << "parentheses on a non-array-method constraint_primary must be diagnosed (IEEE 1800-2023 18.5)";
}

// --- row 614: side-effect operators are not allowed in constraints (18.5) --

TEST_F(Chapter18ErrorRulesTest, Row614_SideEffectOperatorNotAllowedInConstraint) {
  // catalog row 614 | 18.5 | PARSE
  // Operators with side effects, such as ++ and --, are not allowed in
  // constraint expressions. r614_c uses "r614_y++" inside its expression.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_SIDE_EFFECT, "r614_c"), nullptr)
      << "a side-effect operator inside a constraint expression must be diagnosed (IEEE 1800-2023 18.5)";
}

// --- row 615: dist may not appear nested inside another expression (18.5) --

TEST_F(Chapter18ErrorRulesTest, Row615_DistExpressionCannotNestInAnotherExpression) {
  // catalog row 615 | 18.5 | PARSE
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 18.5)";
  // dist expressions may not appear inside other expressions; a dist may
  // only form a complete constraint expression. r615_c nests "r615_x dist
  // {...}" inside a "&&" expression. HLC's grammar already rejects this
  // input outright (a PA_SYNTAX_ERROR fires at compile time), so the
  // catalog's own designated code below never gets a chance to run; the
  // assertion still targets that code, per project convention.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r615_c"), nullptr)
      << "a dist expression nested inside another expression must be diagnosed (IEEE 1800-2023 18.5)";
}

// --- row 616: an external constraint block must follow its class (18.5.1) --

TEST_F(Chapter18ErrorRulesTest, Row616_ExternalConstraintBlockMustFollowItsClass) {
  // catalog row 616 | 18.5.1 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 18.5.1)";
  // An external constraint block shall appear in the same scope as the
  // corresponding class declaration and shall appear after the class
  // declaration in that scope. "constraint r616_C::r616_proto1 { ... }"
  // appears before "class r616_C" is declared.
  EXPECT_NE(findError(ErrorDefinition::COMP_MISPLACED_EXTERN_DECLARATION, "r616_proto1"), nullptr)
      << "an external constraint block preceding its class declaration must be diagnosed "
         "(IEEE 1800-2023 18.5.1)";
}

// --- row 629: a real-valued dist range must use :/ with a weight (18.5.3) --

TEST_F(Chapter18ErrorRulesTest, Row629_RealDistRangeRequiresColonSlashWithWeight) {
  // catalog row 629 | 18.5.3 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 18.5.3)";
  // A dist_item whose value_range is a range of real values shall use the
  // :/ operator and shall specify a weight. r629_c's "[0.5:1.5] := 3" uses
  // := on a real range instead of :/.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "r629_c"), nullptr)
      << "a real-valued dist range not using :/ with a weight must be diagnosed (IEEE 1800-2023 18.5.3)";
}

// --- row 630: the default dist specification must use :/ (18.5.3) ----------

TEST_F(Chapter18ErrorRulesTest, Row630_DistDefaultSpecificationMustUseColonSlash) {
  // catalog row 630 | 18.5.3 | PARSE
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 18.5.3)";
  // The default specification in a distribution shall always use the :/
  // operator; it shall be an error if the operator is omitted or if := is
  // used. r630_c's dist uses "default := 1". HLC's grammar already rejects
  // this input outright (a PA_SYNTAX_ERROR fires at compile time), so the
  // catalog's own designated code below never gets a chance to run; the
  // assertion still targets that code, per project convention.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "r630_c"), nullptr)
      << "a dist default specification using := instead of :/ must be diagnosed (IEEE 1800-2023 18.5.3)";
}

// --- row 632: dist shall not be applied to randc variables (18.5.3) --------

TEST_F(Chapter18ErrorRulesTest, Row632_DistCannotBeAppliedToRandcVariable) {
  // catalog row 632 | 18.5.3 | COMP
  // A dist operation shall not be applied to randc variables. r632_c applies
  // dist to r632_x, a randc variable.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "r632_c"), nullptr)
      << "a dist operation applied to a randc variable must be diagnosed (IEEE 1800-2023 18.5.3)";
}

// --- row 633: a dist expression needs at least one rand variable (18.5.3) --

TEST_F(Chapter18ErrorRulesTest, Row633_DistExpressionRequiresARandVariable) {
  // catalog row 633 | 18.5.3 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 18.5.3)";
  // A dist expression requires that the expression contain at least one
  // rand variable. r633_c applies dist to r633_s, a plain (non-rand) int.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "r633_c"), nullptr)
      << "a dist expression with no rand variable must be diagnosed (IEEE 1800-2023 18.5.3)";
}

// --- row 634: unique{} range_list items must be singular variables (18.5.4)-

TEST_F(Chapter18ErrorRulesTest, Row634_UniqueRangeListItemsMustBeSingularVariables) {
  // catalog row 634 | 18.5.4 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 18.5.4)";
  // The range_list of a uniqueness_constraint shall contain only expressions
  // that denote singular variables of integral or real type, or unpacked
  // array variables (or slices) whose leaf element type is integral or
  // real. r634_u's "unique { r634_a + r634_b, r634_a }" includes an
  // expression, not a bare variable.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "r634_u"), nullptr)
      << "a non-variable expression inside a unique{} range_list must be diagnosed (IEEE 1800-2023 18.5.4)";
}

// --- row 636: no randc variable in a uniqueness constraint group (18.5.4) --

TEST_F(Chapter18ErrorRulesTest, Row636_NoRandcVariableInUniquenessConstraintGroup) {
  // catalog row 636 | 18.5.4 | COMP
  // No randc variable shall appear in the group of a uniqueness constraint.
  // r636_u's "unique { r636_a, r636_b }" includes r636_a, a randc variable.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "r636_u"), nullptr)
      << "a randc variable inside a unique{} group must be diagnosed (IEEE 1800-2023 18.5.4)";
}

// --- row 637: a function call cannot be a foreach array identifier (18.5.7.1)

TEST_F(Chapter18ErrorRulesTest, Row637_FunctionCallCannotBeForeachArrayIdentifier) {
  // catalog row 637 | 18.5.7.1 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 18.5.7.1)";
  // It shall be an error to include a function call as an implicit variable
  // declaration in the array identifier of a foreach iterative constraint.
  // r637_c's "foreach ( r637_A[r637_f()] )" calls r637_f() as the loop
  // variable declaration. HLC's grammar already rejects this input outright
  // (a PA_SYNTAX_ERROR fires at compile time), so the catalog's own
  // designated code below never gets a chance to run; the assertion still
  // targets that code, per project convention.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r637_c"), nullptr)
      << "a function call as a foreach iterative constraint's array identifier must be diagnosed "
         "(IEEE 1800-2023 18.5.7.1)";
}

// --- row 640: only rand variables are allowed in solve...before (18.5.9) ---

TEST_F(Chapter18ErrorRulesTest, Row640_OnlyRandVariablesAllowedInSolveBefore) {
  // catalog row 640 | 18.5.9 | COMP
  // Only rand random variables are allowed in a solve...before ordering
  // constraint; nonrandom (state) variables are not allowed. r640_o's
  // "solve r640_x before r640_y;" names r640_y, a plain (non-rand) int.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "r640_o"), nullptr)
      << "a non-random variable inside solve...before must be diagnosed (IEEE 1800-2023 18.5.9)";
}

// --- row 641: randc variables are not allowed in solve...before (18.5.9) ---

TEST_F(Chapter18ErrorRulesTest, Row641_RandcVariableNotAllowedInSolveBefore) {
  // catalog row 641 | 18.5.9 | COMP
  // randc variables are not allowed in a solve...before ordering constraint
  // (randc variables are always solved first). r641_o's "solve r641_x
  // before r641_y;" names r641_x, a randc variable.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "r641_o"), nullptr)
      << "a randc variable inside solve...before must be diagnosed (IEEE 1800-2023 18.5.9)";
}

// --- row 645: constraint functions cannot have output/inout/ref args ------
// --- (18.5.11) ---------------------------------------------------------------

TEST_F(Chapter18ErrorRulesTest, Row645_ConstraintFunctionCannotHaveOutputArgument) {
  // catalog row 645 | 18.5.11 | LINT
  // Functions that appear in constraint expressions shall not have output,
  // inout, or ref arguments (const ref is allowed). r645_f has an output
  // argument and is called from constraint r645_c.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r645_f"), nullptr)
      << "a function with an output argument used in a constraint expression must be diagnosed "
         "(IEEE 1800-2023 18.5.11)";
}

// --- row 646: constraint functions must be automatic and side-effect free -
// --- (18.5.11) ---------------------------------------------------------------

TEST_F(Chapter18ErrorRulesTest, Row646_ConstraintFunctionMustBeSideEffectFree) {
  // catalog row 646 | 18.5.11 | LINT
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 18.5.11)";
  // Functions that appear in constraint expressions shall be automatic (or
  // preserve no state information) and have no side effects. r646_f retains
  // state via a static local variable and is called from constraint
  // r646_c.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_SIDE_EFFECT, "r646_f"), nullptr)
      << "a stateful/side-effecting function used in a constraint expression must be diagnosed "
         "(IEEE 1800-2023 18.5.11)";
}

// --- row 647: constraint functions cannot modify constraints (18.5.11) -----

TEST_F(Chapter18ErrorRulesTest, Row647_ConstraintFunctionCannotModifyConstraints) {
  // catalog row 647 | 18.5.11 | LINT
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 18.5.11)";
  // Functions called from constraints cannot modify the constraints, e.g.,
  // by calling the rand_mode() or constraint_mode() methods. r647_f calls
  // "this.r647_d.constraint_mode(0)" and is invoked from constraint r647_c.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_SIDE_EFFECT, "r647_f"), nullptr)
      << "a constraint-called function that modifies constraints must be diagnosed (IEEE 1800-2023 18.5.11)";
}

// --- row 650: soft constraints are illegal on randc variables (18.5.13.1) --

TEST_F(Chapter18ErrorRulesTest, Row650_SoftConstraintIllegalOnRandcVariable) {
  // catalog row 650 | 18.5.13.1 | COMP
  // Soft constraints can only be specified on rand random variables; they
  // may not be specified for randc variables. r650_c's "soft r650_x == 3;"
  // applies soft to r650_x, a randc variable.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "r650_c"), nullptr)
      << "a soft constraint on a randc variable must be diagnosed (IEEE 1800-2023 18.5.13.1)";
}

// --- row 652: a scope randomize cannot carry an identifier_list after with,-
// --- nor accept null (18.7) -------------------------------------------------

TEST_F(Chapter18ErrorRulesTest, Row652_ScopeRandomizeCannotUseWithIdentifierListOrNull) {
  // catalog row 652 | 18.7 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 18.7)";
  // In a randomize_call that is not a method call on an object of class
  // type (a scope randomize), the optional parenthesized identifier_list
  // after the keyword with shall be illegal, and the use of null as an
  // argument shall be illegal. Two facets, both in r652_m: "std::randomize
  // (r652_a) with (r652_a) { ... }" carries an illegal identifier_list, and
  // "std::randomize(null)" passes null. The first facet's construct is
  // rejected outright by HLC's grammar today (a PA_SYNTAX_ERROR fires), so
  // the catalog's own designated code below never gets a chance to run for
  // it; both assertions still target that code, per project convention.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r652_a"), nullptr)
      << "an identifier_list after with on a scope randomize must be diagnosed (IEEE 1800-2023 18.7)";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r652_m"), nullptr)
      << "null as an argument to a scope randomize must be diagnosed (IEEE 1800-2023 18.7)";
}

// --- row 659: randomize() arguments must be property names, not -----------
// --- expressions (18.11) ------------------------------------------------------

TEST_F(Chapter18ErrorRulesTest, Row659_RandomizeArgumentMustBeAPropertyNameNotAnExpression) {
  // catalog row 659 | 18.11 | COMP
  // Arguments to the randomize() method are limited to the names of
  // properties of the calling object; expressions are not allowed as
  // arguments. "r659_a.randomize(r659_x + r659_y)" passes an expression.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r659_a"), nullptr)
      << "an expression passed as a randomize() argument must be diagnosed (IEEE 1800-2023 18.11)";
}

// --- row 663: randsequence production weights (18.17.1) ---------------------

TEST_F(Chapter18ErrorRulesTest, Row663_NegativeProductionWeightIsRejected) {
  // catalog row 663 | 18.17.1 | COMP
  // "The rs_weight_specification shall evaluate to an integral non-negative
  // value." The weight on line 195 is -3.
  //
  // HLC does not model randsequence at all yet -- it emits CP5849 "Feature not
  // yet implemented" for the whole construct -- so this row is blocked behind
  // that, not merely unchecked.
  GTEST_SKIP() << "randsequence is not modelled (CP5849 'Feature not yet implemented'); "
                  "IEEE 1800-2023 18.17.1 requires an rs_weight_specification to evaluate to an "
                  "integral non-negative value";
  EXPECT_NE(findError(ErrorDefinition::COMP_VALUE_OUT_OF_RANGE, "add", 195, 21), nullptr)
      << "a production weight must be a non-negative integral value (IEEE 1800-2023 18.17.1)";
}

// --- row 665: a repeat production's count must be non-negative (18.17.4) ---

TEST_F(Chapter18ErrorRulesTest, Row665_RandsequenceRepeatCountMustBeNonNegative) {
  // catalog row 665 | 18.17.4 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 18.17.4)";
  // The repeat expression of a repeat production statement shall evaluate
  // to a non-negative integral value. r665_P's "repeat (-1) r665_PUSH ;"
  // gives repeat a negative count.
  EXPECT_NE(findError(ErrorDefinition::COMP_VALUE_OUT_OF_RANGE, "r665_P"), nullptr)
      << "a negative randsequence repeat count must be diagnosed (IEEE 1800-2023 18.17.4)";
}

// --- row 666: a rand join weight must be a real number in 0.0..1.0 --------
// --- (18.17.5) -----------------------------------------------------------

TEST_F(Chapter18ErrorRulesTest, Row666_RandJoinWeightMustBeInZeroToOneRange) {
  // catalog row 666 | 18.17.5 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 18.17.5)";
  // The optional expression following the rand join keywords shall be a
  // real number in the range 0.0 to 1.0. r666_TOP's "rand join (1.5) ..."
  // gives a weight of 1.5, outside that range.
  EXPECT_NE(findError(ErrorDefinition::COMP_VALUE_OUT_OF_RANGE, "r666_TOP"), nullptr)
      << "a rand join weight outside 0.0..1.0 must be diagnosed (IEEE 1800-2023 18.17.5)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
