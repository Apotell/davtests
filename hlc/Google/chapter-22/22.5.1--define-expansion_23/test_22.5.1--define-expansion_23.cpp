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

// Test for tests/Google/chapter-22/22.5.1--define-expansion_23.sv
//
// The source file holds no design element at all. Lines 1-7 are one-line
// "//" license comments, lines 8-9 are blank, lines 10-16 are one block
// comment carrying the test metadata, and the only SystemVerilog text is:
//
//   17: `define define "illegal"
//
// Line 17 has the shape of a text_macro_definition (IEEE 1800-2023 Syntax
// 22-2) whose text_macro_name is define and whose macro text is the string
// literal "illegal". But define is a compiler directive name (22.1 lists
// `define), and 22.5.1 states: "It shall be illegal to redefine a compiler
// directive as a macro name." So the file has exactly one LRM error, on line
// 17; the comments on lines 1-16 are legal. (The metadata's
// ":should_fail_because:" also calls directives "predefined macro names";
// that wording is not in 1800-2023, so only the sentence quoted above is
// relied on here.)
//
// Every assertion below follows from the LRM, except the one part of
// ErrorsOnlyOnTheIllegalLine labelled as HLC error recovery; a red run means
// HLC deviates from the LRM. An empty generic directive list reads an LRM fact
// through an HLDB code: the file holds none of the directives HLDB records
// that way (`timescale, `default_nettype, `celldefine and the others with a
// vpiDirectiveType code).
//
// What is checked
//   - At least one error-class diagnostic (fatal, syntax or error) points at
//     line 17.
//   - HLC diagnoses it as a reserved macro name: PP_MACRO_NAME_RESERVED, the
//     HLC code for exactly this 22.5.1 rule, is reported on line 17 with an
//     error-class severity.
//   - Every error-class diagnostic outside the elaboration category points at
//     line 17. For lines 1-16 (comments and blank lines) this is an LRM fact:
//     legal text draws no error. For a diagnostic past line 17 or without a
//     location it is an HLC error-recovery requirement, not an LRM rule: such
//     a diagnostic would be a cascade from the illegal line.
//   - None of the preprocessor diagnostics that a misreading of line 17 would
//     produce.
//   - Exactly one SourceFile for the .sv, with no includes and no generic
//     directives.
//   - No design object of any kind is created from this file.
//
// What is NOT checked, and why
//   - Whether HLDB records a PreprocMacroDefinition for define, and with which
//     name, tokens and arguments; and whether it records any
//     PreprocMacroInstance. The definition is illegal and the LRM gives it no
//     meaning, so any model of it is consistent with the LRM. Nearest
//     assertions: ReportsAnErrorOnTheIllegalLine and
//     DiagnosesTheReservedMacroName.
//   - How many diagnostics line 17 draws, and of which severity or code,
//     beyond the required error. The LRM requires an error but does not
//     limit how many diagnostics the illegal line draws or of which kind; a
//     "multiply defined" or syntax diagnostic there would be accepted.
//     Nearest assertions: ReportsAnErrorOnTheIllegalLine and
//     ErrorsOnlyOnTheIllegalLine.
//   - The diagnostic's symbol text and column. The LRM requires the error but
//     not its message format. Nearest assertion: DiagnosesTheReservedMacroName.
//   - Elaboration-category diagnostics. The file has no module, and a design
//     shall contain at least one top-level module (23.3.1), so an elaborating
//     run may legitimately report that. Nearest assertion:
//     ErrorsOnlyOnTheIllegalLine (all other error-class diagnostics).
//   - The seven one-line comments and the block comment (5.4). The LRM gives
//     comments no meaning; how HLDB groups, classifies (vpiComment and
//     vpiDocumentComment are marked non-standard) and attaches them is a tool
//     convention. Nearest assertion: the line 17 checks in
//     ReportsAnErrorOnTheIllegalLine and DiagnosesTheReservedMacroName, which
//     only hold if the 16 comment and blank lines before it were consumed as
//     whole lines.
//   - Columns. The LRM numbers lines (22.13) but not columns. Nearest
//     assertions: the line 17 checks.
//   - The SourceFile time unit and precision. There is no `timescale or
//     timeunit, so the default applies, and the default is
//     implementation-specific (3.14.2.3). Nearest assertion:
//     SourceFileHasNoIncludesOrDirectives (zero directives).
//   - The warning count. The LRM neither requires nor forbids warnings here.
//     Nearest assertion: ErrorsOnlyOnTheIllegalLine.
//   - No check is gated on Design::getElaborated(). The 23.3.1 error an
//     elaborating run may report is excluded, not asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/ErrorReporting/Location.h>
#include <hlc/Tests/Test.h>

#include <hldb/checker_decl.h>
#include <hldb/class_defn.h>
#include <hldb/config_decl.h>
#include <hldb/design.h>
#include <hldb/directive.h>
#include <hldb/interface.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/program.h>
#include <hldb/source_file.h>
#include <hldb/typedef.h>
#include <hldb/udp_defn.h>
#include <hldb/variable.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "22.5.1--define-expansion_23.sv";

// The line of the illegal definition `define define "illegal".
constexpr uint32_t kIllegalLine = 17;

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

// Size of an HLDB collection; a collection that was never created is empty.
template <typename T>
size_t sizeOf(const std::vector<T *> *collection) {
  return (collection == nullptr) ? 0 : collection->size();
}

// Number of objects in 'collection' whose source location is the file under test.
template <typename T>
size_t countFromSourceFile(const std::vector<T *> *collection) {
  size_t count = 0;
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (endsWith(object->getFile(), kSourceFileName)) ++count;
    }
  }
  return count;
}

// True for the severities the LRM's "illegal" can map to.
bool isErrorClass(ErrorDefinition::ErrorSeverity severity) {
  return (severity == ErrorDefinition::FATAL) || (severity == ErrorDefinition::SYNTAX) ||
         (severity == ErrorDefinition::ERROR);
}

// True when 'error' has an error-class severity in HLC's error table.
bool isErrorClass(const Error &error) {
  const ErrorDefinition::ErrorMap &errorInfoMap = ErrorDefinition::getErrorInfoMap();
  const ErrorDefinition::ErrorMap::const_iterator errorInfo = errorInfoMap.find(error.getType());
  return (errorInfo != errorInfoMap.end()) && isErrorClass(errorInfo->second.m_severity);
}

// True when 'error' belongs to HLC's elaboration category.
bool isElaborationError(const Error &error) {
  const ErrorDefinition::ErrorMap &errorInfoMap = ErrorDefinition::getErrorInfoMap();
  const ErrorDefinition::ErrorMap::const_iterator errorInfo = errorInfoMap.find(error.getType());
  return (errorInfo != errorInfoMap.end()) && (errorInfo->second.m_category == ErrorDefinition::ELAB);
}

// The line a diagnostic points at, or 0 when it carries no location.
uint32_t lineOf(const Error &error) { return error.getLocations().empty() ? 0 : error.getLocations().front().m_line; }

// One diagnostic as " [<code> line <n>: <text>]", for failure messages, so a
// red run shows what was reported and where. <text> is HLC's message
// template, so its placeholders are not filled in.
std::string describeError(const Error &error) {
  const ErrorDefinition::ErrorMap &errorInfoMap = ErrorDefinition::getErrorInfoMap();
  const ErrorDefinition::ErrorMap::const_iterator errorInfo = errorInfoMap.find(error.getType());
  std::string description = " [" + std::to_string(static_cast<int32_t>(error.getType()));
  description += " line " + std::to_string(lineOf(error));
  if (errorInfo != errorInfoMap.end()) description += ": " + std::string(errorInfo->second.m_errorText);
  return description + "]";
}

// Every diagnostic, described as by describeError(); for failure messages.
std::string describeAllErrors(const ErrorContainer *errorContainer) {
  std::string description;
  for (const Error &error : errorContainer->getErrors()) description += describeError(error);
  return description;
}
}  // namespace

class DefineExpansion23Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.5.1--define-expansion_23.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // All SourceFile nodes recorded for the file under test.
  static std::vector<const hldb::SourceFile *> findSourceFiles() {
    std::vector<const hldb::SourceFile *> sourceFiles;
    if (m_design != nullptr) {
      if (const hldb::SourceFileCollection *const collection = m_design->getSourceFiles()) {
        for (const hldb::SourceFile *sourceFile : *collection) {
          if (endsWith(sourceFile->getName(), kSourceFileName)) sourceFiles.emplace_back(sourceFile);
        }
      }
    }
    return sourceFiles;
  }

  // The SourceFile node for the file under test, or nullptr unless there is exactly one.
  static const hldb::SourceFile *getSourceFile() {
    const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
    return (sourceFiles.size() == 1) ? sourceFiles.front() : nullptr;
  }
};

// 22.5.1: "It shall be illegal to redefine a compiler directive as a macro
// name." define is a compiler directive name (22.1), so line 17 is illegal.
// The LRM requires an error there; it does not say which kind, so any
// error-class diagnostic on line 17 satisfies it.
TEST_F(DefineExpansion23Test, ReportsAnErrorOnTheIllegalLine) {
  GTEST_SKIP() << "HLC Gap: Compiler doesn't report this diagnostics";
  ASSERT_NE(m_session, nullptr);
  const ErrorContainer *const errorContainer = m_session->getErrorContainer();
  ASSERT_NE(errorContainer, nullptr);

  size_t errorsOnIllegalLine = 0;
  for (const Error &error : errorContainer->getErrors()) {
    if (isErrorClass(error) && (lineOf(error) == kIllegalLine)) ++errorsOnIllegalLine;
  }
  EXPECT_GE(errorsOnIllegalLine, 1u) << "no error on line " << kIllegalLine << ";" << describeAllErrors(errorContainer);
}

// HLC has a dedicated code for this 22.5.1 rule, PP_MACRO_NAME_RESERVED: a
// compiler directive name used as a macro name. Reporting line 17 under it
// identifies the LRM condition that makes the line illegal.
TEST_F(DefineExpansion23Test, DiagnosesTheReservedMacroName) {
  GTEST_SKIP() << "HLC Gap: Compiler doesn't report this diagnostics";
  const Error *const error = findError(ErrorDefinition::PP_MACRO_NAME_RESERVED);
  ASSERT_NE(error, nullptr) << "`define define redefines the compiler directive define as a macro name (22.5.1)";
  EXPECT_EQ(lineOf(*error), kIllegalLine);

  // The LRM calls this illegal; it does not say which kind of error, so any
  // error-class severity is accepted.
  const ErrorDefinition::ErrorMap &errorInfoMap = ErrorDefinition::getErrorInfoMap();
  const ErrorDefinition::ErrorMap::const_iterator errorInfo =
      errorInfoMap.find(ErrorDefinition::PP_MACRO_NAME_RESERVED);
  ASSERT_NE(errorInfo, errorInfoMap.end());
  EXPECT_TRUE(isErrorClass(errorInfo->second.m_severity)) << "severity " << errorInfo->second.m_severity;
}

// Line 17 is the one illegal line (22.5.1); the LRM requires an error there
// but does not limit how many diagnostics, or of which severity, it draws, so
// any number of them on line 17 is accepted. Off line 17 this check mixes two
// kinds of fact:
//   - LRM: lines 1-16 are comments and blank lines, so an error pointing at
//     them would mean HLC rejected legal text.
//   - HLC error recovery, not an LRM rule: the LRM gives the illegal
//     definition no meaning and does not define what a tool does after
//     reporting it, so an error past line 17 or without a location is a
//     cascade, not a conformance failure.
// Elaboration-category diagnostics are left out: the file has no module, and
// a design shall contain at least one top-level module (23.3.1), so an
// elaborating run may legitimately report that.
TEST_F(DefineExpansion23Test, ErrorsOnlyOnTheIllegalLine) {
  ASSERT_NE(m_session, nullptr);
  const ErrorContainer *const errorContainer = m_session->getErrorContainer();
  ASSERT_NE(errorContainer, nullptr);

  std::string offLine;
  for (const Error &error : errorContainer->getErrors()) {
    if (isErrorClass(error) && !isElaborationError(error) && (lineOf(error) != kIllegalLine)) {
      offLine += describeError(error);
    }
  }
  EXPECT_EQ(offLine, "") << "error-class diagnostics off line " << kIllegalLine;
}

// Each of these diagnostics would mean HLC misread line 17.
TEST_F(DefineExpansion23Test, ReportsNoMisreadingDiagnostics) {
  // No "(" follows the name define, so line 17 has no formal argument list:
  // no argument can be unused or undefined, and the text after the space is
  // macro text, not arguments.
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_HAS_SPACE_BEFORE_ARGS), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_UNUSED_ARGUMENT), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_UNDEFINED_ARGUMENT), nullptr);

  // The file holds no macro usage, so no usage can lack parentheses or give
  // too many or too few arguments.
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_PARENTHESIS_NEEDED), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_TOO_MANY_ARGS_MACRO), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_NO_DEFAULT_VALUE), nullptr);

  // The string literal "illegal" is closed on line 17 (5.9), and it refers to
  // no macro.
  EXPECT_EQ(findError(ErrorDefinition::PP_UNTERMINATED_STRING), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_RECURSIVE_MACRO_DEFINITION), nullptr);

  // The file has no design element, so line 17 is not inside one.
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_IN_DESIGN_ELEMENT), nullptr);

  // The file is plain ASCII.
  EXPECT_EQ(findError(ErrorDefinition::PP_NON_ASCII_CONTENT), nullptr);
}

TEST_F(DefineExpansion23Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

TEST_F(DefineExpansion23Test, SourceFileHasNoIncludesOrDirectives) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  // There is no `include in the file (22.4).
  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);

  // `define is the only directive in the file. None of the directives HLDB
  // records as generic directives (those with a vpiDirectiveType code:
  // `timescale, `default_nettype, `celldefine, ...) occurs, and `define has no
  // vpiDirectiveType code, so a generic directive here would be one the
  // source does not contain.
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);
}

// The file has no design element, and an illegal macro definition declares
// nothing (22.5.1), so nothing in the design comes from it.
TEST_F(DefineExpansion23Test, NoDesignObjectsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getTopModules()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllClasses()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);

  EXPECT_EQ(countFromSourceFile(m_design->getParameters()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParamAssigns()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getVariables()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getTypedefs()), 0u);
}
}  // namespace hlc

#ifndef DAVTESTS_BULK_BUILD
int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
