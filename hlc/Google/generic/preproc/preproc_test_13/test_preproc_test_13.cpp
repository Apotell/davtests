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
// LRM 22.5.1 (IEEE 1800-2012+): formal argument defaults may be string
// literals (b="(3,2)") or parenthesised expressions (c=(3,2)). The inner
// parentheses must not confuse the preprocessor when matching the closing `)'
// of the argument list. The multi-line argument list and `\'-continued
// replacement text "a + b /c +345" must both be parsed correctly.
class PreprocArgStringDefaultsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "preproc_test_13.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// The source file contains no module declaration; the design must have no modules.
TEST_F(PreprocArgStringDefaultsTest, NoModules) {
  const hldb::ModuleCollection *const mods = m_design->getAllModules();
  EXPECT_TRUE(mods == nullptr || mods->empty()) << "no module declaration in source";
}

// LONG_MACRO must appear in the source file's macro table.
TEST_F(PreprocArgStringDefaultsTest, LongMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_13.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("LONG_MACRO", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "LONG_MACRO must be defined";
}

// The preprocessor must correctly match the outer `(' / `)' of the argument
// list even when inner parentheses appear in default values such as c=(3,2).
// getArguments() must be non-null and non-empty (arg list parsed), and
// getTokens() non-null (replacement text 'a + b /c +345' captured).
TEST_F(PreprocArgStringDefaultsTest, LongMacroHasReplacementText) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_13.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("LONG_MACRO", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_NE(macro->getArguments(), nullptr)
      << "LONG_MACRO arg list was not parsed -- nested parens in defaults may have broken matching";
  EXPECT_FALSE(macro->getArguments()->empty()) << "LONG_MACRO arg list must not be empty";
  EXPECT_NE(macro->getTokens(), nullptr) << "LONG_MACRO replacement text 'a + b /c +345' was not captured";
}

// LONG_MACRO(a, b="(3,2)", c=(3,2)) has exactly three formal arguments:
// a, b, c. Nested parentheses in the default text must not be miscounted
// as extra formal arguments.
TEST_F(PreprocArgStringDefaultsTest, LongMacroFormalArgNames) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_13.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("LONG_MACRO", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getArguments(), nullptr);
  ASSERT_EQ(macro->getArguments()->size(), 3u)
      << "LONG_MACRO must have exactly 3 formal arguments: a, b, c -- nested parens in defaults must not "
         "be miscounted";
  EXPECT_EQ((*macro->getArguments())[0]->getName(), "a") << "first formal argument must be 'a'";
  EXPECT_EQ((*macro->getArguments())[1]->getName(), "b") << "second formal argument must be 'b'";
  EXPECT_EQ((*macro->getArguments())[2]->getName(), "c") << "third formal argument must be 'c'";
}

// sec. 22.5.1: formal argument defaults may be string literals (b="(3,2)")
// or parenthesized expressions (c=(3,2)). As noted in preproc_test_12, the
// HLDB 'argument' field is modeled as a plain 'identifier' with no
// default-text field, so these default values cannot be retrieved.
TEST_F(PreprocArgStringDefaultsTest, LongMacroArgumentDefaultsAreModeled) {
  GTEST_SKIP() << "HLDB's preproc_macro_definition.argument is typed as a plain 'identifier' with no default-text "
                  "field, so 'b=\"(3,2)\"' and 'c=(3,2)' formal-argument defaults (IEEE 1800-2023 sec. 22.5.1) "
                  "cannot be represented or queried. Fix pending: add a default-text field to the argument model.";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
