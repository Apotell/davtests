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

// Validates that `define, `undef, `ifdef/`ifndef/`elsif/`else/`endif, and
// `undefineall are all recognised, and that the preprocessor correctly selects
// branches and records macro events on the SourceFile.
//
// SV source (preprocessor logic):
//   `define XXX 1          // (1) define XXX
//   `ifdef XXX             // true  (XXX is defined)
//     `undef XXX           // (2) undef XXX
//   `elsif YYY             // skipped (ifdef was taken)
//     `define XXX 0        // skipped
//   `endif
//   `ifndef YYY            // true  (YYY is not defined after undef)
//     `define YYY 0        // (3) define YYY
//   `else                  // skipped
//     `define XXX 0        // skipped
//   `endif
//   `undefineall           // (4) clear all macros
//   module d();
//   endmodule
//
// UHDM: SourceFile records exactly 4 PreprocMacroDefinition entries, in the
// order the preprocessor encountered them:
//   [0] name:"XXX", has body tokens  (`define XXX 1)
//   [1] name:"XXX", no body tokens   (`undef XXX -- no body)
//   [2] name:"YYY", has body tokens  (`define YYY 0)
//   [3] name:""  , no body tokens    (`undefineall -- no name)
//
// Distinguishing the three directive kinds via getTokens():
//   `define      -- name is non-empty AND getTokens() != nullptr
//   `undef       -- name is non-empty AND getTokens() == nullptr
//   `undefineall -- name is empty

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/source_file.h>

namespace hlc {

class CompilerDirectivesDefine : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.6.4--compiler-directives-define.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::SourceFile *getSourceFile(const hldb::Design *d) {
  if (!d->getSourceFiles() || d->getSourceFiles()->empty()) return nullptr;
  return (*d->getSourceFiles())[0];
}

static const hldb::PreprocMacroDefinition *getMacro(const hldb::Design *d, std::size_t idx) {
  const hldb::SourceFile *const sf = getSourceFile(d);
  if (!sf || !sf->getPreprocMacroDefinitions()) return nullptr;
  if (sf->getPreprocMacroDefinitions()->size() <= idx) return nullptr;
  return (*sf->getPreprocMacroDefinitions())[idx];
}

// ---------------------------------------------------------------------------
// Module
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesDefine, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("d", m_design->getAllModules()), nullptr)
      << "module 'd' not found";
}

// ---------------------------------------------------------------------------
// SourceFile records exactly 4 preprocessor macro events
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesDefine, FourMacroEventsRecorded) {
  const hldb::SourceFile *const sf = getSourceFile(m_design);
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getPreprocMacroDefinitions(), nullptr);
  EXPECT_EQ(sf->getPreprocMacroDefinitions()->size(), 4u)
      << "expected 4 macro events (`define XXX, `undef XXX, `define YYY, "
         "`undefineall); skipped branches should not appear";
}

// ---------------------------------------------------------------------------
// Event 0: `define XXX 1  -- name="XXX", has body
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesDefine, FirstEventIsDefineXXX) {
  const hldb::PreprocMacroDefinition *const m = getMacro(m_design, 0);
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getName(), "XXX");
  EXPECT_NE(m->getTokens(), nullptr) << "`define has a body, so getTokens() should be non-null";
}

// ---------------------------------------------------------------------------
// Event 1: `undef XXX  -- name="XXX", no body
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesDefine, SecondEventIsUndefXXX) {
  const hldb::PreprocMacroDefinition *const m = getMacro(m_design, 1);
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getName(), "XXX");
  EXPECT_TRUE(m->getTokens() == nullptr || m->getTokens()->empty())
      << "`undef has no body, so getTokens() should be null or empty";
}

// ---------------------------------------------------------------------------
// Event 2: `define YYY 0  -- name="YYY", has body
// The `elsif and `else branches were skipped by the preprocessor, so only
// the taken `ifndef branch appears here.
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesDefine, ThirdEventIsDefineYYY) {
  const hldb::PreprocMacroDefinition *const m = getMacro(m_design, 2);
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getName(), "YYY");
  EXPECT_NE(m->getTokens(), nullptr) << "`define has a body, so getTokens() should be non-null";
}

// ---------------------------------------------------------------------------
// Event 3: `undefineall  -- empty name, no body
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesDefine, FourthEventIsUndefineall) {
  const hldb::PreprocMacroDefinition *const m = getMacro(m_design, 3);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(m->getName().empty()) << "`undefineall has no macro name, getName() should be empty";
  EXPECT_TRUE(m->getTokens() == nullptr || m->getTokens()->empty())
      << "`undefineall has no body, so getTokens() should be null or empty";
}

// ---------------------------------------------------------------------------
// 5. PreprocMacroDefinition arguments and tokens
// ---------------------------------------------------------------------------

// LRM 22.5.1: `define XXX 1 and `define YYY 0 are simple object-like macros
// with no argument list. getArguments() must return null for both.
TEST_F(CompilerDirectivesDefine, DefineXXXHasNoArguments) {
  const hldb::PreprocMacroDefinition *const m = getMacro(m_design, 0);
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getArguments(), nullptr) << "`define XXX 1 has no argument list";
}

TEST_F(CompilerDirectivesDefine, DefineYYYHasNoArguments) {
  const hldb::PreprocMacroDefinition *const m = getMacro(m_design, 2);
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getArguments(), nullptr) << "`define YYY 0 has no argument list";
}

// LRM 22.5.1: `define XXX 1 and `define YYY 0 have a replacement body.
// getTokens() must return a non-null collection.
TEST_F(CompilerDirectivesDefine, DefineXXXHasTokens) {
  const hldb::PreprocMacroDefinition *const m = getMacro(m_design, 0);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(m->getTokens(), nullptr) << "`define XXX 1 must have body tokens";
}

TEST_F(CompilerDirectivesDefine, DefineYYYHasTokens) {
  const hldb::PreprocMacroDefinition *const m = getMacro(m_design, 2);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(m->getTokens(), nullptr) << "`define YYY 0 must have body tokens";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
