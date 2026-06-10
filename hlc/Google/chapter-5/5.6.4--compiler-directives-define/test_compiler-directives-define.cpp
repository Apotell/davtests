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
//   `define XXX 1          ← (1) define XXX
//   `ifdef XXX             ← true  (XXX is defined)
//     `undef XXX           ← (2) undef XXX
//   `elsif YYY             ← skipped (ifdef was taken)
//     `define XXX 0        ← skipped
//   `endif
//   `ifndef YYY            ← true  (YYY is not defined after undef)
//     `define YYY 0        ← (3) define YYY
//   `else                  ← skipped
//     `define XXX 0        ← skipped
//   `endif
//   `undefineall           ← (4) clear all macros
//   module d();
//   endmodule
//
// UHDM: SourceFile records exactly 4 PreprocMacroDefinition entries, in the
// order the preprocessor encountered them:
//   [0] name:"XXX", bodyStartColumn=13  (`define XXX 1)
//   [1] name:"XXX", bodyStartColumn=0   (`undef XXX — no body)
//   [2] name:"YYY", bodyStartColumn=13  (`define YYY 0)
//   [3] name:""  , bodyStartColumn=0   (`undefineall — no name)
//
// Distinguishing the three directive kinds via UHDM fields:
//   `define     → name is non-empty AND bodyStartColumn > 0
//   `undef      → name is non-empty AND bodyStartColumn == 0
//   `undefineall → name is empty

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/design.h>
#include <uhdm/module.h>
#include <uhdm/preproc_macro_definition.h>
#include <uhdm/source_file.h>

namespace SURELOG {

class CompilerDirectivesDefine : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.6.4--compiler-directives-define.hlc"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

static const uhdm::SourceFile *getSourceFile(const uhdm::Design *d) {
  if (!d->getSourceFiles() || d->getSourceFiles()->empty()) return nullptr;
  return (*d->getSourceFiles())[0];
}

static const uhdm::PreprocMacroDefinition *getMacro(const uhdm::Design *d,
                                                     std::size_t idx) {
  const uhdm::SourceFile *const sf = getSourceFile(d);
  if (!sf || !sf->getPreprocMacroDefinitions()) return nullptr;
  if (sf->getPreprocMacroDefinitions()->size() <= idx) return nullptr;
  return (*sf->getPreprocMacroDefinitions())[idx];
}

// ---------------------------------------------------------------------------
// Module
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesDefine, ModuleExists) {
  ASSERT_NE(
      uhdm::findByName<uhdm::Module>("work@d", m_design->getAllModules()),
      nullptr)
      << "module 'work@d' not found";
}

// ---------------------------------------------------------------------------
// SourceFile records exactly 4 preprocessor macro events
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesDefine, FourMacroEventsRecorded) {
  const uhdm::SourceFile *const sf = getSourceFile(m_design);
  ASSERT_NE(sf, nullptr);
  ASSERT_NE(sf->getPreprocMacroDefinitions(), nullptr);
  EXPECT_EQ(sf->getPreprocMacroDefinitions()->size(), 4u)
      << "expected 4 macro events (`define XXX, `undef XXX, `define YYY, "
         "`undefineall); skipped branches should not appear";
}

// ---------------------------------------------------------------------------
// Event 0: `define XXX 1  — name="XXX", has body
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesDefine, FirstEventIsDefineXXX) {
  const uhdm::PreprocMacroDefinition *const m = getMacro(m_design, 0);
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getName(), "XXX");
  EXPECT_GT(m->getBodyStartColumn(), 0u)
      << "`define has a body, so bodyStartColumn should be non-zero";
}

// ---------------------------------------------------------------------------
// Event 1: `undef XXX  — name="XXX", no body
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesDefine, SecondEventIsUndefXXX) {
  const uhdm::PreprocMacroDefinition *const m = getMacro(m_design, 1);
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getName(), "XXX");
  EXPECT_EQ(m->getBodyStartColumn(), 0u)
      << "`undef has no body, so bodyStartColumn should be zero";
}

// ---------------------------------------------------------------------------
// Event 2: `define YYY 0  — name="YYY", has body
// The `elsif and `else branches were skipped by the preprocessor, so only
// the taken `ifndef branch appears here.
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesDefine, ThirdEventIsDefineYYY) {
  const uhdm::PreprocMacroDefinition *const m = getMacro(m_design, 2);
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getName(), "YYY");
  EXPECT_GT(m->getBodyStartColumn(), 0u)
      << "`define has a body, so bodyStartColumn should be non-zero";
}

// ---------------------------------------------------------------------------
// Event 3: `undefineall  — empty name, no body
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesDefine, FourthEventIsUndefineall) {
  const uhdm::PreprocMacroDefinition *const m = getMacro(m_design, 3);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(m->getName().empty())
      << "`undefineall has no macro name, getName() should be empty";
  EXPECT_EQ(m->getBodyStartColumn(), 0u);
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
