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
class PreprocConditionalTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "preproc_test_4.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 22.6: `ifdef INSANITY is false (INSANITY is not defined), so the `else
// branch fires and `define SANITY 1 executes. The module must compile.
TEST_F(PreprocConditionalTest, ModuleCompiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("work@test", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 'test' must compile after conditional preprocessing";
}

// LRM 22.6: the `else branch defines SANITY; it must appear in the macro table.
TEST_F(PreprocConditionalTest, SanityMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_4.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("SANITY", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "SANITY must be defined (else branch of `ifdef INSANITY)";
}

// LRM 22.5.1: `define SANITY 1 has replacement text "1"; bodyStartColumn > 0.
TEST_F(PreprocConditionalTest, SanityMacroHasReplacementText) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_4.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("SANITY", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_NE(macro->getTokens(), nullptr) << "macro must have non-null body tokens";
}

// LRM 22.6: the `ifdef INSANITY branch was not taken; INSANITY must never
// have been defined.
TEST_F(PreprocConditionalTest, InsanityNotDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_4.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("INSANITY", sf->getPreprocMacroDefinitions());
  EXPECT_EQ(macro, nullptr) << "INSANITY must not be defined; the ifdef branch was not taken";
}

// LRM 22.5.1: SANITY is an object-like macro; it takes no arguments.
TEST_F(PreprocConditionalTest, SanityMacroHasNoArguments) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_4.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("SANITY", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_TRUE(macro->getArguments() == nullptr || macro->getArguments()->empty());
}

// LRM 22.5.1: SANITY expands to "1"; the token list must be non-empty.
TEST_F(PreprocConditionalTest, SanityMacroHasTokens) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_4.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("SANITY", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getTokens(), nullptr);
  EXPECT_GT(macro->getTokens()->size(), 0u);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
