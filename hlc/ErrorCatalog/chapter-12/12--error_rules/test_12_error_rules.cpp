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

// Tests for the IEEE 1800-2023 Clause 12 error scenarios catalogued in
// docs/error_catalog.xml.
//
// Scope: this file asserts ONLY that the diagnostic each catalog row requires
// is emitted. It deliberately makes no assertion about the shape of the
// compiled model. Exactly one TEST_F per catalog row, named Row<N>_... after
// that row and carrying a "catalog row N | clause | category" comment; that
// is the link between the catalog and this file.
//
// Fixture (compiled in one run by 12--error_rules.hlc):
//   12--error_rules.sv   row 385
//
// Catalog row covered here:
//   385 | 12.7.3 | COMP_ILLEGAL_EXPRESSION_CONTEXT
//
// Per project convention, no test below uses GTEST_SKIP(): every assertion
// states the diagnostic IEEE 1800-2023 requires, even where
// ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT currently has zero call
// sites anywhere in src/ (confirmed by grep) -- i.e. it is expected to be
// red today. No test here asserts the absence of a diagnostic.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter12ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "12--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 385: foreach array identifier cannot be a function call (12.7.3) --

TEST_F(Chapter12ErrorRulesTest, Row385_ForeachArrayIdentifierCannotBeAFunctionCall) {
  // catalog row 385 | 12.7.3 | COMP
  // It shall be an error to include a function call as an implicit variable
  // declaration in the foreach-loop array identifier -- "foreach
  // (get_arr()[i])" uses a function call where the loop's array identifier
  // must be a variable/array reference.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r385_m"), nullptr)
      << "a function call cannot serve as the foreach-loop array identifier (IEEE 1800-2023 12.7.3)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
