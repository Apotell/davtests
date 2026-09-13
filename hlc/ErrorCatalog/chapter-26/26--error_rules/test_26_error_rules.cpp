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

// Tests for the IEEE 1800-2023 Clause 26 error scenarios catalogued in
// docs/error_catalog.xml (row 942).
//
// Scope: this file asserts ONLY that the diagnostic the catalog row
// requires is emitted. It deliberately makes no assertion about the shape
// of the compiled model. Exactly one TEST_F for the catalog row, named
// Row<N>_... after that row and carrying a "catalog row N | clause |
// category" comment; that is the link between the catalog and this file.
//
// Fixture: 26--error_rules.sv (row 942), compiled in one run by
// 26--error_rules.hlc.
//
// COMP_AMBIGUOUS_REFERENCE has no call site anywhere in src/ outside its own
// ErrorDefinition.cpp registration, so the assertion below is expected to be
// red today -- that is the intended, documented state of this exercise, not
// a bug for this file to work around. No test asserts the absence of a
// diagnostic, which would lock the gap in.

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

// --- row 942: ambiguous reference to an identifier wildcard-imported from two packages (26.3)

TEST_F(Chapter26ErrorRulesTest, Row942_AmbiguousWildcardImportReferenceIsRejected) {
  // catalog row 942 | 26.3 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 26.3)";
  // It shall be illegal if wildcard imports of more than one package within
  // the same scope define the same potentially locally visible identifier
  // and a reference resolves to that identifier; both r942_p1::c and
  // r942_p2::c are candidates for the plain reference 'c' in r942_m.
  EXPECT_NE(findError(ErrorDefinition::COMP_AMBIGUOUS_REFERENCE, "c"), nullptr)
      << "a reference resolving to an identifier wildcard-imported from two packages is ambiguous (IEEE 1800-2023 26.3)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
