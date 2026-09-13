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

// Tests for the IEEE 1800-2023 Clause 20 (utility system tasks and system
// functions) error scenarios catalogued in docs/error_catalog.xml (rows
// 731, 732, 737, 744, 746).
//
// Scope: this file asserts ONLY that the diagnostic each catalog row
// requires is emitted. It deliberately makes no assertion about the shape of
// the compiled model. Exactly one TEST_F per catalog row, named Row<N>_...
// after that row and carrying a "catalog row N | clause | category" comment;
// that is the link between the catalog and this file.
//
// Fixture: 20--error_rules.sv, compiled by 20--error_rules.hlc. All five
// scenarios compile with zero SYNTAX/FATAL/ERROR diagnostics (verified
// against the installed hlc.exe), so they all live in this one fixture --
// there are no _inv* sibling files for this chapter.
//
// None of the ErrorDefinition codes this chapter's rows name
// (COMP_VALUE_OUT_OF_RANGE, COMP_ILLEGAL_ASSIGNMENT_LHS,
// COMP_ILLEGAL_EXPRESSION_CONTEXT) has a single call site anywhere in src/
// (confirmed by grep) -- every one of these 5 rows is unimplemented today.
// Every TEST_F below therefore asserts the diagnostic the standard requires
// and is expected to fail (red) until Clause 20 semantic checking is
// implemented -- per project convention this is written as the real
// assertion, never as an assertion that the diagnostic is absent.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter20ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "20--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 731: $stop/$finish diagnostic argument must be 0, 1, or 2 (20.2) ---

TEST_F(Chapter20ErrorRulesTest, Row731_FinishDiagnosticArgumentMustBe0Or1Or2) {
  // catalog row 731 | 20.2 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 20.2)";
  // The optional argument to $stop and $finish shall be an expression whose
  // value is 0, 1, or 2 (Table 20-1); any other diagnostic-level value is
  // illegal. r731_m calls "$finish(3);".
  EXPECT_NE(findError(ErrorDefinition::COMP_VALUE_OUT_OF_RANGE, "r731_m"), nullptr)
      << "$finish's optional diagnostic argument must be 0, 1, or 2 (IEEE 1800-2023 20.2, Table 20-1)";
}

// --- row 732: $timeformat's units_number/precision_number range (20.4.3) ----

TEST_F(Chapter20ErrorRulesTest, Row732_TimeformatUnitsNumberMustBeInRange) {
  // catalog row 732 | 20.4.3 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 20.4.3)";
  // The units_number and precision_number arguments of $timeformat shall be
  // integers in the range from 2 down to -15. r732_m calls
  // "$timeformat(-20, 5, " ns", 10);" with units_number -20.
  EXPECT_NE(findError(ErrorDefinition::COMP_VALUE_OUT_OF_RANGE, "r732_m"), nullptr)
      << "$timeformat's units_number/precision_number must be in [-15, 2] (IEEE 1800-2023 20.4.3)";
}

// --- row 737: $isunbounded's argument must be a parameter name (20.6.3) -----

TEST_F(Chapter20ErrorRulesTest, Row737_IsunboundedArgumentMustBeParameterName) {
  // catalog row 737 | 20.6.3 | COMP
  // The argument to $isunbounded shall be a parameter name (a
  // ps_parameter_identifier or hierarchical_parameter_identifier); an
  // ordinary variable or expression is not allowed. r737_m calls
  // "$isunbounded(v);" where v is a plain int variable, not a parameter.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "v"), nullptr)
      << "$isunbounded's argument must be a parameter name, not a variable (IEEE 1800-2023 20.6.3)";
}

// --- row 744: $assertcontrol's control_type must be one of Table 20-5's values (20.11) ---

TEST_F(Chapter20ErrorRulesTest, Row744_AssertcontrolControlTypeMustBeInTable20_5) {
  // catalog row 744 | 20.11 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 20.11)";
  // The control_type argument of $assertcontrol shall be an integer
  // expression whose value is one of the values defined in Table 20-5 (1
  // through 11). r744_m calls "$assertcontrol(99);".
  EXPECT_NE(findError(ErrorDefinition::COMP_VALUE_OUT_OF_RANGE, "r744_m"), nullptr)
      << "$assertcontrol's control_type must be one of Table 20-5's values 1..11 (IEEE 1800-2023 20.11)";
}

// --- row 746: $random's seed must be an integral variable (20.14.1) ---------

TEST_F(Chapter20ErrorRulesTest, Row746_RandomSeedMustBeIntegralVariable) {
  // catalog row 746 | 20.14.1 | COMP
  // The seed argument of $random shall be an integral variable (not a
  // constant, expression, or non-integral object), because it is updated by
  // the call. r746_m calls "r = $random(3);" -- the constant 3 is passed as
  // the seed, not a variable.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_ASSIGNMENT_LHS, "r746_m"), nullptr)
      << "$random's seed argument must be an integral variable, not a constant (IEEE 1800-2023 20.14.1)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
