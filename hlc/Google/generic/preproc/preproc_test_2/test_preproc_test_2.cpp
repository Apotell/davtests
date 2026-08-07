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
class PreprocIncludeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "preproc_test_2.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 22.4: `include must process the file inline before the rest of the
// translation unit. If it failed, the `ifndef SUCCESS branch would introduce
// invalid SV text and prevent module compilation.
TEST_F(PreprocIncludeTest, ModuleCompiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("test", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 'test' must compile; include must have succeeded";
}

// LRM 22.4: the included file must appear in the source file's include list.
TEST_F(PreprocIncludeTest, IncludedFileRecorded) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_2.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("preproc_test_2.svh", sf->getIncludes());
  EXPECT_NE(inc, nullptr) << "preproc_test_2.svh must appear in the include list";
}

// LRM 22.4: `define SUCCESS in the svh must be visible after the include.
// It is recorded on the included SourceFile object.
TEST_F(PreprocIncludeTest, SuccessMacroDefinedInInclude) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_2.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("preproc_test_2.svh", sf->getIncludes());
  ASSERT_NE(inc, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("SUCCESS", inc->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "SUCCESS must be defined inside preproc_test_2.svh";
}

// LRM 22.6: `ifndef SANITY -- SANITY is not yet defined, so it gets defined
// here as a flag macro on the main source file.
TEST_F(PreprocIncludeTest, SanityMacroDefinedInMainFile) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_2.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("SANITY", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "SANITY must be defined in the main source file";
}

// SUCCESS is defined in the included file; it must not be re-defined on the
// main source file's own macro list.
TEST_F(PreprocIncludeTest, SuccessNotRedefinedInMainFile) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_2.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("SUCCESS", sf->getPreprocMacroDefinitions());
  EXPECT_EQ(macro, nullptr) << "SUCCESS belongs to the included file, not to the main source file";
}

// ---------------------------------------------------------------------------
// 1. PreprocMacroDefinition arguments and tokens
// ---------------------------------------------------------------------------

// LRM 22.5.1: `define SUCCESS in preproc_test_2.svh is a flag macro with no
// argument list and no body. getArguments() must be null and getTokens() must
// be null or empty.
TEST_F(PreprocIncludeTest, SuccessMacroHasNoArguments) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_2.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("preproc_test_2.svh", sf->getIncludes());
  ASSERT_NE(inc, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("SUCCESS", inc->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_EQ(macro->getArguments(), nullptr) << "SUCCESS is a flag macro with no argument list";
}

TEST_F(PreprocIncludeTest, SuccessMacroHasNoTokens) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_2.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("preproc_test_2.svh", sf->getIncludes());
  ASSERT_NE(inc, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("SUCCESS", inc->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_TRUE(macro->getTokens() == nullptr || macro->getTokens()->empty())
      << "SUCCESS is a flag macro with no replacement body";
}

// LRM 22.5.1: `define SANITY in preproc_test_2.sv is a flag macro with no
// argument list and no body.
TEST_F(PreprocIncludeTest, SanityMacroHasNoArguments) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_2.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("SANITY", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_EQ(macro->getArguments(), nullptr) << "SANITY is a flag macro with no argument list";
}

TEST_F(PreprocIncludeTest, SanityMacroHasNoTokens) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_2.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("SANITY", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_TRUE(macro->getTokens() == nullptr || macro->getTokens()->empty())
      << "SANITY is a flag macro with no replacement body";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
