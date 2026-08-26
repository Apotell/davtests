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
// catalogued in sv_error_catalog_Latest.xlsx (rows 404, 408, 432).
//
// Scope, fixture layout and test shapes follow the Clause 3 file in this same
// suite; see hlc/ErrorCatalog/chapter-3 for the rationale.
//
// Behaviour observed while writing this file (hlc.exe -d db over the fixture):
// 13--error_rules.sv compiles with no errors, no warnings and no syntax errors
// at all; all three modules are built. None of these three rules is checked in
// HLC today, so all three tests below are skipped.
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

// --- row 404: discarding a function's return value (13.4.1) -----------------

TEST_F(Chapter13ErrorRulesTest, Row404_DiscardedNonvoidReturnValueIsWarned) {
  // catalog row 404 | 13.4.1 | COMP
  // "It shall be legal to call a function as if it had no return value, but
  // then a warning shall be issued. To suppress the warning, the function call
  // can be cast to void." The bare call is on line 17; the void-cast call on
  // line 18 must stay silent, which is the negative half of the same rule.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 13.4.1 requires a warning when a "
                  "nonvoid function is called as a statement, suppressed by a void cast";
  EXPECT_NE(findError(ErrorDefinition::COMP_UNUSED_RETURN_VALUE, "f", 17, 5), nullptr)
      << "discarding a nonvoid function's return value must be warned (IEEE 1800-2023 13.4.1)";
}

// --- row 408: constant functions and argument directions (13.4.3) -----------

TEST_F(Chapter13ErrorRulesTest, Row408_ConstantFunctionWithOutputArgumentIsRejected) {
  // catalog row 408 | 13.4.3 | COMP
  // A constant function -- one called where a constant expression is required
  // -- shall not have output, inout or ref arguments. cf declares an output on
  // line 25 and is called from a localparam initializer on line 30, which is
  // what makes it a constant function call.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 13.4.3 forbids output, inout and ref "
                  "arguments on a function used as a constant function";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_CONSTANT_FUNCTION, "cf", 30, 18), nullptr)
      << "a constant function cannot have an output argument (IEEE 1800-2023 13.4.3)";
}

// --- row 432: omitted arguments need defaults (13.5.3) ----------------------

TEST_F(Chapter13ErrorRulesTest, Row432_OmittedArgumentWithoutDefaultIsRejected) {
  // catalog row 432 | 13.5.3 | COMP
  // "If an unspecified argument is used for an argument that has no default
  // value, a compiler error shall be issued." read's formal k has no default;
  // line 41 omits every argument and line 42 leaves k's position empty.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 13.5.3 requires an error when a formal "
                  "with no default value is left unspecified at the call site";
  EXPECT_NE(findError(ErrorDefinition::COMP_MISSING_ARGUMENT, "k", 41, 5), nullptr)
      << "calling read() omits k, which has no default value (IEEE 1800-2023 13.5.3)";
  EXPECT_NE(findError(ErrorDefinition::COMP_MISSING_ARGUMENT, "k", 42, 5), nullptr)
      << "an empty argument position for k, which has no default value, is illegal "
         "(IEEE 1800-2023 13.5.3)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
