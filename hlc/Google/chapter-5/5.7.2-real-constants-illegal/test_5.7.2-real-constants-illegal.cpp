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

// Spec-based validation of illegal real literal constant forms per IEEE 1800-2023 Sec 5.7.2.
//
// Sec 5.7.2 rule under test:
//   "Real numbers expressed with a decimal point shall have at least one digit
//    on each side of the decimal point."
//
// SV source:
//   16: module top();
//   17:   logic [31:0] a;
//   18:
//   19:   initial begin;
//   20:     a = .12;      -- missing digit BEFORE decimal point
//   21:     a = 9.;       -- missing digit AFTER decimal point
//   22:     a = 4.E3;     -- missing digit AFTER decimal point (before exponent)
//   23:     a = .2e-7;    -- missing digit BEFORE decimal point
//   24:   end
//   25:
//   26: endmodule
//
// All four forms are listed as invalid in Sec 5.7.2.
//
// Checked:
//   - a syntax error on each of lines 20, 21, 22 and 23.
//
// Not checked:
//   - the columns of the errors, any further errors, and what the parser's
//     error recovery leaves behind (whether "top", its initial block and the
//     assignments survive, and the diagnostics that follow from it, e.g. "E3"
//     reported as an undefined name). IEEE 1800-2023 only requires the literals
//     to be rejected; the rest is an implementation detail that changes with
//     the grammar (recovery used to stop at the first literal and discard the
//     module, leaving unnamed module stubs; it now reports each literal and
//     keeps the module).

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class RealConstantsIllegal : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.7.2-real-constants-illegal.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ----
// Per IEEE 1800-2023 Sec 5.7.2, real literals without a digit on each side of
// the decimal point (.12, 9., 4.E3, .2e-7) are illegal and must be reported
// as syntax errors.
// ----
TEST_F(RealConstantsIllegal, Compiler_ReportsSyntaxErrors) {
  EXPECT_NE(findEvent(EventType::PaSyntaxError, 20), nullptr) << "'.12' is illegal (IEEE 1800-2023 Sec 5.7.2)";
  EXPECT_NE(findEvent(EventType::PaSyntaxError, 21), nullptr) << "'9.' is illegal (IEEE 1800-2023 Sec 5.7.2)";
  EXPECT_NE(findEvent(EventType::PaSyntaxError, 22), nullptr) << "'4.E3' is illegal (IEEE 1800-2023 Sec 5.7.2)";
  EXPECT_NE(findEvent(EventType::PaSyntaxError, 23), nullptr) << "'.2e-7' is illegal (IEEE 1800-2023 Sec 5.7.2)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
