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
//   17: module top();
//   18: int a = `VAR_1 + `VAR_2;   // VAR_1 and VAR_2 are NOT pre-defined
//   ...
//   27: endmodule
//
// The block comment carries `:defines: VAR_1=2 VAR_2=5` -- a test-framework
// hint that these macros should be pre-defined. This test's command line does
// not define them, so both usages on line 18 refer to undefined macros.
//
// Checked:
//   - the preprocessor reports each undefined macro, VAR_1 and VAR_2, on
//     line 18 (IEEE 1800-2023 Sec 22.5.1).
//
// Not checked:
//   - the syntax errors the unexpanded usages cause in the parser, and what
//     the parser's error recovery leaves behind (whether "top" and its body
//     survive). Those are implementation details that change with the grammar
//     (recovery used to discard the module and leave unnamed module stubs; it
//     now keeps the module).

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class CompilerDirectivesPreprocessorMacro1 : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.6.4--compiler-directives-preprocessor-macro_1.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ----
// Per IEEE 1800-2023 Sec 22.5.1, using an undefined macro (`VAR_1, `VAR_2) is
// an error.
// ----
TEST_F(CompilerDirectivesPreprocessorMacro1, Compiler_ReportsErrorForUndefinedMacros) {
  EXPECT_NE(findEvent(EventType::PpUnkownMacro, "VAR_1", 18), nullptr) << "`VAR_1 is not defined";
  EXPECT_NE(findEvent(EventType::PpUnkownMacro, "VAR_2", 18), nullptr) << "`VAR_2 is not defined";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
