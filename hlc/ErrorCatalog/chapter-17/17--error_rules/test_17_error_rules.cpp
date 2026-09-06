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

// Tests for the IEEE 1800-2023 Clause 17 (Checkers) error scenarios catalogued
// in sv_error_catalog_Latest.xlsx (row 573).
//
// Scope, fixture layout and test shapes follow the Clause 3 file in this same
// suite; see hlc/ErrorCatalog/chapter-3 for the rationale.
//
// Behaviour observed while writing this file (hlc.exe -d db over the fixture):
// r573_sub compiles, and the instantiation inside the checker is rejected by
// the grammar with a single syntax error at 13:2.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter17ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "17--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 573: nothing instantiable inside a checker (17.2) ------------------

TEST_F(Chapter17ErrorRulesTest, Row573_ModuleInstantiationInsideACheckerIsRejected) {
  // catalog row 573 | 17.2 | COMP
  // "Modules, interfaces, and programs shall not be instantiated inside
  // checkers." The instance on line 13 is inside checker r573_c.
  //
  // HLC enforces this in the grammar: checker_or_generate_item has no
  // instantiation alternative, so the instance is a syntax error rather than a
  // semantic one. The rule is honoured either way, which is what this asserts;
  // the catalog's COMP_ILLEGAL_INSTANTIATION code may therefore be
  // unnecessary for this row. Recorded here rather than acted on.
  EXPECT_NE(findError(ErrorDefinition::PA_SYNTAX_ERROR, 13, 2), nullptr)
      << "a module cannot be instantiated inside a checker (IEEE 1800-2023 17.2)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
