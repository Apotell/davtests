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

// Tests for the IEEE 1800-2023 Clause 21 (Input/output system tasks) error
// scenarios catalogued in sv_error_catalog_Latest.xlsx (rows 754, 755).
//
// Scope, fixture layout and test shapes follow the Clause 3 file in this same
// suite; see hlc/ErrorCatalog/chapter-3 for the rationale.
//
// Behaviour observed while writing this file (hlc.exe -d db over the fixture):
// 21--error_rules.sv compiles with no errors, no warnings and no syntax errors
// at all; both modules are built. Neither rule is checked in HLC today.
//
// Both rules require looking inside a string literal argument of a
// display/write task, which is why both rows are catalogued COMP rather than
// PARSE: the literal is a single token to the parser, and its contents are
// only meaningful once the call's argument list is known.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class Chapter21ErrorRulesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "21--error_rules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// --- row 754: every specifier needs an argument (21.2.1) --------------------

TEST_F(Chapter21ErrorRulesTest, Row754_FormatSpecifierWithoutAnArgumentIsRejected) {
  // catalog row 754 | 21.2.1 | COMP
  // "For each % character that appears in a string literal argument, except
  // for %m, %l, and %%, a corresponding expression argument shall be supplied
  // after the string literal." The literal on line 13 carries two %d and only
  // one argument follows it. %m, %l and %% take no argument and must not be
  // counted -- that exemption is what separates this rule from a naive
  // percent-count.
  GTEST_SKIP() << "no diagnostic implemented; IEEE 1800-2023 21.2.1 requires one expression "
                  "argument per format specifier, excluding %m, %l and %%";
  EXPECT_NE(findError(ErrorDefinition::COMP_FORMAT_ARGUMENT_MISMATCH, "r754_m", 13, 11), nullptr)
      << "each format specifier needs a corresponding argument (IEEE 1800-2023 21.2.1)";
}

// --- row 755: undefined format specifiers (21.2.1.1) ------------------------

TEST_F(Chapter21ErrorRulesTest, Row755_UndefinedFormatSpecifierIsRejected) {
  // catalog row 755 | 21.2.1.1 | COMP
  // "It shall be an error if an undefined format specifier appears in a string
  // literal argument." Table 21-1 defines the set; %q is not in it. The
  // literal is on line 21.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_FORMAT_SPECIFIER, "r755_m", 21, 11), nullptr)
      << "%q is not a defined format specifier (IEEE 1800-2023 21.2.1.1, Table 21-1)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
