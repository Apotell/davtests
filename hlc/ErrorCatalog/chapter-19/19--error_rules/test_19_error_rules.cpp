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
// catalogued in sv_error_catalog_Latest.xlsx / docs/error_catalog.xml (rows
// 670, 672, 680, 682, 684, 685, 688, 694, 696, 705, 706, 707, 711, 713, 719,
// 720, 721, 722, 723, 725, 726, 728, 729).
//
// Scope, fixture layout and test shapes follow the Clause 3 file in this same
// suite; see hlc/ErrorCatalog/chapter-3 for the rationale.
//
// Behaviour observed while writing this file (hlc.exe -d db over the fixture):
// all modules compile. Row 720's procedural option assignment raises two
// syntax errors on its own line, but that is NOT enforcement of the rule -- a
// control fixture assigning a legal procedural option (ci.option.at_least = 2,
// which 19.7 expressly permits after instantiation) is rejected identically.
// The syntax errors are a parser gap covering every procedural option
// assignment, legal or not, so asserting them would lock in the wrong
// behaviour. The parser recovers locally and every module after it still
// compiles. The same parser gap affects rows 680, 711, 721 and 725, each of
// which places a hierarchical "<name>.option...."/"<name>::type_option...."
// assignment directly as a covergroup_item -- HLC's grammar currently rejects
// that shape with a syntax error rather than reaching semantic analysis at
// all, so those rows are asserted (and skipped) against their own catalogued
// COMP/LINT ErrorDefinition code, not against the parser gap.
//
// Rows 670 and 729 were added later. Row 670's covergroup compiles with no
// diagnostic. Row 729's "with function sample (output int v)" raises a
// DB2038 "'vpiCoverageEvent' property value on object is invalid" error at
// its own covergroup declaration line -- but that identical error already
// fires for row 728's own "with function sample" construct above (which has
// no output formal at all), so it is a pre-existing binding gap in modelling
// an overridden sample() method generally, not enforcement of the
// output-direction rule; not asserted here.

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

// --- row 672: covergroup new() must supply every non-defaulted actual (19.3) ---

TEST_F(Chapter19ErrorRulesTest, Row672_CovergroupNewMustSupplyEveryNonDefaultedActual) {
  // catalog row 672 | 19.3 | COMP
  // When a covergroup specifies a list of formal arguments, its instances
  // shall provide to the new operator all the actual arguments that are not
  // defaulted. r672_cg takes (int a, int b), neither defaulted, but
  // "r672_cg ci = new(1);" on line 16 supplies only one actual.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 19.3 requires every non-defaulted "
                  "covergroup formal argument to be supplied to new()";
  EXPECT_NE(findError(ErrorDefinition::COMP_MISSING_ARGUMENT, "b", 16, 3), nullptr)
      << "a covergroup new() call must supply every non-defaulted formal argument (IEEE 1800-2023 19.3)";
}

// --- row 680: a coverpoint name has limited visibility (19.5) ---------------

TEST_F(Chapter19ErrorRulesTest, Row680_CoverpointNameNotUsableForOptionAssignment) {
  // catalog row 680 | 19.5 | COMP
  // A coverpoint name may be referenced only in the coverpoint list of a
  // cross, in a hierarchical name prefixed by a covergroup variable, or
  // after ::. "r680_e.option.weight = 2;" on line 29, inside the covergroup
  // body, uses the coverpoint name r680_e in none of those positions.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 19.5 restricts a coverpoint name to "
                  "a cross's coverpoint list, a covergroup-variable-prefixed hierarchical name, or "
                  "after ::. HLC's grammar also currently rejects this hierarchical-option-"
                  "assignment shape as a covergroup_item with a syntax error before reaching "
                  "semantic analysis";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r680_e", 29, 5), nullptr)
      << "a coverpoint name has limited visibility and cannot be used this way (IEEE 1800-2023 19.5)";
}

// --- row 682: covergroup_expression constants must be class members (19.5) -

TEST_F(Chapter19ErrorRulesTest, Row682_CovergroupExpressionConstantMustBeEnclosingClassMember) {
  // catalog row 682 | 19.5 | LINT
  // Global and instance constants referenced from a covergroup_expression
  // shall be members of the enclosing class. r682_cv's bin range on line 44
  // references "o.lim", an instance constant of r682_Other, not of the
  // enclosing class r682_C.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 19.5 requires a covergroup_expression "
                  "constant to be a member of the enclosing class";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "lim", 44, 35), nullptr)
      << "a constant used in a covergroup_expression must be a member of the enclosing class (IEEE 1800-2023 19.5)";
}

// --- row 684: covergroup_expression functions cannot have ref/output/inout args (19.5) ---

TEST_F(Chapter19ErrorRulesTest, Row684_CovergroupExpressionFunctionCannotHaveRefArgument) {
  // catalog row 684 | 19.5 | LINT
  // Functions participating in a covergroup_expression shall not contain
  // output, inout, or ref arguments (const ref is allowed). r684_f takes
  // "ref int r" and is called from a bins expression on line 57.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r684_f", 57, 33), nullptr)
      << "a function used in a covergroup_expression cannot have a ref/output/inout argument (IEEE 1800-2023 19.5)";
}

// --- row 685: covergroup_expression functions must be side-effect free (19.5) ---

TEST_F(Chapter19ErrorRulesTest, Row685_CovergroupExpressionFunctionMustHaveNoSideEffects) {
  // catalog row 685 | 19.5 | LINT
  // Functions participating in a covergroup_expression shall be automatic
  // (or preserve no state information) and have no side effects. r685_f
  // increments a static local "cnt" and is called from a bins expression on
  // line 69.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 19.5 requires a covergroup_expression "
                  "function to have no side effects";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_SIDE_EFFECT, "r685_f", 69, 33), nullptr)
      << "a function used in a covergroup_expression must have no side effects (IEEE 1800-2023 19.5)";
}

// --- row 688: real coverpoints need explicit bins (19.5) --------------------

TEST_F(Chapter19ErrorRulesTest, Row688_RealCoverpointWithoutExplicitBinsIsRejected) {
  // catalog row 688 | 19.5 | COMP
  // Bins are not automatically created for coverpoints of real expressions, so
  // a real coverpoint shall specify at least one explicit bins construct.
  // cpr on line 81 covers the real r with no bins at all.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_BINS, "cpr", 81, 5), nullptr)
      << "a real coverpoint needs an explicit bins construct (IEEE 1800-2023 19.5)";
}

// --- row 694: only the enclosing coverpoint's own name may replace a covergroup_range_list (19.5.1.1) ---

TEST_F(Chapter19ErrorRulesTest, Row694_OnlyEnclosingCoverpointNameAllowedAsRangeList) {
  // catalog row 694 | 19.5.1.1 | COMP
  // When a coverpoint name is used in place of the covergroup_range_list of
  // a bin, only the name of the coverpoint containing the bin being defined
  // shall be allowed. r694_b's own bin on line 94 uses "r694_a", naming the
  // other coverpoint instead of the enclosing one (r694_b).
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 19.5.1.1 only permits the enclosing "
                  "coverpoint's own name to replace a covergroup_range_list";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r694_a", 94, 39), nullptr)
      << "only the enclosing coverpoint's own name may replace a covergroup_range_list (IEEE 1800-2023 19.5.1.1)";
}

// --- row 696: identifiers declared inside a covergroup are not visible in a set_covergroup_expression (19.5.1.2) ---

TEST_F(Chapter19ErrorRulesTest, Row696_CoverpointIdentifierNotVisibleInSetExpression) {
  // catalog row 696 | 19.5.1.2 | COMP
  // Identifiers declared within the covergroup (such as coverpoint
  // identifiers and bin identifiers) are not visible in a
  // set_covergroup_expression. r696_b's bin set expression on line 106 uses
  // r696_a, the other coverpoint's own label.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 19.5.1.2 hides identifiers declared "
                  "within the covergroup from a set_covergroup_expression";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r696_a", 106, 39), nullptr)
      << "a coverpoint identifier is not visible in a set_covergroup_expression (IEEE 1800-2023 19.5.1.2)";
}

// --- row 705: signed negative bin against an unsigned coverpoint (19.5.7) ---

TEST_F(Chapter19ErrorRulesTest, Row705_NegativeBinValueAgainstUnsignedCoverpointIsWarned) {
  // catalog row 705 | 19.5.7 | COMP
  // "An implementation shall issue a warning if the effective type of the
  // coverpoint expression is unsigned and a bins expression b is signed with a
  // negative value"; the offending element does not participate in the bins
  // values. p1 is a 3-bit unsigned covering 0..7 and bin b2 on line 119 lists
  // -1. Note this is one of the few catalog rows the standard states as a
  // warning rather than an error, and the value is dropped, not rejected.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 19.5.7 requires a warning when a "
                  "signed negative bins value is given for an unsigned coverpoint expression";
  EXPECT_NE(findError(ErrorDefinition::COMP_UNMATCHABLE_BIN_VALUE, "b2", 119, 27), nullptr)
      << "a negative bin value cannot match an unsigned coverpoint (IEEE 1800-2023 19.5.7)";
}

// --- row 706: a bin value that does not fit the coverpoint's effective type (19.5.7) ---

TEST_F(Chapter19ErrorRulesTest, Row706_BinValueNotEqualUnderCoverpointTypeWarns) {
  // catalog row 706 | 19.5.7 | COMP
  // An implementation shall issue a warning if assigning a bins expression b
  // to a variable of the effective type of the coverpoint expression would
  // yield a value that is not equal to b under normal == comparison rules
  // (i.e., the bin value does not fit the coverpoint type). r706_g1's
  // coverpoint p1 is bit [2:0] (0..7); bin b1 on line 131 includes [6:10],
  // which does not fit.
  EXPECT_NE(findError(ErrorDefinition::COMP_UNMATCHABLE_BIN_VALUE, "b1", 131, 45), nullptr)
      << "a bin value that does not fit the coverpoint's effective type must be warned about (IEEE 1800-2023 19.5.7)";
}

// --- row 707: a non-wildcard bin value with x/z bits (19.5.7) ---------------

TEST_F(Chapter19ErrorRulesTest, Row707_NonWildcardBinValueWithXZBitsWarns) {
  // catalog row 707 | 19.5.7 | COMP
  // An implementation shall issue a warning if a bins expression b yields a
  // value with any x or z bits (this rule does not apply to wildcard bins).
  // r707_g1's bin bx on line 142 is a plain (non-wildcard) bin set to
  // 4'b10x1.
  EXPECT_NE(findError(ErrorDefinition::COMP_UNMATCHABLE_BIN_VALUE, "bx", 142, 31), nullptr)
      << "a non-wildcard bin value with x/z bits must be warned about (IEEE 1800-2023 19.5.7)";
}

// --- row 711: a cross name has limited visibility (19.6) --------------------

TEST_F(Chapter19ErrorRulesTest, Row711_CrossNameNotUsableForOptionAssignment) {
  // catalog row 711 | 19.6 | COMP
  // A cross name may be referenced only in a hierarchical name prefixed by a
  // covergroup variable, or after ::. "r711_crs.option.weight = 2;" on line
  // 157, inside the covergroup body, uses the cross name r711_crs in
  // neither position.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 19.6 restricts a cross name to a "
                  "covergroup-variable-prefixed hierarchical name or after ::. HLC's grammar also "
                  "currently rejects this hierarchical-option-assignment shape as a "
                  "covergroup_item with a syntax error before reaching semantic analysis";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r711_crs", 157, 5), nullptr)
      << "a cross name has limited visibility and cannot be used this way (IEEE 1800-2023 19.6)";
}

// --- row 713: only the enclosing cross's own identifier may be used as a select_expression (19.6.1.2) ---

TEST_F(Chapter19ErrorRulesTest, Row713_OnlyEnclosingCrossIdentifierAllowedAsSelectExpression) {
  // catalog row 713 | 19.6.1.2 | COMP
  // When a cross_identifier is used as a select_expression, only the
  // cross_identifier of the enclosing cross may be used. r713_Y's own bin
  // select expression on line 172 references r713_X, the other cross's
  // identifier.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 19.6.1.2 only permits the enclosing "
                  "cross's own identifier to be used as a select_expression";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r713_X", 172, 47), nullptr)
      << "only the enclosing cross's own identifier may be used as a select_expression (IEEE 1800-2023 19.6.1.2)";
}

// --- row 719: option.weight must be non-negative (19.7) ---------------------

TEST_F(Chapter19ErrorRulesTest, Row719_OptionWeightMustBeNonNegative) {
  // catalog row 719 | 19.7 | COMP
  // The weight specified for option.weight (and type_option.weight) shall be
  // a non-negative integral value. r719_cg on line 182 sets
  // "option.weight = -1;".
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 19.7 requires option.weight to be a "
                  "non-negative integral value";
  EXPECT_NE(findError(ErrorDefinition::COMP_VALUE_OUT_OF_RANGE, "r719_cg", 182, 14), nullptr)
      << "option.weight must be a non-negative integral value (IEEE 1800-2023 19.7)";
}

// --- row 720: per_instance is definition-only (19.7) ------------------------

TEST_F(Chapter19ErrorRulesTest, Row720_ProceduralPerInstanceAssignmentIsRejected) {
  // catalog row 720 | 19.7 | COMP
  // per_instance and get_inst_coverage can only be set in the covergroup
  // definition; unlike the other options they cannot be assigned procedurally
  // after instantiation. Line 199 assigns per_instance on the instance ci.
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
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_OPTION, "per_instance", 199, 11), nullptr)
      << "per_instance may only be set in the covergroup definition (IEEE 1800-2023 19.7)";
}

// --- row 721: auto_bin_max/detect_overlap/cross_retain_auto_bins are definition-only (19.7) ---

TEST_F(Chapter19ErrorRulesTest, Row721_AutoBinMaxOnlySettableInDefinition) {
  // catalog row 721 | 19.7 | COMP
  // The auto_bin_max, detect_overlap, and cross_retain_auto_bins options can
  // only be set in the covergroup or coverpoint definition, not assigned
  // procedurally. r721_m assigns "ci.r721_a.option.auto_bin_max = 128;" on
  // line 213, in an initial block after instantiation.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 19.7 allows auto_bin_max to be set "
                  "only in the covergroup/coverpoint definition. HLC's grammar also currently "
                  "rejects this hierarchical-option-assignment shape as a syntax error before "
                  "reaching semantic analysis";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_OPTION, "auto_bin_max", 213, 28), nullptr)
      << "auto_bin_max can only be set in the covergroup/coverpoint definition, not procedurally (IEEE 1800-2023 19.7)";
}

// --- row 722: each instance option is restricted to its own Table 19-2 syntactic level (19.7) ---

TEST_F(Chapter19ErrorRulesTest, Row722_NameOptionNotAllowedAtCoverpointLevel) {
  // catalog row 722 | 19.7 | COMP
  // Each instance coverage option may only be specified at the syntactic
  // levels permitted by Table 19-2; "name" is allowed only at the
  // covergroup level. r722_ca sets "option.name = "cp";" on line 224, at the
  // coverpoint level.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_OPTION, "name", 224, 29), nullptr)
      << "the name option is not permitted at the coverpoint syntactic level (IEEE 1800-2023 19.7, Table 19-2)";
}

// --- row 723: detect_overlap (19.7) -----------------------------------------

TEST_F(Chapter19ErrorRulesTest, Row723_OverlappingBinsAreWarnedWhenDetectOverlapIsSet) {
  // catalog row 723 | 19.7 | COMP
  // "When true, a warning is issued if there is an overlap between the range
  // list or transition list of two bins of a coverpoint." The option is turned
  // on for this coverpoint on line 238; b1 covers 0..10 and b2 on line 240
  // covers 5..20, so 5..10 is in both. The warning is conditional on the
  // option -- with detect_overlap left at its default the same two bins must
  // stay silent.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 19.7 requires a warning for "
                  "overlapping bin range lists when option.detect_overlap is true";
  EXPECT_NE(findError(ErrorDefinition::COMP_OVERLAPPING_BINS, "b2", 240, 7), nullptr)
      << "overlapping bins must be warned when detect_overlap is set (IEEE 1800-2023 19.7)";
}

// --- row 725: strobe/real_interval type options are definition-only (19.7.1) ---

TEST_F(Chapter19ErrorRulesTest, Row725_StrobeOnlySettableInCovergroupDefinition) {
  // catalog row 725 | 19.7.1 | COMP
  // The strobe and real_interval type options can only be set in the
  // covergroup definition, not assigned procedurally. r725_m assigns
  // "r725_cg::type_option.strobe = 1;" on line 254, in an initial block.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 19.7.1 allows strobe to be set only "
                  "in the covergroup definition. HLC's grammar also currently rejects this "
                  "type_option-assignment shape as a syntax error before reaching semantic "
                  "analysis";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_OPTION, "strobe", 254, 32), nullptr)
      << "strobe can only be set in the covergroup definition, not procedurally (IEEE 1800-2023 19.7.1)";
}

// --- row 726: each type option is restricted to its own Table 19-4 syntactic level (19.7.1) ---

TEST_F(Chapter19ErrorRulesTest, Row726_RealIntervalNotAllowedInCross) {
  // catalog row 726 | 19.7.1 | COMP
  // Each covergroup type option may only be specified at the syntactic
  // levels permitted by Table 19-4; real_interval is not allowed in a cross.
  // r726_x sets "type_option.real_interval = 0.5;" on line 266, at the cross
  // level.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_OPTION, "real_interval", 266, 38), nullptr)
      << "real_interval is not a permitted type option at the cross syntactic level (IEEE 1800-2023 19.7.1, Table 19-4)";
}

// --- row 728: an overridden sample() formal is usable only in a coverpoint or guard expression (19.8.1) ---

TEST_F(Chapter19ErrorRulesTest, Row728_SampleFormalUsableOnlyInCoverpointOrGuard) {
  // catalog row 728 | 19.8.1 | COMP
  // It shall be an error to use a formal argument of an overridden sample
  // method in any context other than a coverpoint or a conditional guard
  // expression. r728_C1's sample formal "b" is used directly in
  // "option.per_instance = b;" on line 276, neither position.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 19.8.1 restricts an overridden "
                  "sample() formal argument to a coverpoint or a conditional guard expression";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "b", 276, 27), nullptr)
      << "an overridden sample() formal is usable only in a coverpoint or guard expression (IEEE 1800-2023 19.8.1)";
}

// --- row 670: output/inout formals are illegal in a covergroup argument
//              list (19.3) --------------------------------------------------

TEST_F(Chapter19ErrorRulesTest, Row670_OutputFormalIllegalInCovergroupArgumentList) {
  // catalog row 670 | 19.3 | COMP
  // r670_cg's "covergroup r670_cg (output int a) @(posedge r670_clk);" on
  // line 285 declares formal 'a' with direction output.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_QUALIFIER, "a", 285, 34), nullptr)
      << "an output formal argument is illegal in a covergroup argument list (IEEE 1800-2023 19.3)";
}

// --- row 729: an overridden sample() formal cannot be output (19.8.1) ------

TEST_F(Chapter19ErrorRulesTest, Row729_OverriddenSampleFormalCannotBeOutputDirection) {
  // catalog row 729 | 19.8.1 | COMP
  GTEST_SKIP() << "blocked on an unrelated pre-existing binding gap ('with function sample' -> "
                  "vpiCoverageEvent, shared with row 728) that must be fixed first; see "
                  ".claude/instructions/error_catalog_known_gaps.md item 4";
  // Formal arguments of an overridden sample method shall not designate an
  // output direction. r729_C1's "with function sample (output int v)" on
  // line 295 declares formal 'v' with direction output.
  //
  // HLC does raise a DB2038 "'vpiCoverageEvent' property value on object is
  // invalid" error at 295:3, but the identical error also fires for row
  // 728's own "with function sample" construct (whose formal is a plain
  // bit, no direction at all) -- a pre-existing binding gap in modelling any
  // overridden sample() method, not enforcement of this direction rule; not
  // asserted here.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_QUALIFIER, "v", 295, 55), nullptr)
      << "an overridden sample() formal argument cannot be output (IEEE 1800-2023 19.8.1)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
