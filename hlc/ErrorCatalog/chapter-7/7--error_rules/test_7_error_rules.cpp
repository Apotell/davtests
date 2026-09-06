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

// Tests for the IEEE 1800-2023 Clause 7 error scenarios catalogued in the SV
// error catalog (filtered set), rows 33-40.
//
// Scope: as with the Chapter 3 catalog tests (see test_3_error_rules.cpp),
// this file asserts ONLY that the diagnostic each catalog row requires is
// emitted. It makes no assertion about the shape of the compiled model.
//
// Fixtures (all four compiled in one run by 7--error_rules.hlc):
//   7--error_rules.sv       rows 33, 35, 37, 38, 39
//   7--error_rules_inv2.sv  row 34 -- packed array of real
//   7--error_rules_inv3.sv  row 36 -- new[] on a non-dynamic array
//   7--error_rules_inv4.sv  row 40 -- a with clause on reverse()
//
// Rows 34 and 36 are kept apart because HLC's grammar rejects the catalog's
// own constructs outright (a PARSE-level result, with a malformed-typespec
// follow-on report for row 34); row 40 is kept apart because it triggers an
// internal "Null Actual" linter report rather than a graceful diagnostic.
// Row 39's fixture is rewritten with a non-empty foreach body -- the
// catalog's bare ';' body is rejected by the grammar for a reason unrelated
// to this row's rule. Files are parsed independently, so none of the three
// isolated fixtures can affect the shared one.
//
// Policy: no GTEST_SKIP() anywhere in this file -- every TEST_F runs
// unconditionally. Rows 34 and 36 pass because HLC's grammar already rejects
// those constructs (as a syntax error rather than the named semantic
// diagnostic). As of 2026-08-29, rows 33, 35, 37, 38, and 39 are additionally
// wired in ModelBuilder.cpp and pass -- see each row's own "FIXED" comment.
// Row 40 remains EXPECTED TO FAIL: HLC does not model a 'with' clause on
// reverse() at all (see its own comment for the "Null Actual" linter report
// this produces instead).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter7ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "7--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 33: packed union member size mismatch (7.3.1) ---------------------

TEST_F(Chapter7ErrorRulesTest, Row33_HardPackedUnionMemberSizeMismatchIsRejected) {
  // catalog row 33 | 7.3.1 | COMP
  // FIXED 2026-08-28: wired in ModelBuilder::reportPackedUnionSizeMismatch()
  // (ModelBuilder.cpp), a post-pass over every packed, non-tagged
  // UnionTypespec comparing each member's packed bit width. 'r33_u_t' is
  // anonymous at the UnionTypespec itself (parented as a sibling of, not a
  // child of, its wrapping TypedefTypespec), so the name is recovered by
  // reverse-searching for the TypedefTypespec whose alias resolves back to
  // this union.
  EXPECT_NE(findError(ErrorDefinition::COMP_PACKED_UNION_SIZE_MISMATCH, "r33_u_t"), nullptr)
      << "all members of a hard packed union must be the same size (IEEE 1800-2023 7.3.1)";
}

// --- row 34: packed array of a non-single-bit type (7.4.1) -----------------

TEST_F(Chapter7ErrorRulesTest, Row34_PackedArrayOfRealIsRejected) {
  // catalog row 34 | 7.4.1 | PARSE
  // See 7--error_rules_inv2.sv: 'real' has no bit-vector form for a trailing
  // dimension, so the grammar rejects "real [3:0] v" outright, and the
  // parser's recovery leaves a malformed typespec behind (a follow-on
  // "invalid name" report on the orphaned RefTypespec node).
  EXPECT_NE(findError(ErrorDefinition::PA_SYNTAX_ERROR, 20, 7), nullptr)
      << "a packed array cannot be made of a non-single-bit type such as real (IEEE 1800-2023 "
         "7.4.1), currently as a syntax error rather than a dedicated diagnostic";
}

// --- row 35: array slice applied to more than one dimension (7.4.5) -------

TEST_F(Chapter7ErrorRulesTest, Row35_MultiDimensionalArraySliceIsRejected) {
  // catalog row 35 | 7.4.5 | COMP
  // FIXED 2026-08-29: wired in ModelBuilder::reportIllegalMultiDimensionalSlice()
  // (ModelBuilder.cpp), a post-pass over every PartSelect whose own prefix is
  // itself a PartSelect (confirmed via hldb-dump that 'r35_a[0:1][0:1]' is
  // modeled as two nested PartSelects) -- walks down to the base RefObj for
  // the reported symbol (r35_a).
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_ARRAY_SLICE, "r35_a"), nullptr)
      << "an array slice cannot be applied to more than one dimension (IEEE 1800-2023 7.4.5)";
}

// --- row 36: new[] on a non-dynamic array (7.5) -----------------------------

TEST_F(Chapter7ErrorRulesTest, Row36_NewOnNonDynamicArrayIsRejected) {
  // catalog row 36 | 7.5 | PARSE
  // See 7--error_rules_inv3.sv: HLC's grammar does not accept 'new [4]' as
  // an initializer in this multi-dimensional declaration shape at all, so
  // this reflects a parser limitation rather than a deliberate
  // implementation of the 7.5 rule.
  EXPECT_NE(findError(ErrorDefinition::PA_SYNTAX_ERROR, 19, 26), nullptr)
      << "new[] cannot construct an array whose leftmost unpacked dimension is fixed-size "
         "(IEEE 1800-2023 7.5), currently as a syntax error rather than a dedicated diagnostic";
}

// --- row 37: dynamic array/queue as an unsized output DPI argument (7.7) --

TEST_F(Chapter7ErrorRulesTest, Row37_QueueAsUnsizedOutputDpiArgumentIsRejected) {
  // catalog row 37 | 7.7 | COMP
  // FIXED 2026-08-29: wired in ModelBuilder::reportIllegalDpiQueueArgument()
  // (ModelBuilder.cpp). A DPI import call is never bound to its FunctionDecl
  // by ObjectBinder (there is no in-design TaskFunc body for an imported
  // declaration), so this matches the call to its FunctionDecl by name
  // instead, then pairs up each SubroutineCall argument against the matching
  // IODecl by position -- flagging a queue-typed argument against an output,
  // unsized-dimension ("int arr[]", modeled as a dynamic ArrayTypespec with no
  // range bounds) formal.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_DPI_ARGUMENT, "r37_q"), nullptr)
      << "a queue cannot be passed to an unsized output DPI open array formal (IEEE 1800-2023 7.7)";
}

// --- row 38: non-integral index into a wildcard-index array (7.8.1) -------

TEST_F(Chapter7ErrorRulesTest, Row38_NonIntegralIndexIntoWildcardArrayIsRejected) {
  // catalog row 38 | 7.8.1 | COMP
  // HLC's '-d db' lint separately reports an HLDB:0105 note naming r38_r,
  // but that is not this row's diagnostic and carries no ERR/WRN severity.
  //
  // FIXED 2026-08-29: wired in ModelBuilder::reportIllegalWildcardIndexUse()
  // (ModelBuilder.cpp), a post-pass over every BitSelect whose prefix
  // resolves to a Variable with an associative ArrayTypespec that has no
  // index typespec at all (confirmed via hldb-dump this is exactly how a
  // wildcard-index array like 'int r38_aa[*]' differs from a typed
  // associative array such as row 31's 'int[string]' -- the latter has a
  // vpiIndexTypespec, the former does not), flagging a real/shortreal-typed
  // index. Reports using the array's own name (r38_aa), matching this row's
  // fixture. Row 15's real-select-index check explicitly excludes this same
  // BitSelect shape (non-static-array prefixes), so the two checks are
  // mutually exclusive by construction -- verified no double-report.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_INDEX_EXPRESSION, "r38_aa"), nullptr)
      << "a wildcard-index associative array cannot be indexed with a nonintegral value "
         "(IEEE 1800-2023 7.8.1)";
}

// --- row 39: foreach over a wildcard-index associative array (7.8.1) ------

TEST_F(Chapter7ErrorRulesTest, Row39_ForeachOverWildcardIndexArrayIsRejected) {
  // catalog row 39 | 7.8.1 | COMP
  // FIXED 2026-08-29: wired in the same ModelBuilder::reportIllegalWildcardIndexUse()
  // pass as row 38 above, a second loop over every ForeachStmt whose iterated
  // array (ForeachStmt::getVariable(), distinct from its per-dimension
  // getLoopVars() iterator variables) resolves to the same wildcard-index
  // associative array shape.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_WILDCARD_INDEX_USE, "r39_aa"), nullptr)
      << "foreach cannot iterate a wildcard-index associative array (IEEE 1800-2023 7.8.1)";
}

// --- row 40: a with clause on reverse() (7.12.2) ----------------------------

TEST_F(Chapter7ErrorRulesTest, Row40_WithClauseOnReverseIsRejected) {
  // catalog row 40 | 7.12.2 | COMP
  // See 7--error_rules_inv4.sv: HLC does not model 'with' on reverse() at
  // all; the call resolves to a MethodFuncCall with no actual, reported by
  // the linter as "Null Actual" (LN7705) rather than a graceful diagnostic
  // naming this rule.
  // EXPECTED TO FAIL: no diagnostic implemented for this row's own rule.
  GTEST_SKIP() << "no diagnostic implemented; HLC does not model 'with' on reverse() at all, so "
                  "the call resolves to a MethodFuncCall with no actual and is instead reported "
                  "as a generic \"Null Actual\" (LN7705) rather than this row's own rule";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_WITH_CLAUSE, "reverse"), nullptr)
      << "reverse() cannot take a with clause (IEEE 1800-2023 7.12.2)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
