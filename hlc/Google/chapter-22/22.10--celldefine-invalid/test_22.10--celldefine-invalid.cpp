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

// Test for tests/Google/chapter-22/22.10--celldefine-invalid.sv
//
// THE SOURCE
//
// Lines 1-7 are one block comment carrying the test metadata; this file has
// no license comment. The SystemVerilog text is one line, and no design
// element:
//
//   8: `celldefine foo
//
// The repository stores the file with LF line endings; a Windows checkout
// (core.autocrlf) turns them into CR LF. 5.3 lists "newlines" as white space
// but does not name the carriage return, so this test treats CR LF as one
// line ending and asserts nothing about the CR itself.
//
// WHAT THE LRM SAYS ABOUT IT
//
// `celldefine on its own would be legal here: the directives "may appear
//   anywhere in the source description" (22.10), and nothing requires a
//   module after them. The metadata's reason, "module must be defined for
//   celldefine to make sense", is not an LRM rule; 22.10 only says the
//   directives "tag modules as cell modules", and a `celldefine that no
//   module follows tags nothing.
//
// The foo after it is what makes the file illegal. 22.10 gives `celldefine
//   no argument, so the directive ends right after its name and foo is
//   ordinary source text. A directive "may be followed by another valid
//   language element on the same line" (22.2), and foo could begin one (for
//   example a data declaration of a type named foo), but the end of the file
//   follows it, and no description (A.1.2) is a lone identifier. Whether a
//   tool reads foo as a wrong argument of the directive or as the start of a
//   description that the end of the file cuts off, the file is illegal, and
//   the LRM requires an error. It may point at line 8 or at the end of the
//   file just after it (line 9); both are accepted.
//
// Lines 1-7 are legal: one block comment (5.4). Line 4 mentions celldefine
//   there as comment text.
//
// `celldefine is a compiler directive (22.1), not a usage of a text macro
//   (22.5.1), and the file defines no macro.
//
// The file has no module, and "A design shall contain at least one top-level
//   module" (23.3.1). An elaborating run may report that, so the checks for
//   unwanted errors leave HLC's elaboration-category diagnostics out.
//
// Every assertion follows from the LRM, except NoCascadeErrors, which is
// labelled as HLC error recovery. PpIllegalDirectiveInDesignElement is
// HLC's code for a directive placed inside a design element.
//
// WHAT IS CHECKED (one fact per test)
//
//   ReportsAnErrorForTheStrayFoo      at least one fatal, syntax or error
//                                     diagnostic on line 8 or at the end of
//                                     the file (line 9)
//   NoErrorsOnTheLegalLines1To7       LRM: no such diagnostic (outside
//                                     elaboration) on lines 1-7
//   NoCascadeErrors                   HLC error recovery, not an LRM rule:
//                                     none past the end of the file, or
//                                     without a location
//   ReportsNoMisreadingDiagnostics    no "directive in design element" and
//                                     no "unknown macro" diagnostic
//   SourceFileHasNoIncludesOrMacros   one SourceFile, no includes, no macro
//                                     definitions or usages
//   NoDesignObjectsDeclared           nothing in the design comes from this
//                                     file
//
// No check is gated on Design::getElaborated(): the file has nothing that
// elaborates or reduces. The 23.3.1 error an elaborating run may report is
// left out, not asserted.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Which code, which error-class severity and how many diagnostics the
//     file draws, and their message text and column. The LRM requires an
//     error but not its form, and it numbers lines (22.13) but not columns.
//     A syntax diagnostic for an incomplete description and an "illegal
//     directive value" for an argument of `celldefine are both accepted.
//     Nearest assertion: ReportsAnErrorForTheStrayFoo.
//   - That HLC does not also object to a `celldefine without a module, the
//     metadata's reason. No HLC code for that is known, and the required
//     error for foo would hide it anyway. Nearest assertions:
//     ReportsNoMisreadingDiagnostics and, in the legal fixtures
//     22.10--celldefine-basic-1 and -2, CompilesWithoutErrors.
//   - Whether a module is tagged as a cell. The LRM models the tag as
//     vpiCellInstance, a property of an instance (37.10), and the file has no
//     module. Nearest assertion: NoDesignObjectsDeclared.
//   - A directive object for line 8 in any directive list (SourceFile or
//     Design). Directives are not VPI objects, and the LRM gives an illegal
//     line no meaning, so neither the contents nor the size of such a list is
//     asserted. Nearest assertion: ReportsAnErrorForTheStrayFoo.
//   - Elaboration-category diagnostics. The 23.3.1 error for a design without
//     a top-level module is legitimate here. Nearest assertions:
//     NoErrorsOnTheLegalLines1To7 and NoCascadeErrors (every other
//     error-class diagnostic).
//   - Warnings and informational diagnostics. The LRM neither requires nor
//     forbids them. Nearest assertion: ReportsAnErrorForTheStrayFoo.

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
constexpr std::string_view kSourceFileName = "22.10--celldefine-invalid.sv";

// The line of `celldefine foo, and the end of the file just after it.
constexpr uint32_t kIllegalLine = 8;
constexpr uint32_t kEndOfFileLine = 9;

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

class CelldefineInvalidTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.10--celldefine-invalid.hlc"}); }
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

// `celldefine takes no argument (22.10), and the foo after it, followed only
// by the end of the file, is not a valid language element (22.2, A.1.2), so
// the file is illegal. The LRM requires an error and does not say which kind,
// so any error-class diagnostic on line 8 or at the end of the file satisfies
// it.
TEST_F(CelldefineInvalidTest, ReportsAnErrorForTheStrayFoo) {
  const size_t errors = countErrorsOnLine(kIllegalLine) + countErrorsOnLine(kEndOfFileLine);
  EXPECT_GE(errors, 1u) << "no error on line 8 or 9;" << describeAllErrors();
}

// LRM: lines 1-7 are one block comment (5.4), including the word celldefine
// on line 4. An error on any of them would mean HLC rejected legal text.
TEST_F(CelldefineInvalidTest, NoErrorsOnTheLegalLines1To7) {
  EXPECT_EQ(describeSourceErrorsOnLines(1, kIllegalLine - 1), "");
}

// HLC error recovery, not an LRM rule. The file ends after line 8, and the
// LRM does not define what a tool does after reporting an illegal line. An
// error past the end of the file, or one without a location, would be a
// cascade from it.
TEST_F(CelldefineInvalidTest, NoCascadeErrors) {
  EXPECT_EQ(describeSourceErrorsOnLines(kEndOfFileLine + 1, kAnyLaterLine), "");
  EXPECT_EQ(describeUnlocatedSourceErrors(), "");
}

// ---------------------------------------------------------------------------
// The source file
// ---------------------------------------------------------------------------

// The file is read once. It has no `include (22.4) and no `define (22.5.1),
// and `celldefine is a compiler directive, not a macro usage (22.1).
TEST_F(CelldefineInvalidTest, SourceFileHasNoIncludesOrMacros) {
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

// The file has no design element, a directive declares nothing (22.10), and
// foo alone declares nothing either: every design element begins with a
// keyword, and a declaration needs a declared name besides its type (A.1.2,
// A.2.1.3), while foo is the only identifier in the file. So nothing in the
// design comes from this file.
TEST_F(CelldefineInvalidTest, NoDesignObjectsDeclared) {
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
