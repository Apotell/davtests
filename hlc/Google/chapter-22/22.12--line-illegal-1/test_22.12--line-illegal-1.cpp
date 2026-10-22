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

// Test for tests/Google/chapter-22/22.12--line-illegal-1.sv
//
// THE SOURCE
//
// Lines 1-7 are one-line comments with the license, lines 8-9 are blank and
// lines 10-16 are one block comment carrying the test metadata. The
// SystemVerilog text is one line, and no design element:
//
//   17: `line 1 "somefile" 3
//
// Nothing follows line 17's line ending, so the end of the file is at line
// 18.
//
// The repository stores the file with LF line endings; a Windows checkout
// (core.autocrlf) turns them into CR LF. 5.3 lists "newlines" as white space
// but does not name the carriage return, so this test treats CR LF as one
// line ending and asserts nothing about the CR itself.
//
// WHAT THE LRM SAYS ABOUT IT
//
// Syntax 22-9 (22.12) gives the form of the directive:
//
//     line_compiler_directive ::= `line number " filename " level
//
//   The number 1 is a positive integer and "somefile" a string literal, as
//   22.12 requires, but the level is 3, and "The level parameter shall be 0,
//   1, or 2" (22.12). Line 17 is therefore illegal, and the LRM requires an
//   error. It may point at line 17 or at the end of the file just after it
//   (line 18); both are accepted.
//
// An illegal directive sets no new location: the location of the following
// line is not somefile:1. No text follows anyway.
//
// Lines 1-16 are legal: one-line comments, blank lines and one block comment
//   (5.4).
//
// A `line names a file; it does not read one: the file name is "treated as
//   the new name of the file" (22.12), so somefile need not exist.
//
// `line is a compiler directive (22.1), not a usage of a text macro (22.5.1),
//   and the file defines no macro.
//
// The file has no module, and "A design shall contain at least one top-level
//   module" (23.3.1). An elaborating run may report that, so every check here
//   leaves HLC's elaboration-category diagnostics out; in particular such a
//   diagnostic cannot stand in for the error that line 17 requires.
//
// Every assertion follows from the LRM, except NoCascadeErrors, which is
// labelled as HLC error recovery. PP_ILLEGAL_DIRECTIVE_IN_DESIGN_ELEMENT is
// HLC's code for a directive placed inside a design element.
//
// WHAT IS CHECKED (one fact per test)
//
//   ReportsAnErrorForTheIllegalLevel   at least one fatal, syntax or error
//                                      diagnostic, outside elaboration, on
//                                      line 17 or at the end of the file
//                                      (line 18)
//   NoErrorsOnTheLegalLines1To16       LRM: no such diagnostic on lines 1-16
//   NoCascadeErrors                    HLC error recovery, not an LRM rule:
//                                      none past the end of the file, or
//                                      without a location
//   ReportsNoMisreadingDiagnostics     no "directive in design element", no
//                                      "unknown macro", no failure to open
//                                      somefile, no unterminated string and
//                                      no non-ASCII diagnostic
//   SourceFileHasNoIncludesOrMacros    one SourceFile, no includes, no macro
//                                      definitions or usages
//   NoDesignObjectsDeclared            nothing in the design comes from this
//                                      file, under its own name or under
//                                      somefile
//
// No check is gated on Design::getElaborated(): the file has nothing that
// elaborates or reduces. The 23.3.1 error an elaborating run may report is
// left out, not asserted.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Which code, which error-class severity and how many diagnostics line 17
//     draws, and their message text and column. The LRM requires an error but
//     not its form, and it numbers lines (22.12, 22.13) but not columns. An
//     "illegal `line value" diagnostic and a syntax diagnostic are both
//     accepted. Nearest assertion: ReportsAnErrorForTheIllegalLevel.
//   - A directive object for line 17 in any directive list (SourceFile or
//     Design). The LRM gives an illegal line no meaning, so neither the
//     contents nor the size of such a list is asserted. Nearest assertion:
//     ReportsAnErrorForTheIllegalLevel.
//   - Run-time use of locations. How a simulator prints file names and line
//     numbers only exists while it runs; this is permanently out of scope.
//     Nearest assertion: ReportsAnErrorForTheIllegalLevel.
//   - Elaboration-category diagnostics. The 23.3.1 error for a design without
//     a top-level module is legitimate here. Nearest assertions:
//     NoErrorsOnTheLegalLines1To16 and NoCascadeErrors (every other
//     error-class diagnostic).
//   - Warnings and informational diagnostics. The LRM neither requires nor
//     forbids them. Nearest assertion: ReportsAnErrorForTheIllegalLevel.

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
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "22.12--line-illegal-1.sv";

// The names an object from this file could carry: its own, and the one the
// illegal `line names, which it must not set.
constexpr std::string_view kFileNames[] = {kSourceFileName, "somefile"};

// The illegal `line, and the end of the file just after it.
constexpr uint32_t kIllegalLine = 17;
constexpr uint32_t kEndOfFileLine = 18;

// Upper bound for "any later line".
constexpr uint32_t kAnyLaterLine = std::numeric_limits<uint32_t>::max();

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
}  // namespace

class LineIllegal1Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.12--line-illegal-1.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // The diagnostics of the run, or nullptr.
  static const ErrorContainer *getErrorContainer() {
    return (m_session == nullptr) ? nullptr : m_session->getErrorContainer();
  }

  // Every diagnostic of the run, described; for failure messages.
  static std::string describeAllErrors() {
    if (getErrorContainer() == nullptr) return "no error container";
    std::string description;
    for (const Error &error : getErrorContainer()->getErrors()) description += describeError(error);
    return description;
  }

  // Number of source errors (see isSourceError) that point at 'line'.
  static size_t countSourceErrorsOnLine(uint32_t line) {
    if (getErrorContainer() == nullptr) return 0;
    size_t count = 0;
    for (const Error &error : getErrorContainer()->getErrors()) {
      if (isSourceError(error) && (lineOf(error) == line)) ++count;
    }
    return count;
  }

  // Every source error on a line from 'firstLine' to 'lastLine', described;
  // empty when there is none. A run without an error container is reported,
  // not passed.
  static std::string describeSourceErrorsOnLines(uint32_t firstLine, uint32_t lastLine) {
    if (getErrorContainer() == nullptr) return "no error container";
    std::string description;
    for (const Error &error : getErrorContainer()->getErrors()) {
      const uint32_t line = lineOf(error);
      if (isSourceError(error) && (line >= firstLine) && (line <= lastLine)) description += describeError(error);
    }
    return description;
  }

  // Every source error without a location, described; empty when there is none.
  static std::string describeUnlocatedSourceErrors() {
    if (getErrorContainer() == nullptr) return "no error container";
    std::string description;
    for (const Error &error : getErrorContainer()->getErrors()) {
      if (isSourceError(error) && error.getLocations().empty()) description += describeError(error);
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
};

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// "The level parameter shall be 0, 1, or 2" (22.12), and line 17 gives 3, so
// the file is illegal. The LRM requires an error and does not say which kind,
// so any fatal, syntax or error diagnostic outside elaboration on line 17 or
// at the end of the file satisfies it.
TEST_F(LineIllegal1Test, ReportsAnErrorForTheIllegalLevel) {
  const size_t errors = countSourceErrorsOnLine(kIllegalLine) + countSourceErrorsOnLine(kEndOfFileLine);
  EXPECT_GE(errors, 1u) << "no error on line 17 or 18;" << describeAllErrors();
}

// LRM: lines 1-16 are comments and blank lines (5.4). An error on any of them
// would mean HLC rejected legal text.
TEST_F(LineIllegal1Test, NoErrorsOnTheLegalLines1To16) {
  EXPECT_EQ(describeSourceErrorsOnLines(1, kIllegalLine - 1), "");
}

// HLC error recovery, not an LRM rule. The file ends after line 17, and the
// LRM does not define what a tool does after reporting an illegal line. An
// error past the end of the file, or one without a location, would be a
// cascade from it.
TEST_F(LineIllegal1Test, NoCascadeErrors) {
  EXPECT_EQ(describeSourceErrorsOnLines(kEndOfFileLine + 1, kAnyLaterLine), "");
  EXPECT_EQ(describeUnlocatedSourceErrors(), "");
}

// Each of these diagnostics, at any severity, would mean HLC misread the
// file, whatever it reports for line 17.
TEST_F(LineIllegal1Test, ReportsNoMisreadingDiagnostics) {
  // A `line "can be specified anywhere" (22.12), and the file has no design
  // element anyway.
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_IN_DESIGN_ELEMENT), nullptr);

  // `line is a compiler directive (22.1), not a usage of a text macro.
  EXPECT_EQ(findError(ErrorDefinition::PP_UNKOWN_MACRO), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PA_UNKOWN_MACRO), nullptr);

  // A `line reads no file (22.12), so somefile is never opened.
  EXPECT_EQ(findError(ErrorDefinition::PP_CANNOT_OPEN_FILE), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_CANNOT_OPEN_INCLUDE_FILE), nullptr);

  // "somefile" is closed by its second double quote on line 17 (5.9).
  EXPECT_EQ(findError(ErrorDefinition::PP_UNTERMINATED_STRING), nullptr);

  // Every character of the file is ASCII.
  EXPECT_EQ(findError(ErrorDefinition::PP_NON_ASCII_CONTENT), nullptr);
}

// ---------------------------------------------------------------------------
// The source file
// ---------------------------------------------------------------------------

// The file is read once. It has no `include (22.4) and no `define (22.5.1),
// and `line is a compiler directive, not a macro usage (22.1).
TEST_F(LineIllegal1Test, SourceFileHasNoIncludesOrMacros) {
  const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
  ASSERT_EQ(sourceFiles.size(), 1u);
  const hldb::SourceFile *const sourceFile = sourceFiles.front();

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// ---------------------------------------------------------------------------
// Nothing is declared
// ---------------------------------------------------------------------------

// The file has no design element, and a `line declares nothing (22.12); the
// only SystemVerilog text is the illegal line 17. So nothing in the design
// comes from this file, under its own name or under somefile.
TEST_F(LineIllegal1Test, NoDesignObjectsDeclared) {
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
