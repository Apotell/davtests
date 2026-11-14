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

// Validates that identifiers with illegal first characters are rejected.
//
// SV source:
//   16: module identifiers();
//   17:   reg $dollar;   -- '$' is not a legal first character (only legal mid-name)
//   18:   reg 0number;   -- digit is not a legal first character
//   19: endmodule
//
// Checked:
//   - a syntax error at each illegal identifier (17:6 and 18:6).
//
// Not checked:
//   - any further errors, and what the parser's error recovery leaves behind
//     (whether module "identifiers" survives, and what it declares). IEEE
//     1800-2023 only requires the identifiers to be rejected; the rest is an
//     implementation detail that changes with the grammar (recovery used to
//     discard the module and leave unnamed module stubs; it now keeps the
//     module).

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class WrongIdentifiers : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.6--wrong-identifiers.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ----
// Per IEEE 1800-2023 Sec 5.6 (simple_identifier grammar, Annex A.9.3), the
// first character of a simple identifier must be [a-zA-Z_]; '$' and digits
// are not legal leading characters. `reg $dollar;` and `reg 0number;` are
// both illegal and must be reported as syntax errors.
// ----
TEST_F(WrongIdentifiers, Compiler_ReportsSyntaxErrors) {
  EXPECT_NE(findEvent(EventType::PaSyntaxError, 17, 6), nullptr)
      << "illegal identifier '$dollar' (IEEE 1800-2023 Sec 5.6) must be reported as a syntax error";
  EXPECT_NE(findEvent(EventType::PaSyntaxError, 18, 6), nullptr)
      << "illegal identifier '0number' (IEEE 1800-2023 Sec 5.6) must be reported as a syntax error";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
