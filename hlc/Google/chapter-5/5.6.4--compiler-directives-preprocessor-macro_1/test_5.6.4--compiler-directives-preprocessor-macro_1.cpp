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

// Validates the behaviour when macros used in module body expressions are
// undefined at compile time.
//
// SV source:
//   module top();
//   int a = `VAR_1 + `VAR_2;   // VAR_1 and VAR_2 are NOT pre-defined
//   ...
//   endmodule
//
// The block comment carries `:defines: VAR_1=2 VAR_2=5` — a test-framework
// hint that these macros should be pre-defined.  Without them, Surelog
// substitutes `VAR_1 → "SURELOG_MACRO_NOT_DEFINED:VAR_1!!!" which produces
// syntax errors and breaks the module parse.
//
// UHDM: 2 nameless stub modules (parse fragments) — the module name "top"
// was never captured due to the parse failure on line 18.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>

namespace hlc {

class CompilerDirectivesPreprocessorMacro1 : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.6.4--compiler-directives-preprocessor-macro_1.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ---------------------------------------------------------------------------
// The module name "top" was never captured — the syntax error on the `int a`
// line broke the parse before the module body could be resolved.
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesPreprocessorMacro1, NoModuleNamedTop) {
  EXPECT_EQ(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr)
      << "'work@top' should not exist — parse failure swallowed the module name";
}

// ---------------------------------------------------------------------------
// UHDM contains 2 nameless stub modules produced by the broken parse.
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesPreprocessorMacro1, TwoStubModulesExist) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  EXPECT_EQ(m_design->getAllModules()->size(), 2u) << "broken parse should produce exactly 2 nameless stub modules";
}

TEST_F(CompilerDirectivesPreprocessorMacro1, StubModulesHaveNoName) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  for (const hldb::Module *const m : *m_design->getAllModules()) {
    EXPECT_TRUE(m->getName().empty()) << "stub module should have an empty name, got: " << m->getName();
  }
}

// ---------------------------------------------------------------------------
// Neither stub module contains nets — the broken variable declaration was
// not added to any module scope.
// ---------------------------------------------------------------------------
TEST_F(CompilerDirectivesPreprocessorMacro1, NoNetsInStubModules) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  for (const hldb::Module *const m : *m_design->getAllModules()) {
    EXPECT_TRUE(!m->getNets() || m->getNets()->empty()) << "stub module should have no nets";
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
