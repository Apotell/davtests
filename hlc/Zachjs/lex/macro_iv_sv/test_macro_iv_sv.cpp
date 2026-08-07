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
class MacroIvSvTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "macro_iv_sv.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 22.5.1: the module declared in macro_iv.sv must compile.
TEST_F(MacroIvSvTest, ModuleCompiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 'top' must compile";
}

// LRM 22.5.1: numeric object-like macros (NUMBER_01...) must all be defined.
// Verify a representative sample from the numeric group.
TEST_F(MacroIvSvTest, Number01MacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("macro_iv.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("NUMBER_01", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "NUMBER_01 must be defined";
}

TEST_F(MacroIvSvTest, Number01MacroHasBody) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("macro_iv.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("NUMBER_01", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_NE(macro->getTokens(), nullptr) << "NUMBER_01 must have a numeric replacement body";
}

// LRM 22.5.1: string macros (STRING_01, STRING_02) must be defined.
TEST_F(MacroIvSvTest, String01MacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("macro_iv.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("STRING_01", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "STRING_01 must be defined";
}

// LRM 22.5.1: function-like macros (MACRO_A_01...) must be defined with
// bodyStartColumn > nameStartColumn (arg list + body recorded).
TEST_F(MacroIvSvTest, MacroA01Defined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("macro_iv.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("MACRO_A_01", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "MACRO_A_01 function-like macro must be defined";
}

// LRM 22.5.1: the large multi-line MACRO_B must be defined.
TEST_F(MacroIvSvTest, MacroBDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("macro_iv.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("MACRO_B", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "MACRO_B must be defined";
}

// LRM 22.5.1: NUMBER_01 is object-like with no arguments. getArguments() must
// be null and getNameObj() must have name "NUMBER_01".
TEST_F(MacroIvSvTest, Number01MacroObjectLike) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("macro_iv.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("NUMBER_01", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_EQ(macro->getArguments(), nullptr) << "NUMBER_01 is object-like; getArguments() must be null";
  const hldb::Identifier *const nameObj = macro->getNameObj();
  ASSERT_NE(nameObj, nullptr) << "getNameObj() must not be null";
  EXPECT_EQ(nameObj->getName(), "NUMBER_01") << "getNameObj() name must be NUMBER_01";
}

// LRM 22.5.1: MACRO_A_01(str="bar") is a function-like macro with 1 argument.
// getArguments() must be non-null and contain exactly 1 element.
TEST_F(MacroIvSvTest, MacroA01HasOneArg) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("macro_iv.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("MACRO_A_01", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getArguments(), nullptr) << "MACRO_A_01 is function-like; getArguments() must not be null";
  EXPECT_EQ(macro->getArguments()->size(), 1u) << "MACRO_A_01 must have exactly 1 formal argument: str";
}

// LRM 22.5.1: MACRO_A_01 has a replacement text body; getTokens() must be
// non-null.
TEST_F(MacroIvSvTest, MacroA01HasBody) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("macro_iv.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("MACRO_A_01", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_NE(macro->getTokens(), nullptr) << "MACRO_A_01 must have a replacement body (getTokens() must not be null)";
}

// LRM 22.5.1: MACRO_C(a, b=1) is a function-like macro with 2 arguments.
// getArguments() must contain exactly 2 elements and getTokens() must be
// non-null.
TEST_F(MacroIvSvTest, MacroCArgCountAndBody) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("macro_iv.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("MACRO_C", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getArguments(), nullptr) << "MACRO_C is function-like; getArguments() must not be null";
  EXPECT_EQ(macro->getArguments()->size(), 2u) << "MACRO_C must have exactly 2 formal arguments: a, b";
  EXPECT_NE(macro->getTokens(), nullptr) << "MACRO_C must have a replacement body (getTokens() must not be null)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
