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
class MacroWhitespaceSvTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "macro_whitespace_sv.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 22.5.1: `define FOO(a, b) -- no space before `(', so FOO is a
// function-like macro. Verify the macro is present in the compilation output.
TEST_F(MacroWhitespaceSvTest, FooMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("macro_whitespace.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("FOO", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "FOO macro must be defined";
}

// LRM 22.5.1: `define FOO(a, b) -- no space before `(', so FOO is a
// function-like macro. The `(a, b)' is the formal arg list; getArguments()
// must be non-null and contain exactly 2 formal parameters: a and b.
TEST_F(MacroWhitespaceSvTest, FooMacroArgListIsBody) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("macro_whitespace.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("FOO", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_NE(macro->getArguments(), nullptr) << "FOO arg list was not parsed -- getArguments() must not be null";
  EXPECT_EQ(macro->getArguments()->size(), 2u) << "FOO must have exactly 2 formal arguments: a, b";
}

// LRM 22.5.1: FOO has a replacement body `((a)+(b))'; getTokens() must be
// non-null.
TEST_F(MacroWhitespaceSvTest, FooMacroHasBody) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("macro_whitespace.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("FOO", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_NE(macro->getTokens(), nullptr) << "FOO must have a body (getTokens() must not be null)";
}

// LRM 22.5.1: getNameObj() must return a non-null Identifier with name "FOO".
TEST_F(MacroWhitespaceSvTest, FooMacroNameObj) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("macro_whitespace.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("FOO", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  const hldb::Identifier *const nameObj = macro->getNameObj();
  ASSERT_NE(nameObj, nullptr) << "getNameObj() must not be null";
  EXPECT_EQ(nameObj->getName(), "FOO") << "getNameObj() must have name FOO";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
