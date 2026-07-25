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
class PreprocIfdefTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "t_preproc_ifdef.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 22.6: `ifdef / `ifndef / `elsif / `else / `endif correctly select code
// branches; the module must compile cleanly.
TEST_F(PreprocIfdefTest, ModuleCompiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("work@t", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 't' must compile with conditional compilation directives";
}

// LRM 22.5.1: `define EMPTY_TRUE with no replacement text is a valid flag macro.
// It must appear in the source file's macro table.
TEST_F(PreprocIfdefTest, EmptyTrueMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_ifdef.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("EMPTY_TRUE", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "EMPTY_TRUE flag macro must be defined";
}

// LRM 22.5.1: a flag macro has no replacement text.
TEST_F(PreprocIfdefTest, EmptyTrueIsFlagMacro) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_ifdef.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("EMPTY_TRUE", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_TRUE(macro->getTokens() == nullptr || macro->getTokens()->empty()) << "macro must have no body tokens";
}

// LRM 22.5.1: `define A with no replacement text must also be defined.
TEST_F(PreprocIfdefTest, AMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_ifdef.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("A", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "A flag macro must be defined";
}

// LRM 22.5.1: A is also a flag macro; it has no replacement text.
TEST_F(PreprocIfdefTest, AIsFlagMacro) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_ifdef.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("A", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_TRUE(macro->getTokens() == nullptr || macro->getTokens()->empty()) << "macro must have no body tokens";
}

// LRM 22.5.1: EMPTY_TRUE is a flag macro; it takes no arguments.
TEST_F(PreprocIfdefTest, EmptyTrueMacroHasNoArguments) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_ifdef.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("EMPTY_TRUE", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_TRUE(macro->getArguments() == nullptr || macro->getArguments()->empty());
}

// LRM 22.5.1: EMPTY_TRUE is a flag macro; it has no body tokens.
TEST_F(PreprocIfdefTest, EmptyTrueMacroHasNoTokens) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_ifdef.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("EMPTY_TRUE", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_TRUE(macro->getTokens() == nullptr || macro->getTokens()->empty());
}

// LRM 22.5.1: A is a flag macro; it takes no arguments.
TEST_F(PreprocIfdefTest, AMacroHasNoArguments) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_ifdef.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("A", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_TRUE(macro->getArguments() == nullptr || macro->getArguments()->empty());
}

// LRM 22.5.1: A is a flag macro; it has no body tokens.
TEST_F(PreprocIfdefTest, AMacroHasNoTokens) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_ifdef.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("A", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_TRUE(macro->getTokens() == nullptr || macro->getTokens()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
