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
class Sky130MacroSparecellFunctionalTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "sky130_fd_sc_hd__macro_sparecell.functional.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 22.5.1: the sky130 sparecell module must compile cleanly.
TEST_F(Sky130MacroSparecellFunctionalTest, ModuleCompiles) {
  const hldb::Module *const module =
      hldb::findByName<hldb::Module>("work@sky130_fd_sc_hd__macro_sparecell", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "sky130_fd_sc_hd__macro_sparecell module must compile";
}

// LRM 22.5.1 + 22.6: SKY130_FD_SC_HD__MACRO_SPARECELL_FUNCTIONAL_V is a
// flag include guard defined at the top of the file with no replacement text.
TEST_F(Sky130MacroSparecellFunctionalTest, IncludeGuardDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("sky130_fd_sc_hd__macro_sparecell.functional.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro = hldb::findByName<hldb::PreprocMacroDefinition>(
      "SKY130_FD_SC_HD__MACRO_SPARECELL_FUNCTIONAL_V", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "SKY130_FD_SC_HD__MACRO_SPARECELL_FUNCTIONAL_V include guard must be defined";
}

TEST_F(Sky130MacroSparecellFunctionalTest, IncludeGuardIsFlag) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("sky130_fd_sc_hd__macro_sparecell.functional.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro = hldb::findByName<hldb::PreprocMacroDefinition>(
      "SKY130_FD_SC_HD__MACRO_SPARECELL_FUNCTIONAL_V", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_TRUE(macro->getTokens() == nullptr || macro->getTokens()->empty())
      << "include guard must be a flag macro (no body tokens)";
}

// ---------------------------------------------------------------------------
// 1. PreprocMacroDefinition arguments and tokens
// ---------------------------------------------------------------------------

// LRM 22.5.1: the include guard is a flag macro -- no argument list, no body.
TEST_F(Sky130MacroSparecellFunctionalTest, IncludeGuardHasNoArguments) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("sky130_fd_sc_hd__macro_sparecell.functional.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro = hldb::findByName<hldb::PreprocMacroDefinition>(
      "SKY130_FD_SC_HD__MACRO_SPARECELL_FUNCTIONAL_V", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_EQ(macro->getArguments(), nullptr) << "include guard is a flag macro with no argument list";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
