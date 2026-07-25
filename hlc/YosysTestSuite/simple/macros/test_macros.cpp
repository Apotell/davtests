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
class YosysTestSuiteMacrosTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "macros.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 22.5.1: all three test modules must compile cleanly.
TEST_F(YosysTestSuiteMacrosTest, TestDefModuleCompiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("work@test_def", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 'test_def' must compile";
}

TEST_F(YosysTestSuiteMacrosTest, TestIfdefModuleCompiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("work@test_ifdef", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 'test_ifdef' must compile";
}

TEST_F(YosysTestSuiteMacrosTest, TestCommentInMacroModuleCompiles) {
  const hldb::Module *const module =
      hldb::findByName<hldb::Module>("work@test_comment_in_macro", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 'test_comment_in_macro' must compile";
}

// LRM 22.5.1: MSB_LSB_SEP is an object-like macro (body at col 21).
TEST_F(YosysTestSuiteMacrosTest, MsbLsbSepMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("macros.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("MSB_LSB_SEP", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "MSB_LSB_SEP must be defined in macros.v";
  ASSERT_NE(macro->getTokens(), nullptr);
  ASSERT_FALSE(macro->getTokens()->empty());
  ASSERT_NE(macro->getTokens()->front(), nullptr);
  EXPECT_EQ(macro->getTokens()->front()->getStartColumn(), 21u) << "MSB_LSB_SEP body starts at column 21";
}

// LRM 22.5.1: get_msb and get_lsb are function-like macros (body at col 27).
TEST_F(YosysTestSuiteMacrosTest, GetMsbMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("macros.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("get_msb", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "get_msb must be defined";
  ASSERT_NE(macro->getTokens(), nullptr);
  ASSERT_FALSE(macro->getTokens()->empty());
  ASSERT_NE(macro->getTokens()->front(), nullptr);
  EXPECT_EQ(macro->getTokens()->front()->getStartColumn(), 27u) << "get_msb body starts at column 27";
}

TEST_F(YosysTestSuiteMacrosTest, GetLsbMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("macros.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("get_lsb", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "get_lsb must be defined";
  ASSERT_NE(macro->getTokens(), nullptr);
  ASSERT_FALSE(macro->getTokens()->empty());
  ASSERT_NE(macro->getTokens()->front(), nullptr);
  EXPECT_EQ(macro->getTokens()->front()->getStartColumn(), 27u) << "get_lsb body starts at column 27";
}

// LRM 22.5.1: sel_bits is a function-like macro (body at col 31).
TEST_F(YosysTestSuiteMacrosTest, SelBitsMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("macros.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("sel_bits", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "sel_bits must be defined";
  ASSERT_NE(macro->getTokens(), nullptr);
  ASSERT_FALSE(macro->getTokens()->empty());
  ASSERT_NE(macro->getTokens()->front(), nullptr);
  EXPECT_EQ(macro->getTokens()->front()->getStartColumn(), 31u) << "sel_bits body starts at column 31";
}

// LRM 22.5.1: get_msb and get_lsb are function-like (arg list `(' follows
// name immediately). The body start column (27) must not equal name start
// column (9) plus 10, which would indicate the arg-list `(' column was
// recorded as the body start.
TEST_F(YosysTestSuiteMacrosTest, GetMsbBodyNotArgListColumn) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("macros.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("get_msb", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getNameObj(), nullptr);
  ASSERT_NE(macro->getTokens(), nullptr);
  ASSERT_FALSE(macro->getTokens()->empty());
  ASSERT_NE(macro->getTokens()->front(), nullptr);
  EXPECT_NE(macro->getTokens()->front()->getStartColumn(), macro->getNameObj()->getStartColumn() + 10u)
      << "get_msb body column must not be nameStartColumn+10; "
         "that indicates the arg-list `(' column was recorded as the body start";
}

// LRM 22.5.1: MSB_LSB_SEP is object-like (no argument list).
TEST_F(YosysTestSuiteMacrosTest, MsbLsbSepHasNoArguments) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("macros.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("MSB_LSB_SEP", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_TRUE(macro->getArguments() == nullptr || macro->getArguments()->empty())
      << "MSB_LSB_SEP is object-like; getArguments() must be null or empty";
}

// LRM 22.5.1: get_msb and get_lsb each have two formal parameters (off, len).
TEST_F(YosysTestSuiteMacrosTest, GetMsbHasTwoArguments) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("macros.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("get_msb", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getArguments(), nullptr);
  EXPECT_EQ(macro->getArguments()->size(), 2u) << "get_msb has two formal parameters: off, len";
}

TEST_F(YosysTestSuiteMacrosTest, GetLsbHasTwoArguments) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("macros.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("get_lsb", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getArguments(), nullptr);
  EXPECT_EQ(macro->getArguments()->size(), 2u) << "get_lsb has two formal parameters: off, len";
}

// LRM 22.5.1: sel_bits has two formal parameters (offset, len).
TEST_F(YosysTestSuiteMacrosTest, SelBitsHasTwoArguments) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("macros.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("sel_bits", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getArguments(), nullptr);
  EXPECT_EQ(macro->getArguments()->size(), 2u) << "sel_bits has two formal parameters: offset, len";
}

TEST_F(YosysTestSuiteMacrosTest, GetMsbHasTokens) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("macros.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("get_msb", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getTokens(), nullptr);
  EXPECT_FALSE(macro->getTokens()->empty()) << "get_msb body must have tokens";
}

TEST_F(YosysTestSuiteMacrosTest, GetLsbHasTokens) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("macros.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("get_lsb", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getTokens(), nullptr);
  EXPECT_FALSE(macro->getTokens()->empty()) << "get_lsb body must have tokens";
}

TEST_F(YosysTestSuiteMacrosTest, SelBitsHasTokens) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("macros.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("sel_bits", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getTokens(), nullptr);
  EXPECT_FALSE(macro->getTokens()->empty()) << "sel_bits body must have tokens";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
