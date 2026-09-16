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

// Tests for the IEEE 1800-2023 Clause 13 (Tasks and functions) error scenarios
// catalogued in sv_error_catalog_Latest.xlsx (rows 402, 404, 408, 420, 421,
// 432).
//
// Scope, fixture layout and test shapes follow the Clause 3 file in this same
// suite; see hlc/ErrorCatalog/chapter-3 for the rationale.
//
// Behaviour observed while writing this file (hlc.exe -d db over the fixture):
// 13--error_rules.sv compiles with no errors, no warnings and no syntax errors
// at all; every module is built. None of these six rules is checked in HLC
// today.
//
// Row 404 is the one row in this file whose rule has two halves that must be
// asserted together -- the warning on the bare call AND its absence on the
// void-cast call. Only the first half is written as an assertion here: an
// EXPECT_EQ(..., nullptr) on the second would, today, pass for the wrong
// reason (nothing is reported for either call), which is exactly the shape
// davtests.md warns against. When the diagnostic is implemented, the
// void-cast half needs adding at the same time.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter13ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "13--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 402: function with output/inout/ref args outside a procedural
//              statement or in an event expression (13.4) -----------------

TEST_F(Chapter13ErrorRulesTest, Row402_FunctionWithOutputArgIllegalOutsideProceduralStatement) {
  // catalog row 402 | 13.4 | COMP
  // It shall be illegal to call a function with output, inout or (non-const)
  // ref arguments in an event expression, in an expression within a
  // procedural continuous assignment, or in an expression that is not
  // within a procedural statement. r402_m violates this twice: line 19's
  // "assign w = f(a);" (continuous assignment) and line 20's "always
  // @(f(a)) ;" (event expression).
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r402_m", 19, 14), nullptr)
      << "a function with an output argument cannot be called from a continuous assignment "
         "(IEEE 1800-2023 13.4)";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r402_m", 20, 12), nullptr)
      << "a function with an output argument cannot be called from an event expression "
         "(IEEE 1800-2023 13.4)";
}

// --- row 404: discarding a function's return value (13.4.1) -----------------

TEST_F(Chapter13ErrorRulesTest, Row404_DiscardedNonvoidReturnValueIsWarned) {
  // catalog row 404 | 13.4.1 | COMP
  // "It shall be legal to call a function as if it had no return value, but
  // then a warning shall be issued. To suppress the warning, the function call
  // can be cast to void." The bare call is on line 33; the void-cast call on
  // line 34 must stay silent, which is the negative half of the same rule.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 13.4.1 requires a warning when a "
                  "nonvoid function is called as a statement, suppressed by a void cast";
  EXPECT_NE(findError(ErrorDefinition::COMP_UNUSED_RETURN_VALUE, "f", 33, 5), nullptr)
      << "discarding a nonvoid function's return value must be warned (IEEE 1800-2023 13.4.1)";
}

// --- row 408: constant functions and argument directions (13.4.3) -----------

TEST_F(Chapter13ErrorRulesTest, Row408_ConstantFunctionWithOutputArgumentIsRejected) {
  // catalog row 408 | 13.4.3 | COMP
  // A constant function -- one called where a constant expression is required
  // -- shall not have output, inout or ref arguments. cf declares an output on
  // line 41 and is called from a localparam initializer on line 46, which is
  // what makes it a constant function call.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_CONSTANT_FUNCTION, "cf", 46, 18), nullptr)
      << "a constant function cannot have an output argument (IEEE 1800-2023 13.4.3)";
}

// --- row 420: function scheduling a post-return event outside an
//              initial/always/fork thread (13.4.4) -------------------------

TEST_F(Chapter13ErrorRulesTest, Row420_PostReturnEventSchedulingOutsideInitialAlwaysForkThread) {
  // catalog row 420 | 13.4.4 | LINT
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 13.4.4)";
  // Calling a function that schedules an event that cannot become active
  // until after the function returns is allowed only from a thread created
  // by an initial procedure, an always procedure, or a fork from one of
  // those. r420_m's line 62, "bit y = watch_for_zero(stack);", calls it from
  // a variable initializer, which is none of those.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r420_m"), nullptr)
      << "a function scheduling a post-return event may only be called from an "
         "initial/always/fork thread (IEEE 1800-2023 13.4.4)";
}

// --- row 421: output/inout formal actual must be a valid procedural
//              assignment LHS (13.5) ---------------------------------------

TEST_F(Chapter13ErrorRulesTest, Row421_OutputFormalActualMustBeValidAssignmentLhs) {
  // catalog row 421 | 13.5 | COMP
  // If a subroutine formal argument is declared output or inout, the
  // corresponding expression in the call shall be restricted to an
  // expression valid on the left-hand side of a procedural assignment.
  // r421_m's line 72, "t(1 + 2);", binds an arithmetic expression to an
  // output formal.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_ASSIGNMENT_LHS, "o", 72, 13), nullptr)
      << "an output formal's actual argument must be a valid assignment LHS (IEEE 1800-2023 13.5)";
}

// --- row 432: omitted arguments need defaults (13.5.3) ----------------------

TEST_F(Chapter13ErrorRulesTest, Row432_OmittedArgumentWithoutDefaultIsRejected) {
  // catalog row 432 | 13.5.3 | COMP
  // "If an unspecified argument is used for an argument that has no default
  // value, a compiler error shall be issued." read's formal k has no default;
  // line 83 omits every argument and line 84 leaves k's position empty.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 13.5.3 requires an error when a formal "
                  "with no default value is left unspecified at the call site";
  EXPECT_NE(findError(ErrorDefinition::COMP_MISSING_ARGUMENT, "k", 83, 5), nullptr)
      << "calling read() omits k, which has no default value (IEEE 1800-2023 13.5.3)";
  EXPECT_NE(findError(ErrorDefinition::COMP_MISSING_ARGUMENT, "k", 84, 5), nullptr)
      << "an empty argument position for k, which has no default value, is illegal "
         "(IEEE 1800-2023 13.5.3)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
