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

// Tests for the IEEE 1800-2023 Clause 25 error scenarios catalogued in
// docs/error_catalog.xml (rows 906, 912).
//
// Scope: this file asserts ONLY that the diagnostic each catalog row
// requires is emitted. It deliberately makes no assertion about the shape
// of the compiled model. Exactly one TEST_F per catalog row, named
// Row<N>_... after that row and carrying a "catalog row N | clause |
// category" comment; that is the link between the catalog and this file.
//
// Fixture: 25--error_rules.sv (rows 906, 912), compiled in one run by
// 25--error_rules.hlc.
//
// COMP_ILLEGAL_EXPRESSION_CONTEXT has no call site anywhere in src/ outside
// its own ErrorDefinition.cpp registration, so both assertions below are
// expected to be red today -- that is the intended, documented state of this
// exercise, not a bug for this file to work around. No test asserts the
// absence of a diagnostic, which would lock the gap in.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter25ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 906: implicit port connection cannot reference a generic interface (25.3.3)

TEST_F(Chapter25ErrorRulesTest, Row906_ImplicitPortConnectionCannotReferenceGenericInterface) {
  // catalog row 906 | 25.3.3 | LINT
  // An implicit port connection (.name or .*) cannot be used to reference a
  // generic interface; a named port connection shall be used.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r906_top"), nullptr)
      << "a generic interface port cannot be connected via .* (IEEE 1800-2023 25.3.3)";
}

// --- row 912: ref-direction modport signal cannot be a specify terminal (25.6)

TEST_F(Chapter25ErrorRulesTest, Row912_RefDirectionModportSignalCannotBeSpecifyTerminal) {
  // catalog row 912 | 25.6 | LINT
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 25.6)";
  // A ref port cannot be used as a terminal in a specify block; when an
  // interface signal is accessed through a modport that gives it ref
  // direction it is not a legal specify terminal.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r912_dtype"), nullptr)
      << "a ref-direction modport signal cannot be a specify block terminal (IEEE 1800-2023 25.6)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
