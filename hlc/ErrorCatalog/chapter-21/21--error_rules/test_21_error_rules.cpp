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
// scenarios catalogued in sv_error_catalog_Latest.xlsx / docs/error_catalog.xml
// (rows 754, 755, 762, 765, 779, 780, 781, 782, 786).
//
// Scope, fixture layout and test shapes follow the Clause 3 file in this same
// suite; see hlc/ErrorCatalog/chapter-3 for the rationale.
//
// Behaviour observed while writing this file (hlc.exe -d db over the fixture):
// 21--error_rules.sv compiles with no errors, no warnings and no syntax errors
// at all; both original modules are built. Neither of rows 754/755 is checked
// in HLC today.
//
// Both original rules require looking inside a string literal argument of a
// display/write task, which is why both rows are catalogued COMP rather than
// PARSE: the literal is a single token to the parser, and its contents are
// only meaningful once the call's argument list is known.
//
// Rows 762, 765, 779, 780, 781, 782 and 786 were merged in from a sibling
// snapshot of this same catalog file. Most are not yet implemented in HLC's
// Linter and are marked GTEST_SKIP() accordingly -- see each test's own
// comment.

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

// --- row 762: $fopen's type argument must be one of Table 21-6's forms (21.3.1) ---

TEST_F(Chapter21ErrorRulesTest, Row762_FopenTypeMustBeATable21_6Form) {
  // catalog row 762 | 21.3.1 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 21.3.1)";
  // The type argument of $fopen shall contain a character string of one of
  // the forms listed in Table 21-6 (r, rb, w, wb, a, ab, r+, ...). r762_m
  // calls "fd = $fopen("f.txt", "q");" -- "q" is not one of those forms.
  EXPECT_NE(findError(ErrorDefinition::COMP_VALUE_OUT_OF_RANGE, "r762_m"), nullptr)
      << "$fopen's type argument must be one of Table 21-6's forms (IEEE 1800-2023 21.3.1)";
}

// --- row 765: $sformat argument count must match its format specifiers (21.3.3) ---

TEST_F(Chapter21ErrorRulesTest, Row765_SformatArgumentCountMustMatchFormatSpecifiers) {
  // catalog row 765 | 21.3.3 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 21.3.3)";
  // If not enough arguments are supplied for the format specifiers in
  // $sformat's format_string, or too many are supplied, a warning shall be
  // issued (an implementation may instead issue a compile-time error).
  // r765_m calls "$sformat(s, "%d %d", 1);" -- one argument for two %d
  // specifiers.
  EXPECT_NE(findError(ErrorDefinition::COMP_FORMAT_ARGUMENT_MISMATCH, "r765_m"), nullptr)
      << "$sformat's argument count must match its format string's specifiers (IEEE 1800-2023 21.3.3)";
}

// --- row 779: $dumpvars cannot dump a part-select or expression (21.7.2) ----

TEST_F(Chapter21ErrorRulesTest, Row779_DumpvarsCannotDumpPartSelectOrExpression) {
  // catalog row 779 | 21.7.2 | COMP
  // The VCD format provides no mechanism to dump part of a vector or to dump
  // an expression; only whole variables and module scopes may be named as
  // $dumpvars arguments. r779_m calls "$dumpvars(0, v[8:15]);", a
  // part-select of v.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "v"), nullptr)
      << "$dumpvars can only be given whole variables or module scopes, not a part-select (IEEE 1800-2023 21.7.2)";
}

// --- row 780: $dumpports's scope_list must contain only module_identifiers (21.7.3.1) ---

TEST_F(Chapter21ErrorRulesTest, Row780_DumpportsScopeListMustContainOnlyModuleIdentifiers) {
  // catalog row 780 | 21.7.3.1 | COMP
  // The scope_list argument of $dumpports shall consist only of
  // module_identifiers; variables are not allowed. r780_m calls
  // "$dumpports(v, "ports.vcd");" where v is a plain logic variable.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "v"), nullptr)
      << "$dumpports's scope_list must contain only module_identifiers, not variables (IEEE 1800-2023 21.7.3.1)";
}

// --- row 781: $dumpports scope_list entries cannot be string literals (21.7.3.1) ---

TEST_F(Chapter21ErrorRulesTest, Row781_DumpportsScopeListEntryCannotBeStringLiteral) {
  // catalog row 781 | 21.7.3.1 | COMP
  // String literals are not allowed for the module_identifier entries of the
  // $dumpports scope_list. r781_m calls
  // "$dumpports("testbench.DUT", "ports.vcd");", naming the module scope as
  // a string literal.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r781_m"), nullptr)
      << "a $dumpports scope_list entry cannot be written as a string literal (IEEE 1800-2023 21.7.3.1)";
}

// --- row 782: an omitted $dumpports scope_list still requires a leading comma (21.7.3.1) ---

TEST_F(Chapter21ErrorRulesTest, Row782_OmittedDumpportsScopeListRequiresLeadingComma) {
  // catalog row 782 | 21.7.3.1 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 21.7.3.1)";
  // If the first argument of $dumpports is null, a comma shall still be used
  // before specifying the second argument. r782_m calls
  // "$dumpports("ports.vcd");" -- a well-formed one-argument call whose sole
  // argument is not a module scope, since the intended filename argument
  // requires the omitted first (scope_list) position to be written with its
  // leading comma.
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_EXPRESSION_CONTEXT, "r782_m"), nullptr)
      << "$dumpports's sole argument here is not a legal module scope (IEEE 1800-2023 21.7.3.1)";
}

// --- row 786: $dumpportslimit's filesize argument is required (21.7.3.4) ----

TEST_F(Chapter21ErrorRulesTest, Row786_DumpportslimitFilesizeArgumentIsRequired) {
  // catalog row 786 | 21.7.3.4 | COMP
  GTEST_SKIP() << "not yet implemented in HLC's Linter (IEEE 1800-2023 21.7.3.4)";
  // The filesize integer argument of $dumpportslimit is required; it may not
  // be omitted. r786_m calls "$dumpportslimit(, "ports.vcd");" with an
  // empty first argument.
  EXPECT_NE(findError(ErrorDefinition::COMP_MISSING_ARGUMENT, "r786_m"), nullptr)
      << "$dumpportslimit's filesize argument is required and may not be omitted (IEEE 1800-2023 21.7.3.4)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
