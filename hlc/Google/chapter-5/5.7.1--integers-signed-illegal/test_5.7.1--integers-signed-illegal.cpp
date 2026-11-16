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

// Validates that an illegal negative decimal literal is rejected.
//
// SV source:
//   16: module top();
//   17:   logic  [7:0] a;
//   18:
//   19:   initial begin
//   20:     a = 8'd-6;   // ILLEGAL: minus must precede the literal: -8'd6
//   21:   end
//   22:
//   23: endmodule
//
// The '-' in '8'd-6' is not valid SV syntax; the token '8'd' is a size+base
// specifier and cannot be followed by a minus sign.  The correct negation
// form is '-8'd6' (unary minus applied to the whole sized constant).
//
// Checked:
//   - a syntax error on line 20.
//
// Not checked:
//   - the column of the error (which token the parser reports it at), the
//     number of errors, and what the parser's error recovery leaves behind
//     (whether "top" and its body survive). IEEE 1800-2023 only requires the
//     literal to be rejected; the rest is an implementation detail that
//     changes with the grammar (recovery used to discard the module and leave
//     unnamed module stubs; it now keeps the module).

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class IntegersSignedIllegal : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.7.1--integers-signed-illegal.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ----
// Per IEEE 1800-2023 Sec 5.7.1 / Annex A.8.7 (decimal_number grammar), the
// unary minus must precede the entire sized literal (-8'd6); '8'd-6' is not
// valid syntax and must be reported as a syntax error.
// ----
TEST_F(IntegersSignedIllegal, Compiler_ReportsSyntaxErrors) {
  EXPECT_NE(findEvent(EventType::PaSyntaxError, 20), nullptr)
      << "'8'd-6' is illegal (IEEE 1800-2023 Sec 5.7.1) and must be a syntax error";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
