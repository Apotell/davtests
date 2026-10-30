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

// Test for tests/Google/generic/class/class_test_34.sv
//
// THE SOURCE
//
// Lines 1-5 are one block comment carrying the test metadata (tags 6.15 and
// 8.3, no expected-failure marker); this file has no license comment. The
// SystemVerilog text is:
//
//   6: class zzyyy;
//   7: extern function void set_port(dbg_pkg::analysis_port #(1,N) apb);
//   8: endclass
//
// Nothing follows endclass, not even a line ending, so the end of the file is
// on line 8. "White space shall contain the characters for spaces, tabs,
// newlines, formfeeds, and end of file" (5.3).
//
// The repository stores the file with LF line endings; a Windows checkout
// (core.autocrlf) turns them into CR LF. 5.3 lists "newlines" as white space
// but does not name the carriage return, so this test treats CR LF as one
// line ending and asserts nothing about the CR itself.
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// Lines 6-8 have the form of a class_declaration (8.3, A.1.2) of zzyyy whose
//   one class_item is an extern method prototype ("..." marks shortened
//   parts):
//
//     class_method ::= ... | extern { method_qualifier } method_prototype ;
//     function_prototype ::= function [ dynamic_override_specifiers ]
//         data_type_or_void function_identifier [ ( [ tf_port_list ] ) ]
//     tf_port_item ::= { attribute_instance } [ tf_port_direction ] [ var ]
//         data_type_or_implicit [ port_identifier { variable_dimension } ... ]
//     data_type ::= ... | class_type
//     class_type ::= ps_class_identifier [ parameter_value_assignment ] ...
//     ps_class_identifier ::= [ package_scope ] class_identifier
//     package_scope ::= package_identifier :: | $unit ::
//
//   (class_method: A.1.9; function_prototype: A.2.6; tf_port_item: A.2.7;
//   data_type, class_type: A.2.2.1; ps_class_identifier, package_scope:
//   A.9.3.) The argument "dbg_pkg::analysis_port #(1,N) apb" has the data
//   type "dbg_pkg::analysis_port #(1,N)": a class of the package dbg_pkg with
//   a parameter value assignment by ordered list (23.10.2.1). Its grammar is
//   sound, but the names it refers to are declared nowhere:
//   - "The left operand of the scope resolution operator :: shall be a class
//     type name, package name (see 26.2), covergroup type name, coverpoint
//     name, cross name (see 19.5, 19.6), typedef name, or type parameter name"
//     (8.23). The file declares no package, class or type named dbg_pkg.
//   - The rules on "references to package items (see 26.3)" are ones "where
//     the compilation of a package is required to precede references to it"
//     (3.12), and no package dbg_pkg is compiled at all, so there is no class
//     dbg_pkg::analysis_port, and no parameters of it to assign.
//   - N is declared nowhere either. Whether it is read as a value ("Data
//     shall be declared before they are used, apart from implicit nets",
//     6.5) or as a type ("The declaration of a user-defined data type shall
//     precede any reference to its type_identifier", 6.18), the reference is
//     illegal; with dbg_pkg unknown, nothing says which reading applies.
//   - The prototype has no out-of-block declaration (8.24). The LRM states no
//     rule for that (see class_test_35), so it does not decide this test.
//
// The file is therefore illegal: it breaks the "shall" of 8.23, and that of
//   6.5 or 6.18, and "shall" marks "mandatory requirements strictly to be
//   followed in order to conform to the standard" (1.5). This test takes that
//   to mean a conforming tool cannot accept the file, and expects an error;
//   the LRM has no single sentence that says so, and does not say whether one
//   diagnostic or one per name. Name resolution may happen during
//   elaboration, so an elaboration-category diagnostic satisfies the
//   requirement too.
//
// Where the error points is a test convention, not an LRM rule: the LRM never
//   requires a diagnostic to carry a line. The undeclared names are on line
//   7, so this test expects the error on line 7 and none on lines 1-5. Lines
//   6 and 8 open and close the class that holds the illegal prototype, so a
//   diagnostic there is neither required nor forbidden.
//
// This holds for the file compiled on its own, as its metadata names it. A
//   compilation that first adds files declaring a package dbg_pkg with a
//   class analysis_port, and N, is a different design.
//
// The fixture's metadata does not mark the file as expected to fail. This
//   test follows 8.23 on purpose: a red run here is an HLC deviation to
//   report, not a reason to weaken the test.
//
// Lines 1-5 are legal: one block comment (5.4).
//
// The file has no module, and "A design shall contain at least one top-level
//   module" (23.3.1). An elaborating run may report that, so that diagnostic
//   (HLC's ElabNoTopLevelModule) never counts as the required error, and
//   the checks for unwanted errors leave HLC's elaboration-category
//   diagnostics out.
//
// Every assertion follows from the LRM, except NoCascadeErrors (HLC error
// recovery), the single-SourceFile precondition in SourceFileHasNoMacros (run
// configuration), and the lines diagnostics are matched against (the
// convention above).
//
// WHAT IS CHECKED (one fact per test)
//
//   ReportsAnErrorForTheUndeclaredNames    at least one fatal, syntax or
//                                          error diagnostic of any category
//                                          but the 23.3.1 one, on line 7
//   NoErrorsOnTheLegalLines1To5            no fatal, syntax or error
//                                          diagnostic outside elaboration on
//                                          lines 1-5
//   NoCascadeErrors                        HLC error recovery, not an LRM
//                                          rule: none past the end of the
//                                          file, or without a line
//   ReportsNoMisreadingDiagnostics         no "unknown macro", identifier
//                                          length or non-ASCII diagnostic
//   SourceFileHasNoMacros                  one SourceFile, no macro
//                                          definitions or usages
//   NoDesignObjectsDeclared                no module, interface, program,
//                                          primitive, configuration, checker
//                                          or variable comes from this file
//
// No check is gated on Design::getElaborated(): the file has nothing that
// elaborates or reduces. The 23.3.1 error an elaborating run may report is
// left out, not asserted.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Which code, which error-class severity, which phase and how many
//     diagnostics line 7 draws, which names they mention, and their message
//     text and column. The LRM requires no particular form for a diagnostic:
//     "The compiler shall maintain the current line number and file name of
//     the file being compiled" (22.12), but that information only "can be
//     used for error messages" (22.12), and no column is assigned to a
//     diagnostic. Nearest assertion: ReportsAnErrorForTheUndeclaredNames.
//   - Diagnostics on lines 6 and 8, the class around the illegal prototype.
//     The LRM does not fix where an error is reported. Nearest assertions:
//     ReportsAnErrorForTheUndeclaredNames and NoCascadeErrors.
//   - Any object HLC records for the class declaration of lines 6-8: a class
//     zzyyy, its method set_port, the io decl apb and its typespec, or
//     entries in the SourceFile's includes and directives. The declaration is
//     illegal, so whatever HLC records for it is a model of illegal code,
//     which this test does not assert on. That it declares no variable or
//     design element is checked by NoDesignObjectsDeclared. Nearest
//     assertion: ReportsAnErrorForTheUndeclaredNames.
//   - A package object for the compilation unit of this file. The class is in
//     the compilation-unit scope (3.12.1), and the VPI object model
//     represents a compilation unit as a package with vpiUnit (37.10),
//     although "the compilation-unit scope is not a package" (3.12.1).
//     Whether HLDB lists one for this file is a tool convention, and it would
//     be HLC's model of the scope that holds the illegal class. Nearest
//     assertion: NoDesignObjectsDeclared.
//   - Run-time behavior. The file has no procedural code and no new (8.4), so
//     it creates no object and nothing in it exists only while simulation
//     time advances; this is permanently out of scope. Nearest assertion:
//     NoDesignObjectsDeclared.
//   - Elaboration-category diagnostics other than the required error. The
//     23.3.1 error for a design without a top-level module is legitimate
//     here. Nearest assertions: NoErrorsOnTheLegalLines1To5 and
//     NoCascadeErrors (every other error-class diagnostic).
//   - Warnings and informational diagnostics. The LRM neither requires nor
//     forbids them. Nearest assertion: ReportsAnErrorForTheUndeclaredNames.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
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
constexpr std::string_view kSourceFileName = "class_test_34.sv";

// The line of the prototype that names the undeclared names, the last line
// of the legal block comment, and the line where the file ends.
constexpr uint32_t kIllegalLine = 7;
constexpr uint32_t kLastCommentLine = 5;
constexpr uint32_t kLastLine = 8;

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
uint32_t lineOf(const Event &error) { return error.getArguments().empty() ? 0 : error.getArguments().front().m_line; }

// True when HLC's error table gives 'error' a fatal, syntax or error severity.
bool isErrorClass(const Event &error) {
  const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
  if (errorInfo == nullptr) return false;
  const EventSeverity severity = errorInfo->m_severity;
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}

// True when 'error' can be the error that the undeclared names require: an
// error-class diagnostic of any category, elaboration included, other than
// the one for a design without a top-level module (23.3.1).
bool isRequiredErrorCandidate(const Event &error) {
  return isErrorClass(error) && (error.getType() != EventType::ElabNoTopLevelModule);
}

// True when 'error' is an error-class diagnostic outside HLC's elaboration
// category. Elaboration diagnostics are left out of the checks for unwanted
// errors because a design without a top-level module may draw one (23.3.1).
bool isSourceError(const Event &error) {
  const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
  return isErrorClass(error) && (errorInfo->m_category->m_name != "Elab");
}

// One diagnostic as " [<code> line <n>: <message template>]", so that a red
// run shows what was reported and where.
std::string describeError(const Event &error) {
  const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
  std::string description = " [" + std::to_string(static_cast<int32_t>(error.getType()));
  description += " line " + std::to_string(lineOf(error));
  if (errorInfo != nullptr) description += ": " + std::string(errorInfo->m_lines[0].m_text);
  return description + "]";
}
}  // namespace

class ClassTest34Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_test_34.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // The diagnostics of the run, or nullptr.
  static const EventListener *getEventListener() {
    return (m_session == nullptr) ? nullptr : m_session->getEventListener();
  }

  // Every diagnostic of the run, described; for failure messages.
  static std::string describeAllErrors() {
    if (getEventListener() == nullptr) return "no error container";
    std::string description;
    for (const Event &error : getEventListener()->getEvents()) description += describeError(error);
    return description;
  }

  // Number of diagnostics that can be the required error (see
  // isRequiredErrorCandidate) and point at 'line'.
  static size_t countRequiredErrorCandidatesOnLine(uint32_t line) {
    if (getEventListener() == nullptr) return 0;
    size_t count = 0;
    for (const Event &error : getEventListener()->getEvents()) {
      if (isRequiredErrorCandidate(error) && (lineOf(error) == line)) ++count;
    }
    return count;
  }

  // Every source error on a line from 'firstLine' to 'lastLine', described;
  // empty when there is none. A run without an error container is reported,
  // not passed.
  static std::string describeSourceErrorsOnLines(uint32_t firstLine, uint32_t lastLine) {
    if (getEventListener() == nullptr) return "no error container";
    std::string description;
    for (const Event &error : getEventListener()->getEvents()) {
      const uint32_t line = lineOf(error);
      if (isSourceError(error) && (line >= firstLine) && (line <= lastLine)) description += describeError(error);
    }
    return description;
  }

  // Every source error without a line (no location, or line 0), described;
  // empty when there is none.
  static std::string describeUnlocatedSourceErrors() {
    if (getEventListener() == nullptr) return "no error container";
    std::string description;
    for (const Event &error : getEventListener()->getEvents()) {
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

// The argument type is dbg_pkg::analysis_port #(1,N), but "The left operand
// of the scope resolution operator :: shall be a class type name, package name
// ..., typedef name, or type parameter name" (8.23), nothing named dbg_pkg is
// declared, and neither is N (6.5, 6.18). The LRM does not say which kind of
// error, in which phase, or how many, so any fatal, syntax or error
// diagnostic on line 7 other than the 23.3.1 one satisfies it.
TEST_F(ClassTest34Test, ReportsAnErrorForTheUndeclaredNames) {
  EXPECT_GE(countRequiredErrorCandidatesOnLine(kIllegalLine), 1u) << "no error on line 7;" << describeAllErrors();
}

// LRM: lines 1-5 are one block comment (5.4). An error on any of them would
// mean HLC rejected legal text.
TEST_F(ClassTest34Test, NoErrorsOnTheLegalLines1To5) {
  EXPECT_EQ(describeSourceErrorsOnLines(1, kLastCommentLine), "");
}

// HLC error recovery, not an LRM rule. The file ends on line 8, and the LRM
// does not define what a tool does after reporting an illegal declaration. An
// error past the end of the file, or one without a line, would be a cascade
// from it.
TEST_F(ClassTest34Test, NoCascadeErrors) {
  EXPECT_EQ(describeSourceErrorsOnLines(kLastLine + 1, kAnyLaterLine), "");
  EXPECT_EQ(describeUnlocatedSourceErrors(), "");
}

// Each of these diagnostics, at any severity, would mean HLC misread the
// file, whatever it reports for line 7.
TEST_F(ClassTest34Test, ReportsNoMisreadingDiagnostics) {
  // The file contains no grave accent (`), which begins every macro usage
  // (22.5.1), so nothing in it is a macro usage.
  EXPECT_EQ(findEvent(EventType::PpUnkownMacro), nullptr);
  EXPECT_EQ(findEvent(EventType::PaUnkownMacro), nullptr);

  // The longest identifier, analysis_port, has 13 characters, within any
  // limit, which "shall be at least 1024 characters" (5.6).
  EXPECT_EQ(findEvent(EventType::PaMaxLengthIdentifier), nullptr);

  // Every character of the file is ASCII.
  EXPECT_EQ(findEvent(EventType::PpNonAsciiContent), nullptr);
}

// ---------------------------------------------------------------------------
// The source file
// ---------------------------------------------------------------------------

// The file is read once (run configuration). It has no `define (22.5.1) and no
// macro usage. Its includes are not checked: the class declaration is illegal
// (see WHAT IS NOT CHECKED).
TEST_F(ClassTest34Test, SourceFileHasNoMacros) {
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
// a data_declaration (A.2.1.3), and the tokens of lines 6-8 make up one
// complete class_declaration with nothing left over: the file is illegal only
// because dbg_pkg and N are declared nowhere (8.23, 6.5, 6.18), not because of
// its grammar. So none of these comes from the file. Packages are not
// counted, and objects HLC may record for the class declaration itself are
// not checked (see WHAT IS NOT CHECKED).
TEST_F(ClassTest34Test, NoDesignObjectsDeclared) {
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
