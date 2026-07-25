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
class PPMacroTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PPMacro.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 22.5.1: both modules that use FOO1 and FOO2 must compile cleanly.
TEST_F(PPMacroTest, Top1ModuleCompiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("work@top1", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 'top1' must compile";
}

TEST_F(PPMacroTest, Top2ModuleCompiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("work@top2", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 'top2' must compile";
}

// LRM 22.5.1: FOO1 and FOO2 are object-like macros (name at col 9, body
// at col 14 -- separated by a space and the 4-char name "FOO1"/"FOO2").
TEST_F(PPMacroTest, Foo1MacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("FOO1", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "FOO1 must be defined in dut.sv";
}

TEST_F(PPMacroTest, Foo1MacroHasBody) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("FOO1", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getNameObj(), nullptr);
  EXPECT_EQ(macro->getNameObj()->getStartColumn(), 9u) << "FOO1 name starts at column 9";
  ASSERT_NE(macro->getTokens(), nullptr);
  ASSERT_FALSE(macro->getTokens()->empty());
  ASSERT_NE(macro->getTokens()->front(), nullptr);
  EXPECT_EQ(macro->getTokens()->front()->getStartColumn(), 14u) << "FOO1 body starts at column 14 (after `FOO1 ')";
}

TEST_F(PPMacroTest, Foo2MacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("FOO2", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "FOO2 must be defined in dut.sv";
}

TEST_F(PPMacroTest, Foo2MacroHasBody) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("FOO2", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getNameObj(), nullptr);
  EXPECT_EQ(macro->getNameObj()->getStartColumn(), 9u) << "FOO2 name starts at column 9";
  ASSERT_NE(macro->getTokens(), nullptr);
  ASSERT_FALSE(macro->getTokens()->empty());
  ASSERT_NE(macro->getTokens()->front(), nullptr);
  EXPECT_EQ(macro->getTokens()->front()->getStartColumn(), 14u) << "FOO2 body starts at column 14";
}

// ---------------------------------------------------------------------------
// 1. Macro arguments and body tokens
// ---------------------------------------------------------------------------

// LRM 22.5.1: FOO1 and FOO2 are object-like macros (no argument list).
TEST_F(PPMacroTest, Foo1MacroHasNoArguments) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("FOO1", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_TRUE(macro->getArguments() == nullptr || macro->getArguments()->empty())
      << "FOO1 is object-like; getArguments() must be null or empty";
}

TEST_F(PPMacroTest, Foo1MacroHasOneToken) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("FOO1", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getTokens(), nullptr);
  EXPECT_FALSE(macro->getTokens()->empty()) << "FOO1 body must have at least one token ($clog2)";
}

TEST_F(PPMacroTest, Foo2MacroHasNoArguments) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("FOO2", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_TRUE(macro->getArguments() == nullptr || macro->getArguments()->empty())
      << "FOO2 is object-like; getArguments() must be null or empty";
}

TEST_F(PPMacroTest, Foo2MacroHasOneToken) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("FOO2", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getTokens(), nullptr);
  EXPECT_FALSE(macro->getTokens()->empty()) << "FOO2 body must have at least one token (42)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
