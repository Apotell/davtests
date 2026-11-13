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

// Test for tests/Google/chapter-22/22.12--line-illegal-2.sv
//
// THE SOURCE
//
// Lines 1-7 are one-line comments with the license, lines 8-9 are blank and
// lines 10-16 are one block comment carrying the test metadata. The
// SystemVerilog text is one line, and no design element:
//
//   17: `line 1 somefile 2
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
//   The number 1 is a positive integer and the level 2 is one of 0, 1 and 2,
//   as 22.12 requires, but the file name somefile has no double quotes: it is
//   an identifier, not a string literal, and "The filename parameter shall be
//   a string literal" (22.12); Syntax 22-9 writes it between double quotes.
//   Line 17 is therefore illegal, and the LRM requires an error. It may point
//   at line 17 or at the end of the file just after it (line 18); both are
//   accepted.
//
// An illegal directive sets no new location: the location of the following
// line is not somefile:1. No text follows anyway.
//
// Lines 1-16 are legal: one-line comments, blank lines and one block comment
//   (5.4).
//
// somefile has no grave accent, so it is no macro usage, and a `line reads no
//   file (22.12), so nothing named somefile is opened.
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
// labelled as HLC error recovery. PpIllegalDirectiveInDesignElement is
// HLC's code for a directive placed inside a design element.
//
// WHAT IS CHECKED (one fact per test)
//
//   ReportsAnErrorForTheUnquotedFileName   at least one fatal, syntax or error
//                                          diagnostic, outside elaboration,
//                                          on line 17 or at the end of the
//                                          file (line 18)
//   NoErrorsOnTheLegalLines1To16           LRM: no such diagnostic on lines
//                                          1-16
//   NoCascadeErrors                        HLC error recovery, not an LRM
//                                          rule: none past the end of the
//                                          file, or without a location
//   ReportsNoMisreadingDiagnostics         no "directive in design element",
//                                          no "unknown macro", no failure to
//                                          open somefile and no non-ASCII
//                                          diagnostic
//   SourceFileHasNoIncludesOrMacros        one SourceFile, no includes, no
//                                          macro definitions or usages
//   NoDesignObjectsDeclared                nothing in the design comes from
//                                          this file, under its own name or
//                                          under somefile
//
// No check is gated on Design::getElaborated(): the file has nothing that
// elaborates or reduces. The 23.3.1 error an elaborating run may report is
// left out, not asserted.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Which code, which error-class severity and how many diagnostics line 17
//     draws, and their message text and column. The LRM requires an error but
//     not its form, and it numbers lines (22.12, 22.13) but not columns. A
//     syntax diagnostic and an "illegal directive value" are both accepted.
//     Nearest assertion: ReportsAnErrorForTheUnquotedFileName.
//   - A directive object for line 17 in any directive list (SourceFile or
//     Design). The LRM gives an illegal line no meaning, so neither the
//     contents nor the size of such a list is asserted. Nearest assertion:
//     ReportsAnErrorForTheUnquotedFileName.
//   - Run-time use of locations. How a simulator prints file names and line
//     numbers only exists while it runs; this is permanently out of scope.
//     Nearest assertion: ReportsAnErrorForTheUnquotedFileName.
//   - Elaboration-category diagnostics. The 23.3.1 error for a design without
//     a top-level module is legitimate here. Nearest assertions:
//     NoErrorsOnTheLegalLines1To16 and NoCascadeErrors (every other
//     error-class diagnostic).
//   - Warnings and informational diagnostics. The LRM neither requires nor
//     forbids them. Nearest assertion: ReportsAnErrorForTheUnquotedFileName.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "22.12--line-illegal-2.sv";

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
uint32_t lineOf(const Event &event) { return event.getArguments().empty() ? 0 : event.getArgument(0)->m_line; }

// True when HLC's event table gives 'event' a fatal, syntax or error
// severity outside the elaboration category. Elaboration diagnostics are left
// out because a design without a top-level module may draw one (23.3.1).
bool isSourceEvent(const Event &event) {
  const EventSeverity severity = event.getDefinition()->m_severity;
  const bool isErrorClass =
      (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) || (severity == EventSeverity::Error);
  return isErrorClass && (event.getDefinition()->m_category->m_name != std::string_view("Elab"));
}

// One event as hldb::renderEvent() prints it, one line per output line, so that a
// failing test shows what was reported and where.
std::string describeEvent(const Event &event) {
  std::ostringstream oss;
  const std::vector<std::string> lines = hldb::renderEvent(event);
  std::copy(lines.cbegin(), lines.cend(), std::ostream_iterator<std::string>(oss, "\n"));
  return oss.str();
}
}  // namespace

class LineIllegal2Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.12--line-illegal-2.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Every diagnostic of the run, described; for failure messages.
  static std::string describeAllEvents() {
    EventListener *const listener = m_session->getEventListener();
    std::string description;
    for (const Event &event : listener->getEvents()) description += describeEvent(event);
    return description;
  }

  // Number of source errors (see isSourceEvent) that point at 'line'.
  static size_t countSourceErrorsOnLine(uint32_t line) {
    EventListener *const listener = m_session->getEventListener();
    size_t count = 0;
    for (const Event &event : listener->getEvents()) {
      if (isSourceEvent(event) && (lineOf(event) == line)) ++count;
    }
    return count;
  }

  // Every source error on a line from 'firstLine' to 'lastLine', described;
  // empty when there is none.
  static std::string describeSourceErrorsOnLines(uint32_t firstLine, uint32_t lastLine) {
    EventListener *const listener = m_session->getEventListener();
    std::string description;
    for (const Event &event : listener->getEvents()) {
      const uint32_t line = lineOf(event);
      if (isSourceEvent(event) && (line >= firstLine) && (line <= lastLine)) description += describeEvent(event);
    }
    return description;
  }

  // Every source error without a location, described; empty when there is none.
  static std::string describeUnlocatedSourceErrors() {
    EventListener *const listener = m_session->getEventListener();
    std::string description;
    for (const Event &event : listener->getEvents()) {
      if (!isSourceEvent(event)) continue;
      if (const EventArg *const arg = event.getArgument(0); (arg != nullptr) && (arg->m_line == 0)) {
        description += describeEvent(event);
      }
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

// "The filename parameter shall be a string literal" (22.12), and line 17
// gives the identifier somefile instead, so the file is illegal. The LRM
// requires an error and does not say which kind, so any fatal, syntax or
// error diagnostic outside elaboration on line 17 or at the end of the file
// satisfies it.
TEST_F(LineIllegal2Test, ReportsAnErrorForTheUnquotedFileName) {
  const size_t errors = countSourceErrorsOnLine(kIllegalLine) + countSourceErrorsOnLine(kEndOfFileLine);
  EXPECT_GE(errors, 1u) << "no error on line 17 or 18;" << describeAllEvents();
}

// LRM: lines 1-16 are comments and blank lines (5.4). An error on any of them
// would mean HLC rejected legal text.
TEST_F(LineIllegal2Test, NoErrorsOnTheLegalLines1To16) {
  EXPECT_EQ(describeSourceErrorsOnLines(1, kIllegalLine - 1), "");
}

// HLC error recovery, not an LRM rule. The file ends after line 17, and the
// LRM does not define what a tool does after reporting an illegal line. An
// error past the end of the file, or one without a location, would be a
// cascade from it.
TEST_F(LineIllegal2Test, NoCascadeErrors) {
  EXPECT_EQ(describeSourceErrorsOnLines(kEndOfFileLine + 1, kAnyLaterLine), "");
  EXPECT_EQ(describeUnlocatedSourceErrors(), "");
}

// Each of these diagnostics, at any severity, would mean HLC misread the
// file, whatever it reports for line 17.
TEST_F(LineIllegal2Test, ReportsNoMisreadingDiagnostics) {
  // A `line "can be specified anywhere" (22.12), and the file has no design
  // element anyway.
  EXPECT_EQ(findEvent(EventType::PpIllegalDirectiveInDesignElement), nullptr);

  // `line is a compiler directive (22.1), not a usage of a text macro, and
  // somefile has no grave accent, so it is no macro usage either.
  EXPECT_EQ(findEvent(EventType::PpUnkownMacro), nullptr);
  EXPECT_EQ(findEvent(EventType::PaUnkownMacro), nullptr);

  // A `line reads no file (22.12), so nothing named somefile is opened.
  EXPECT_EQ(findEvent(EventType::PpCannotOpenFile), nullptr);
  EXPECT_EQ(findEvent(EventType::PpCannotOpenIncludeFile), nullptr);

  // Every character of the file is ASCII.
  EXPECT_EQ(findEvent(EventType::PpNonAsciiContent), nullptr);
}

// ---------------------------------------------------------------------------
// The source file
// ---------------------------------------------------------------------------

// The file is read once. It has no `include (22.4) and no `define (22.5.1),
// and `line is a compiler directive, not a macro usage (22.1).
TEST_F(LineIllegal2Test, SourceFileHasNoIncludesOrMacros) {
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
// only SystemVerilog text is the illegal line 17, and somefile on it is no
// declaration. So nothing in the design comes from this file, under its own
// name or under somefile.
TEST_F(LineIllegal2Test, NoDesignObjectsDeclared) {
  ASSERT_NE(m_design, nullptr);
  ASSERT_NE(m_unit, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getTopModules()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getTopPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllClasses()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);

  EXPECT_EQ(countFromSourceFile(m_unit->getCheckerDecls()), 0u);
  EXPECT_EQ(countFromSourceFile(m_unit->getParameters()), 0u);
  EXPECT_EQ(countFromSourceFile(m_unit->getVariables()), 0u);
  EXPECT_EQ(countFromSourceFile(m_unit->getTypedefs()), 0u);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
