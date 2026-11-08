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
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/generic_directive.h>
#include <hldb/hldb_vpi_user.h>
#include <hldb/identifier.h>
#include <hldb/module.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/source_file.h>

#include <string_view>

namespace hlc {
class PreprocLineTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PreprocLine.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// LRM 22.5.1: the module must compile cleanly despite `line directives.
TEST_F(PreprocLineTest, TopModuleCompiles) {
  const hldb::Module *const module = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
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

// LRM 22.12: `line only changes the file name and line number reported for the
// text after it; it does not include a file. Both `line directives of dut.sv
// (lines 4 and 9) are recorded, in source order, as directives of dut.sv
// itself -- no SourceFile is created for "fake.v".
static const hldb::SourceFile *getDut(const hldb::Design *d) {
  return hldb::findByName<hldb::SourceFile>("dut.sv", d->getSourceFiles());
}

TEST_F(PreprocLineTest, LineDirectivesRecordedOnDut) {
  const hldb::SourceFile *const sf = getDut(m_design);
  ASSERT_NE(sf, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::SourceFile>("fake.v", sf->getIncludes()), nullptr)
      << "22.12: `line does not include a file";
  ASSERT_NE(sf->getDirectives(), nullptr);
  ASSERT_EQ(sf->getDirectives()->size(), 2u);
  for (const hldb::Any *const directive : *sf->getDirectives()) {
    const hldb::GenericDirective *const gd = any_cast<hldb::GenericDirective>(directive);
    ASSERT_NE(gd, nullptr);
    EXPECT_EQ(gd->getDirectiveType(), vpiDirectiveTypeLine);
  }
}

// `line 101 "fake.v" 1 carries its arguments as written.
TEST_F(PreprocLineTest, FirstLineDirectiveValue) {
  const hldb::SourceFile *const sf = getDut(m_design);
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getDirectives(), nullptr);
  ASSERT_FALSE(sf->getDirectives()->empty());
  const hldb::GenericDirective *const gd = any_cast<hldb::GenericDirective>(sf->getDirectives()->front());
  ASSERT_NE(gd, nullptr);
  EXPECT_EQ(gd->getStartLine(), 4u);
  const hldb::Constant *const value = gd->getValue<hldb::Constant>();
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getValue(), std::string_view("101 \"fake.v\" 1"));
}

// ----
// 1. Macro arguments and body tokens
// ----

// The macros defined after `line 101 stay on dut.sv (no file is included) but
// carry the remapped file and line numbers (22.12): dut.sv line 5 is reported as
// fake.v:101, so `define foo (line 6) is at fake.v:102 and `define new_line_num
// (line 8) at fake.v:104.
static const hldb::PreprocMacroDefinition *getMacro(const hldb::Design *d, std::string_view name) {
  const hldb::SourceFile *const sf = getDut(d);
  if (sf == nullptr) return nullptr;
  return hldb::findByName<hldb::PreprocMacroDefinition>(name, sf->getPreprocMacroDefinitions());
}

TEST_F(PreprocLineTest, MacrosCarryRemappedLines) {
  const hldb::PreprocMacroDefinition *const foo = getMacro(m_design, "foo");
  ASSERT_NE(foo, nullptr);
  EXPECT_NE(foo->getFile().find("fake.v"), std::string_view::npos) << foo->getFile();
  EXPECT_EQ(foo->getStartLine(), 102u);
  const hldb::PreprocMacroDefinition *const newLineNum = getMacro(m_design, "new_line_num");
  ASSERT_NE(newLineNum, nullptr);
  EXPECT_NE(newLineNum->getFile().find("fake.v"), std::string_view::npos) << newLineNum->getFile();
  EXPECT_EQ(newLineNum->getStartLine(), 104u);
}

// LRM 22.5.1: `define foo(filename) ... is a function-like macro with one
// formal parameter (filename).
TEST_F(PreprocLineTest, FooMacroHasOneArgument) {
  const hldb::PreprocMacroDefinition *const macro = getMacro(m_design, "foo");
  ASSERT_NE(macro, nullptr) << "foo must be defined in dut.sv";
  ASSERT_NE(macro->getArguments(), nullptr) << "foo is function-like; getArguments() must not be null";
  EXPECT_EQ(macro->getArguments()->size(), 1u) << "foo has exactly one formal parameter (filename)";
}

// LRM 22.5.1: `define foo(filename) `"filename.vh`" -- the body is not empty.
TEST_F(PreprocLineTest, FooMacroHasBodyTokens) {
  const hldb::PreprocMacroDefinition *const macro = getMacro(m_design, "foo");
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getTokens(), nullptr) << "foo has a replacement body; getTokens() must not be null";
  EXPECT_FALSE(macro->getTokens()->empty());
}

// LRM 22.5.1: `define new_line_num 200 is an object-like macro (no args).
TEST_F(PreprocLineTest, NewLineNumMacroHasNoArguments) {
  const hldb::PreprocMacroDefinition *const macro = getMacro(m_design, "new_line_num");
  ASSERT_NE(macro, nullptr) << "new_line_num must be defined in dut.sv";
  EXPECT_TRUE(macro->getArguments() == nullptr || macro->getArguments()->empty())
      << "new_line_num is object-like; getArguments() must be null or empty";
}

// LRM 22.5.1: `define new_line_num 200 -- body token '200' must be present.
TEST_F(PreprocLineTest, NewLineNumMacroHasBodyToken) {
  const hldb::PreprocMacroDefinition *const macro = getMacro(m_design, "new_line_num");
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getTokens(), nullptr) << "new_line_num has body '200'; getTokens() must not be null";
  EXPECT_FALSE(macro->getTokens()->empty());
}

// ----
// 2. Missing include file
// ----

// `include `foo(line) (dut.sv line 7) expands to `include "line.vh"`, but
// no such file exists in this test's source directory. This is a genuine
// missing-file condition and must be reported as exactly one error
// (PP0101), not silently ignored or promoted to a fatal/syntax failure.
TEST_F(PreprocLineTest, MissingIncludeFileIsReportedAsError) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 1) << "including the nonexistent 'line.vh' must be reported as exactly one error";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
