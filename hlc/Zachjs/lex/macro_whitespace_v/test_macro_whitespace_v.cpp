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
class MacroWhitespaceVTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "macro_whitespace_v.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// macro_whitespace.v is a thin wrapper that includes macro_whitespace.sv.
// LRM 22.4: macro_whitespace.sv must be recorded in the include list.
TEST_F(MacroWhitespaceVTest, IncludedFileRecorded) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("macro_whitespace.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("macro_whitespace.sv", sf->getIncludes());
  EXPECT_NE(inc, nullptr) << "macro_whitespace.sv must be recorded as an include";
}

// LRM 22.5.1: FOO must be defined inside the included file.
TEST_F(MacroWhitespaceVTest, FooMacroDefinedInInclude) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("macro_whitespace.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("macro_whitespace.sv", sf->getIncludes());
  ASSERT_NE(inc, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("FOO", inc->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "FOO macro must be defined inside the included macro_whitespace.sv";
}

// LRM 22.5.1: FOO is function-like (no space before `(' in macro_whitespace.sv);
// `(a, b)' is the formal arg list and must be parsed. getArguments() must be
// non-null and contain exactly 2 formal parameters: a and b.
TEST_F(MacroWhitespaceVTest, FooMacroArgListIsBody) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("macro_whitespace.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("macro_whitespace.sv", sf->getIncludes());
  ASSERT_NE(inc, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("FOO", inc->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_NE(macro->getArguments(), nullptr) << "FOO arg list was not parsed -- getArguments() must not be null";
  EXPECT_EQ(macro->getArguments()->size(), 2u) << "FOO must have exactly 2 formal arguments: a, b";
}

// LRM 22.5.1: FOO has a replacement body `((a)+(b))'; getTokens() must be
// non-null.
TEST_F(MacroWhitespaceVTest, FooMacroHasBody) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("macro_whitespace.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("macro_whitespace.sv", sf->getIncludes());
  ASSERT_NE(inc, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("FOO", inc->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  EXPECT_NE(macro->getTokens(), nullptr) << "FOO must have a body (getTokens() must not be null)";
}

// LRM 22.5.1: getNameObj() must return a non-null Identifier with name "FOO".
TEST_F(MacroWhitespaceVTest, FooMacroNameObj) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("macro_whitespace.v", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getIncludes(), nullptr);
  const hldb::SourceFile *const inc = hldb::findByName<hldb::SourceFile>("macro_whitespace.sv", sf->getIncludes());
  ASSERT_NE(inc, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("FOO", inc->getPreprocMacroDefinitions());
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
