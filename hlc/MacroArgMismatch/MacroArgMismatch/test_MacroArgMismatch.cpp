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
#include <hldb/preproc_macro_definition.h>
#include <hldb/source_file.h>

namespace hlc {
class MacroArgMismatchTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "MacroArgMismatch.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 22.5.1: dut.sv defines the object-like macro D; the source file must
// be recorded in the design even when a PP0111 argument-mismatch error occurs.
TEST_F(MacroArgMismatchTest, SourceFileRecorded) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  EXPECT_NE(sf, nullptr) << "dut.sv must be recorded as a source file";
}

// LRM 22.5.1: `define D #1 -- D is an object-like macro and must be recorded
// as a macro definition in dut.sv.
TEST_F(MacroArgMismatchTest, DMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("D", sf->getPreprocMacroDefinitions());
  EXPECT_NE(macro, nullptr) << "macro 'D' must be defined in dut.sv";
}

// ---------------------------------------------------------------------------
// 1. Macro arguments and body tokens
// ---------------------------------------------------------------------------

// LRM 22.5.1: D is object-like; it must have no formal argument list.
TEST_F(MacroArgMismatchTest, DMacroHasNoArguments) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("D", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_TRUE(macro->getArguments() == nullptr || macro->getArguments()->empty())
      << "D is object-like; getArguments() must be null or empty";
}

// LRM 22.5.1: `define D #1 -- the body token '#1' must be present.
TEST_F(MacroArgMismatchTest, DMacroHasBodyTokens) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("D", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_NE(macro->getTokens(), nullptr) << "D has body '#1'; getTokens() must not be null";
}

// LRM 22.5.1: the name 'D' starts at column 9 in "`define D #1".
TEST_F(MacroArgMismatchTest, DMacroNameColumn) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("D", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getNameObj(), nullptr);
  EXPECT_EQ(macro->getNameObj()->getStartColumn(), 9u) << "D name starts at column 9 in `define D #1";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
