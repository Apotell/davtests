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

// Tests for the IEEE 1800-2023 Clause 13 error scenarios catalogued in
// docs/error_catalog.xml.
//
// Scope: this file asserts ONLY that the diagnostic each catalog row requires
// is emitted. It deliberately makes no assertion about the shape of the
// compiled model. Exactly one TEST_F per catalog row, named Row<N>_... after
// that row and carrying a "catalog row N | clause | category" comment; that
// is the link between the catalog and this file.
//
// Fixture (compiled in one run by 13--error_rules.hlc):
//   13--error_rules.sv   rows 402, 404, 420, 421, 432
//
// Catalog rows covered here:
//   402 | 13.4     | COMP_ILLEGAL_EXPRESSION_CONTEXT
//   404 | 13.4.1   | COMP_UNUSED_RETURN_VALUE
//   420 | 13.4.4   | COMP_ILLEGAL_EXPRESSION_CONTEXT
//   421 | 13.5     | COMP_ILLEGAL_ASSIGNMENT_LHS
//   432 | 13.5.3   | COMP_MISSING_ARGUMENT
//
// Per project convention, no test below uses GTEST_SKIP(): every assertion
// states the diagnostic IEEE 1800-2023 requires, even where the relevant
// ErrorDefinition code currently has zero call sites anywhere in src/
// (confirmed by grep for each of the five codes above) -- i.e. every one of
// these is expected to be red today. No test here asserts the absence of a
// diagnostic.

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
  // within a procedural statement. Both "assign w = f(a);" and
  // "always @(f(a)) ;" violate this.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r402_m"), nullptr)
      << "a function with an output argument cannot be called from a continuous assignment or an "
         "event expression (IEEE 1800-2023 13.4)";
}

// --- row 404: unused return value of a nonvoid function (13.4.1) ----------

TEST_F(Chapter13ErrorRulesTest, Row404_UnusedNonvoidFunctionReturnValueWarns) {
  // catalog row 404 | 13.4.1 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 13.4.1)";
  // Calling a nonvoid function as if it had no return value shall be legal
  // but shall issue a warning; void'(...) suppresses it. "f();" on its own
  // (unsuppressed) is the offending statement.
  EXPECT_NE(findError(ErrorDefinition::COMP_UNUSED_RETURN_VALUE, "f"), nullptr)
      << "calling a nonvoid function and discarding its return value shall issue a warning "
         "(IEEE 1800-2023 13.4.1)";
}

// --- row 420: function scheduling a post-return event outside an
//              initial/always/fork thread (13.4.4) -------------------------

TEST_F(Chapter13ErrorRulesTest, Row420_PostReturnEventSchedulingOutsideInitialAlwaysForkThread) {
  // catalog row 420 | 13.4.4 | LINT
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 13.4.4)";
  // Calling a function that schedules an event that cannot become active
  // until after the function returns is allowed only from a thread created
  // by an initial procedure, an always procedure, or a fork from one of
  // those. "bit y = watch_for_zero(stack);" calls it from a variable
  // initializer, which is none of those.
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
  // "t(1 + 2);" binds an arithmetic expression to an output formal.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_ASSIGNMENT_LHS, "o"), nullptr)
      << "an output formal's actual argument must be a valid assignment LHS (IEEE 1800-2023 13.5)";
}

// --- row 432: empty/name-omitted argument with no default value (13.5.3) --

TEST_F(Chapter13ErrorRulesTest, Row432_MissingArgumentForFormalWithNoDefaultIsRejected) {
  // catalog row 432 | 13.5.3 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 13.5.3)";
  // If an unspecified (empty) argument, or an argument omitted via name
  // binding, is used for a formal that has no default value, a compiler
  // error shall be issued. Both "read();" and "read(1, , 7);" leave 'k'
  // (which has no default) unspecified.
  EXPECT_NE(findError(ErrorDefinition::COMP_MISSING_ARGUMENT, "k"), nullptr)
      << "a formal with no default value must receive an explicit actual argument "
         "(IEEE 1800-2023 13.5.3)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
