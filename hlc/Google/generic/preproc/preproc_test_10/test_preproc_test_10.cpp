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

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/source_file.h>

namespace hlc {
// LRM 22.5.1: LONG_MACRO(a, b, c) has a multi-line argument list (no `\') and
// replacement text "text goes here" on the same line as the closing `)'.
// The macro must be registered and its replacement text must be recognised.
class PreprocMultiLineArgListBodyTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "preproc_test_10.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// The source file contains no module declaration; the design must have no modules.
TEST_F(PreprocMultiLineArgListBodyTest, NoModules) {
  const hldb::ModuleCollection *const mods = m_design->getAllModules();
  EXPECT_TRUE(mods == nullptr || mods->empty()) << "no module declaration in source";
}

// LONG_MACRO must appear in the source file's macro table.
TEST_F(PreprocMultiLineArgListBodyTest, LongMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_10.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("LONG_MACRO", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "LONG_MACRO must be defined";
}

// The argument list must be parsed and the replacement text "text goes here"
// must be captured. getArguments() must be non-null and non-empty (arg list
// was recognised), and getTokens() must be non-null (body was recorded).
TEST_F(PreprocMultiLineArgListBodyTest, LongMacroHasReplacementText) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_10.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("LONG_MACRO", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_NE(macro->getArguments(), nullptr) << "LONG_MACRO arg list was not parsed -- getArguments() must not be null";
  EXPECT_FALSE(macro->getArguments()->empty()) << "LONG_MACRO arg list must not be empty";
  EXPECT_NE(macro->getTokens(), nullptr) << "LONG_MACRO replacement text 'text goes here' was not captured";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
