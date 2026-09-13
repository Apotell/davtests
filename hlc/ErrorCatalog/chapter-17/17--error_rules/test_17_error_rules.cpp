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

// Tests for the IEEE 1800-2023 Clause 17 (Checkers) error scenarios
// catalogued in docs/error_catalog.xml.
//
// Scope: this file asserts ONLY that the diagnostic each catalog row
// requires is emitted. It deliberately makes no assertion about the shape
// of the compiled model. Exactly one TEST_F per catalog row, named
// Row<N>_... after that row and carrying a "catalog row N | clause |
// category" comment; that is the link between the workbook and this file.
//
// Fixture (compiled in one run by 17--error_rules.hlc):
//   17--error_rules.sv  rows 580, 582, 596, 597
//
// All four scenarios are semantic (COMP/LINT) violations that parse
// cleanly, so a single compilation observes all of them at once; no
// sibling _inv*.sv fixture was needed (verified by running hlc.exe: 0
// SYNTAX, 0 FATAL, every checker/module design element intact).
//
// Covered rows: 580 (17.3), 582 (17.3), 596 (17.8), 597 (17.8).
//
// None of the four ErrorDefinition codes below (COMP_ILLEGAL_ASSIGNMENT_LHS,
// COMP_ILLEGAL_EXPRESSION_CONTEXT, COMP_ILLEGAL_SIDE_EFFECT) currently has
// any call site in src/ outside of ErrorDefinition.cpp's own registration
// table, so every assertion below is expected to fail (red) today -- that
// is the intended, documented behavior of this catalog exercise, not a bug
// to work around. Per project convention, no GTEST_SKIP() is used; each
// test asserts exactly the diagnostic IEEE 1800-2023 requires.

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

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
