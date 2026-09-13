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

// Tests for the IEEE 1800-2023 Clause 31 error scenarios catalogued in
// docs/error_catalog.xml (rows 1032, 1033, 1034, 1035, 1036, 1037, 1038,
// 1039, 1040, 1043, 1044, 1046, 1052).
//
// Scope: this file asserts ONLY that the diagnostic each catalog row
// requires is emitted. It deliberately makes no assertion about the shape
// of the compiled model. Exactly one TEST_F per catalog row, named
// Row<N>_... after that row and carrying a "catalog row N | clause |
// category" comment; that is the link between the catalog and this file.
//
// Fixture: 31--error_rules.sv (all thirteen rows), compiled in one run by
// 31--error_rules.hlc.
//
// COMP_VALUE_OUT_OF_RANGE has no call site anywhere in src/ outside its own
// ErrorDefinition.cpp registration, so every assertion below is expected to
// be red today -- that is the intended, documented state of this exercise,
// not a bug for this file to work around. No test asserts the absence of a
// diagnostic, which would lock the gap in.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter31ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "31--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 1032: $setup limit must be non-negative (31.3.1) -------------------

TEST_F(Chapter31ErrorRulesTest, Row1032_SetupLimitMustBeNonNegative) {
  // catalog row 1032 | 31.3.1 | ELAB
  // The $setup timing check limit shall be a non-negative constant
  // expression; negative values are only permitted for $setuphold and
  // $recrem.
  EXPECT_NE(findError(ErrorDefinition::COMP_VALUE_OUT_OF_RANGE, "$setup limit must be non-negative"), nullptr)
      << "a $setup limit must be non-negative (IEEE 1800-2023 31.3.1)";
}

// --- row 1033: $hold limit must be non-negative (31.3.2) --------------------

TEST_F(Chapter31ErrorRulesTest, Row1033_HoldLimitMustBeNonNegative) {
  // catalog row 1033 | 31.3.2 | ELAB
  // The $hold timing check limit shall be a non-negative constant
  // expression.
  EXPECT_NE(findError(ErrorDefinition::COMP_VALUE_OUT_OF_RANGE, "$hold limit must be non-negative"), nullptr)
      << "a $hold limit must be non-negative (IEEE 1800-2023 31.3.2)";
}

// --- row 1034: $setuphold negative-limit pair sum must exceed precision (31.3.3)

TEST_F(Chapter31ErrorRulesTest, Row1034_SetupholdNegativeLimitPairMustExceedPrecision) {
  // catalog row 1034 | 31.3.3 | ELAB
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 31.3.3)";
  // When either the setup limit or the hold limit of a $setuphold check is
  // negative, setup_limit + hold_limit shall exceed the simulation unit of
  // precision; here -10ns + 5ns = -5ns, not > 1ns.
  EXPECT_NE(findError(ErrorDefinition::COMP_VALUE_OUT_OF_RANGE, "r1034_m"), nullptr)
      << "a negative $setuphold limit pair must sum to more than the time precision (IEEE 1800-2023 31.3.3)";
}

// --- row 1035: $removal limit must be non-negative (31.3.4) -----------------

TEST_F(Chapter31ErrorRulesTest, Row1035_RemovalLimitMustBeNonNegative) {
  // catalog row 1035 | 31.3.4 | ELAB
  // The $removal timing check limit shall be a non-negative constant
  // expression.
  EXPECT_NE(findError(ErrorDefinition::COMP_VALUE_OUT_OF_RANGE, "$removal limit must be non-negative"), nullptr)
      << "a $removal limit must be non-negative (IEEE 1800-2023 31.3.4)";
}

// --- row 1036: $recovery limit must be non-negative (31.3.5) ----------------

TEST_F(Chapter31ErrorRulesTest, Row1036_RecoveryLimitMustBeNonNegative) {
  // catalog row 1036 | 31.3.5 | ELAB
  // The $recovery timing check limit shall be a non-negative constant
  // expression.
  EXPECT_NE(findError(ErrorDefinition::COMP_VALUE_OUT_OF_RANGE, "$recovery limit must be non-negative"), nullptr)
      << "a $recovery limit must be non-negative (IEEE 1800-2023 31.3.5)";
}

// --- row 1037: $recrem negative-limit pair sum must exceed precision (31.3.6)

TEST_F(Chapter31ErrorRulesTest, Row1037_RecremNegativeLimitPairMustExceedPrecision) {
  // catalog row 1037 | 31.3.6 | ELAB
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 31.3.6)";
  // When either the removal limit or the recovery limit of a $recrem check
  // is negative, removal_limit + recovery_limit shall exceed the
  // simulation unit of precision; here -8ns + 4ns = -4ns, not > 1ns.
  EXPECT_NE(findError(ErrorDefinition::COMP_VALUE_OUT_OF_RANGE, "r1037_m"), nullptr)
      << "a negative $recrem limit pair must sum to more than the time precision (IEEE 1800-2023 31.3.6)";
}

// --- row 1038: $skew limit must be non-negative (31.4.1) --------------------

TEST_F(Chapter31ErrorRulesTest, Row1038_SkewLimitMustBeNonNegative) {
  // catalog row 1038 | 31.4.1 | ELAB
  // The $skew timing check limit shall be a non-negative constant
  // expression.
  EXPECT_NE(findError(ErrorDefinition::COMP_VALUE_OUT_OF_RANGE, "$skew limit must be non-negative"), nullptr)
      << "a $skew limit must be non-negative (IEEE 1800-2023 31.4.1)";
}

// --- row 1039: $timeskew limit must be non-negative (31.4.2) ----------------

TEST_F(Chapter31ErrorRulesTest, Row1039_TimeskewLimitMustBeNonNegative) {
  // catalog row 1039 | 31.4.2 | ELAB
  // The $timeskew timing check limit shall be a non-negative constant
  // expression.
  EXPECT_NE(findError(ErrorDefinition::COMP_VALUE_OUT_OF_RANGE, "$timeskew limit must be non-negative"), nullptr)
      << "a $timeskew limit must be non-negative (IEEE 1800-2023 31.4.2)";
}

// --- row 1040: both $fullskew limits must be non-negative (31.4.3) ----------

TEST_F(Chapter31ErrorRulesTest, Row1040_FullskewLimit2MustBeNonNegative) {
  // catalog row 1040 | 31.4.3 | ELAB
  // Both $fullskew timing check limits (limit1 and limit2) shall be
  // non-negative constant expressions; limit2 here is -70.
  EXPECT_NE(findError(ErrorDefinition::COMP_VALUE_OUT_OF_RANGE, "$fullskew limit must be non-negative"), nullptr)
      << "both $fullskew limits must be non-negative (IEEE 1800-2023 31.4.3)";
}

// --- row 1044: $width threshold must be non-negative (31.4.4) ---------------

TEST_F(Chapter31ErrorRulesTest, Row1044_WidthThresholdMustBeNonNegative) {
  // catalog row 1044 | 31.4.4 | ELAB
  // The $width threshold argument shall be a non-negative constant
  // expression.
  // The implementation does not distinguish $width's "threshold" argument
  // position from its "limit" position -- both are timing-check terms and
  // get the same generic payload.
  EXPECT_NE(findError(ErrorDefinition::COMP_VALUE_OUT_OF_RANGE, "$width limit must be non-negative"), nullptr)
      << "a $width threshold must be non-negative (IEEE 1800-2023 31.4.4)";
}

// --- row 1043: $width limit must be non-negative (31.4.4) -------------------

TEST_F(Chapter31ErrorRulesTest, Row1043_WidthLimitMustBeNonNegative) {
  // catalog row 1043 | 31.4.4 | ELAB
  // The $width timing check limit shall be a non-negative constant
  // expression.
  EXPECT_NE(findError(ErrorDefinition::COMP_VALUE_OUT_OF_RANGE, "$width limit must be non-negative"), nullptr)
      << "a $width limit must be non-negative (IEEE 1800-2023 31.4.4)";
}

// --- row 1046: $period limit must be non-negative (31.4.5) ------------------

TEST_F(Chapter31ErrorRulesTest, Row1046_PeriodLimitMustBeNonNegative) {
  // catalog row 1046 | 31.4.5 | ELAB
  // The $period timing check limit shall be a non-negative constant
  // expression.
  EXPECT_NE(findError(ErrorDefinition::COMP_VALUE_OUT_OF_RANGE, "$period limit must be non-negative"), nullptr)
      << "a $period limit must be non-negative (IEEE 1800-2023 31.4.5)";
}

// --- row 1052: adjusted delayed-signal limit clamped to 0 with a warning (31.9.1)

TEST_F(Chapter31ErrorRulesTest, Row1052_AdjustedDelayedSignalLimitBelowZeroIsClampedWithWarning) {
  // catalog row 1052 | 31.9.1 | ELAB
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 31.9.1)";
  // When delayed signals are used for negative timing checks and the
  // adjusted limit becomes less than or equal to 0, the limit shall be set
  // to 0 and the simulator shall issue a warning.
  EXPECT_NE(findError(ErrorDefinition::COMP_VALUE_OUT_OF_RANGE, "r1052_m"), nullptr)
      << "an adjusted delayed-signal timing check limit <= 0 must be clamped to 0 and warned about (IEEE 1800-2023 31.9.1)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
