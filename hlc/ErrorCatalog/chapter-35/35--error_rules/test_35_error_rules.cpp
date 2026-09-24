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

// Tests for the IEEE 1800-2023 Clause 35 (DPI) error scenarios catalogued in
// sv_error_catalog_Latest.xlsx / docs/error_catalog.xml (rows 1108, 1109,
// 1116, 1123, 1126).
//
// Scope, fixture layout and test shapes follow the Clause 3 file in this same
// suite; see hlc/ErrorCatalog/chapter-3 for the rationale.
//
// Behaviour observed while writing this file (hlc.exe -d db over the fixture):
// r1108_m compiles, and the import raises three syntax errors starting at
// 14:17. Those are NOT enforcement of 35.4 -- a control fixture importing with
// a perfectly legal escaped linkage name is rejected identically, while the
// same import written with a plain (non-escaped) identifier parses cleanly.
// HLC simply cannot parse an escaped identifier in the linkage-name position,
// legal or not, so the syntax errors are asserted nowhere below.
//
// Rows 1116 and 1126 were added later: both r1116_m's "pure function void"
// and r1126_m's "function void ...(ref int a)" compile with no diagnostic at
// all.

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

// --- row 1108: linkage names follow C identifier rules (35.4) ---------------

TEST_F(Chapter35ErrorRulesTest, Row1108_LinkageNameThatIsNotACIdentifierIsRejected) {
  // catalog row 1108 | 35.4 | COMP
  // "A global name shall follow C conventions for naming: the name shall start
  // with a letter or underscore, followed by letters, digits or underscores."
  // When the name is written as an escaped identifier the leading backslash
  // and trailing whitespace are stripped first, and the result shall still
  // comply. The import on line 14 strips to "init[1]", which does not.
  //
  // The escaped form is what makes this rule reachable at all -- the brackets
  // could not appear in a plain identifier -- and it is also what HLC cannot
  // parse today, so this row is blocked behind a grammar gap rather than
  // merely unchecked.
  GTEST_SKIP() << "HLC cannot parse an escaped identifier as a DPI linkage name at all -- a legal "
                  "escaped name is rejected the same way -- so the syntax errors on this line are a "
                  "grammar gap, not this rule. IEEE 1800-2023 35.4 requires the stripped linkage "
                  "name to be a valid C identifier";
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_DPI_DECLARATION, "init[1]", 14, 18), nullptr)
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

// --- row 1116: only nonvoid functions may be pure (35.5.2) ------------------

TEST_F(Chapter35ErrorRulesTest, Row1116_VoidImportedFunctionCannotBePure) {
  // catalog row 1116 | 35.5.2 | COMP
  // r1116_m's "import \"DPI-C\" pure function void r1116_f(input int i);" on
  // line 41 specifies pure on a void function.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_QUALIFIER, "r1116_f", 41, 3), nullptr)
      << "a void imported function cannot be specified pure (IEEE 1800-2023 35.5.2)";
}

// --- row 1126: ref cannot be used in an import declaration (35.5.4) ---------

TEST_F(Chapter35ErrorRulesTest, Row1126_RefQualifierIllegalOnDpiFormal) {
  // catalog row 1126 | 35.5.4 | COMP
  // r1126_m's "import \"DPI-C\" function void r1126_f(ref int a);" on line
  // 48 declares a ref formal on a DPI import.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_QUALIFIER, "a", 48, 48), nullptr)
      << "ref cannot be used on a DPI import declaration's formal argument (IEEE 1800-2023 35.5.4)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
