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

// Test for tests/Google/generic/class/class_test_14.sv
//
// THE SOURCE
//
// Lines 1-5 are one block comment carrying the test metadata (tags 6.15 and
// 8.3); this file has no license comment. The SystemVerilog text is:
//
//   6: class Foo #(T=int);
//   7: endclass
//
// Nothing follows endclass, not even a line ending, so the end of the file is
// on line 7. "White space shall contain the characters for spaces, tabs,
// newlines, formfeeds, and end of file" (5.3), so endclass ends there.
//
// The repository stores the file with LF line endings; a Windows checkout
// (core.autocrlf) turns them into CR LF. 5.3 lists "newlines" as white space
// but does not name the carriage return, so this test treats CR LF as one
// line ending and asserts nothing about the CR itself.
//
// WHAT THE LRM SAYS ABOUT IT
//
// Lines 6-7 are a class_declaration (8.3, A.1.2) of the class Foo with a
//   parameter_port_list (A.1.3) of the form # ( list_of_param_assignments ):
//
//     parameter_port_list ::= # ( list_of_param_assignments { , parameter_port_declaration } ) | ...
//     param_assignment ::= parameter_identifier { variable_dimension } [ = constant_param_expression ]
//     constant_param_expression ::= constant_mintypmax_expression | data_type | $
//
//   - "T=int" is a param_assignment, so T is a value parameter whose
//     parameter keyword is omitted ("The parameter keyword can be omitted in
//     a parameter port list", 6.20.1). A type parameter needs the type
//     keyword: type_parameter_declaration ::= type [ forward_type ]
//     list_of_type_assignments (A.2.1.1), and the line has none.
//   - The grammar lets constant_param_expression be a data_type, and int is
//     one, so the line parses. But "A value parameter (parameter, localparam,
//     or specparam) can only be set to an expression of literals, value
//     parameters or local parameters, genvars, enumerated names, or a
//     constant function of these" (6.20.2), and the data type int is none of
//     these. The file is therefore illegal, and the LRM requires an error.
//
// Where the error points is a test convention, not an LRM rule: the LRM never
//   requires a diagnostic to carry a line. T's default is on line 6, and the
//   declaration and the file end on line 7, so this test accepts the error on
//   either line and expects none on lines 1-5.
//
// Lines 1-5 are legal: one block comment (5.4).
//
// The file has no module, and "A design shall contain at least one top-level
//   module" (23.3.1). An elaborating run may report that, so that diagnostic
//   (HLC's ELAB_NO_TOP_LEVEL_MODULE) never counts as the required error, and
//   the checks for unwanted errors leave HLC's elaboration-category
//   diagnostics out. A parameter's value may be checked during elaboration,
//   so an elaboration-category diagnostic for T's default does count.
//
// Every assertion follows from the LRM, except NoCascadeErrors (HLC error
// recovery), the single-SourceFile precondition in SourceFileHasNoMacros (run
// configuration), and the lines diagnostics are matched against (the
// convention above).
//
// WHAT IS CHECKED (one fact per test)
//
//   ReportsAnErrorForTheTypeDefaultOfT   at least one fatal, syntax or error
//                                        diagnostic of any category but the
//                                        23.3.1 one, on line 6 or 7
//   NoErrorsOnTheLegalLines1To5          LRM: no such diagnostic outside
//                                        elaboration on lines 1-5
//   NoCascadeErrors                      HLC error recovery, not an LRM rule:
//                                        none past the end of the file, or
//                                        without a line
//   ReportsNoMisreadingDiagnostics       no "unknown macro", identifier length
//                                        or non-ASCII diagnostic
//   SourceFileHasNoMacros                one SourceFile, no macro definitions
//                                        or usages
//   NoDesignObjectsDeclared              no module, interface, program,
//                                        primitive, configuration, checker or
//                                        variable comes from this file
//
// No check is gated on Design::getElaborated(): the file has nothing that
// elaborates or reduces. The 23.3.1 error an elaborating run may report is
// left out, not asserted.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Which code, which error-class severity, which phase and how many
//     diagnostics T's default draws, and their message text and column. The
//     LRM requires no particular form for a diagnostic: "The compiler shall
//     maintain the current line number and file name of the file being
//     compiled" (22.12), but that information only "can be used for error
//     messages" (22.12), and no column is assigned to a diagnostic. Nearest
//     assertion: ReportsAnErrorForTheTypeDefaultOfT.
//   - Any object HLC records for the class declaration on lines 6-7: the class
//     Foo or its parameter T, in the Design's class or parameter lists or in
//     the SourceFile's includes and directives. The declaration is illegal,
//     so whatever HLC records for it is a model of illegal code, which this
//     test does not assert on. That lines 6-7 declare nothing but a class is
//     checked by NoDesignObjectsDeclared. Nearest assertion:
//     ReportsAnErrorForTheTypeDefaultOfT.
//   - A package object for the compilation unit of this file. Foo is in the
//     compilation-unit scope (3.12.1), and the VPI object model represents a
//     compilation unit as a package with vpiUnit (37.10), although "the
//     compilation-unit scope is not a package" (3.12.1). Whether HLDB lists
//     one for this file is a tool convention, and it would be HLC's model of
//     the scope that holds the illegal lines 6-7. Nearest assertion:
//     NoDesignObjectsDeclared.
//   - Run-time behavior. The file has no procedural code and no new (8.4), so
//     it creates no object and nothing in it exists only while simulation
//     time advances; this is permanently out of scope. Nearest assertion:
//     NoDesignObjectsDeclared.
//   - Elaboration-category diagnostics other than the required error. The
//     23.3.1 error for a design without a top-level module is legitimate
//     here. Nearest assertions: NoErrorsOnTheLegalLines1To5 and
//     NoCascadeErrors (every other error-class diagnostic).
//   - Warnings and informational diagnostics. The LRM neither requires nor
//     forbids them. Nearest assertion: ReportsAnErrorForTheTypeDefaultOfT.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/ErrorReporting/Location.h>
#include <hlc/Tests/Test.h>

#include <hldb/checker_decl.h>
#include <hldb/config_decl.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/module.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/preproc_macro_instance.h>
#include <hldb/program.h>
#include <hldb/source_file.h>
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
constexpr std::string_view kSourceFileName = "class_test_14.sv";

// The line of T's default, and the line where the declaration and the file
// end.
constexpr uint32_t kDefaultLine = 6;
constexpr uint32_t kEndOfFileLine = 7;

// Upper bound for "any later line".
constexpr uint32_t kAnyLaterLine = std::numeric_limits<uint32_t>::max();

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

// The line a diagnostic points at, or 0 when it carries no location.
uint32_t lineOf(const Error &error) { return error.getLocations().empty() ? 0 : error.getLocations().front().m_line; }

// True when HLC's error table gives 'error' a fatal, syntax or error severity.
bool isErrorClass(const Error &error) {
  const ErrorDefinition::ErrorMap &errorInfoMap = ErrorDefinition::getErrorInfoMap();
  const ErrorDefinition::ErrorMap::const_iterator errorInfo = errorInfoMap.find(error.getType());
  if (errorInfo == errorInfoMap.end()) return false;
  const ErrorDefinition::ErrorSeverity severity = errorInfo->second.m_severity;
  return (severity == ErrorDefinition::FATAL) || (severity == ErrorDefinition::SYNTAX) ||
         (severity == ErrorDefinition::ERROR);
}

// True when 'error' can be the error that T's default requires: an
// error-class diagnostic of any category, elaboration included, other than
// the one for a design without a top-level module (23.3.1).
bool isRequiredErrorCandidate(const Error &error) {
  return isErrorClass(error) && (error.getType() != ErrorDefinition::ELAB_NO_TOP_LEVEL_MODULE);
}

// True when 'error' is an error-class diagnostic outside HLC's elaboration
// category. Elaboration diagnostics are left out of the checks for unwanted
// errors because a design without a top-level module may draw one (23.3.1).
bool isSourceError(const Error &error) {
  const ErrorDefinition::ErrorMap &errorInfoMap = ErrorDefinition::getErrorInfoMap();
  const ErrorDefinition::ErrorMap::const_iterator errorInfo = errorInfoMap.find(error.getType());
  return isErrorClass(error) && (errorInfo->second.m_category != ErrorDefinition::ELAB);
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

class ClassTest14Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_test_14.hlc"}); }
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

  // Number of diagnostics that can be the required error (see
  // isRequiredErrorCandidate) and point at a line from 'firstLine' to
  // 'lastLine'.
  static size_t countRequiredErrorCandidatesOnLines(uint32_t firstLine, uint32_t lastLine) {
    if (getErrorContainer() == nullptr) return 0;
    size_t count = 0;
    for (const Error &error : getErrorContainer()->getErrors()) {
      const uint32_t line = lineOf(error);
      if (isRequiredErrorCandidate(error) && (line >= firstLine) && (line <= lastLine)) ++count;
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

  // Every source error without a line (no location, or line 0), described;
  // empty when there is none.
  static std::string describeUnlocatedSourceErrors() {
    if (getErrorContainer() == nullptr) return "no error container";
    std::string description;
    for (const Error &error : getErrorContainer()->getErrors()) {
      if (isSourceError(error) && (lineOf(error) == 0)) description += describeError(error);
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

// T is a value parameter (A.1.3) and its default is the data type int, but "A
// value parameter ... can only be set to an expression of literals, value
// parameters or local parameters, genvars, enumerated names, or a constant
// function of these" (6.20.2). The LRM requires an error and does not say
// which kind or in which phase, so any fatal, syntax or error diagnostic on
// line 6 or 7 other than the 23.3.1 one satisfies it.
TEST_F(ClassTest14Test, ReportsAnErrorForTheTypeDefaultOfT) {
  EXPECT_GE(countRequiredErrorCandidatesOnLines(kDefaultLine, kEndOfFileLine), 1u)
      << "no error on line 6 or 7;" << describeAllErrors();
}

// LRM: lines 1-5 are one block comment (5.4). An error on any of them would
// mean HLC rejected legal text.
TEST_F(ClassTest14Test, NoErrorsOnTheLegalLines1To5) {
  EXPECT_EQ(describeSourceErrorsOnLines(1, kDefaultLine - 1), "");
}

// HLC error recovery, not an LRM rule. The file ends on line 7, and the LRM
// does not define what a tool does after reporting an illegal declaration. An
// error past the end of the file, or one without a line, would be a cascade
// from it.
TEST_F(ClassTest14Test, NoCascadeErrors) {
  EXPECT_EQ(describeSourceErrorsOnLines(kEndOfFileLine + 1, kAnyLaterLine), "");
  EXPECT_EQ(describeUnlocatedSourceErrors(), "");
}

// Each of these diagnostics, at any severity, would mean HLC misread the
// file, whatever it reports for T's default.
TEST_F(ClassTest14Test, ReportsNoMisreadingDiagnostics) {
  // The file contains no grave accent (`), which begins every macro usage
  // (22.5.1), so nothing in it is a macro usage.
  EXPECT_EQ(findError(ErrorDefinition::PP_UNKOWN_MACRO), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PA_UNKOWN_MACRO), nullptr);

  // Foo and T are within any identifier length limit, which "shall be at least
  // 1024 characters" (5.6).
  EXPECT_EQ(findError(ErrorDefinition::PA_MAX_LENGTH_IDENTIFIER), nullptr);

  // Every character of the file is ASCII.
  EXPECT_EQ(findError(ErrorDefinition::PP_NON_ASCII_CONTENT), nullptr);
}

// ---------------------------------------------------------------------------
// The source file
// ---------------------------------------------------------------------------

// The file is read once (run configuration). It has no `define (22.5.1) and no
// macro usage. Its includes are not checked: the class declaration is illegal
// (see WHAT IS NOT CHECKED).
TEST_F(ClassTest14Test, SourceFileHasNoMacros) {
  const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
  ASSERT_EQ(sourceFiles.size(), 1u);
  const hldb::SourceFile *const sourceFile = sourceFiles.front();

  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// ---------------------------------------------------------------------------
// Nothing else is declared
// ---------------------------------------------------------------------------

// Module, interface, program, primitive, configuration and checker
// declarations each contain their own keyword (module or macromodule,
// interface, program, primitive, config, checker; A.1.2, A.1.5, A.5.1), and
// none of these keywords appears in the file. A variable at file level needs
// a data_declaration (A.2.1.3). Lines 6-7 are illegal only because 6.20.2 does
// not let a value parameter be set to a data type, not because of their
// grammar: T=int is a param_assignment (A.2.4) whose constant_param_expression
// is the data_type int (A.8.3, A.2.2.1), so the tokens of lines 6-7 make up
// one complete class_declaration with nothing left over, and no reading of
// them yields any of these objects. Packages are not counted, and objects HLC
// may record for the class declaration itself are not checked (see WHAT IS NOT
// CHECKED).
TEST_F(ClassTest14Test, NoDesignObjectsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getTopModules()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getVariables()), 0u);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
