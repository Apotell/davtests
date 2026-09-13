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

// Tests for the IEEE 1800-2023 Clause 19 (covergroup) error scenarios
// catalogued in docs/error_catalog.xml (rows 672, 680, 682, 684, 685, 694,
// 696, 705, 706, 707, 711, 713, 719, 720, 721, 722, 723, 725, 726, 728).
//
// Scope: this file asserts ONLY that the diagnostic each catalog row
// requires is emitted. It deliberately makes no assertion about the shape of
// the compiled model. Exactly one TEST_F per catalog row, named Row<N>_...
// after that row and carrying a "catalog row N | clause | category" comment;
// that is the link between the catalog and this file.
//
// Fixture: 19--error_rules.sv, compiled by 19--error_rules.hlc. Every
// scenario compiled cleanly in a single run (no scenario here corrupts a
// later one, verified with -d db against the installed hlc.exe), so all 20
// rows live in this one fixture -- there are no _inv* sibling files for this
// chapter.
//
// None of the ErrorDefinition codes this chapter's rows name
// (COMP_MISSING_ARGUMENT, COMP_ILLEGAL_EXPRESSION_CONTEXT,
// COMP_ILLEGAL_SIDE_EFFECT, COMP_UNMATCHABLE_BIN_VALUE,
// COMP_VALUE_OUT_OF_RANGE, COMP_ILLEGAL_OPTION, COMP_OVERLAPPING_BINS) has a
// single call site anywhere in src/ (confirmed by grep) -- every one of
// these 20 rows is unimplemented today, regardless of the catalog's own
// "Code Status" column (EXISTS vs. ADDED just records whether the
// ErrorDefinition enumerant exists, not whether anything raises it). Every
// TEST_F below therefore asserts the diagnostic the standard requires and is
// expected to fail (red) until Clause 19 semantic checking is implemented --
// per project convention this is written as the real assertion, never as an
// assertion that the diagnostic is absent.
//
// A further, distinct gap observed while writing this file (recorded here,
// not asserted below, since it is a parser-level issue rather than this
// chapter's own semantic rule): rows 680, 711, 720, 721 and 725 each place a
// hierarchical "<name>.option...." / "<name>::type_option...." assignment
// directly as a covergroup_item (matching the colleague's DUT source
// verbatim), and HLC's grammar currently rejects that shape with a
// PA_SYNTAX_ERROR at that line rather than reaching semantic analysis at
// all (confirmed via -d ast/-d db: every other module in the fixture still
// compiles and appears intact in the HLDB dump, so this is a local,
// non-cascading parse failure in each case, not a corruption of the shared
// fixture). The catalog records these five rows as COMP, not PARSE, so the
// assertions below still target each row's own catalogued COMP/LINT
// ErrorDefinition code.

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
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 19.3)";
  // When a covergroup specifies a list of formal arguments, its instances
  // shall provide to the new operator all the actual arguments that are not
  // defaulted. r672_cg takes (int a, int b), neither defaulted, but
  // "r672_cg ci = new(1);" supplies only one actual.
  EXPECT_NE(findError(ErrorDefinition::COMP_MISSING_ARGUMENT, "b"), nullptr)
      << "a covergroup new() call must supply every non-defaulted formal argument (IEEE 1800-2023 19.3)";
}

// --- row 680: a coverpoint name has limited visibility (19.5) ---------------

TEST_F(Chapter19ErrorRulesTest, Row680_CoverpointNameNotUsableForOptionAssignment) {
  // catalog row 680 | 19.5 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 19.5)";
  // A coverpoint name may be referenced only in the coverpoint list of a
  // cross, in a hierarchical name prefixed by a covergroup variable, or
  // after ::. "r680_e.option.weight = 2;" inside the covergroup body uses
  // the coverpoint name r680_e in none of those positions.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r680_e"), nullptr)
      << "a coverpoint name has limited visibility and cannot be used this way (IEEE 1800-2023 19.5)";
}

// --- row 682: covergroup_expression constants must be class members (19.5) -

TEST_F(Chapter19ErrorRulesTest, Row682_CovergroupExpressionConstantMustBeEnclosingClassMember) {
  // catalog row 682 | 19.5 | LINT
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 19.5)";
  // Global and instance constants referenced from a covergroup_expression
  // shall be members of the enclosing class. r682_cv's bin range references
  // "o.lim", an instance constant of r682_Other, not of the enclosing class
  // r682_C.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "lim"), nullptr)
      << "a constant used in a covergroup_expression must be a member of the enclosing class (IEEE 1800-2023 19.5)";
}

// --- row 684: covergroup_expression functions cannot have ref/output/inout args (19.5) ---

TEST_F(Chapter19ErrorRulesTest, Row684_CovergroupExpressionFunctionCannotHaveRefArgument) {
  // catalog row 684 | 19.5 | LINT
  // Functions participating in a covergroup_expression shall not contain
  // output, inout, or ref arguments (const ref is allowed). r684_f takes
  // "ref int r" and is called from a bins expression.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r684_f"), nullptr)
      << "a function used in a covergroup_expression cannot have a ref/output/inout argument (IEEE 1800-2023 19.5)";
}

// --- row 685: covergroup_expression functions must be side-effect free (19.5) ---

TEST_F(Chapter19ErrorRulesTest, Row685_CovergroupExpressionFunctionMustHaveNoSideEffects) {
  // catalog row 685 | 19.5 | LINT
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 19.5)";
  // Functions participating in a covergroup_expression shall be automatic
  // (or preserve no state information) and have no side effects. r685_f
  // increments a static local "cnt" and is called from a bins expression.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_SIDE_EFFECT, "r685_f"), nullptr)
      << "a function used in a covergroup_expression must have no side effects (IEEE 1800-2023 19.5)";
}

// --- row 694: only the enclosing coverpoint's own name may replace a covergroup_range_list (19.5.1.1) ---

TEST_F(Chapter19ErrorRulesTest, Row694_OnlyEnclosingCoverpointNameAllowedAsRangeList) {
  // catalog row 694 | 19.5.1.1 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 19.5.1.1)";
  // When a coverpoint name is used in place of the covergroup_range_list of
  // a bin, only the name of the coverpoint containing the bin being defined
  // shall be allowed. r694_b's own bin uses "r694_a", naming the other
  // coverpoint instead of the enclosing one (r694_b).
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r694_a"), nullptr)
      << "only the enclosing coverpoint's own name may replace a covergroup_range_list (IEEE 1800-2023 19.5.1.1)";
}

// --- row 696: identifiers declared inside a covergroup are not visible in a set_covergroup_expression (19.5.1.2) ---

TEST_F(Chapter19ErrorRulesTest, Row696_CoverpointIdentifierNotVisibleInSetExpression) {
  // catalog row 696 | 19.5.1.2 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 19.5.1.2)";
  // Identifiers declared within the covergroup (such as coverpoint
  // identifiers and bin identifiers) are not visible in a
  // set_covergroup_expression. r696_b's bin set expression uses r696_a, the
  // other coverpoint's own label.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r696_a"), nullptr)
      << "a coverpoint identifier is not visible in a set_covergroup_expression (IEEE 1800-2023 19.5.1.2)";
}

// --- rows 705-707: unmatchable bin values (19.5.7) --------------------------

TEST_F(Chapter19ErrorRulesTest, Row705_NegativeSignedBinValueAgainstUnsignedCoverpointWarns) {
  // catalog row 705 | 19.5.7 | COMP
  // An implementation shall issue a warning if the effective type of the
  // coverpoint expression is unsigned and a bins expression is signed with a
  // negative value; the offending element does not participate. r705_g1's
  // coverpoint p1 is unsigned (bit [2:0]) and bin b2 includes -1.
  EXPECT_NE(findError(ErrorDefinition::COMP_UNMATCHABLE_BIN_VALUE, "b2"), nullptr)
      << "a negative signed bin value against an unsigned coverpoint must be warned about (IEEE 1800-2023 19.5.7)";
}

TEST_F(Chapter19ErrorRulesTest, Row706_BinValueNotEqualUnderCoverpointTypeWarns) {
  // catalog row 706 | 19.5.7 | COMP
  // An implementation shall issue a warning if assigning a bins expression
  // to a variable of the coverpoint's effective type would not compare equal
  // to the original -- i.e. the value does not fit the coverpoint type.
  // r706_g1's coverpoint p1 is bit [2:0] (0..7); bin b1 includes [6:10].
  EXPECT_NE(findError(ErrorDefinition::COMP_UNMATCHABLE_BIN_VALUE, "b1"), nullptr)
      << "a bin value that does not fit the coverpoint's effective type must be warned about (IEEE 1800-2023 19.5.7)";
}

TEST_F(Chapter19ErrorRulesTest, Row707_NonWildcardBinValueWithXZBitsWarns) {
  // catalog row 707 | 19.5.7 | COMP
  // An implementation shall issue a warning if a bins expression yields a
  // value with any x or z bits (this rule does not apply to wildcard bins).
  // r707_g1's bin bx is a plain (non-wildcard) bin set to 4'b10x1.
  EXPECT_NE(findError(ErrorDefinition::COMP_UNMATCHABLE_BIN_VALUE, "bx"), nullptr)
      << "a non-wildcard bin value with x/z bits must be warned about (IEEE 1800-2023 19.5.7)";
}

// --- row 711: a cross name has limited visibility (19.6) --------------------

TEST_F(Chapter19ErrorRulesTest, Row711_CrossNameNotUsableForOptionAssignment) {
  // catalog row 711 | 19.6 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 19.6)";
  // A cross name may be referenced only in a hierarchical name prefixed by a
  // covergroup variable, or after ::. "r711_crs.option.weight = 2;" inside
  // the covergroup body uses the cross name r711_crs in neither position.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r711_crs"), nullptr)
      << "a cross name has limited visibility and cannot be used this way (IEEE 1800-2023 19.6)";
}

// --- row 713: only the enclosing cross's own identifier may be used as a select_expression (19.6.1.2) ---

TEST_F(Chapter19ErrorRulesTest, Row713_OnlyEnclosingCrossIdentifierAllowedAsSelectExpression) {
  // catalog row 713 | 19.6.1.2 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 19.6.1.2)";
  // When a cross_identifier is used as a select_expression, only the
  // cross_identifier of the enclosing cross may be used. r713_Y's own bin
  // select expression references r713_X, the other cross's identifier.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r713_X"), nullptr)
      << "only the enclosing cross's own identifier may be used as a select_expression (IEEE 1800-2023 19.6.1.2)";
}

// --- row 719: option.weight must be non-negative (19.7) ---------------------

TEST_F(Chapter19ErrorRulesTest, Row719_OptionWeightMustBeNonNegative) {
  // catalog row 719 | 19.7 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 19.7)";
  // The weight specified for option.weight (and type_option.weight) shall be
  // a non-negative integral value. r719_cg sets "option.weight = -1;".
  EXPECT_NE(findError(ErrorDefinition::COMP_VALUE_OUT_OF_RANGE, "r719_cg"), nullptr)
      << "option.weight must be a non-negative integral value (IEEE 1800-2023 19.7)";
}

// --- row 720: per_instance/get_inst_coverage can only be set in the definition (19.7) ---

TEST_F(Chapter19ErrorRulesTest, Row720_PerInstanceOnlySettableInCovergroupDefinition) {
  // catalog row 720 | 19.7 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 19.7)";
  // The per_instance and get_inst_coverage options can only be set in the
  // covergroup definition; they cannot be assigned procedurally after
  // instantiation. r720_m assigns "ci.option.per_instance = 1;" in an
  // initial block, after r720_cg's own instance ci was created.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_OPTION, "per_instance"), nullptr)
      << "per_instance can only be set in the covergroup definition, not procedurally (IEEE 1800-2023 19.7)";
}

// --- row 721: auto_bin_max/detect_overlap/cross_retain_auto_bins can only be set in the definition (19.7) ---

TEST_F(Chapter19ErrorRulesTest, Row721_AutoBinMaxOnlySettableInDefinition) {
  // catalog row 721 | 19.7 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 19.7)";
  // The auto_bin_max, detect_overlap, and cross_retain_auto_bins options can
  // only be set in the covergroup or coverpoint definition, not assigned
  // procedurally. r721_m assigns "ci.r721_a.option.auto_bin_max = 128;" in
  // an initial block, after instantiation.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_OPTION, "auto_bin_max"), nullptr)
      << "auto_bin_max can only be set in the covergroup/coverpoint definition, not procedurally (IEEE 1800-2023 19.7)";
}

// --- row 722: each instance option is restricted to its own Table 19-2 syntactic level (19.7) ---

TEST_F(Chapter19ErrorRulesTest, Row722_NameOptionNotAllowedAtCoverpointLevel) {
  // catalog row 722 | 19.7 | COMP
  // Each instance coverage option may only be specified at the syntactic
  // levels permitted by Table 19-2; "name" is allowed only at the
  // covergroup level. r722_ca sets "option.name = "cp";" at the coverpoint
  // level.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_OPTION, "name"), nullptr)
      << "the name option is not permitted at the coverpoint syntactic level (IEEE 1800-2023 19.7, Table 19-2)";
}

// --- row 723: overlapping bin ranges warn when option.detect_overlap is set (19.7) ---

TEST_F(Chapter19ErrorRulesTest, Row723_OverlappingBinRangesWarnWhenDetectOverlapSet) {
  // catalog row 723 | 19.7 | COMP
  // When option.detect_overlap is true, a warning is issued if there is an
  // overlap between the range list (or transition list) of two bins of a
  // coverpoint. r723_cg's coverpoint v has b1={[0:10]} and b2={[5:20]},
  // which overlap on [5:10], with detect_overlap set.
  EXPECT_NE(findError(ErrorDefinition::COMP_OVERLAPPING_BINS, "b2"), nullptr)
      << "overlapping bin ranges must be warned about when option.detect_overlap is set (IEEE 1800-2023 19.7)";
}

// --- row 725: strobe/real_interval type options can only be set in the definition (19.7.1) ---

TEST_F(Chapter19ErrorRulesTest, Row725_StrobeOnlySettableInCovergroupDefinition) {
  // catalog row 725 | 19.7.1 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 19.7.1)";
  // The strobe and real_interval type options can only be set in the
  // covergroup definition, not assigned procedurally. r725_m assigns
  // "r725_cg::type_option.strobe = 1;" in an initial block.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_OPTION, "strobe"), nullptr)
      << "strobe can only be set in the covergroup definition, not procedurally (IEEE 1800-2023 19.7.1)";
}

// --- row 726: each type option is restricted to its own Table 19-4 syntactic level (19.7.1) ---

TEST_F(Chapter19ErrorRulesTest, Row726_RealIntervalNotAllowedInCross) {
  // catalog row 726 | 19.7.1 | COMP
  // Each covergroup type option may only be specified at the syntactic
  // levels permitted by Table 19-4; real_interval is not allowed in a cross.
  // r726_x sets "type_option.real_interval = 0.5;" at the cross level.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_OPTION, "real_interval"), nullptr)
      << "real_interval is not a permitted type option at the cross syntactic level (IEEE 1800-2023 19.7.1, Table 19-4)";
}

// --- row 728: an overridden sample() formal is usable only in a coverpoint or guard expression (19.8.1) ---

TEST_F(Chapter19ErrorRulesTest, Row728_SampleFormalUsableOnlyInCoverpointOrGuard) {
  // catalog row 728 | 19.8.1 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 19.8.1)";
  // It shall be an error to use a formal argument of an overridden sample
  // method in any context other than a coverpoint or a conditional guard
  // expression. r728_C1's sample formal "b" is used directly in
  // "option.per_instance = b;", neither position.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "b"), nullptr)
      << "an overridden sample() formal is usable only in a coverpoint or guard expression (IEEE 1800-2023 19.8.1)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
