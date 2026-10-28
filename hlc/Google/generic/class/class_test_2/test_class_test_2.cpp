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

// Test for tests/Google/generic/class/class_test_2.sv
//
// THE SOURCE
//
// Lines 1-5 are one block comment carrying the test metadata (tags 6.15 and
// 8.3, no expected-failure marker); this file has no license comment. The
// SystemVerilog text is one line:
//
//   6: class static Foo; endclass
//
// Nothing follows endclass, not even a line ending, so the end of the file is
// on line 6. "White space shall contain the characters for spaces, tabs,
// newlines, formfeeds, and end of file" (5.3), so endclass ends there.
//
// The repository stores the file with LF line endings; a Windows checkout
// (core.autocrlf) turns them into CR LF. 5.3 lists "newlines" as white space
// but does not name the carriage return, so this test treats CR LF as one
// line ending and asserts nothing about the CR itself.
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// A class declaration has this form (8.3; A.1.2 for class_declaration, A.2.7
// for final_specifier; "..." marks shortened parts):
//
//     class_declaration ::= [ virtual ] class [ final_specifier ] class_identifier
//         [ parameter_port_list ] [ extends class_type ... ] [ implements ... ] ;
//         { class_item } endclass [ : class_identifier ]
//     final_specifier ::= : final
//
//   - After the keyword class, the grammar allows only a final_specifier,
//     which begins with ":", or the class_identifier. Line 6 has static
//     there. The file has no `begin_keywords, so the reserved keywords are
//     the implementation's default set (22.14); this test takes that set to
//     be Table B.1, which lists class, static and endclass. "A keyword (see
//     5.6.2) may not be used as a user-defined identifier" (5.6), so static
//     cannot be the class_identifier. Even if it were, Foo follows it, and
//     after the class_identifier only a parameter_port_list (which begins
//     with "#"), extends, implements or ";" may come. So line 6 matches no
//     class_declaration under any keyword set.
//   - No other description begins with the keyword class: an interface class
//     begins with interface (A.1.2), and a forward typedef begins with
//     typedef (A.2.1.3).
//   - The class header has no place for a lifetime, unlike the module,
//     interface, program and package headers (A.1.2). A default lifetime
//     qualifier is for a task, function or block "defined within a module,
//     interface, package, or program" (6.21), and "The lifetime of methods
//     declared as part of a class type shall be automatic. It shall be
//     illegal to declare a class method with a static lifetime" (8.6).
//     37.3.7 still describes vpiAutomatic as "a property of a class defn ...
//     indicating the default lifetime for variables of any of its declared
//     tasks/functions"; that is a property the object model reports, not
//     syntax for the class header, and by 8.6 and 6.21 its value is fixed.
//
// Line 6 is therefore not SystemVerilog source text under the grammar of
//   Annex A, which is normative, and "shall" marks "mandatory requirements
//   strictly to be followed in order to conform to the standard" (1.5). A
//   conforming tool cannot accept line 6, so this test expects an error. The
//   LRM has no single sentence that requires one; the requirement is this
//   inference.
//
// Where the error points is a test convention, not an LRM rule: the LRM never
//   requires a diagnostic to carry a line. Line 6 is the only line of
//   SystemVerilog text and the file ends on it, so this test expects the
//   error on line 6 and none on lines 1-5.
//
// The fixture's metadata does not mark the file as expected to fail. This
//   test follows 1800-2023 on purpose: a red run here is an HLC deviation (or
//   an older-edition reading) to report, not a reason to weaken the test.
//
// Lines 1-5 are legal: one block comment (5.4).
//
// The file has no module, and "A design shall contain at least one top-level
//   module" (23.3.1). An elaborating run may report that. That diagnostic
//   cannot stand in for the error that line 6 requires, so the required-error
//   check ignores it, and the checks for unwanted errors leave out HLC's whole
//   elaboration category.
//
// Every assertion follows from the LRM, except NoCascadeErrors (HLC error
// recovery), the single-SourceFile precondition in SourceFileHasNoMacros (run
// configuration), and the lines diagnostics are matched against (the
// convention above).
//
// WHAT IS CHECKED (one fact per test)
//
//   ReportsAnErrorForStaticAfterClass   at least one fatal, syntax or error
//                                       diagnostic on line 6, other than the
//                                       23.3.1 one
//   NoErrorsOnTheLegalLines1To5         no fatal, syntax or error diagnostic
//                                       outside elaboration on lines 1-5
//   NoCascadeErrors                     HLC error recovery, not an LRM rule:
//                                       none past the end of the file, or
//                                       without a line
//   ReportsNoMisreadingDiagnostics      no "unknown macro", identifier length
//                                       or non-ASCII diagnostic
//   SourceFileHasNoMacros               one SourceFile, no macro definitions
//                                       or usages
//   NoDesignObjectsDeclared             no module, interface, program,
//                                       primitive, configuration or checker
//                                       comes from this file
//
// No check is gated on Design::getElaborated(): the file has nothing that
// elaborates or reduces. The 23.3.1 error an elaborating run may report is
// left out, not asserted.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Which code, which error-class severity, which phase and how many
//     diagnostics line 6 draws, and their message text and column. The LRM
//     requires no particular form for a diagnostic: "The compiler shall
//     maintain the current line number and file name of the file being
//     compiled" (22.12), but that information only "can be used for error
//     messages" (22.12), and no column is assigned to a diagnostic. A syntax
//     diagnostic and a "reserved keyword" diagnostic for static are both
//     accepted, provided HLC's error table gives them a fatal, syntax or error
//     severity. Nearest assertion: ReportsAnErrorForStaticAfterClass.
//   - Any object HLC records for line 6: a class Foo, its vpiAutomatic, a
//     variable Foo, or entries in the SourceFile's includes and directives.
//     The line is illegal, so whatever HLC records for it is a model of
//     illegal code, which this test does not assert on. A variable is a
//     likely record: without the keyword class, "static Foo;" matches
//     data_declaration ([ lifetime ] data_type_or_implicit
//     list_of_variable_decl_assignments ;, A.2.1.3) with an empty implicit
//     type, and only footnote 14 of Annex A makes that illegal. Nearest
//     assertion: ReportsAnErrorForStaticAfterClass.
//   - A package object for the compilation unit of this file. Line 6 is in
//     the compilation-unit scope (3.12.1), and the VPI object model
//     represents a compilation unit as a package with vpiUnit (37.10),
//     although "the compilation-unit scope is not a package" (3.12.1).
//     Whether HLDB lists one for this file is a tool convention, and it would
//     be HLC's model of the scope that holds the illegal line 6. Nearest
//     assertion: NoDesignObjectsDeclared.
//   - Run-time behavior. The file has no procedural code and no new (8.4), so
//     it creates no object and nothing in it exists only while simulation
//     time advances; this is permanently out of scope. Nearest assertion:
//     NoDesignObjectsDeclared.
//   - Elaboration-category diagnostics off line 6. The 23.3.1 error for a
//     design without a top-level module is legitimate here. Nearest
//     assertions: NoErrorsOnTheLegalLines1To5 and NoCascadeErrors (every
//     other error-class diagnostic).
//   - Warnings and informational diagnostics. The LRM neither requires nor
//     forbids them. Nearest assertion: ReportsAnErrorForStaticAfterClass.

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

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "class_test_2.sv";

// The illegal line, which is also the line where the file ends.
constexpr uint32_t kIllegalLine = 6;

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

// True when 'error' can be the error that line 6 requires: an error-class
// diagnostic of any category, elaboration included, other than the one for a
// design without a top-level module (23.3.1).
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

class ClassTest2Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_test_2.hlc"}); }
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
  // isRequiredErrorCandidate) and point at 'line'.
  static size_t countRequiredErrorCandidatesOnLine(uint32_t line) {
    if (getErrorContainer() == nullptr) return 0;
    size_t count = 0;
    for (const Error &error : getErrorContainer()->getErrors()) {
      if (isRequiredErrorCandidate(error) && (lineOf(error) == line)) ++count;
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

// After class comes ": final" or the class_identifier (8.3, A.1.2, A.2.7), and
// the keyword static can be neither (5.6); a class header has no place for a
// lifetime (6.21, 8.6). The LRM does not say which kind of error, or in which
// phase, so any fatal, syntax or error diagnostic on line 6 other than the
// 23.3.1 one satisfies it.
TEST_F(ClassTest2Test, ReportsAnErrorForStaticAfterClass) {
  EXPECT_GE(countRequiredErrorCandidatesOnLine(kIllegalLine), 1u) << "no error on line 6;" << describeAllErrors();
}

// LRM: lines 1-5 are one block comment (5.4). An error on any of them would
// mean HLC rejected legal text.
TEST_F(ClassTest2Test, NoErrorsOnTheLegalLines1To5) { EXPECT_EQ(describeSourceErrorsOnLines(1, kIllegalLine - 1), ""); }

// HLC error recovery, not an LRM rule. The file ends on line 6, and the LRM
// does not define what a tool does after reporting an illegal declaration. An
// error past the end of the file, or one without a line, would be a cascade
// from it.
TEST_F(ClassTest2Test, NoCascadeErrors) {
  EXPECT_EQ(describeSourceErrorsOnLines(kIllegalLine + 1, kAnyLaterLine), "");
  EXPECT_EQ(describeUnlocatedSourceErrors(), "");
}

// Each of these diagnostics, at any severity, would mean HLC misread the
// file, whatever it reports for line 6.
TEST_F(ClassTest2Test, ReportsNoMisreadingDiagnostics) {
  // The file contains no grave accent (`), which begins every macro usage
  // (22.5.1), so nothing in it is a macro usage.
  EXPECT_EQ(findError(ErrorDefinition::PP_UNKOWN_MACRO), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PA_UNKOWN_MACRO), nullptr);

  // Foo is within any identifier length limit, which "shall be at least 1024
  // characters" (5.6).
  EXPECT_EQ(findError(ErrorDefinition::PA_MAX_LENGTH_IDENTIFIER), nullptr);

  // Every character of the file is ASCII.
  EXPECT_EQ(findError(ErrorDefinition::PP_NON_ASCII_CONTENT), nullptr);
}

// ---------------------------------------------------------------------------
// The source file
// ---------------------------------------------------------------------------

// The file is read once (run configuration). It has no `define (22.5.1) and no
// macro usage. Its includes are not checked: line 6 is illegal (see WHAT IS
// NOT CHECKED).
TEST_F(ClassTest2Test, SourceFileHasNoMacros) {
  const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
  ASSERT_EQ(sourceFiles.size(), 1u);
  const hldb::SourceFile *const sourceFile = sourceFiles.front();

  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// ---------------------------------------------------------------------------
// Nothing else is declared
// ---------------------------------------------------------------------------

// Each of these design elements contains its own keyword (module or
// macromodule, interface, program, primitive, config or checker; A.1.2,
// A.1.5, A.5.1), and none of these keywords appears in the file. So none of
// them comes from the file, however HLC recovers from line 6. Packages are not
// counted, and objects HLC may record for line 6 itself are not checked (see
// WHAT IS NOT CHECKED).
TEST_F(ClassTest2Test, NoDesignObjectsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getTopModules()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
