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

// Test for tests/Google/generic/class/class_test_46.sv
//
// THE SOURCE
//
// Lines 1-5 are one block comment carrying the test metadata (tags 6.15 and
// 8.3, no expected-failure marker); this file has no license comment. The
// SystemVerilog text is:
//
//    6: class macros_as_class_item;
//    7:  `uvm_component_utils(my_type)
//    8:  `uvm_component_utils_begin(my_type)
//    9:    `uvm_field_object(blah1, F1)
//   10:    `uvm_field_event(blah2, F2)
//   11:    `uvm_field_string(blah3, F3)
//   12:  `uvm_component_utils_end
//   13: endclass
//
// Nothing follows endclass, not even a line ending, so the end of the file is
// on line 13. "White space shall contain the characters for spaces, tabs,
// newlines, formfeeds, and end of file" (5.3).
//
// The repository stores the file with LF line endings; a Windows checkout
// (core.autocrlf) turns them into CR LF. 5.3 lists "newlines" as white space
// but does not name the carriage return, so this test treats CR LF as one
// line ending and asserts nothing about the CR itself.
//
// WHAT THE LRM (IEEE 1800-2023) SAYS ABOUT IT
//
// Lines 6 and 13 open and close a class_declaration (8.3, A.1.2) of
//   macros_as_class_item. Its body, lines 7-12, is six macro usages:
//   `uvm_component_utils and `uvm_component_utils_begin with one actual
//   argument each, three `uvm_field_... usages with two, and
//   `uvm_component_utils_end without arguments.
//
//   - "The ` character (the ASCII value 0x60, called grave accent) introduces
//     a language construct used to implement compiler directives" (5.6.4).
//   - None of `uvm_component_utils, `uvm_component_utils_begin,
//     `uvm_component_utils_end, `uvm_field_event, `uvm_field_object or
//     `uvm_field_string is a compiler directive (22.1), and the file has no
//     `define (22.5.1), so none of them names a text macro.
//   - A text macro can be used only once it is defined: "After a text macro
//     is defined, it can be used in the source description by using the (`)
//     character, followed by the macro name. The compiler shall substitute the
//     text of the macro for the string `text_macro_name and any actual
//     arguments that follow it" (22.5.1). With no definition there is no text
//     to substitute, and a grave accent followed by a name is part of no
//     production of Annex A, which is normative.
//
// The file is therefore not SystemVerilog source text, and "shall" marks
//   "mandatory requirements strictly to be followed in order to conform to the
//   standard" (1.5). This test takes that to mean a conforming tool cannot
//   accept the file, and expects an error. The LRM has no sentence that says
//   so for an undefined macro; of one it says only that it "has no value,
//   just as if it had never been defined" (22.5.2). The requirement is this
//   inference. The LRM does not say whether one diagnostic or one per usage.
//
// Where the error points is a test convention, not an LRM rule: the LRM never
//   requires a diagnostic to carry a line. The macro usages are on
//   lines 7-12, so this test expects the error there and none on lines 1-5.
//   Lines 6 and 13 open and close the class that holds the usages, so a
//   diagnostic there is neither required nor forbidden.
//
// This holds for the file compiled on its own, as its metadata names it. "A
//   compiler directive in one description file can, therefore, control
//   compilation behavior in multiple description files" (5.6.4), so a
//   compilation that first reads a file defining these macros is a different
//   design.
//
// The fixture's metadata does not mark the file as expected to fail. This
//   test follows 5.6.4 and 22.5.1 on purpose: a red run here is an HLC
//   deviation to report, not a reason to weaken the test.
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
// recovery), the single-SourceFile precondition in
// SourceFileHasNoMacroDefinitions (run configuration), and the lines
// diagnostics are matched against (the convention above).
//
// WHAT IS CHECKED (one fact per test)
//
//   ReportsAnErrorForTheUndefinedMacros  at least one fatal, syntax or error
//                                        diagnostic of any category but the
//                                        23.3.1 one, on one of lines 7-12
//   NoErrorsOnTheLegalLines1To5          no fatal, syntax or error diagnostic
//                                        outside elaboration on lines 1-5
//   NoCascadeErrors                      HLC error recovery, not an LRM rule:
//                                        none past the end of the file, or
//                                        without a line
//   ReportsNoMisreadingDiagnostics       no identifier length or non-ASCII
//                                        diagnostic
//   SourceFileHasNoMacroDefinitions      one SourceFile, no macro definitions
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
//     diagnostics lines 7-12 draw, which macros they name, and their message
//     text and column. An "unknown macro" diagnostic is the expected kind, but
//     any error-class diagnostic satisfies the LRM. The LRM requires no
//     particular form for a diagnostic: "The compiler shall maintain the
//     current line number and file name of the file being compiled" (22.12),
//     but that information only "can be used for error messages" (22.12), and
//     no column is assigned to a diagnostic. Nearest assertion:
//     ReportsAnErrorForTheUndefinedMacros.
//   - Diagnostics on lines 6 and 13, the class around the usages. The LRM
//     does not fix where an error is reported. Nearest assertions:
//     ReportsAnErrorForTheUndefinedMacros and NoCascadeErrors.
//   - Any object HLC records for the class declaration of lines 6-13: a
//     class macros_as_class_item, macro usages in the SourceFile's macro
//     instances, or its includes and directives. The usages are illegal, so
//     whatever HLC records for them is a model of illegal code, which this test
//     does not assert on. That the file declares no macro, variable or design
//     element is checked by SourceFileHasNoMacroDefinitions and
//     NoDesignObjectsDeclared. Nearest assertion:
//     ReportsAnErrorForTheUndefinedMacros.
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
//     forbids them. Nearest assertion: ReportsAnErrorForTheUndefinedMacros.

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
constexpr std::string_view kSourceFileName = "class_test_46.sv";

// The first and last line with a macro usage, the last line of the legal
// block comment, and the line where the file ends.
constexpr uint32_t kFirstUsageLine = 7;
constexpr uint32_t kLastUsageLine = 12;
constexpr uint32_t kLastCommentLine = 5;
constexpr uint32_t kLastLine = 13;

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

// True when 'error' can be the error that the undefined macros require: an
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

class ClassTest46Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_test_46.hlc"}); }
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
  // isRequiredErrorCandidate) and point at a line from 'firstLine' to
  // 'lastLine'.
  static size_t countRequiredErrorCandidatesOnLines(uint32_t firstLine, uint32_t lastLine) {
    if (getEventListener() == nullptr) return 0;
    size_t count = 0;
    for (const Event &error : getEventListener()->getEvents()) {
      const uint32_t line = lineOf(error);
      if (isRequiredErrorCandidate(error) && (line >= firstLine) && (line <= lastLine)) ++count;
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

// A macro is used "After a text macro is defined" (22.5.1), and none of the
// macros of lines 7-12 is defined, so the usages are not source text. The
// LRM does not say which kind of error, in which phase, or how many, so any
// fatal, syntax or error diagnostic on one of lines 7-12 other than the 23.3.1
// one satisfies it.
TEST_F(ClassTest46Test, ReportsAnErrorForTheUndefinedMacros) {
  EXPECT_GE(countRequiredErrorCandidatesOnLines(kFirstUsageLine, kLastUsageLine), 1u)
      << "no error on lines 7-12;" << describeAllErrors();
}

// LRM: lines 1-5 are one block comment (5.4). An error on any of them would
// mean HLC rejected legal text.
TEST_F(ClassTest46Test, NoErrorsOnTheLegalLines1To5) {
  EXPECT_EQ(describeSourceErrorsOnLines(1, kLastCommentLine), "");
}

// HLC error recovery, not an LRM rule. The file ends on line 13, and the LRM
// does not define what a tool does after reporting an illegal construct. An
// error past the end of the file, or one without a line, would be a cascade
// from it.
TEST_F(ClassTest46Test, NoCascadeErrors) {
  EXPECT_EQ(describeSourceErrorsOnLines(kLastLine + 1, kAnyLaterLine), "");
  EXPECT_EQ(describeUnlocatedSourceErrors(), "");
}

// Each of these diagnostics, at any severity, would mean HLC misread the
// file, whatever it reports for lines 7-12. An "unknown macro" diagnostic is
// not among them: it is the expected kind of error here.
TEST_F(ClassTest46Test, ReportsNoMisreadingDiagnostics) {
  // The longest identifier, uvm_component_utils_begin, has 25 characters,
  // within any limit, which "shall be at least 1024 characters" (5.6).
  EXPECT_EQ(findEvent(EventType::PaMaxLengthIdentifier), nullptr);

  // Every character of the file is ASCII.
  EXPECT_EQ(findEvent(EventType::PpNonAsciiContent), nullptr);
}

// ---------------------------------------------------------------------------
// The source file
// ---------------------------------------------------------------------------

// The file is read once (run configuration). It has no `define (22.5.1), so
// it defines no macro. Its macro usages and includes are not checked: the
// usages are illegal (see WHAT IS NOT CHECKED).
TEST_F(ClassTest46Test, SourceFileHasNoMacroDefinitions) {
  const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
  ASSERT_EQ(sourceFiles.size(), 1u);
  const hldb::SourceFile *const sourceFile = sourceFiles.front();

  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
}

// ---------------------------------------------------------------------------
// Nothing else is declared
// ---------------------------------------------------------------------------

// Module, interface, program, primitive, configuration and checker
// declarations each contain their own keyword (module or macromodule,
// interface, program, primitive, config, checker; A.1.2, A.1.5, A.5.1), and
// none of these keywords appears in the file, not even inside the macro
// arguments. A variable at file level needs a data_declaration (A.2.1.3), and
// the only text outside the class is "class macros_as_class_item;" and
// "endclass". So none of these comes from the file, whatever HLC makes of the
// usages. Packages are not counted, and objects HLC may record for the class
// itself are not checked (see WHAT IS NOT CHECKED).
TEST_F(ClassTest46Test, NoDesignObjectsDeclared) {
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
