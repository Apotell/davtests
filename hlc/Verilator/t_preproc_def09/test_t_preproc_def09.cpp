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
class PreprocDef09Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "t_preproc_def09.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 22.5.1 (IEEE 1800-2012): default argument values in function-like macros.
// The file contains only macro definitions and test expressions; no module.
TEST_F(PreprocDef09Test, NoModules) {
  ASSERT_TRUE((m_design->getAllModules() == nullptr) || (m_design->getAllModules()->size() == 0u))
      << "t_preproc_def09 contains no module declarations";
}

// LRM 22.5.1: `define D(a=5, b=a) ... is a function-like macro with defaults.
// D must appear in the source file's macro table.
TEST_F(PreprocDef09Test, DMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_def09.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("D", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "D macro with default args must be defined";
}

// LRM 22.5.1: D has a replacement text body; getTokens() must be non-null.
TEST_F(PreprocDef09Test, DMacroHasBody) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_def09.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("D", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_NE(macro->getTokens(), nullptr) << "D must have a replacement text body (getTokens() must not be null)";
}

// LRM 22.5.1: `define MACRO1(a=1) ... must be defined.
TEST_F(PreprocDef09Test, Macro1Defined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_def09.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("MACRO1", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "MACRO1 with default arg must be defined";
}

// LRM 22.5.1: `define MACRO2(a=1, b=2) ... must be defined.
TEST_F(PreprocDef09Test, Macro2Defined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_def09.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("MACRO2", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "MACRO2 with two default args must be defined";
}

// LRM 22.5.1: `define MACRO3(a=1, b=2, c=3) ... must be defined.
TEST_F(PreprocDef09Test, Macro3Defined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("t_preproc_def09.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("MACRO3", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "MACRO3 with three default args must be defined";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
