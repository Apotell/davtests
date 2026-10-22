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

// Test for tests/Google/chapter-22/22.12--line-basic.sv
//
// THE SOURCE
//
// Lines 1-7 are one-line comments with the license, lines 8-9 are blank and
// lines 10-15 are one block comment carrying the test metadata. The
// SystemVerilog text is one compiler directive and no design element:
//
//   16: `line 1 "somefile" 2
//
// Nothing follows line 16's line ending.
//
// The repository stores the file with LF line endings; a Windows checkout
// (core.autocrlf) turns them into CR LF. 5.3 lists "newlines" as white space
// but does not name the carriage return, so this test treats CR LF as one
// line ending and asserts nothing about the CR itself.
//
// WHAT THE LRM SAYS ABOUT IT
//
// Line 16 is the only compiler directive in the file. "All compiler
//   directives are preceded by the (`) character" (22.2), and `line is one of
//   them (22.1). Lines 1-15 hold none: they are comments (5.4) and blank
//   lines, and "Directives are not recognized within comments or string
//   literals" (22.2).
//
// Syntax 22-9 (22.12) gives the form of the directive:
//
//     line_compiler_directive ::= `line number " filename " level
//
//   - number is 1: "The number parameter shall be a positive integer that
//     specifies the new line number of the following text line".
//   - filename is somefile, written as the string literal "somefile": "The
//     filename parameter shall be a string literal that is treated as the new
//     name of the file". The string has 8 letters, no escape sequence and no
//     newline (5.9).
//   - level is 2: "The level parameter shall be 0, 1, or 2", and "The value 2
//     indicates that the following line is the first line after an include
//     file has been exited".
//   - "only white space may appear on the same line as the `line directive",
//     "Comments are not allowed on the same line", and "All parameters in the
//     `line directive are required" (22.12): line 16 has the three
//     parameters and nothing else.
//   The directive "can be specified anywhere within the SystemVerilog source
//   description" (22.12). Line 16 is legal, so it draws no error.
//
// "The compiler shall maintain the current line number and file name of the
//   file being compiled. The `line directive shall set the line number and
//   file name of the following line to those specified in the directive"
//   (22.12). The directive changes the location of the line after it, not of
//   its own line: line 16 itself is line 16 of this file, as "The line number
//   shall be reset to 1 at the beginning of each file" and "incremented as
//   each line is read" (22.12). The following line would be line 1 of
//   somefile, but the file ends after line 16, so no text gets that location.
//
// A `line names a file; it does not read one. The file name is only "treated
//   as the new name of the file", and "The mechanism of library searching is
//   not affected by the effects of the `line compiler directive" (22.12).
//   Unlike `include (22.4), it inserts no text, so somefile need not exist.
//
// `line is a compiler directive (22.1), not a usage of a text macro (22.5.1),
//   and the file defines no macro.
//
// The file has no module, and "A design shall contain at least one top-level
//   module" (23.3.1). An elaborating run may report that, so the error checks
//   below leave HLC's elaboration-category diagnostics out.
//
// HOW HLDB RECORDS IT
//
// The SourceFile node is HLDB's record of the text of one file (its includes,
// macro definitions, macro uses and directives) and owns what it records.
// HLDB gives `line a directive type code of its own, vpiDirectiveTypeLine, so
// line 16 is expected as the one directive of the SourceFile: a
// GenericDirective of type vpiDirectiveTypeLine. HLDB reports the location of
// an object through getFile() and getStartLine(), its vpiFile and vpiLineNo
// (HLDB defines vpiStartLine as vpiLineNo), which is the location the LRM
// requires to be "available for PLI access" (22.12).
//
// Every expected value comes from the source text and the LRM, so a red test
// means HLC deviates from the LRM or from its own model.
// PP_ILLEGAL_TICK_LINE_VALUE is HLC's code for a `line parameter it does not
// accept, PP_ILLEGAL_DIRECTIVE_VALUE its code for a directive argument it does
// not accept, and PP_ILLEGAL_DIRECTIVE_IN_DESIGN_ELEMENT its code for a
// directive placed inside a design element.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors                     no fatal, syntax or error
//                                             diagnostic (outside elaboration)
//   Line16LineDirectiveIsAccepted             no such diagnostic on line 16
//   ReportsNoDirectiveMisreadingDiagnostics   no diagnostic of any severity
//                                             that misreads line 16 as a
//                                             directive
//   ReportsNoFileOpeningDiagnostics           none that tries to read the file
//                                             somefile
//   ReportsNoStringLiteralDiagnostics         none that misreads the string
//                                             literal or the ASCII text
//   SourceFileHasNoIncludesOrMacros           one SourceFile, no includes,
//                                             no macro definitions or usages
//   SourceFileRecordsOneDirective             the SourceFile records exactly
//                                             one directive
//   Line16DirectiveIsALineDirective           that directive is a
//                                             GenericDirective of type
//                                             vpiDirectiveTypeLine
//   LineDirectiveKeepsItsOwnLocation          it lies on line 16 of this file,
//                                             not in somefile, and is owned by
//                                             the SourceFile
//   NoDesignObjectsDeclared                   nothing in the design comes
//                                             from this file or somefile
//
// No check is gated on Design::getElaborated(): nothing in the file
// elaborates or reduces. There is no design element, and the parameters of
// the directive are literals with no operator or parameter to evaluate. The
// 23.3.1 error an elaborating run may report is left out, not asserted.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The new location, line 1 of somefile. It applies to the line after the
//     directive (22.12), and the file has none, so no object carries it.
//     22.12--line-complex checks the new locations. Nearest assertion:
//     LineDirectiveKeepsItsOwnLocation (the directive's own line keeps its
//     location).
//   - The values 1, "somefile" and 2 as stored on the directive. A
//     GenericDirective holds a single value, so it cannot keep all three
//     parameters, and the LRM requires the parameters to take effect, not to
//     be stored. Nearest assertions: Line16DirectiveIsALineDirective and
//     ReportsNoDirectiveMisreadingDiagnostics (all three are accepted).
//   - What level 2 means for include-file bookkeeping. The level describes
//     the following line (22.12), which does not exist, and no include file
//     is entered or exited. Nearest assertion:
//     ReportsNoDirectiveMisreadingDiagnostics.
//   - Run-time use of the location. How a simulator prints file names and
//     line numbers in its messages only exists while it runs; this is
//     permanently out of scope. Nearest assertion:
//     LineDirectiveKeepsItsOwnLocation.
//   - The location as seen through getPpFile() and getPpStartLine(). The LRM
//     defines one current location (22.12); HLDB's preprocessor location has
//     no LRM counterpart. Nearest assertion: LineDirectiveKeepsItsOwnLocation.
//   - The directive list of the Design. It has no LRM counterpart, and the
//     directive is text of this file, which HLDB records on the SourceFile.
//     Nearest assertion: SourceFileRecordsOneDirective.
//   - Elaboration-category diagnostics. The 23.3.1 error for a design without
//     a top-level module is legitimate here. Nearest assertion:
//     CompilesWithoutErrors (every other error-class diagnostic).
//   - Warnings and informational diagnostics. The LRM neither requires nor
//     forbids them for this file. Nearest assertions: CompilesWithoutErrors
//     and the three ReportsNo... tests.
//   - The comments on lines 1-15 (5.4) and columns. The LRM gives comments no
//     meaning, how HLDB keeps them is a tool convention, and the LRM numbers
//     lines (22.12, 22.13) but not columns. Nearest assertion:
//     LineDirectiveKeepsItsOwnLocation, which holds only if lines 1-15 were
//     read as whole lines.

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
#include <hldb/generic_directive.h>
#include <hldb/hldb_vpi_user.h>
#include <hldb/interface.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/preproc_macro_instance.h>
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
constexpr std::string_view kSourceFileName = "22.12--line-basic.sv";

// The file name the `line directive gives to the line after it.
constexpr std::string_view kNewFileName = "somefile";

// The names an object from this file may carry: its own, and the one the
// `line directive gives to the lines after it (22.12).
constexpr std::string_view kFileNames[] = {kSourceFileName, kNewFileName};

// The line of the `line directive, the only SystemVerilog text in the file.
constexpr uint32_t kLineDirectiveLine = 16;

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

// True when 'file' is one of kFileNames.
bool isFromSourceFile(std::string_view file) {
  for (const std::string_view name : kFileNames) {
    if (endsWith(file, name)) return true;
  }
  return false;
}

// Size of an HLDB collection; a collection that was never created is empty.
template <typename T>
size_t sizeOf(const std::vector<T *> *collection) {
  return (collection == nullptr) ? 0 : collection->size();
}

// Number of objects in 'collection' located in the file under test, under
// any of its names.
template <typename T>
size_t countFromSourceFile(const std::vector<T *> *collection) {
  size_t count = 0;
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (isFromSourceFile(object->getFile())) ++count;
    }
  }
  return count;
}

// The line a diagnostic points at, or 0 when it carries no location.
uint32_t lineOf(const Error &error) { return error.getLocations().empty() ? 0 : error.getLocations().front().m_line; }

// True when HLC's error table gives 'error' a fatal, syntax or error
// severity outside the elaboration category. Elaboration diagnostics are left
// out because a design without a top-level module may draw one (23.3.1).
bool isSourceError(const Error &error) {
  const ErrorDefinition::ErrorMap &errorInfoMap = ErrorDefinition::getErrorInfoMap();
  const ErrorDefinition::ErrorMap::const_iterator errorInfo = errorInfoMap.find(error.getType());
  if (errorInfo == errorInfoMap.end()) return false;
  const ErrorDefinition::ErrorSeverity severity = errorInfo->second.m_severity;
  const bool isErrorClass = (severity == ErrorDefinition::FATAL) || (severity == ErrorDefinition::SYNTAX) ||
                            (severity == ErrorDefinition::ERROR);
  return isErrorClass && (errorInfo->second.m_category != ErrorDefinition::ELAB);
}

// One diagnostic as " [<code> line <n>: <message template>]", so that a red
// run shows what was reported and where.
std::string describeError(const Error &error) {
  const ErrorDefinition::ErrorMap &errorInfoMap = ErrorDefinition::getErrorInfoMap();
  const ErrorDefinition::ErrorMap::const_iterator errorInfo = errorInfoMap.find(error.getType());
  std::string description = " [" + std::to_string(static_cast<int32_t>(error.getType()));
  description += " line " + std::to_string(lineOf(error));
  if (errorInfo != errorInfoMap.end()) description += ": " + std::string(errorInfo->second.m_errorText);
  return description + "]";
}

// Every directive 'sourceFile' records as " [vpiType <type> <file>:<line>]",
// so that a red run shows what HLDB recorded instead.
std::string describeDirectives(const hldb::SourceFile *sourceFile) {
  std::string description;
  if (const hldb::DirectiveCollection *const directives = sourceFile->getDirectives()) {
    for (const hldb::Directive *directive : *directives) {
      description += " [vpiType " + std::to_string(directive->getVpiType()) + " ";
      description += std::string(directive->getFile()) + ":" + std::to_string(directive->getStartLine()) + "]";
    }
  }
  return description;
}
}  // namespace

class LineBasicTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.12--line-basic.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Every source error (see isSourceError), described; empty when there is
  // none. A run without an error container is reported, not passed.
  static std::string sourceErrors() {
    if ((m_session == nullptr) || (m_session->getErrorContainer() == nullptr)) return "no error container";
    std::string description;
    for (const Error &error : m_session->getErrorContainer()->getErrors()) {
      if (isSourceError(error)) description += describeError(error);
    }
    return description;
  }

  // The same, for the source errors that point at 'line' only.
  static std::string sourceErrorsOnLine(uint32_t line) {
    if ((m_session == nullptr) || (m_session->getErrorContainer() == nullptr)) return "no error container";
    std::string description;
    for (const Error &error : m_session->getErrorContainer()->getErrors()) {
      if (isSourceError(error) && (lineOf(error) == line)) description += describeError(error);
    }
    return description;
  }

  // The SourceFile nodes of the file under test.
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

  // The SourceFile node of the file under test, or nullptr unless there is
  // exactly one.
  static const hldb::SourceFile *getSourceFile() {
    const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
    return (sourceFiles.size() == 1) ? sourceFiles.front() : nullptr;
  }

  // The first directive the SourceFile records, as a GenericDirective, or
  // nullptr.
  static const hldb::GenericDirective *findFirstGenericDirective() {
    const hldb::SourceFile *const sourceFile = getSourceFile();
    if ((sourceFile == nullptr) || (sizeOf(sourceFile->getDirectives()) == 0)) return nullptr;
    return any_cast<hldb::GenericDirective>(sourceFile->getDirectives()->front());
  }
};

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// Every line of the file is legal, so nothing is fatal, a syntax error or an
// error, apart from the 23.3.1 elaboration error left out above.
TEST_F(LineBasicTest, CompilesWithoutErrors) { EXPECT_EQ(sourceErrors(), ""); }

// Line 16: `line 1 "somefile" 2. It matches Syntax 22-9 with a positive line
// number, a string literal file name and a level of 0, 1 or 2 (22.12).
TEST_F(LineBasicTest, Line16LineDirectiveIsAccepted) { EXPECT_EQ(sourceErrorsOnLine(kLineDirectiveLine), ""); }

// Each of these diagnostics, at any severity and on any line, would mean HLC
// misread line 16 as a directive.
TEST_F(LineBasicTest, ReportsNoDirectiveMisreadingDiagnostics) {
  // Line 16 matches Syntax 22-9, for the preprocessor and for the parser.
  EXPECT_EQ(findError(ErrorDefinition::PP_SYNTAX_ERROR), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PA_SYNTAX_ERROR), nullptr);

  // 1 is a positive integer, "somefile" a string literal and 2 one of the
  // levels 0, 1 and 2 (22.12).
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_TICK_LINE_VALUE), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_VALUE), nullptr);

  // The directive "can be specified anywhere" (22.12), and the file has no
  // design element anyway.
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_IN_DESIGN_ELEMENT), nullptr);

  // `line is a compiler directive (22.1), not a usage of a text macro.
  EXPECT_EQ(findError(ErrorDefinition::PP_UNKOWN_MACRO), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PA_UNKOWN_MACRO), nullptr);
}

// somefile is "treated as the new name of the file" (22.12); a `line reads no
// file, unlike `include (22.4). A failure to open somefile, at any severity,
// would mean HLC tried to read it.
TEST_F(LineBasicTest, ReportsNoFileOpeningDiagnostics) {
  EXPECT_EQ(findError(ErrorDefinition::PP_CANNOT_OPEN_FILE), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_CANNOT_OPEN_INCLUDE_FILE), nullptr);
}

// "somefile" is a well-formed quoted string (5.9) in a plain ASCII file.
TEST_F(LineBasicTest, ReportsNoStringLiteralDiagnostics) {
  // Its closing double quote is on line 16.
  EXPECT_EQ(findError(ErrorDefinition::PP_UNTERMINATED_STRING), nullptr);

  // Letters are quoted_string_items; none needs an escape.
  EXPECT_EQ(findError(ErrorDefinition::PP_UNESCAPED_CHARACTER_IN_STRING), nullptr);

  // It has no backslash, so it has no escape sequence (5.9.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_UNRECOGNIZED_ESCAPED_SEQUENCE), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_ESCAPE_SEQUENCE_OUT_OF_RANGE), nullptr);

  // Every character of the file is ASCII.
  EXPECT_EQ(findError(ErrorDefinition::PP_NON_ASCII_CONTENT), nullptr);
}

// ---------------------------------------------------------------------------
// The source file
// ---------------------------------------------------------------------------

// The file is read once. It has no `include (22.4) and no `define (22.5.1);
// the file name in a `line is not an include (22.12), and `line is a
// compiler directive, not a macro usage (22.1).
TEST_F(LineBasicTest, SourceFileHasNoIncludesOrMacros) {
  const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
  ASSERT_EQ(sourceFiles.size(), 1u);
  const hldb::SourceFile *const sourceFile = sourceFiles.front();

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// ---------------------------------------------------------------------------
// Line 16: the `line directive
// ---------------------------------------------------------------------------

// Line 16 is the only directive of the file: lines 1-15 are comments and
// blank lines, and no directive is recognized within a comment (22.2).
TEST_F(LineBasicTest, SourceFileRecordsOneDirective) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 1u) << describeDirectives(sourceFile);
}

// That directive is a `line (22.12), which HLDB records as a GenericDirective
// of type vpiDirectiveTypeLine.
TEST_F(LineBasicTest, Line16DirectiveIsALineDirective) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);
  ASSERT_GE(sizeOf(sourceFile->getDirectives()), 1u);
  const hldb::Directive *const directive = sourceFile->getDirectives()->front();
  ASSERT_NE(directive, nullptr);
  ASSERT_EQ(directive->getAnyType(), hldb::AnyType::GenericDirective) << describeDirectives(sourceFile);

  const hldb::GenericDirective *const lineDirective = any_cast<hldb::GenericDirective>(directive);
  ASSERT_NE(lineDirective, nullptr);
  EXPECT_EQ(lineDirective->getDirectiveType(), vpiDirectiveTypeLine);
}

// The directive sets the location of the following line, not of its own
// (22.12): it is all on line 16 (22.2) of this file, not in somefile. The
// SourceFile that records it owns it.
TEST_F(LineBasicTest, LineDirectiveKeepsItsOwnLocation) {
  const hldb::GenericDirective *const lineDirective = findFirstGenericDirective();
  ASSERT_NE(lineDirective, nullptr);

  EXPECT_TRUE(endsWith(lineDirective->getFile(), kSourceFileName)) << lineDirective->getFile();
  EXPECT_EQ(lineDirective->getStartLine(), kLineDirectiveLine);
  EXPECT_EQ(lineDirective->getEndLine(), kLineDirectiveLine);
  EXPECT_EQ(lineDirective->getParent(), static_cast<const hldb::Any *>(getSourceFile()));
}

// ---------------------------------------------------------------------------
// Nothing is declared
// ---------------------------------------------------------------------------

// The file has no design element, and a `line only renames lines (22.12); it
// declares nothing. So nothing in the design comes from this file, under its
// own name or under somefile.
TEST_F(LineBasicTest, NoDesignObjectsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getTopModules()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getTopPackages()), 0u);
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

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
