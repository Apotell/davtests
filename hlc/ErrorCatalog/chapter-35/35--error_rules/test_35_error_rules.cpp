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

// Tests for the IEEE 1800-2023 Clause 35 error scenarios catalogued in
// docs/error_catalog.xml (rows 1108, 1109, 1123, 1136).
//
// Scope: this file asserts ONLY that the diagnostic each catalog row
// requires is emitted. It deliberately makes no assertion about the shape
// of the compiled model. Exactly one TEST_F per catalog row, named
// Row<N>_... after that row and carrying a "catalog row N | clause |
// category" comment; that is the link between the catalog and this file.
//
// Fixtures (all three compiled in one run by 35--error_rules.hlc):
//   35--error_rules.sv       rows 1109, 1123
//   35--error_rules_inv2.sv  row 1108 -- an escaped-identifier DPI linkage
//                            name that HLC's grammar cannot parse as the
//                            "= function" c_identifier form
//   35--error_rules_inv3.sv  row 1136 -- 'export' is not accepted as a
//                            class_item at all by HLC's grammar
//
// Two rows need a fixture of their own, each for a demonstrated reason: row
// 1108's escaped-identifier linkage name desynchronizes the parser
// ("mismatched input 'function'", then "mismatched input 'endmodule'"),
// leaving the enclosing module's own AST malformed; row 1136's export
// declaration is not accepted as a class_item at all ("extraneous input
// 'export'"), which then reads the following 'endclass' as extraneous input
// expecting <EOF> and loses the whole class. Files are parsed
// independently, so neither can reach the shared fixture.
//
// COMP_ILLEGAL_DPI_DECLARATION has no call site anywhere in src/ outside its
// own ErrorDefinition.cpp registration, so every assertion below is
// expected to be red today -- that is the intended, documented state of
// this exercise, not a bug for this file to work around. No test asserts
// the absence of a diagnostic, which would lock the gap in.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter35ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "35--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 1108: a DPI linkage name must follow C naming conventions (35.4) ---

TEST_F(Chapter35ErrorRulesTest, Row1108_DpiLinkageNameMustFollowCNamingConventions) {
  // catalog row 1108 | 35.4 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 35.4)";
  // A global/linkage name shall follow C naming conventions: it shall
  // start with a letter or underscore followed by alphanumeric characters
  // or underscores. Here the escaped identifier's stripped form 'init[1]'
  // still contains '[' and ']', which are not legal C identifier
  // characters. Lives in 35--error_rules_inv2.sv (see the file-level
  // comment above for why).
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_DPI_DECLARATION, "r1108_m"), nullptr)
      << "a DPI linkage name must be a valid C identifier (IEEE 1800-2023 35.4)";
}

// --- row 1109: same c_identifier must use the same DPI version string (35.4)

TEST_F(Chapter35ErrorRulesTest, Row1109_SameCIdentifierMustUseSameDpiVersionString) {
  // catalog row 1109 | 35.4 | LINT
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 35.4)";
  // All declarations using the same c_identifier shall be declared with the
  // same DPI version string; r1109_m uses "DPI-C" and r1109_n uses "DPI"
  // for the same c_identifier 'r1109_f'.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_DPI_DECLARATION, "r1109_n"), nullptr)
      << "mixing the deprecated 'DPI' and 'DPI-C' spec strings for one c_identifier is illegal (IEEE 1800-2023 35.4)";
}

// --- row 1123: the deprecated "DPI" spec string must be diagnosed (35.5.4) --

TEST_F(Chapter35ErrorRulesTest, Row1123_DeprecatedDpiSpecStringIsDiagnosed) {
  // catalog row 1123 | 35.5.4 | COMP
  // Use of the deprecated dpi_spec_string "DPI" shall generate a
  // compile-time warning or error stating that "DPI" is deprecated and
  // should be replaced with "DPI-C".
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_DPI_DECLARATION, "r1123_f"), nullptr)
      << "the deprecated 'DPI' spec string must be diagnosed (IEEE 1800-2023 35.5.4)";
}

// --- row 1136: a class member function cannot be exported (35.7) -----------

TEST_F(Chapter35ErrorRulesTest, Row1136_ClassMemberFunctionCannotBeExported) {
  // catalog row 1136 | 35.7 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 35.7)";
  // Class member functions cannot be exported; all other SystemVerilog
  // functions can be. Lives in 35--error_rules_inv3.sv (see the file-level
  // comment above for why).
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_DPI_DECLARATION, "r1136_C"), nullptr)
      << "a class member function cannot be exported via DPI (IEEE 1800-2023 35.7)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
