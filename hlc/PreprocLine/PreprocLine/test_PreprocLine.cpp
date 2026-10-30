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

#include <hlc/ErrorReporting/ErrorContainer.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/generic_directive.h>
#include <hldb/hldb_vpi_user.h>
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
  const hldb::Module *const module = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
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

// IEEE 1800-2023 Sec 22.12: `line 101 "fake.v" 1 (dut.sv line 4) only changes the file name
// and line number reported for the lines after it ("the line number of the next line": line
// 5 is fake.v:101). fake.v is not read or included, so dut.sv still owns everything written
// in it: `foo` (line 6, reported as fake.v:102) and `new_line_num` (line 8, fake.v:104).
static const hldb::SourceFile *getDutSv(const hldb::Design *d) {
  return hldb::findByName<hldb::SourceFile>("dut.sv", d->getSourceFiles());
}

static const hldb::PreprocMacroDefinition *findDutMacro(const hldb::Design *d, std::string_view name) {
  const hldb::SourceFile *const sf = getDutSv(d);
  return (sf == nullptr) ? nullptr
                         : hldb::findByName<hldb::PreprocMacroDefinition>(name, sf->getPreprocMacroDefinitions());
}

TEST_F(PreprocLineTest, DutOwnsItsMacros) {
  const hldb::SourceFile *const sf = getDutSv(m_design);
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getPreprocMacroDefinitions(), nullptr);
  EXPECT_EQ(sf->getPreprocMacroDefinitions()->size(), 2u) << "dut.sv defines foo and new_line_num";
  EXPECT_NE(findDutMacro(m_design, "foo"), nullptr);
  EXPECT_NE(findDutMacro(m_design, "new_line_num"), nullptr);
}

TEST_F(PreprocLineTest, LineFileNameIsNotASourceFile) {
  const hldb::SourceFile *const sf = getDutSv(m_design);
  ASSERT_NE(sf, nullptr);
  if (sf->getIncludes() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::SourceFile>("fake.v", sf->getIncludes()), nullptr)
        << "fake.v is a reported name, not an included file";
  }
  EXPECT_EQ(hldb::findByName<hldb::SourceFile>("fake.v", m_design->getSourceFiles()), nullptr);
}

TEST_F(PreprocLineTest, LineDirectiveIsRecordedOnDut) {
  // Sec 22.1, 22.12: the `line is a compiler directive on the SourceFile it is written in,
  // at its own place (dut.sv line 4).
  const hldb::SourceFile *const sf = getDutSv(m_design);
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getDirectives(), nullptr);
  ASSERT_FALSE(sf->getDirectives()->empty());
  const hldb::GenericDirective *const gd = any_cast<hldb::GenericDirective>(sf->getDirectives()->front());
  ASSERT_NE(gd, nullptr);
  EXPECT_EQ(gd->getDirectiveType(), vpiDirectiveTypeLine);
  EXPECT_NE(gd->getFile().find("dut.sv"), std::string_view::npos) << gd->getFile();
  EXPECT_EQ(gd->getStartLine(), 4u);
}

TEST_F(PreprocLineTest, MacrosAfterTheLineAreReportedInFakeV) {
  const hldb::PreprocMacroDefinition *const foo = findDutMacro(m_design, "foo");
  const hldb::PreprocMacroDefinition *const num = findDutMacro(m_design, "new_line_num");
  ASSERT_NE(foo, nullptr);
  ASSERT_NE(num, nullptr);
  EXPECT_NE(foo->getFile().find("fake.v"), std::string_view::npos) << foo->getFile();
  EXPECT_EQ(foo->getStartLine(), 102u) << "dut.sv line 6 is fake.v:102";
  EXPECT_NE(num->getFile().find("fake.v"), std::string_view::npos) << num->getFile();
  EXPECT_EQ(num->getStartLine(), 104u) << "dut.sv line 8 is fake.v:104";
}

// ----
// 1. Macro arguments and body tokens
// ----

// LRM 22.5.1: `define foo(filename) ... is a function-like macro with one
// formal parameter (filename).
TEST_F(PreprocLineTest, FooMacroHasOneArgument) {
  const hldb::PreprocMacroDefinition *const macro = findDutMacro(m_design, "foo");
  ASSERT_NE(macro, nullptr) << "foo must be defined in dut.sv";
  ASSERT_NE(macro->getArguments(), nullptr) << "foo is function-like; getArguments() must not be null";
  EXPECT_EQ(macro->getArguments()->size(), 1u) << "foo has exactly one formal parameter (filename)";
}

// LRM 22.5.1: `define foo(filename) `"filename.vh`" -- the body is not empty.
TEST_F(PreprocLineTest, FooMacroHasBodyTokens) {
  const hldb::PreprocMacroDefinition *const macro = findDutMacro(m_design, "foo");
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getTokens(), nullptr) << "foo has a replacement body; getTokens() must not be null";
  EXPECT_FALSE(macro->getTokens()->empty());
}

// LRM 22.5.1: `define new_line_num 200 is an object-like macro (no args).
TEST_F(PreprocLineTest, NewLineNumMacroHasNoArguments) {
  const hldb::PreprocMacroDefinition *const macro = findDutMacro(m_design, "new_line_num");
  ASSERT_NE(macro, nullptr) << "new_line_num must be defined in dut.sv";
  EXPECT_TRUE(macro->getArguments() == nullptr || macro->getArguments()->empty())
      << "new_line_num is object-like; getArguments() must be null or empty";
}

// LRM 22.5.1: `define new_line_num 200 -- body token '200' must be present.
TEST_F(PreprocLineTest, NewLineNumMacroHasBodyToken) {
  const hldb::PreprocMacroDefinition *const macro = findDutMacro(m_design, "new_line_num");
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
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 1) << "including the nonexistent 'line.vh' must be reported as exactly one error";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
