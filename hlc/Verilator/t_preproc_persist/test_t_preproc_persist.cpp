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
class PreprocPersistTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "t_preproc_persist.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// The file contains only preprocessor directives; no module is declared.
TEST_F(PreprocPersistTest, NoModules) {
  ASSERT_TRUE((m_design->getAllModules() == nullptr) || (m_design->getAllModules()->size() == 0u))
      << "t_preproc_persist contains no module declarations";
}

// LRM 22.4: `include must cause the compiler to record the included file.
TEST_F(PreprocPersistTest, IncludedFileRecorded) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_persist.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr) << "t_preproc_persist.v must record the included file t_preproc_persist_inc.v";
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("t_preproc_persist_inc.v", sf->getIncludes());
  EXPECT_NE(inc, nullptr) << "t_preproc_persist_inc.v must be recorded as an include";
}

// LRM 22.5.1: COMMON_GUARD is defined inside the included file. It must be
// accessible via the included source file's macro definition list.
TEST_F(PreprocPersistTest, CommonGuardDefinedInInclude) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_persist.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("t_preproc_persist_inc.v", sf->getIncludes());
  ASSERT_NE(inc, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("COMMON_GUARD", inc->getPreprocMacroDefinitions());
  EXPECT_NE(macro, nullptr) << "COMMON_GUARD must be defined inside the included file";
}

// ---------------------------------------------------------------------------
// 1. PreprocMacroDefinition arguments and tokens
// ---------------------------------------------------------------------------

// LRM 22.5.1: COMMON_GUARD is defined as `define COMMON_GUARD 1 -- an
// object-like macro with no argument list and a body containing "1".
TEST_F(PreprocPersistTest, CommonGuardHasNoArguments) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_persist.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("t_preproc_persist_inc.v", sf->getIncludes());
  ASSERT_NE(inc, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("COMMON_GUARD", inc->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_EQ(macro->getArguments(), nullptr) << "COMMON_GUARD is an object-like macro with no argument list";
}

TEST_F(PreprocPersistTest, CommonGuardHasTokens) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_persist.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("t_preproc_persist_inc.v", sf->getIncludes());
  ASSERT_NE(inc, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("COMMON_GUARD", inc->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_NE(macro->getTokens(), nullptr) << "COMMON_GUARD has a body token (1)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
