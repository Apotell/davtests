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

// Test for tests/Google/chapter-22/22.9--unconnected_drive-invalid-1.sv
//
// THE SOURCE
//
// Lines 1-7 are one-line license comments, lines 8-9 are blank, and lines
// 10-16 are one block comment carrying the test metadata. The SystemVerilog
// text is two compiler directives and no design element:
//
//   17: `unconnected_drive
//   18: `nounconnected_drive
//
// The repository stores the file with LF line endings; a Windows checkout
// (core.autocrlf) turns them into CR LF. 5.3 lists "newlines" as white space
// but does not name the carriage return, so this test treats CR LF as one
// line ending and asserts nothing about the CR itself.
//
// WHAT THE LRM SAYS ABOUT IT
//
// The `unconnected_drive of line 17 is illegal: it "takes one of two
//   arguments--pull1 or pull0" (22.9), and no argument follows it on line
//   17. The LRM does not say whether the argument has to be on the same line
//   as the directive: 22.9 has no syntax box, and a newline is white space
//   that only separates tokens (5.3). So a tool may read the directive as
//   ending at the end of line 17, with no argument, or read on and take the
//   next token, the `nounconnected_drive of line 18, as its argument, which
//   is not pull1 or pull0 either. Under both readings the directive is
//   illegal and the LRM requires an error, but the error may point at line
//   17 or at line 18. Both are accepted, and the illegal directive is taken
//   to span lines 17-18.
//
// Lines 1-16 are legal: comments and blank lines. Line 13 mentions
//   `unconnected_drive, but inside the block comment of lines 10-16, which
//   ends only at */ (5.4), so it is comment text, not a directive.
//
// Both directives are outside any design element, as 22.9 requires ("These
//   directives shall be specified outside the design element
//   declarations"): the file has none. Both are compiler directives (22.1),
//   not usages of a text macro (22.5.1), and the file defines no macro.
//
// The metadata's ":type: simulation" does not make this a simulation check:
//   the required argument is fixed by the source text, so the error exists
//   before any time advances.
//
// The file has no module, and "A design shall contain at least one top-level
//   module" (23.3.1). An elaborating run may report that, so the checks for
//   unwanted errors leave HLC's elaboration-category diagnostics out.
//
// Every assertion follows from the LRM, except NoCascadeErrorsAfterLine18,
// which is labelled as HLC error recovery. PpIllegalDirectiveInDesignElement
// is HLC's code for the 22.9 placement rule.
//
// WHAT IS CHECKED (one fact per test)
//
//   ReportsAnErrorForTheMissingArgument   at least one fatal, syntax or error
//                                         diagnostic on line 17 or 18
//   NoErrorsOnTheLegalLines1To16          LRM: no such diagnostic (outside
//                                         elaboration) on lines 1-16
//   NoCascadeErrorsAfterLine18            HLC error recovery, not an LRM
//                                         rule: none past line 18, or
//                                         without a location
//   ReportsNoMisreadingDiagnostics        no "directive in design element"
//                                         and no "unknown macro" diagnostic
//   SourceFileHasNoIncludesOrMacros       one SourceFile, no includes, no
//                                         macro definitions or usages
//   NoDesignObjectsDeclared               nothing in the design comes from
//                                         this file
//
// No check is gated on Design::getElaborated(): the file has nothing that
// elaborates or reduces. The 23.3.1 error an elaborating run may report is
// left out, not asserted.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Whether line 18 is read as a directive of its own or as the missing
//     argument of line 17, and so whether a diagnostic on line 18 is
//     accepted as part of the illegal directive. The LRM does not settle it
//     (see above), so any number of error-class diagnostics on lines 17-18 is
//     accepted. Nearest assertion: ReportsAnErrorForTheMissingArgument.
//   - Which code, which error-class severity and how many diagnostics the
//     illegal directive draws, and their message text and column. The LRM
//     requires an error but not its form, and it numbers lines (22.13) but
//     not columns. Nearest assertion: ReportsAnErrorForTheMissingArgument.
//   - Which drive applies to unconnected ports after line 17 or 18. The LRM
//     gives the illegal directive no meaning, and the drive it models,
//     vpiUnconnDrive, is a property of an instance (37.10); the file has no
//     module, program or interface, so there is no instance to carry it.
//     Nearest assertion: NoDesignObjectsDeclared.
//   - Directive objects for lines 17-18 in any directive list (SourceFile or
//     Design). Directives are not VPI objects, and the LRM gives an illegal
//     one no meaning, so neither the contents nor the size of such a list is
//     asserted. Nearest assertion: ReportsAnErrorForTheMissingArgument.
//   - Elaboration-category diagnostics. The 23.3.1 error for a design without
//     a top-level module is legitimate here. Nearest assertions:
//     NoErrorsOnTheLegalLines1To16 and NoCascadeErrorsAfterLine18 (every
//     other error-class diagnostic).
//   - Warnings and informational diagnostics. The LRM neither requires nor
//     forbids them. Nearest assertion: ReportsAnErrorForTheMissingArgument.

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

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "22.9--unconnected_drive-invalid-1.sv";

// The illegal `unconnected_drive starts on line 17 and, if its argument is
// looked for on the next line, takes in line 18.
constexpr uint32_t kFirstIllegalLine = 17;
constexpr uint32_t kLastIllegalLine = 18;

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

// True when HLC's error table gives 'error' a fatal, syntax or error severity,
// the severities the LRM's "illegal" can map to.
bool isErrorClass(const Event &error) {
  const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
  if (errorInfo == nullptr) return false;
  const EventSeverity severity = errorInfo->m_severity;
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}

// True when 'error' is an error-class diagnostic outside HLC's elaboration
// category. Elaboration diagnostics are left out because a design without a
// top-level module may draw one (23.3.1).
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

class UnconnectedDriveInvalid1Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.9--unconnected_drive-invalid-1.hlc"}); }
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

  // Number of error-class diagnostics, of any category, that point at 'line'.
  static size_t countErrorsOnLine(uint32_t line) {
    if (getEventListener() == nullptr) return 0;
    size_t count = 0;
    for (const Event &error : getEventListener()->getEvents()) {
      if (isErrorClass(error) && (lineOf(error) == line)) ++count;
    }
    return count;
  }

  // Every source error (see isSourceError) on a line from 'firstLine' to
  // 'lastLine', described; empty when there is none. A run without an error
  // container is reported, not passed.
  static std::string describeSourceErrorsOnLines(uint32_t firstLine, uint32_t lastLine) {
    if (getEventListener() == nullptr) return "no error container";
    std::string description;
    for (const Event &error : getEventListener()->getEvents()) {
      const uint32_t line = lineOf(error);
      if (isSourceError(error) && (line >= firstLine) && (line <= lastLine)) description += describeError(error);
    }
    return description;
  }

  // Every source error without a location, described; empty when there is none.
  static std::string describeUnlocatedSourceErrors() {
    if (getEventListener() == nullptr) return "no error container";
    std::string description;
    for (const Event &error : getEventListener()->getEvents()) {
      if (isSourceError(error) && error.getArguments().empty()) description += describeError(error);
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

// 22.9: `unconnected_drive "takes one of two arguments--pull1 or pull0", and
// the one on line 17 has none (or, read across the newline, has
// `nounconnected_drive). The LRM requires an error and does not say which
// kind, so any error-class diagnostic on line 17 or 18 satisfies it.
TEST_F(UnconnectedDriveInvalid1Test, ReportsAnErrorForTheMissingArgument) {
  const size_t errorsOnIllegalLines = countErrorsOnLine(kFirstIllegalLine) + countErrorsOnLine(kLastIllegalLine);
  EXPECT_GE(errorsOnIllegalLines, 1u) << "no error on line 17 or 18;" << describeAllErrors();
}

// LRM: lines 1-16 are comments and blank lines, including the
// "`unconnected_drive" in the block comment on line 13 (5.4). An error on
// any of them would mean HLC rejected legal text.
TEST_F(UnconnectedDriveInvalid1Test, NoErrorsOnTheLegalLines1To16) {
  EXPECT_EQ(describeSourceErrorsOnLines(1, kFirstIllegalLine - 1), "");
}

// HLC error recovery, not an LRM rule. Nothing but the end of the file
// follows line 18, and the LRM does not define what a tool does after
// reporting an illegal directive. An error past line 18, or one without a
// location, would be a cascade from the illegal directive.
TEST_F(UnconnectedDriveInvalid1Test, NoCascadeErrorsAfterLine18) {
  EXPECT_EQ(describeSourceErrorsOnLines(kLastIllegalLine + 1, kAnyLaterLine), "");
  EXPECT_EQ(describeUnlocatedSourceErrors(), "");
}

// ---------------------------------------------------------------------------
// The source file
// ---------------------------------------------------------------------------

// The file is read once. It has no `include (22.4) and no `define (22.5.1),
// and both directives are compiler directives, not macro usages (22.1).
TEST_F(UnconnectedDriveInvalid1Test, SourceFileHasNoIncludesOrMacros) {
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

// The file has no design element, and a directive, legal or not, declares
// nothing (22.9), so nothing in the design comes from this file.
TEST_F(UnconnectedDriveInvalid1Test, NoDesignObjectsDeclared) {
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

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
