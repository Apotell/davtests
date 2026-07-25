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
// LRM 22.5.1: the `(' immediately follows the macro name with no whitespace,
// making LONG_MACRO a function-like macro. The argument list spans to the
// closing `)' on the next physical line. Although the LRM requires `\' to
// extend a directive across lines, the `(' / `)' grouping is widely treated as
// sufficient to continue the argument list. LONG_MACRO(a, b, c) with no
// replacement text should therefore be a valid zero-body function-like macro.
class PreprocMultiLineArgListTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "preproc_test_9.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// The source file contains no module declaration; the design must have no modules.
TEST_F(PreprocMultiLineArgListTest, NoModules) {
  const hldb::ModuleCollection *const mods = m_design->getAllModules();
  EXPECT_TRUE(mods == nullptr || mods->empty()) << "no module declaration in source";
}

// LONG_MACRO must appear in the source file's macro table.
TEST_F(PreprocMultiLineArgListTest, LongMacroDefined) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_9.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("LONG_MACRO", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "LONG_MACRO must be defined";
}

// LONG_MACRO(a, b, c) has no replacement text. The argument list must be
// parsed correctly: getArguments() must be non-null and contain exactly the
// three formal parameters a, b, c.
TEST_F(PreprocMultiLineArgListTest, LongMacroArgListParsedNotTreatedAsBody) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf =
      hldb::findByName<hldb::SourceFile>("preproc_test_9.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("LONG_MACRO", sf->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  // The argument list must have been parsed: getArguments() is non-null and
  // contains the three formal parameters a, b, c.
  EXPECT_NE(macro->getArguments(), nullptr) << "LONG_MACRO arg list was not parsed -- getArguments() must not be null";
  EXPECT_EQ(macro->getArguments()->size(), 3u) << "LONG_MACRO must have exactly 3 formal arguments: a, b, c";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
