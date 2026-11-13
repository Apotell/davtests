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

// Test for tests/Google/chapter-22/22.11--pragma-invalid.sv
//
// THE SOURCE
//
// Lines 1-7 are one-line comments with the license, lines 8-9 are blank and
// lines 10-16 are one block comment carrying the test metadata. The
// SystemVerilog text is one line, and no design element:
//
//   17: `pragma
//
// Nothing follows `pragma on line 17, and nothing follows line 17's line
// ending, so the end of the file is at line 18.
//
// The repository stores the file with LF line endings; a Windows checkout
// (core.autocrlf) turns them into CR LF. 5.3 lists "newlines" as white space
// but does not name the carriage return, so this test treats CR LF as one
// line ending and asserts nothing about the CR itself.
//
// WHAT THE LRM SAYS ABOUT IT
//
// Syntax 22-8 (22.11) gives the form of the directive:
//
//     pragma ::= `pragma pragma_name [ pragma_expression { , pragma_expression } ]
//     pragma_name ::= simple_identifier
//
//   The pragma_name is not optional: only the pragma_expression list is in
//   brackets. "The pragma specification is identified by the pragma_name,
//   which follows the `pragma directive" (22.11). On line 17 nothing follows
//   `pragma, and "Unless specified below as supporting multiple lines,
//   compiler directives shall be all on one line" (22.2), so the directive
//   cannot take its pragma_name from a later line, and there is none anyway.
//   Line 17 matches no form of Syntax 22-8; the file is illegal, and the LRM
//   requires an error. It may point at line 17 or at the end of the file
//   just after it (line 18); both are accepted.
//
// Lines 1-16 are legal: one-line comments, blank lines and one block comment
//   (5.4). Line 13 quotes "`pragma directive" inside that block comment, and
//   "Directives are not recognized within comments or string literals"
//   (22.2), so it is comment text, not a directive.
//
// `pragma is a compiler directive (22.1), not a usage of a text macro
//   (22.5.1), and the file defines no macro.
//
// The file has no module, and "A design shall contain at least one top-level
//   module" (23.3.1). An elaborating run may report that, so every check here
//   leaves HLC's elaboration-category diagnostics out; in particular such a
//   diagnostic cannot stand in for the error that line 17 requires.
//
// Every assertion follows from the LRM, except NoCascadeErrors, which is
// labelled as HLC error recovery. PpIllegalDirectiveInDesignElement is
// HLC's code for a directive placed inside a design element, and
// PpIllegalProtectPragma its code for a protect pragma it rejects.
//
// WHAT IS CHECKED (one fact per test)
//
//   ReportsAnErrorForTheMissingPragmaName   at least one fatal, syntax or
//                                           error diagnostic, outside
//                                           elaboration, on line 17 or at the
//                                           end of the file (line 18)
//   NoErrorsOnTheLegalLines1To16            LRM: no such diagnostic on lines
//                                           1-16, the comment on line 13
//                                           included
//   NoCascadeErrors                         HLC error recovery, not an LRM
//                                           rule: none past the end of the
//                                           file, or without a location
//   ReportsNoMisreadingDiagnostics          no "directive in design element",
//                                           no "unknown macro", no protect
//                                           pragma diagnostic and no non-ASCII
//                                           one
//   SourceFileHasNoIncludesOrMacros         one SourceFile, no includes, no
//                                           macro definitions or usages
//   NoDesignObjectsDeclared                 nothing in the design comes from
//                                           this file
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
//     Nearest assertion: ReportsAnErrorForTheMissingPragmaName.
//   - A directive object for line 17 in any directive list (SourceFile or
//     Design), and its name or expressions. The LRM gives an illegal line no
//     meaning, so neither the contents nor the size of such a list is
//     asserted. Nearest assertion: ReportsAnErrorForTheMissingPragmaName.
//   - Run-time behavior. The file has no procedural code, and a pragma acts on
//     "interpretation of the SystemVerilog source" (22.11), at compile time,
//     so nothing in it exists only while simulation time advances. This is
//     permanently out of scope. Nearest assertion:
//     ReportsAnErrorForTheMissingPragmaName.
//   - Elaboration-category diagnostics. The 23.3.1 error for a design without
//     a top-level module is legitimate here. Nearest assertions:
//     NoErrorsOnTheLegalLines1To16 and NoCascadeErrors (every other
//     error-class diagnostic).
//   - Warnings and informational diagnostics. The LRM neither requires nor
//     forbids them. Nearest assertion: ReportsAnErrorForTheMissingPragmaName.

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
#include <hldb/unit.h>
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
constexpr std::string_view kSourceFileName = "22.11--pragma-invalid.sv";

// The line of the `pragma without a pragma_name, and the end of the file just
// after it.
constexpr uint32_t kIllegalLine = 17;
constexpr uint32_t kEndOfFileLine = 18;

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

class PragmaInvalidTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.11--pragma-invalid.hlc"}); }
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

// Syntax 22-8 requires a pragma_name after `pragma, on the same line (22.2),
// and line 17 has none, so the file is illegal. The LRM requires an error and
// does not say which kind, so any fatal, syntax or error diagnostic outside
// elaboration on line 17 or at the end of the file satisfies it.
TEST_F(PragmaInvalidTest, ReportsAnErrorForTheMissingPragmaName) {
  const size_t errors = countSourceErrorsOnLine(kIllegalLine) + countSourceErrorsOnLine(kEndOfFileLine);
  EXPECT_GE(errors, 1u) << "no error on line 17 or 18;" << describeAllEvents();
}

// LRM: lines 1-16 are comments and blank lines (5.4), and the "`pragma" that
// line 13 quotes inside the block comment is no directive (22.2). An error on
// any of them would mean HLC rejected legal text.
TEST_F(PragmaInvalidTest, NoErrorsOnTheLegalLines1To16) {
  EXPECT_EQ(describeSourceErrorsOnLines(1, kIllegalLine - 1), "");
}

// HLC error recovery, not an LRM rule. The file ends after line 17, and the
// LRM does not define what a tool does after reporting an illegal line. An
// error past the end of the file, or one without a location, would be a
// cascade from it.
TEST_F(PragmaInvalidTest, NoCascadeErrors) {
  EXPECT_EQ(describeSourceErrorsOnLines(kEndOfFileLine + 1, kAnyLaterLine), "");
  EXPECT_EQ(describeUnlocatedSourceErrors(), "");
}

// Each of these diagnostics, at any severity, would mean HLC misread the
// file, whatever it reports for line 17.
TEST_F(PragmaInvalidTest, ReportsNoMisreadingDiagnostics) {
  // The file has no design element, and 22.11 does not restrict where a
  // `pragma may appear.
  EXPECT_EQ(findEvent(EventType::PpIllegalDirectiveInDesignElement), nullptr);

  // `pragma is a compiler directive (22.1), not a usage of a text macro.
  EXPECT_EQ(findEvent(EventType::PpUnkownMacro), nullptr);
  EXPECT_EQ(findEvent(EventType::PaUnkownMacro), nullptr);

  // Line 17 has no pragma_name at all, so it names no protect pragma and
  // starts no protected envelope (22.11.1, Clause 34).
  EXPECT_EQ(findEvent(EventType::PpIllegalProtectPragma), nullptr);
  EXPECT_EQ(findEvent(EventType::PpDecryptionFailed), nullptr);

  // Every character of the file is ASCII.
  EXPECT_EQ(findEvent(EventType::PpNonAsciiContent), nullptr);
}

// ---------------------------------------------------------------------------
// The source file
// ---------------------------------------------------------------------------

// The file is read once. It has no `include (22.4) and no `define (22.5.1),
// and `pragma is a compiler directive, not a macro usage (22.1).
TEST_F(PragmaInvalidTest, SourceFileHasNoIncludesOrMacros) {
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

// The file has no design element, and a directive declares nothing; the only
// SystemVerilog text is the illegal line 17. So nothing in the design comes
// from this file.
TEST_F(PragmaInvalidTest, NoDesignObjectsDeclared) {
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
