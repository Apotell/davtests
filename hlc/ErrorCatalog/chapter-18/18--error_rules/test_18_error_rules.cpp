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

// Tests for the IEEE 1800-2023 Clause 18 (Constrained random value generation)
// error scenarios catalogued in sv_error_catalog_Latest.xlsx (rows 608, 663).
//
// Scope, fixture layout and test shapes follow the Clause 3 file in this same
// suite; see hlc/ErrorCatalog/chapter-3 for the rationale.
//
// Behaviour observed while writing this file (hlc.exe -d db over the fixture):
// r663_m compiles and r608_c parses. Two diagnostics appear, neither of them
// the required one and neither asserted below: a CP5849 "Feature not yet
// implemented" warning at 19:5 for the randsequence, and a Linter null-actual
// error at 12:21 because the aa.size method call in the constraint never
// binds. The latter is a binding gap that happens to sit on top of the row 608
// construct; it would keep passing after aa.size started binding correctly, at
// which point the real rule would go unchecked.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter18ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 608: associative array size is not randomizable (18.4) -------------

TEST_F(Chapter18ErrorRulesTest, Row608_ConstrainingAssociativeArraySizeIsRejected) {
  // catalog row 608 | 18.4 | COMP
  // Dynamic arrays and queues may have their size constrained; associative
  // arrays may not -- their size and index values are not randomizable. The
  // constraint on line 12 constrains aa.size for an associative array keyed by
  // string.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 18.4 makes the size and index values "
                  "of an associative array non-randomizable, so constraining aa.size is illegal";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "aa", 12, 18), nullptr)
      << "the size of an associative array cannot be constrained (IEEE 1800-2023 18.4)";
}

// --- row 663: randsequence production weights (18.17.1) ---------------------

TEST_F(Chapter18ErrorRulesTest, Row663_NegativeProductionWeightIsRejected) {
  // catalog row 663 | 18.17.1 | COMP
  // "The rs_weight_specification shall evaluate to an integral non-negative
  // value." The weight on line 20 is -3.
  //
  // HLC does not model randsequence at all yet -- it emits CP5849 "Feature not
  // yet implemented" for the whole construct -- so this row is blocked behind
  // that, not merely unchecked.
  GTEST_SKIP() << "randsequence is not modelled (CP5849 'Feature not yet implemented'); "
                  "IEEE 1800-2023 18.17.1 requires an rs_weight_specification to evaluate to an "
                  "integral non-negative value";
  EXPECT_NE(findError(ErrorDefinition::COMP_VALUE_OUT_OF_RANGE, "add", 20, 21), nullptr)
      << "a production weight must be a non-negative integral value (IEEE 1800-2023 18.17.1)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
