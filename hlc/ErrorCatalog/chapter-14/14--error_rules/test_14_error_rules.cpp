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

// Tests for the IEEE 1800-2023 Clause 14 (Clocking blocks) error scenarios
// catalogued in sv_error_catalog_Latest.xlsx (rows 439, 441, 444, 450, 453).
//
// Scope, fixture layout and test shapes follow the Clause 3 file in this same
// suite; see hlc/ErrorCatalog/chapter-3 for the rationale.
//
// Behaviour observed while writing this file (hlc.exe -d db over the fixture):
// row 439's and row 444's modules compile. Row 439's only diagnostic is a
// CP5851 "Failed to bind cb" warning at 15:15 -- HLC does not resolve the
// clockvar reference cb.b at all, which is a binding gap, not the direction
// check row 439 calls for. It is not asserted here: it would pass for the
// wrong reason and would keep passing after cb.b starts binding correctly, at
// which point the real rule would be silently unchecked.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter14ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "14--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 439: output clockvars are write-only (14.3) ------------------------

TEST_F(Chapter14ErrorRulesTest, Row439_ReadingAnOutputClockvarIsRejected) {
  // catalog row 439 | 14.3 | COMP
  // "It shall be illegal to read the value of any clockvar whose
  // clocking_direction is output." cb declares b as output on line 13; line 15
  // reads cb.b.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_READ, "b", 15, 15), nullptr)
      << "an output clockvar cannot be read (IEEE 1800-2023 14.3)";
}

// --- row 441: clocking signal initializer must be legal for the port
//              connection direction (14.5) ---------------------------------

TEST_F(Chapter14ErrorRulesTest, Row441_ClockingSignalInitializerMustMatchPortDirectionLegality) {
  // catalog row 441 | 14.5 | COMP
  // An expression assigned to a clocking signal in its declaration shall be
  // legal as a port connection of the corresponding direction; line 27's
  // "output w = 8'hFF;" binds a constant to an output clocking signal, which
  // is not a legal output-port-connection expression.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_ASSIGNMENT_LHS, "w", 27, 12), nullptr)
      << "an output clocking signal's initializer must be legal for an output port connection "
         "(IEEE 1800-2023 14.5)";
}

// --- row 444: cycle delays need a default clocking (14.11) ------------------

TEST_F(Chapter14ErrorRulesTest, Row444_CycleDelayWithoutDefaultClockingIsRejected) {
  // catalog row 444 | 14.11 | COMP
  // "If no default clocking has been specified for the current module,
  // interface, checker, or program, use of the ## operator shall cause the
  // compiler to issue an error." r444_m declares cb on line 38 but never makes
  // it the default, and uses ##5 on line 41.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 14.11 requires an error when a ## "
                  "cycle delay is used with no default clocking in scope";
  EXPECT_NE(findError(ErrorDefinition::COMP_MISSING_DEFAULT_CLOCKING, "r444_m", 41, 5), nullptr)
      << "a ## cycle delay needs a default clocking in scope (IEEE 1800-2023 14.11)";
}

// --- row 450: $global_clock with no effective global clocking in the
//              hierarchy (14.14) --------------------------------------------

TEST_F(Chapter14ErrorRulesTest, Row450_GlobalClockWithNoEffectiveGlobalClockingIsRejected) {
  // catalog row 450 | 14.14 | ELAB
  // $global_clock resolution shall result in an error if no effective
  // global clocking declaration is found in the enclosing instance scope or
  // any ancestor up to and including a top-level hierarchy block; line 50's
  // "r450_sub s();" instantiates a sub-hierarchy with no global clocking
  // anywhere in its ancestry.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 14.14 requires an error when "
                  "$global_clock has no effective global clocking declaration in its instance's "
                  "ancestry";
  EXPECT_NE(findError(ErrorDefinition::COMP_MISSING_DEFAULT_CLOCKING, "s", 50, 12), nullptr)
      << "$global_clock requires an effective global clocking declaration somewhere in the "
         "instance's ancestry (IEEE 1800-2023 14.14)";
}

// --- row 453: clockvar_expression LHS cannot be a concatenation (14.16) ----

TEST_F(Chapter14ErrorRulesTest, Row453_ClockvarExpressionCannotBeAConcatenation) {
  // catalog row 453 | 14.16 | PARSE
  // The clockvar_expression on the left-hand side of a synchronous drive
  // shall be a whole clockvar, a bit-select, or a slice; line 66's
  // "{cb.a, cb.b} <= 2'b10;" uses a concatenation, which is not allowed.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 14.16 forbids a concatenation as a "
                  "clockvar_expression on the left-hand side of a synchronous drive";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_ASSIGNMENT_LHS, "cb", 66, 12), nullptr)
      << "a synchronous drive's clockvar_expression cannot be a concatenation "
         "(IEEE 1800-2023 14.16)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
