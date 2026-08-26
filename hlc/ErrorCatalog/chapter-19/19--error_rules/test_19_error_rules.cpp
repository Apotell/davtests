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

// Tests for the IEEE 1800-2023 Clause 19 (Functional coverage) error scenarios
// catalogued in sv_error_catalog_Latest.xlsx (rows 688, 705, 720, 723).
//
// Scope, fixture layout and test shapes follow the Clause 3 file in this same
// suite; see hlc/ErrorCatalog/chapter-3 for the rationale.
//
// Behaviour observed while writing this file (hlc.exe -d db over the fixture):
// all four modules compile. Row 720's procedural option assignment raises two
// syntax errors on line 43, but that is NOT enforcement of the rule -- a
// control fixture assigning a legal procedural option (ci.option.at_least = 2,
// which 19.7 expressly permits after instantiation) is rejected identically.
// The syntax errors are a parser gap covering every procedural option
// assignment, legal or not, so asserting them would lock in the wrong
// behaviour. The parser recovers locally and r723_m is still compiled. The
// CP5851 / Linter pair at 42:16 is the unrelated "new" binding gap.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter19ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "19--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 688: real coverpoints need explicit bins (19.5) --------------------

TEST_F(Chapter19ErrorRulesTest, Row688_RealCoverpointWithoutExplicitBinsIsRejected) {
  // catalog row 688 | 19.5 | COMP
  // Bins are not automatically created for coverpoints of real expressions, so
  // a real coverpoint shall specify at least one explicit bins construct.
  // cpr on line 15 covers the real r with no bins at all.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 19.5 requires a coverpoint of a real "
                  "expression to specify at least one explicit bins construct, because no automatic "
                  "bins are created for it";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_BINS, "cpr", 15, 5), nullptr)
      << "a real coverpoint needs an explicit bins construct (IEEE 1800-2023 19.5)";
}

// --- row 705: signed negative bin against an unsigned coverpoint (19.5.7) ---

TEST_F(Chapter19ErrorRulesTest, Row705_NegativeBinValueAgainstUnsignedCoverpointIsWarned) {
  // catalog row 705 | 19.5.7 | COMP
  // "An implementation shall issue a warning if the effective type of the
  // coverpoint expression is unsigned and a bins expression b is signed with a
  // negative value"; the offending element does not participate in the bins
  // values. p1 is a 3-bit unsigned covering 0..7 and bin b2 on line 28 lists
  // -1. Note this is one of the few catalog rows the standard states as a
  // warning rather than an error, and the value is dropped, not rejected.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 19.5.7 requires a warning when a "
                  "signed negative bins value is given for an unsigned coverpoint expression";
  EXPECT_NE(findError(ErrorDefinition::COMP_UNMATCHABLE_BIN_VALUE, "b2", 28, 27), nullptr)
      << "a negative bin value cannot match an unsigned coverpoint (IEEE 1800-2023 19.5.7)";
}

// --- row 720: per_instance is definition-only (19.7) ------------------------

TEST_F(Chapter19ErrorRulesTest, Row720_ProceduralPerInstanceAssignmentIsRejected) {
  // catalog row 720 | 19.7 | COMP
  // per_instance and get_inst_coverage can only be set in the covergroup
  // definition; unlike the other options they cannot be assigned procedurally
  // after instantiation. Line 43 assigns per_instance on the instance ci.
  //
  // HLC currently reports two syntax errors on that line, but they are not the
  // required diagnostic: the same errors appear for ci.option.at_least = 2,
  // which 19.7 expressly allows procedurally. The parser cannot handle any
  // procedural option assignment, so asserting PA_SYNTAX_ERROR here would lock
  // in a gap that rejects legal code. The rule needs a real check once the
  // grammar accepts the construct.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 19.7 allows per_instance and "
                  "get_inst_coverage to be set only in the covergroup definition. HLC cannot parse "
                  "any procedural option assignment today -- including the legal ones -- so the "
                  "syntax errors on this line are a parser gap, not this rule";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_OPTION, "per_instance", 43, 11), nullptr)
      << "per_instance may only be set in the covergroup definition (IEEE 1800-2023 19.7)";
}

// --- row 723: detect_overlap (19.7) -----------------------------------------

TEST_F(Chapter19ErrorRulesTest, Row723_OverlappingBinsAreWarnedWhenDetectOverlapIsSet) {
  // catalog row 723 | 19.7 | COMP
  // "When true, a warning is issued if there is an overlap between the range
  // list or transition list of two bins of a coverpoint." The option is turned
  // on for this coverpoint on line 55; b1 covers 0..10 and b2 on line 57
  // covers 5..20, so 5..10 is in both. The warning is conditional on the
  // option -- with detect_overlap left at its default the same two bins must
  // stay silent.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 19.7 requires a warning for "
                  "overlapping bin range lists when option.detect_overlap is true";
  EXPECT_NE(findError(ErrorDefinition::COMP_OVERLAPPING_BINS, "b2", 57, 7), nullptr)
      << "overlapping bins must be warned when detect_overlap is set (IEEE 1800-2023 19.7)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
