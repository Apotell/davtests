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

// LRM 22.5.1: dut.sv itself defines no macros textually before the `line
// directive on line 4 remaps the reported file to "fake.v" -- `foo` and
// `new_line_num` (lines 6 and 8) are therefore recorded against the
// synthetic "fake.v" SourceFile (see FooMacro*/NewLineNumMacro* below), not
// directly against dut.sv.
TEST_F(PreprocLineTest, NoMacroDefinitionsInDut) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(sf, nullptr);
  EXPECT_EQ(sf->getPreprocMacroDefinitions(), nullptr)
      << "dut.sv defines no macros directly; `foo`/`new_line_num` are reported under fake.v after the "
         "`line directive remaps the file";
}

// The `line 101 "fake.v" 1 directive (dut.sv line 4) remaps the reported
// file for all subsequent text to "fake.v"; this synthetic SourceFile is
// recorded as a child of dut.sv's getIncludes(), alongside the (missing)
// line.vh include.
static const hldb::SourceFile *getFakeV(const hldb::Design *d) {
  const hldb::SourceFile *const sf = hldb::findByName<hldb::SourceFile>("dut.sv", d->getSourceFiles());
  if (sf == nullptr || sf->getIncludes() == nullptr) return nullptr;
  return hldb::findByName<hldb::SourceFile>("fake.v", sf->getIncludes());
}

TEST_F(PreprocLineTest, FakeVRecordedAfterLineDirective) {
  EXPECT_NE(getFakeV(m_design), nullptr) << "the `line 101 \"fake.v\" 1 directive must create a synthetic "
                                             "SourceFile named fake.v";
}

// ----
// 1. Macro arguments and body tokens
// ----

// LRM 22.5.1: `define foo(filename) ... is a function-like macro with one
// formal parameter (filename), reported under fake.v.
TEST_F(PreprocLineTest, FooMacroHasOneArgument) {
  const hldb::SourceFile *const fakeV = getFakeV(m_design);
  ASSERT_NE(fakeV, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("foo", fakeV->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "foo must be defined under fake.v";
  ASSERT_NE(macro->getArguments(), nullptr) << "foo is function-like; getArguments() must not be null";
  EXPECT_EQ(macro->getArguments()->size(), 1u) << "foo has exactly one formal parameter (filename)";
}

// LRM 22.5.1: `define foo(filename) `"filename.vh`" -- the body is not empty.
TEST_F(PreprocLineTest, FooMacroHasBodyTokens) {
  const hldb::SourceFile *const fakeV = getFakeV(m_design);
  ASSERT_NE(fakeV, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("foo", fakeV->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr);
  ASSERT_NE(macro->getTokens(), nullptr) << "foo has a replacement body; getTokens() must not be null";
  EXPECT_FALSE(macro->getTokens()->empty());
}

// LRM 22.5.1: `define new_line_num 200 is an object-like macro (no args),
// reported under fake.v.
TEST_F(PreprocLineTest, NewLineNumMacroHasNoArguments) {
  const hldb::SourceFile *const fakeV = getFakeV(m_design);
  ASSERT_NE(fakeV, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("new_line_num", fakeV->getPreprocMacroDefinitions());
  ASSERT_NE(macro, nullptr) << "new_line_num must be defined under fake.v";
  EXPECT_TRUE(macro->getArguments() == nullptr || macro->getArguments()->empty())
      << "new_line_num is object-like; getArguments() must be null or empty";
}

// LRM 22.5.1: `define new_line_num 200 -- body token '200' must be present.
TEST_F(PreprocLineTest, NewLineNumMacroHasBodyToken) {
  const hldb::SourceFile *const fakeV = getFakeV(m_design);
  ASSERT_NE(fakeV, nullptr);
  const hldb::PreprocMacroDefinition *const macro =
      hldb::findByName<hldb::PreprocMacroDefinition>("new_line_num", fakeV->getPreprocMacroDefinitions());
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
