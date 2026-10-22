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

// Test for tests/Google/chapter-22/22.12--line-complex.sv
//
// THE SOURCE
//
// Lines 1-7 are one-line comments with the license, lines 8-9 are blank and
// lines 10-15 are one block comment carrying the test metadata. The
// SystemVerilog text is five compiler directives and no design element:
//
//   16: `line 1 "somefile" 2
//   17: `line 123 "someother" 1
//   18: `line 8273 "somefile" 0
//   19: `resetall
//   20: `line 5 "foo" 2
//
// Nothing follows line 20's line ending.
//
// The repository stores the file with LF line endings; a Windows checkout
// (core.autocrlf) turns them into CR LF. 5.3 lists "newlines" as white space
// but does not name the carriage return, so this test treats CR LF as one
// line ending and asserts nothing about the CR itself.
//
// WHAT THE LRM SAYS ABOUT IT
//
// Lines 16-20 are the only compiler directives in the file. "All compiler
//   directives are preceded by the (`) character" (22.2), and `line and
//   `resetall are among them (22.1). Lines 1-15 hold none: they are comments
//   (5.4) and blank lines, and "Directives are not recognized within comments
//   or string literals" (22.2).
//
// Syntax 22-9 (22.12) gives the form of a `line directive:
//
//     line_compiler_directive ::= `line number " filename " level
//
//   Each of the four `line directives has a positive integer number (1, 123,
//   8273, 5), a string literal file name ("somefile", "someother",
//   "somefile", "foo") and a level of 0, 1 or 2 (2, 1, 0, 2), as 22.12
//   requires: "The number parameter shall be a positive integer", "The
//   filename parameter shall be a string literal", "The level parameter shall
//   be 0, 1, or 2", "All parameters in the `line directive are required", and
//   "only white space may appear on the same line as the `line directive".
//   Each string is letters only, with no escape sequence and no newline (5.9).
//   The directive "can be specified anywhere within the SystemVerilog source
//   description" (22.12).
//
// `resetall takes no argument (22.3), and "It shall be illegal for the
//   `resetall directive to be specified within a design element" (22.3); the
//   file has no design element. All five lines are legal, so they draw no
//   error.
//
// The location of each line follows from 22.12. "The compiler shall maintain
//   the current line number and file name of the file being compiled. The
//   `line directive shall set the line number and file name of the following
//   line to those specified in the directive." Otherwise "the line number
//   shall be incremented as each line is read", starting from 1 "at the
//   beginning of each file", and "The results of this directive are not
//   affected by the `resetall directive". "The updated line number and file
//   name information shall be available for PLI access." So:
//
//     line  directive                 file name   line number
//     16    `line 1 "somefile" 2      this file   16    (no `line before it)
//     17    `line 123 "someother" 1   somefile    1     (set by line 16)
//     18    `line 8273 "somefile" 0   someother   123   (set by line 17)
//     19    `resetall                 somefile    8273  (set by line 18)
//     20    `line 5 "foo" 2           somefile    8274  (incremented; the
//                                                       `resetall on line 19
//                                                       does not undo it)
//
//   The line after line 20 would be line 5 of foo, but the file ends there.
//   22.13 confirms the effect: "A `line directive changes `__LINE__ and may
//   change `__FILE__ as well."
//
// A `line names a file; it does not read one. The file name is only "treated
//   as the new name of the file", and "The mechanism of library searching is
//   not affected by the effects of the `line compiler directive" (22.12).
//   Unlike `include (22.4), it inserts no text, so somefile, someother and foo
//   need not exist.
//
// `line and `resetall are compiler directives (22.1), not usages of a text
//   macro (22.5.1), and the file defines no macro.
//
// The file has no module, and "A design shall contain at least one top-level
//   module" (23.3.1). An elaborating run may report that, so the error checks
//   below leave HLC's elaboration-category diagnostics out.
//
// HOW HLDB RECORDS IT
//
// The SourceFile node is HLDB's record of the text of one file (its includes,
// macro definitions, macro uses and directives) and owns what it records.
// HLDB gives both directives a type code of their own, vpiDirectiveTypeLine
// and vpiDirectiveTypeResetAll, so the five lines are expected as the five
// directives of the SourceFile, in source order, each a GenericDirective of
// its type. HLDB reports the location of an object through getFile() and
// getStartLine(), its vpiFile and vpiLineNo (HLDB defines vpiStartLine as
// vpiLineNo), which is the location the LRM requires to be "available for PLI
// access" (22.12). So each directive is expected at the file name and line
// number of the table above. The directives stay text of this file, so the
// SourceFile of this file owns all five, whatever their file names.
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
//                                             that misreads lines 16-20 as
//                                             directives
//   ReportsNoFileOpeningDiagnostics           none that tries to read
//                                             somefile, someother or foo
//   ReportsNoStringLiteralDiagnostics         none that misreads the string
//                                             literals or the ASCII text
//   SourceFileHasNoIncludesOrMacros           one SourceFile, no includes,
//                                             no macro definitions or usages
//   SourceFileRecordsFiveDirectives           the SourceFile records exactly
//                                             five directives
//   Line16DirectiveKeepsItsOwnLocation        `line, at line 16 of this file
//   Line17DirectiveIsOnLine1OfSomefile        `line, at somefile:1
//   Line18DirectiveIsOnLine123OfSomeother     `line, at someother:123
//   Line19ResetallIsOnLine8273OfSomefile      `resetall, at somefile:8273
//   Line20DirectiveIsOnLine8274OfSomefile     `line, at somefile:8274
//   NoDesignObjectsDeclared                   nothing in the design comes
//                                             from this file under any of
//                                             its names
//
// Each of the five directive checks covers the directive's kind and type, its
// file name, its start and end line, and that the SourceFile owns it.
//
// No check is gated on Design::getElaborated(): nothing in the file
// elaborates or reduces. There is no design element, and the parameters of
// the directives are literals with no operator or parameter to evaluate. The
// 23.3.1 error an elaborating run may report is left out, not asserted.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - The location after line 20, line 5 of foo. The file ends there, so no
//     object carries it. Nearest assertion:
//     Line20DirectiveIsOnLine8274OfSomefile.
//   - Diagnostics by line for lines 17-20. A diagnostic for one of them would
//     carry the line number that the `line before it set, a number that HLC's
//     diagnostics do not tie to a file name here. Nearest assertions:
//     CompilesWithoutErrors (no error on any line) and the four directive
//     location checks.
//   - The number, file name and level as stored on each `line directive. A
//     GenericDirective holds a single value, so it cannot keep all three
//     parameters, and the LRM requires the parameters to take effect, not to
//     be stored. Their effect is checked instead. Nearest assertions: the
//     four checks of lines 17-20.
//   - What the levels 1, 0 and 2 mean for include-file bookkeeping. The level
//     only describes the following line (22.12), and no include file is
//     entered or exited. Nearest assertion:
//     ReportsNoDirectiveMisreadingDiagnostics (all levels are accepted).
//   - The other effects of `resetall. It sets the directives that have a
//     default back to it (22.3), and no design element follows that could
//     show one. Nearest assertions: Line19ResetallIsOnLine8273OfSomefile and
//     Line20DirectiveIsOnLine8274OfSomefile (it leaves the location alone).
//   - Run-time use of the locations. How a simulator prints file names and
//     line numbers in its messages only exists while it runs; this is
//     permanently out of scope. Nearest assertions: the five directive
//     location checks.
//   - The location as seen through getPpFile() and getPpStartLine(). The LRM
//     defines one current location (22.12); HLDB's preprocessor location has
//     no LRM counterpart. Nearest assertions: the five directive location
//     checks.
//   - The directive list of the Design. It has no LRM counterpart, and the
//     directives are text of this file, which HLDB records on the SourceFile.
//     Nearest assertion: SourceFileRecordsFiveDirectives.
//   - Elaboration-category diagnostics. The 23.3.1 error for a design without
//     a top-level module is legitimate here. Nearest assertion:
//     CompilesWithoutErrors (every other error-class diagnostic).
//   - Warnings and informational diagnostics. The LRM neither requires nor
//     forbids them for this file. Nearest assertions: CompilesWithoutErrors
//     and the three ReportsNo... tests.
//   - The comments on lines 1-15 (5.4) and columns. The LRM gives comments no
//     meaning, how HLDB keeps them is a tool convention, and the LRM numbers
//     lines (22.12, 22.13) but not columns. Nearest assertion:
//     Line16DirectiveKeepsItsOwnLocation, which holds only if lines 1-15 were
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
constexpr std::string_view kSourceFileName = "22.12--line-complex.sv";

// The file names the `line directives give to the lines after them.
constexpr std::string_view kSomefile = "somefile";
constexpr std::string_view kSomeother = "someother";
constexpr std::string_view kFoo = "foo";

// The names an object from this file may carry: its own, and those the
// `line directives give to the lines after them (22.12).
constexpr std::string_view kFileNames[] = {kSourceFileName, kSomefile, kSomeother, kFoo};

// The line of the first `line directive, the only line before any `line.
constexpr uint32_t kFirstDirectiveLine = 16;

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

class LineComplexTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.12--line-complex.hlc"}); }
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

  // Checks the directive at 'index' (0-based, in source order): a
  // GenericDirective of 'type', all on line 'line' of the file named 'file',
  // owned by the SourceFile.
  static void expectDirective(size_t index, int32_t type, std::string_view file, uint32_t line) {
    const hldb::SourceFile *const sourceFile = getSourceFile();
    ASSERT_NE(sourceFile, nullptr);
    ASSERT_GT(sizeOf(sourceFile->getDirectives()), index) << describeDirectives(sourceFile);
    const hldb::Directive *const directive = sourceFile->getDirectives()->at(index);
    ASSERT_NE(directive, nullptr);
    ASSERT_EQ(directive->getAnyType(), hldb::AnyType::GenericDirective) << describeDirectives(sourceFile);
    const hldb::GenericDirective *const genericDirective = any_cast<hldb::GenericDirective>(directive);
    ASSERT_NE(genericDirective, nullptr);

    EXPECT_EQ(genericDirective->getDirectiveType(), type);
    EXPECT_TRUE(endsWith(genericDirective->getFile(), file)) << genericDirective->getFile() << " is not " << file;
    EXPECT_EQ(genericDirective->getStartLine(), line);
    EXPECT_EQ(genericDirective->getEndLine(), line);
    EXPECT_EQ(genericDirective->getParent(), static_cast<const hldb::Any *>(sourceFile));
  }
};

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// Every line of the file is legal, so nothing is fatal, a syntax error or an
// error, apart from the 23.3.1 elaboration error left out above.
TEST_F(LineComplexTest, CompilesWithoutErrors) { EXPECT_EQ(sourceErrors(), ""); }

// Line 16: `line 1 "somefile" 2, the only line whose location no `line has
// changed. It matches Syntax 22-9 with a positive line number, a string
// literal file name and a level of 0, 1 or 2 (22.12).
TEST_F(LineComplexTest, Line16LineDirectiveIsAccepted) { EXPECT_EQ(sourceErrorsOnLine(kFirstDirectiveLine), ""); }

// Each of these diagnostics, at any severity and on any line, would mean HLC
// misread lines 16-20 as directives.
TEST_F(LineComplexTest, ReportsNoDirectiveMisreadingDiagnostics) {
  // The four `line lines match Syntax 22-9 and `resetall takes no argument,
  // for the preprocessor and for the parser.
  EXPECT_EQ(findError(ErrorDefinition::PP_SYNTAX_ERROR), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PA_SYNTAX_ERROR), nullptr);

  // 1, 123, 8273 and 5 are positive integers, the file names are string
  // literals, and 2, 1, 0 and 2 are among the levels 0, 1 and 2 (22.12).
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_TICK_LINE_VALUE), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_VALUE), nullptr);

  // A `line "can be specified anywhere" (22.12), and the `resetall is outside
  // any design element (22.3): the file has none.
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_IN_DESIGN_ELEMENT), nullptr);

  // `line and `resetall are compiler directives (22.1), not usages of a text
  // macro.
  EXPECT_EQ(findError(ErrorDefinition::PP_UNKOWN_MACRO), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PA_UNKOWN_MACRO), nullptr);
}

// somefile, someother and foo are each "treated as the new name of the file"
// (22.12); a `line reads no file, unlike `include (22.4). A failure to open
// one of them, at any severity, would mean HLC tried to read it.
TEST_F(LineComplexTest, ReportsNoFileOpeningDiagnostics) {
  EXPECT_EQ(findError(ErrorDefinition::PP_CANNOT_OPEN_FILE), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_CANNOT_OPEN_INCLUDE_FILE), nullptr);
}

// "somefile", "someother" and "foo" are well-formed quoted strings (5.9) in a
// plain ASCII file.
TEST_F(LineComplexTest, ReportsNoStringLiteralDiagnostics) {
  // Each closing double quote is on the line of its opening one.
  EXPECT_EQ(findError(ErrorDefinition::PP_UNTERMINATED_STRING), nullptr);

  // Letters are quoted_string_items; none needs an escape.
  EXPECT_EQ(findError(ErrorDefinition::PP_UNESCAPED_CHARACTER_IN_STRING), nullptr);

  // None has a backslash, so none has an escape sequence (5.9.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_UNRECOGNIZED_ESCAPED_SEQUENCE), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_ESCAPE_SEQUENCE_OUT_OF_RANGE), nullptr);

  // Every character of the file is ASCII.
  EXPECT_EQ(findError(ErrorDefinition::PP_NON_ASCII_CONTENT), nullptr);
}

// ---------------------------------------------------------------------------
// The source file
// ---------------------------------------------------------------------------

// The file is read once. It has no `include (22.4) and no `define (22.5.1);
// the file names in the `line directives are not includes (22.12), and `line
// and `resetall are compiler directives, not macro usages (22.1).
TEST_F(LineComplexTest, SourceFileHasNoIncludesOrMacros) {
  const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
  ASSERT_EQ(sourceFiles.size(), 1u);
  const hldb::SourceFile *const sourceFile = sourceFiles.front();

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// ---------------------------------------------------------------------------
// Lines 16-20: the directives and their locations
// ---------------------------------------------------------------------------

// Lines 16-20 are the directives of the file: lines 1-15 are comments and
// blank lines, and no directive is recognized within a comment (22.2).
TEST_F(LineComplexTest, SourceFileRecordsFiveDirectives) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 5u) << describeDirectives(sourceFile);
}

// Line 16: `line 1 "somefile" 2. No `line comes before it, so it is line 16
// of this file; it sets the location of line 17, not its own (22.12).
TEST_F(LineComplexTest, Line16DirectiveKeepsItsOwnLocation) {
  expectDirective(0, vpiDirectiveTypeLine, kSourceFileName, kFirstDirectiveLine);
}

// Line 17: `line 123 "someother" 1. Line 16 set it to line 1 of somefile.
TEST_F(LineComplexTest, Line17DirectiveIsOnLine1OfSomefile) { expectDirective(1, vpiDirectiveTypeLine, kSomefile, 1); }

// Line 18: `line 8273 "somefile" 0. Line 17 set it to line 123 of someother.
TEST_F(LineComplexTest, Line18DirectiveIsOnLine123OfSomeother) {
  expectDirective(2, vpiDirectiveTypeLine, kSomeother, 123);
}

// Line 19: `resetall. Line 18 set it to line 8273 of somefile.
TEST_F(LineComplexTest, Line19ResetallIsOnLine8273OfSomefile) {
  expectDirective(3, vpiDirectiveTypeResetAll, kSomefile, 8273);
}

// Line 20: `line 5 "foo" 2. No `line set it, so its number is one more than
// that of line 19, in the same file (22.12). The `resetall on line 19 does
// not undo the earlier `line: "The results of this directive are not affected
// by the `resetall directive" (22.12).
TEST_F(LineComplexTest, Line20DirectiveIsOnLine8274OfSomefile) {
  expectDirective(4, vpiDirectiveTypeLine, kSomefile, 8274);
}

// ---------------------------------------------------------------------------
// Nothing is declared
// ---------------------------------------------------------------------------

// The file has no design element, a `line only renames lines (22.12), and
// `resetall only restores defaults (22.3); none of them declares anything. So
// nothing in the design comes from this file, under its own name or under
// somefile, someother or foo.
TEST_F(LineComplexTest, NoDesignObjectsDeclared) {
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
