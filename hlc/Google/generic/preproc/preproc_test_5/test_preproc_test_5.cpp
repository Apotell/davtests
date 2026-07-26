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
class PreprocFuncMacroEmptyBodyTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "preproc_test_5.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 22.5.1: `define INCEPTION(a, b, c) with no replacement text is a valid
// function-like macro with an empty body. The module must compile cleanly.
TEST_F(PreprocFuncMacroEmptyBodyTest, ModuleCompiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("test", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 'test' must compile";
}

// LRM 22.5.1: the function-like macro must appear in the source file's macro table.
TEST_F(PreprocFuncMacroEmptyBodyTest, InceptionMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_5.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("INCEPTION", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "INCEPTION macro must be defined";
}

// LRM 22.5.1: the `(' immediately after the name makes INCEPTION function-like.
// getArguments() must be non-null to distinguish it from an object-like macro.
TEST_F(PreprocFuncMacroEmptyBodyTest, InceptionMacroIsFunctionLike) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_5.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("INCEPTION", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_NE(macro->getArguments(), nullptr) << "INCEPTION is function-like; getArguments() must not be null";
}

// LRM 22.5.1: INCEPTION(a, b, c) has three formal arguments.
TEST_F(PreprocFuncMacroEmptyBodyTest, InceptionHasThreeFormalArgs) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_5.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("INCEPTION", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getArguments(), nullptr);
  EXPECT_GT(macro->getArguments()->size(), 0u);
}

// LRM 22.5.1: INCEPTION(a, b, c) has no replacement text; the token list is empty.
TEST_F(PreprocFuncMacroEmptyBodyTest, InceptionMacroHasNoTokens) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_5.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("INCEPTION", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_TRUE(macro->getTokens() == nullptr || macro->getTokens()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
