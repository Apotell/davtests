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
class PreprocPersistIncTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "t_preproc_persist_inc.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// The include guard file itself contains only preprocessor directives; no module.
TEST_F(PreprocPersistIncTest, NoModules) {
  ASSERT_TRUE((m_design->getAllModules() == nullptr) || (m_design->getAllModules()->size() == 0u))
      << "t_preproc_persist_inc contains no module declarations";
}

// LRM 22.5.1: COMMON_GUARD is the include guard macro defined in the file.
// It must appear directly in the source file's macro table.
TEST_F(PreprocPersistIncTest, CommonGuardDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_persist_inc.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("COMMON_GUARD", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "COMMON_GUARD include-guard macro must be defined";
}

// ---------------------------------------------------------------------------
// 1. PreprocMacroDefinition arguments and tokens
// ---------------------------------------------------------------------------

// LRM 22.5.1: `define COMMON_GUARD 1 is an object-like macro with no argument
// list and a body containing the token "1".
TEST_F(PreprocPersistIncTest, CommonGuardHasNoArguments) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_persist_inc.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("COMMON_GUARD", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_EQ(macro->getArguments(), nullptr) << "COMMON_GUARD is an object-like macro with no argument list";
}

TEST_F(PreprocPersistIncTest, CommonGuardHasTokens) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_persist_inc.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("COMMON_GUARD", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_NE(macro->getTokens(), nullptr) << "COMMON_GUARD has a body token (1)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
