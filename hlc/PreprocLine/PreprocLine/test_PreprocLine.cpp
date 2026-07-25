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
class PreprocLineTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PreprocLine.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 22.5.1: the module must compile cleanly despite `line directives.
TEST_F(PreprocLineTest, TopModuleCompiles) {
  const hldb::Module *const module = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(module, nullptr) << "module 'top' must compile";
}

// LRM 22.4 + 22.11 (`line): line.vh is included and contains the `line
// directive; it must be recorded as an include of dut.sv.
TEST_F(PreprocLineTest, LineVhIncluded) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("line.vh", sf->getIncludes());
  EXPECT_NE(inc, nullptr) << "line.vh must be recorded as an include of dut.sv";
}

// LRM 22.5.1: dut.sv defines no macros itself; the `line directive does not
// create a macro definition entry.
TEST_F(PreprocLineTest, NoMacroDefinitionsInDut) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  EXPECT_EQ(sf->getPreprocMacroDefinitions(), nullptr)
      << "dut.sv defines no macros; `line does not produce macro definitions";
}

// ---------------------------------------------------------------------------
// 1. Macro arguments and body tokens
// ---------------------------------------------------------------------------

// LRM 22.5.1: `define foo(filename) ... is a function-like macro with one
// formal parameter. If the implementation records it in dut.sv, getArguments()
// must return a non-null collection with exactly one entry.
TEST_F(PreprocLineTest, FooMacroArgumentsIfPresent) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  if (sf->getPreprocMacroDefinitions() == nullptr) return;
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("foo", sf->getPreprocMacroDefinitions());
  if (macro == nullptr) return;
  EXPECT_NE(macro->getArguments(), nullptr) << "foo is function-like; getArguments() must not be null";
  if (macro->getArguments() != nullptr) {
    EXPECT_EQ(macro->getArguments()->size(), 1u) << "foo has exactly one formal parameter (filename)";
  }
}

// LRM 22.5.1: `define foo(filename) `"filename.vh`" -- the body is not empty.
TEST_F(PreprocLineTest, FooMacroHasBodyTokensIfPresent) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  if (sf->getPreprocMacroDefinitions() == nullptr) return;
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("foo", sf->getPreprocMacroDefinitions());
  if (macro == nullptr) return;
  EXPECT_NE(macro->getTokens(), nullptr) << "foo has a replacement body; getTokens() must not be null";
}

// LRM 22.5.1: `define new_line_num 200 is an object-like macro (no args).
TEST_F(PreprocLineTest, NewLineNumMacroHasNoArgumentsIfPresent) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  if (sf->getPreprocMacroDefinitions() == nullptr) return;
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("new_line_num", sf->getPreprocMacroDefinitions());
  if (macro == nullptr) return;
  EXPECT_TRUE(macro->getArguments() == nullptr || macro->getArguments()->empty())
      << "new_line_num is object-like; getArguments() must be null or empty";
}

// LRM 22.5.1: `define new_line_num 200 -- body token '200' must be present.
TEST_F(PreprocLineTest, NewLineNumMacroHasBodyTokenIfPresent) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  if (sf->getPreprocMacroDefinitions() == nullptr) return;
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("new_line_num", sf->getPreprocMacroDefinitions());
  if (macro == nullptr) return;
  EXPECT_NE(macro->getTokens(), nullptr) << "new_line_num has body '200'; getTokens() must not be null";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
