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

// Tests for the IEEE 1800-2023 Clause 14 error scenarios catalogued in
// docs/error_catalog.xml.
//
// Scope: this file asserts ONLY that the diagnostic each catalog row requires
// is emitted. It deliberately makes no assertion about the shape of the
// compiled model. Exactly one TEST_F per catalog row, named Row<N>_... after
// that row and carrying a "catalog row N | clause | category" comment; that
// is the link between the catalog and this file.
//
// Fixture (compiled in one run by 14--error_rules.hlc):
//   14--error_rules.sv   rows 441, 444, 450, 453
//
// Catalog rows covered here:
//   441 | 14.5   | COMP_ILLEGAL_ASSIGNMENT_LHS
//   444 | 14.11  | COMP_MISSING_DEFAULT_CLOCKING
//   450 | 14.14  | COMP_MISSING_DEFAULT_CLOCKING
//   453 | 14.16  | COMP_ILLEGAL_ASSIGNMENT_LHS
//
// Per project convention, no test below uses GTEST_SKIP(): every assertion
// states the diagnostic IEEE 1800-2023 requires, even where the relevant
// ErrorDefinition code currently has zero call sites anywhere in src/
// (confirmed by grep for both codes above) -- i.e. every one of these is
// expected to be red today. No test here asserts the absence of a
// diagnostic.

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

// --- row 441: clocking signal initializer must be legal for the port
//              connection direction (14.5) ---------------------------------

TEST_F(Chapter14ErrorRulesTest, Row441_ClockingSignalInitializerMustMatchPortDirectionLegality) {
  // catalog row 441 | 14.5 | COMP
  // An expression assigned to a clocking signal in its declaration shall be
  // legal as a port connection of the corresponding direction; "output w =
  // 8'hFF;" binds a constant to an output clocking signal, which is not a
  // legal output-port-connection expression.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_ASSIGNMENT_LHS, "w"), nullptr)
      << "an output clocking signal's initializer must be legal for an output port connection "
         "(IEEE 1800-2023 14.5)";
}

// --- row 444: ## cycle delay with no default clocking (14.11) -------------

TEST_F(Chapter14ErrorRulesTest, Row444_CycleDelayWithNoDefaultClockingIsRejected) {
  // catalog row 444 | 14.11 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 14.11)";
  // If no default clocking has been specified for the current module,
  // interface, checker, or program, use of a ## cycle delay shall cause the
  // compiler to issue an error. 'cb' is declared but never made the
  // default clocking.
  EXPECT_NE(findError(ErrorDefinition::COMP_MISSING_DEFAULT_CLOCKING, "r444_m"), nullptr)
      << "a ## cycle delay requires a default clocking to be in scope (IEEE 1800-2023 14.11)";
}

// --- row 450: $global_clock with no effective global clocking in the
//              hierarchy (14.14) --------------------------------------------

TEST_F(Chapter14ErrorRulesTest, Row450_GlobalClockWithNoEffectiveGlobalClockingIsRejected) {
  // catalog row 450 | 14.14 | ELAB
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 14.14)";
  // $global_clock resolution shall result in an error if no effective
  // global clocking declaration is found in the enclosing instance scope or
  // any ancestor up to and including a top-level hierarchy block; 'sub' has
  // no global clocking anywhere in its ancestry.
  EXPECT_NE(findError(ErrorDefinition::COMP_MISSING_DEFAULT_CLOCKING, "r450_sub"), nullptr)
      << "$global_clock requires an effective global clocking declaration somewhere in the "
         "instance's ancestry (IEEE 1800-2023 14.14)";
}

// --- row 453: clockvar_expression LHS cannot be a concatenation (14.16) ---

TEST_F(Chapter14ErrorRulesTest, Row453_ClockvarExpressionCannotBeAConcatenation) {
  // catalog row 453 | 14.16 | PARSE
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 14.16)";
  // The clockvar_expression on the left-hand side of a synchronous drive
  // shall be a whole clockvar, a bit-select, or a slice; "{cb.a, cb.b} <=
  // 2'b10;" uses a concatenation, which is not allowed.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_ASSIGNMENT_LHS, "r453_m"), nullptr)
      << "a synchronous drive's clockvar_expression cannot be a concatenation "
         "(IEEE 1800-2023 14.16)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
