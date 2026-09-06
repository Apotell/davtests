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

// Tests for the IEEE 1800-2023 Clause 26 (Packages) error scenarios catalogued
// in sv_error_catalog_Latest.xlsx (row 942).
//
// Scope, fixture layout and test shapes follow the Clause 3 file in this same
// suite; see hlc/ErrorCatalog/chapter-3 for the rationale.
//
// Behaviour observed while writing this file (hlc.exe -d db over the fixture):
// 26--error_rules.sv compiles with no errors, no warnings and no syntax errors
// at all -- the ambiguous reference on line 22 resolves silently to one of the
// two packages.
//
// This row is the 26.3 half of the same unbound/ambiguous-reference gap that
// Clause 3's row 7 skip message points at; the two should be picked up
// together.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter26ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "26--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 942: colliding wildcard imports (26.3) -----------------------------

TEST_F(Chapter26ErrorRulesTest, Row942_AmbiguousWildcardImportedNameIsRejected) {
  // catalog row 942 | 26.3 | COMP
  // "It shall be illegal if wildcard imports of more than one package within
  // the same scope define the same potentially locally visible identifier and
  // a reference resolves to that identifier." Both r942_p1 and r942_p2 declare
  // c, both are wildcard-imported into r942_m, and line 22 references c.
  //
  // The rule turns on the reference, not on the imports: two wildcard imports
  // that collide are legal as long as nothing in the scope actually resolves
  // to the colliding name. A check that flags the import pair alone would
  // reject legal designs.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 26.3 makes a reference illegal when it "
                  "resolves to an identifier made potentially locally visible by more than one "
                  "wildcard import in the same scope";
  EXPECT_NE(findError(ErrorDefinition::COMP_AMBIGUOUS_REFERENCE, "c", 22, 11), nullptr)
      << "a name made visible by two wildcard imports cannot be referenced (IEEE 1800-2023 26.3)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
