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

// Test for tests/Google/chapter-22/22.7--timescale-reset.sv
//
// THE SOURCE
//
// Lines 1-15 are license and metadata comments. The SystemVerilog text is
// three compiler directives and no design element:
//
//   16: `timescale 1 ns / 1 ps
//   17: `resetall
//   18: `timescale 10 us / 100 ns
//
// Lines 16 and 18 are, word for word, the first two examples of IEEE
// 1800-2023 22.7. The run is assumed to pass no -timescale option.
//
// The repository stores the file with LF line endings; a Windows checkout
// (core.autocrlf) turns them into CR LF. 5.3 lists "newlines" as white space
// but does not name the carriage return, so this test treats CR LF as one
// line ending and asserts nothing about the CR itself.
//
// WHAT THE LRM SAYS ABOUT IT
//
// Line 16 is a legal `timescale (Syntax 22-6). 1 is a valid integer ("the
//   valid integers are 1, 10, and 100"), ns and ps are valid units ("the
//   valid character strings are s, ms, us, ns, ps, and fs"), and the
//   precision 1 ps, a thousandth of 1 ns, "shall be at least as precise as"
//   the unit 1 ns, which it is (22.7).
//
// Line 17 is a legal `resetall. "When the `resetall compiler directive is
//   encountered during compilation, all compiler directives are set to the
//   default values" (22.3), so it takes back the `timescale of line 16; "If
//   there is no `timescale specified or it has been reset by a `resetall
//   directive, the default time unit and precision are tool-specific" (22.7).
//
// Line 18 is a legal `timescale. 10 and 100 are valid integers, us and ns
//   are valid units, and the precision 100 ns, a hundredth of 10 us, is at
//   least as precise as the unit 10 us (22.7). A second `timescale in one
//   file is legal: a directive's scope lasts "to the point where another
//   compiler directive supersedes it" (22.2), and a `timescale applies
//   "until another `timescale compiler directive is read" (22.7).
//
// All three lines are outside any design element, because the file has none.
//   That keeps them legal: "It shall be illegal for the `timescale directive
//   to be specified within a design element" (22.7), and the same holds for
//   `resetall (22.3). It also means no design element follows any of them, so
//   neither time unit and precision, nor the reset between them, takes effect
//   on anything in the design.
//
// `timescale and `resetall are compiler directives (22.1), not usages of a
//   text macro (22.5.1), and the file defines no macro.
//
// The file has no module, and "A design shall contain at least one top-level
//   module" (23.3.1). An elaborating run may report that, so the error checks
//   below leave HLC's elaboration-category diagnostics out.
//
// Every expected value comes from the LRM, so a red test means HLC deviates
// from it. PaTimescaleInvalidValue and PaTimescaleInvalidScale are HLC's
// codes for two of the 22.7 argument rules (valid integer, precision at
// least as precise as unit), and PpIllegalDirectiveInDesignElement is its
// code for the 22.3 / 22.7 placement rule.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors                     no fatal, syntax or error
//                                             diagnostic (outside elaboration)
//   Line16TimescaleIsAccepted                 no such diagnostic on line 16
//   Line17ResetallIsAccepted                  no such diagnostic on line 17
//   Line18TimescaleIsAccepted                 no such diagnostic on line 18
//   ReportsNoDirectiveMisreadingDiagnostics   no diagnostic of any severity
//                                             that misreads lines 16-18
//   SourceFileHasNoIncludesOrMacros           one SourceFile, no includes,
//                                             no macro definitions or usages
//   NoDesignObjectsDeclared                   nothing in the design comes
//                                             from this file
//
// No check is gated on Design::getElaborated(): the file has nothing that
// elaborates or reduces. The 23.3.1 error an elaborating run may report is
// left out, not asserted.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Which time unit and precision are in effect after each line: 1 ns and
//     1 ps after line 16, the tool's default after line 17, 10 us and 100 ns
//     after line 18. The LRM gives a `timescale and a `resetall effect only
//     on the design elements that follow them (22.3, 22.7), and none
//     follows. The SourceFile time unit and precision are an HLDB convention:
//     if they stand for the compilation-unit scope, "The time unit of the
//     compilation-unit scope can only be set by a timeunit declaration, not a
//     `timescale directive" (3.14.2.3). The default after the reset is
//     "tool-specific" (22.7) anyway. Nearest assertions:
//     Line16TimescaleIsAccepted, Line17ResetallIsAccepted,
//     Line18TimescaleIsAccepted and NoDesignObjectsDeclared.
//   - Directive objects for lines 16-18 in any directive list (SourceFile or
//     Design). The LRM requires the directives to take effect, not to be
//     recorded, and directives are not VPI objects, so neither the contents
//     nor the size of such a list is asserted. The HLDB headers have no
//     directive type for `resetall at all. Nearest assertions: the three
//     LineNN tests.
//   - The rounding and scaling of delays (3.14, 22.7). They exist only while
//     simulation time advances, so this is permanently out of scope; the file
//     has no delay anyway. Nearest assertion:
//     ReportsNoDirectiveMisreadingDiagnostics (the units and precisions the
//     rounding would use are read as valid).
//   - Elaboration-category diagnostics. The 23.3.1 error for a design without
//     a top-level module is legitimate here. Nearest assertion:
//     CompilesWithoutErrors (every other error-class diagnostic).
//   - The comments on lines 1-15 (5.4) and columns. The LRM gives comments no
//     meaning, how HLDB keeps them is a tool convention, and the LRM numbers
//     lines (22.13) but not columns. Nearest assertions: the three LineNN
//     tests, whose line numbers hold only if lines 1-15 were read as whole
//     lines.
//   - Warnings and informational diagnostics. The LRM neither requires nor
//     forbids them for a file without design elements. Nearest assertions:
//     CompilesWithoutErrors and ReportsNoDirectiveMisreadingDiagnostics.

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
#include <string>
#include <string_view>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "22.7--timescale-reset.sv";

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

// True when HLC's error table gives 'error' a fatal, syntax or error
// severity outside the elaboration category. Elaboration diagnostics are left
// out because a design without a top-level module may draw one (23.3.1).
bool isSourceError(const Event &error) {
  const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
  if (errorInfo == nullptr) return false;
  const EventSeverity severity = errorInfo->m_severity;
  const bool isErrorClass = (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
                            (severity == EventSeverity::Error);
  return isErrorClass && (errorInfo->m_category->m_name != "Elab");
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

// Every source error (see isSourceError), described; empty when there is none.
std::string describeSourceErrors(const EventListener *eventListener) {
  std::string description;
  for (const Event &error : eventListener->getEvents()) {
    if (isSourceError(error)) description += describeError(error);
  }
  return description;
}

// Every source error that points at 'line', described; empty when there is none.
std::string describeSourceErrorsOnLine(const EventListener *eventListener, uint32_t line) {
  std::string description;
  for (const Event &error : eventListener->getEvents()) {
    if (isSourceError(error) && (lineOf(error) == line)) description += describeError(error);
  }
  return description;
}
}  // namespace

class TimescaleResetTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.7--timescale-reset.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // The diagnostics of the run, or nullptr.
  static const EventListener *getEventListener() {
    return (m_session == nullptr) ? nullptr : m_session->getEventListener();
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

// Every line of the file is legal, so nothing is fatal, a syntax error or an
// error, apart from the 23.3.1 elaboration error left out above.
TEST_F(TimescaleResetTest, CompilesWithoutErrors) {
  const EventListener *const eventListener = getEventListener();
  ASSERT_NE(eventListener, nullptr);
  EXPECT_EQ(describeSourceErrors(eventListener), "");
}

// Line 16: `timescale 1 ns / 1 ps. 1 is a valid integer, ns and ps are valid
// units, 1 ps is at least as precise as 1 ns, and the line is outside any
// design element (22.7).
TEST_F(TimescaleResetTest, Line16TimescaleIsAccepted) {
  const EventListener *const eventListener = getEventListener();
  ASSERT_NE(eventListener, nullptr);
  EXPECT_EQ(describeSourceErrorsOnLine(eventListener, 16), "");
}

// Line 17: `resetall, outside any design element (22.3). It resets the
// `timescale of line 16 to the tool's default (22.7).
TEST_F(TimescaleResetTest, Line17ResetallIsAccepted) {
  const EventListener *const eventListener = getEventListener();
  ASSERT_NE(eventListener, nullptr);
  EXPECT_EQ(describeSourceErrorsOnLine(eventListener, 17), "");
}

// Line 18: `timescale 10 us / 100 ns. 10 and 100 are valid integers, us and
// ns are valid units, 100 ns is at least as precise as 10 us, and the line is
// outside any design element (22.7). A `timescale after a `resetall, and a
// second `timescale in one file, are both legal (22.2, 22.7).
TEST_F(TimescaleResetTest, Line18TimescaleIsAccepted) {
  const EventListener *const eventListener = getEventListener();
  ASSERT_NE(eventListener, nullptr);
  EXPECT_EQ(describeSourceErrorsOnLine(eventListener, 18), "");
}

// ---------------------------------------------------------------------------
// The source file
// ---------------------------------------------------------------------------

// The file is read once. It has no `include (22.4) and no `define (22.5.1),
// and `timescale and `resetall are compiler directives, not macro usages
// (22.1).
TEST_F(TimescaleResetTest, SourceFileHasNoIncludesOrMacros) {
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

// The file has no design element, and a `timescale or a `resetall declares
// nothing (22.3, 22.7), so nothing in the design comes from this file.
TEST_F(TimescaleResetTest, NoDesignObjectsDeclared) {
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
