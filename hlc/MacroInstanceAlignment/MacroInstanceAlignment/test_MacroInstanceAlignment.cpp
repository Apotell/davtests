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
class MacroInstanceAlignmentTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "MacroInstanceAlignment.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 22.5.1: the design must compile the top-level module.
TEST_F(MacroInstanceAlignmentTest, ModuleCompiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("m", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 'm' must compile";
}

// LRM 22.5.1: arithmetic utility macros MIN and MAX must be defined.
TEST_F(MacroInstanceAlignmentTest, MinMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("MIN", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "MIN macro must be defined in dut.sv";
}

TEST_F(MacroInstanceAlignmentTest, MinMacroHasNoBody) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("MIN", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_TRUE(macro->getTokens() == nullptr || macro->getTokens()->empty())
      << "MIN has no replacement body (`define MIN(x, y) with nothing after)";
}

TEST_F(MacroInstanceAlignmentTest, MaxMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("MAX", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "MAX macro must be defined in dut.sv";
}

// LRM 22.5.1: ASSERT must be defined. It is a function-like macro; the
// argument list must be parsed (getArguments() non-null and non-empty).
TEST_F(MacroInstanceAlignmentTest, AssertMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("ASSERT", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "ASSERT macro must be defined in dut.sv";
}

TEST_F(MacroInstanceAlignmentTest, AssertMacroBodyNotArgListColumn) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("ASSERT", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_NE(macro->getArguments(), nullptr) << "ASSERT arg list was not parsed -- getArguments() must not be null";
  EXPECT_FALSE(macro->getArguments()->empty()) << "ASSERT arg list must not be empty";
}

// LRM 22.5.1: uvm_info and uvm_fatal are complex function-like macros that
// must be defined.
TEST_F(MacroInstanceAlignmentTest, UvmInfoMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("uvm_info", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "uvm_info macro must be defined";
}

TEST_F(MacroInstanceAlignmentTest, UvmFatalMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("uvm_fatal", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "uvm_fatal macro must be defined";
}

// LRM 22.5.1: ADD must be a function-like macro. Its arg list `(' immediately
// follows the name without a space, so it is function-like (not object-like).
TEST_F(MacroInstanceAlignmentTest, AddMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("ADD", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "ADD macro must be defined";
}

// LRM 22.5.1: ADD(a, b, c, d) is a function-like macro with 4 arguments and
// a replacement body. getArguments() must have size 4 and getTokens() must be
// non-null.
TEST_F(MacroInstanceAlignmentTest, AddMacroArgCountAndBody) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("ADD", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getArguments(), nullptr) << "ADD is function-like; getArguments() must not be null";
  EXPECT_EQ(macro->getArguments()->size(), 4u) << "ADD must have exactly 4 formal arguments: a, b, c, d";
  EXPECT_NE(macro->getTokens(), nullptr) << "ADD must have a replacement body (getTokens() must not be null)";
}

// LRM 22.5.1: MIN(x, y) is a function-like macro with 2 arguments and no
// replacement text. getArguments() must have size 2 and getTokens() must be
// null.
TEST_F(MacroInstanceAlignmentTest, MinMacroArgCount) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("MIN", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getArguments(), nullptr) << "MIN is function-like; getArguments() must not be null";
  EXPECT_EQ(macro->getArguments()->size(), 2u) << "MIN must have exactly 2 formal arguments: x, y";
}

// LRM 22.5.1: getNameObj() on MIN must return an Identifier with name "MIN".
TEST_F(MacroInstanceAlignmentTest, MinMacroNameObj) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("MIN", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  const hldb::Identifier *const nameObj = macro->getNameObj();
  ASSERT_NE(nameObj, nullptr) << "getNameObj() must not be null";
  EXPECT_EQ(nameObj->getName(), "MIN") << "getNameObj() name must be MIN";
}

// LRM 22.5.1: uvm_info(ID, MSG, VERBOSITY) is a function-like macro with 3
// arguments. getArguments() must have size 3.
TEST_F(MacroInstanceAlignmentTest, UvmInfoMacroArgCount) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("uvm_info", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getArguments(), nullptr) << "uvm_info is function-like; getArguments() must not be null";
  EXPECT_EQ(macro->getArguments()->size(), 3u) << "uvm_info must have exactly 3 formal arguments: ID, MSG, VERBOSITY";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
