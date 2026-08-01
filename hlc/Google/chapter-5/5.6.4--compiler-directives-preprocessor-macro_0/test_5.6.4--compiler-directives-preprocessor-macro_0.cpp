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

// Validates the behaviour when a required preprocessor macro is absent.
//
// SV source:
//   `ifdef TEST_VAR          <- TEST_VAR is NOT pre-defined at compile time
//   `else
//   TEST_VAR parsed not correctly from template   <- invalid SV, `else taken
//   `endif
//
// The block comment carries `:defines: TEST_VAR` -- a test-framework hint that
// TEST_VAR should be pre-defined.  When that define is absent (as it is here),
// `ifdef TEST_VAR is false, the `else branch is taken, and the bare text
// "TEST_VAR parsed not correctly from template" causes a syntax error.
//
// UHDM: no module nodes -- the file contains no module declaration at all,
// and the syntax error in the `else branch leaves no compilable content.
// The design contains only the SourceFile entry.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>
#include <hldb/module.h>

namespace hlc {

class CompilerDirectivesPreprocessorMacro0 : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.6.4--compiler-directives-preprocessor-macro_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ----
// Without TEST_VAR defined, the `else branch is taken and produces a syntax
// error.  The file has no module declaration, so no modules reach UHDM.
// ----
TEST_F(CompilerDirectivesPreprocessorMacro0, DesignHasNoModules) {
  EXPECT_TRUE(!m_design->getAllModules() || m_design->getAllModules()->empty())
      << "no modules should be present: the file has no module declaration "
         "and the `else branch produces a syntax error";
}

// ----
// The SourceFile entry is still created even when there are syntax errors.
// ----
TEST_F(CompilerDirectivesPreprocessorMacro0, DesignHasOneSourceFile) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  EXPECT_EQ(m_design->getSourceFiles()->size(), 1u);
}

// ----
// Per IEEE 1800-2023 Sec 22.5.4/22.6, the bare text taken by the `else
// branch ("TEST_VAR parsed not correctly from template") is not valid
// SystemVerilog and must be reported as a syntax error.
// ----
TEST_F(CompilerDirectivesPreprocessorMacro0, Compiler_ReportsSyntaxError) {
  const ErrorContainer::Stats stats = m_compiler->getErrorStats();
  EXPECT_GT(stats.nbSyntax, 0) << "the `else branch's invalid text must produce a syntax error";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
