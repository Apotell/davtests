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
class PreprocTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "t_preproc.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// t_preproc.v is a large preprocessor-only file with no compilable module.
TEST_F(PreprocTest, NoModules) {
  ASSERT_TRUE((m_design->getAllModules() == nullptr) || (m_design->getAllModules()->size() == 0u))
      << "t_preproc.v contains only preprocessor definitions and tests, no module declarations";
}

// LRM 22.5.1: `define FOOBAR <body> is an object-like macro with replacement text.
// bodyStartColumn must be > nameStartColumn.
TEST_F(PreprocTest, FoobarMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("t_preproc.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("FOOBAR", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "FOOBAR macro must be defined";
}

TEST_F(PreprocTest, FoobarMacroHasBody) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("t_preproc.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("FOOBAR", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_NE(macro->getTokens(), nullptr) << "FOOBAR must have a replacement text body";
}

// LRM 22.5.1: `define MULTILINE <body with line continuation> must span multiple
// physical lines and be recorded as a single macro definition.
TEST_F(PreprocTest, MultilineMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("t_preproc.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("MULTILINE", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "MULTILINE multi-line macro must be defined";
}

// LRM 22.5.1: `define DEF_A3 with no replacement text is a valid flag macro.
// getTokens() must be null (no body tokens).
TEST_F(PreprocTest, DefA3IsFlagMacro) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("t_preproc.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("DEF_A3", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_EQ(macro->getTokens(), nullptr) << "DEF_A3 has no replacement text; getTokens() must be null";
}

// LRM 22.4: t_preproc.v includes t_preproc_inc2.vh; that file must be recorded.
TEST_F(PreprocTest, IncludedFileRecorded) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("t_preproc.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("t_preproc_inc2.vh", sf->getIncludes());
  EXPECT_NE(inc, nullptr) << "t_preproc_inc2.vh must be recorded as an include";
}

// LRM 22.5.1: INCFILE is defined inside t_preproc_inc2.vh and must be
// accessible via that file's macro definition list.
TEST_F(PreprocTest, IncfileMacroDefinedInInclude) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("t_preproc.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("t_preproc_inc2.vh", sf->getIncludes());
  ASSERT_NE(inc, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("INCFILE", inc->getPreprocMacroDefinitions());
  EXPECT_NE(macro, nullptr) << "INCFILE macro must be recorded inside t_preproc_inc2.vh";
}

// LRM 22.5.1: FOOBAR is object-like (no arg list). getArguments() must be null
// and getNameObj() must have name "FOOBAR".
TEST_F(PreprocTest, FoobarMacroObjectLike) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("t_preproc.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("FOOBAR", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_EQ(macro->getArguments(), nullptr) << "FOOBAR is object-like; getArguments() must be null";
  const hldb::Identifier *const nameObj = macro->getNameObj();
  ASSERT_NE(nameObj, nullptr) << "getNameObj() must not be null";
  EXPECT_EQ(nameObj->getName(), "FOOBAR") << "getNameObj() name must be FOOBAR";
}

// LRM 22.5.1: `define withparam(a, b) is a function-like macro with 2
// arguments and a replacement body. getArguments() must have size 2 and
// getTokens() must be non-null.
TEST_F(PreprocTest, WithparamArgCountAndBody) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("t_preproc.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("withparam", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "withparam must be defined";
  ASSERT_NE(macro->getArguments(), nullptr) << "withparam is function-like; getArguments() must not be null";
  EXPECT_EQ(macro->getArguments()->size(), 2u) << "withparam must have exactly 2 formal arguments: a, b";
  EXPECT_NE(macro->getTokens(), nullptr) << "withparam must have a replacement body (getTokens() must not be null)";
}

// LRM 22.5.1: DEF_A3 is a flag macro with no replacement text. getTokens()
// must be null and getArguments() must be null.
TEST_F(PreprocTest, DefA3FlagMacroNoArgs) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("t_preproc.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("DEF_A3", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_EQ(macro->getArguments(), nullptr) << "DEF_A3 is object-like; getArguments() must be null";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
