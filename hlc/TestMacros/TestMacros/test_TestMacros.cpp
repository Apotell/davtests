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
class TestMacrosTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "TestMacros.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// The source file entry must still be recorded even when preprocessing fails.
TEST_F(TestMacrosTest, SourceFileRecorded) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("TestMacros.v", m_design->getSourceFiles());
  EXPECT_NE(sf, nullptr) << "TestMacros.v must be recorded as a source file despite the PP0111 error";
}

// ---------------------------------------------------------------------------
// 1. Macro arguments and body tokens
// ---------------------------------------------------------------------------

// LRM 22.5.1: MY_NUMBER is an object-like macro (`define MY_NUMBER 5).
// If it is recorded despite the PP0111 error, it must have no argument list
// and must have a non-empty body token list.
TEST_F(TestMacrosTest, MyNumberMacroIfPresent) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("TestMacros.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  if (sf->getPreprocMacroDefinitions() == nullptr) return;
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("MY_NUMBER", sf->getPreprocMacroDefinitions());
  if (macro == nullptr) return;
  EXPECT_TRUE(macro->getArguments() == nullptr || macro->getArguments()->empty())
      << "MY_NUMBER is object-like; getArguments() must be null or empty";
  EXPECT_NE(macro->getTokens(), nullptr) << "MY_NUMBER has body '5'; getTokens() must not be null";
}

// LRM 22.5.1: ADD5(RESULT, SOURCE) is a function-like macro with two formal
// parameters. If recorded, getArguments() must return exactly 2 entries.
TEST_F(TestMacrosTest, Add5MacroIfPresent) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("TestMacros.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  if (sf->getPreprocMacroDefinitions() == nullptr) return;
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("ADD5", sf->getPreprocMacroDefinitions());
  if (macro == nullptr) return;
  EXPECT_NE(macro->getArguments(), nullptr) << "ADD5 is function-like; getArguments() must not be null";
  if (macro->getArguments() != nullptr) {
    EXPECT_EQ(macro->getArguments()->size(), 2u) << "ADD5 has two formal parameters (RESULT, SOURCE)";
  }
  EXPECT_NE(macro->getTokens(), nullptr) << "ADD5 has a multi-line body; getTokens() must not be null";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
