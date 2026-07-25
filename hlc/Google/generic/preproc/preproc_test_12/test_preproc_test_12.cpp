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
#include <hldb/identifier.h>
#include <hldb/module.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/source_file.h>

namespace hlc {
// LRM 22.5.1 (IEEE 1800-2012+): formal arguments may carry default values
// (e.g. b=2, c=42). The argument list here spans lines (no `\') and is
// followed by a `\'-continued replacement text "a + b /c +345".
class PreprocArgDefaultsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "preproc_test_12.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// The source file contains no module declaration; the design must have no modules.
TEST_F(PreprocArgDefaultsTest, NoModules) {
  const hldb::ModuleCollection *const mods = m_design->getAllModules();
  EXPECT_TRUE(mods == nullptr || mods->empty()) << "no module declaration in source";
}

// LONG_MACRO must appear in the source file's macro table.
TEST_F(PreprocArgDefaultsTest, LongMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_12.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("LONG_MACRO", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "LONG_MACRO must be defined";
}

// The replacement text "a + b /c +345" must be captured. The name starts at
// column 9. The body token must be captured (not lost due to arg-list confusion).
TEST_F(PreprocArgDefaultsTest, LongMacroHasReplacementText) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_12.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("LONG_MACRO", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_EQ(macro->getStartLine(), 7);
  EXPECT_EQ(macro->getStartColumn(), 1);
  EXPECT_EQ(macro->getEndLine(), 9);
  EXPECT_EQ(macro->getEndColumn(), 14);
  ASSERT_NE(macro->getNameObj(), nullptr);
  EXPECT_EQ(macro->getNameObj()->getStartColumn(), 9u);
  ASSERT_NE(macro->getTokens(), nullptr);
  ASSERT_FALSE(macro->getTokens()->empty());
  ASSERT_NE(macro->getTokens()->front(), nullptr);
  EXPECT_EQ(macro->getTokens()->front()->getStartColumn(), 19u);
}

// LRM 22.5.1: LONG_MACRO has three formal parameters: a, b (default 2),
// c (default 42).
TEST_F(PreprocArgDefaultsTest, LongMacroHasThreeArguments) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_12.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("LONG_MACRO", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getArguments(), nullptr);
  EXPECT_EQ(macro->getArguments()->size(), 3u)
      << "LONG_MACRO has three formal parameters: a, b (default 2), c (default 42)";
}

TEST_F(PreprocArgDefaultsTest, LongMacroHasTokens) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_12.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("LONG_MACRO", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getTokens(), nullptr);
  EXPECT_FALSE(macro->getTokens()->empty()) << "LONG_MACRO body must have tokens (a + b /c +345)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
