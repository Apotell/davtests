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

// Test for tests/Google/chapter-22/22.10--celldefine-basic-2.sv
//
// THE SOURCE
//
// Lines 1-15 are license and metadata comments. The SystemVerilog text is
// one compiler directive and no design element:
//
//   16: `celldefine
//
// The repository stores the file with LF line endings; a Windows checkout
// (core.autocrlf) turns them into CR LF. 5.3 lists "newlines" as white space
// but does not name the carriage return, so this test treats CR LF as one
// line ending and asserts nothing about the CR itself.
//
// WHAT THE LRM SAYS ABOUT IT
//
// Line 16 is a legal `celldefine; 22.10 gives it no argument. "The directives
//   `celldefine and `endcelldefine tag modules as cell modules" (22.10): a
//   module after line 16 would be a cell.
//
// No `endcelldefine follows, and none is needed: "It is advisable to pair
//   each `celldefine with an `endcelldefine, but it is not required; these
//   are two independent directives" (22.10). The file ends with the cell
//   setting in effect, which is legal.
//
// The directives "may appear anywhere in the source description, but it is
//   recommended that the directives be specified outside any design
//   elements" (22.10). Line 16 is outside one, because the file has no design
//   element. For the same reason no module follows it, so it tags nothing in
//   the design.
//
// `celldefine is a compiler directive (22.1), not a usage of a text macro
//   (22.5.1), and the file defines no macro.
//
// The file has no module, and "A design shall contain at least one top-level
//   module" (23.3.1). An elaborating run may report that, so the error checks
//   below leave HLC's elaboration-category diagnostics out.
//
// Every expected value comes from the LRM, so a red test means HLC deviates
// from it. PpIllegalDirectiveValue is HLC's code for a directive argument
// it does not accept, and PpIllegalDirectiveInDesignElement its code for
// a directive placed inside a design element.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors                     no fatal, syntax or error
//                                             diagnostic (outside elaboration),
//                                             so the missing `endcelldefine
//                                             is accepted
//   Line16CelldefineIsAccepted                no such diagnostic on line 16
//   ReportsNoDirectiveMisreadingDiagnostics   no diagnostic of any severity
//                                             that misreads line 16
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
//   - Whether a module is tagged as a cell. The LRM models the tag as
//     vpiCellInstance, a property of an instance (37.10), and the file has no
//     module, so there is no instance to carry it. Nearest assertions:
//     Line16CelldefineIsAccepted and NoDesignObjectsDeclared.
//   - A directive object for line 16 in any directive list (SourceFile or
//     Design). The LRM requires the directive to take effect, not to be
//     recorded, and directives are not VPI objects, so neither the contents
//     nor the size of such a list is asserted. Nearest assertion:
//     Line16CelldefineIsAccepted.
//   - Elaboration-category diagnostics. The 23.3.1 error for a design without
//     a top-level module is legitimate here. Nearest assertion:
//     CompilesWithoutErrors (every other error-class diagnostic).
//   - The comments on lines 1-15 (5.4) and columns. The LRM gives comments no
//     meaning, how HLDB keeps them is a tool convention, and the LRM numbers
//     lines (22.13) but not columns. Nearest assertion:
//     Line16CelldefineIsAccepted, whose line number holds only if lines 1-15
//     were read as whole lines.
//   - Warnings and informational diagnostics, for example about the
//     `celldefine that no `endcelldefine closes. The LRM calls the pairing
//     advisable, so it neither requires nor forbids a warning. Nearest
//     assertions: CompilesWithoutErrors and
//     ReportsNoDirectiveMisreadingDiagnostics.

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
constexpr std::string_view kSourceFileName = "22.10--celldefine-basic-2.sv";

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
}  // namespace

class CelldefineBasic2Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.10--celldefine-basic-2.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Every source error (see isSourceError), described; empty when there is
  // none. A run without an error container is reported, not passed.
  static std::string sourceErrors() {
    if ((m_session == nullptr) || (m_session->getEventListener() == nullptr)) return "no error container";
    std::string description;
    for (const Event &error : m_session->getEventListener()->getEvents()) {
      if (isSourceError(error)) description += describeError(error);
    }
    return description;
  }

  // The same, for the source errors that point at 'line' only.
  static std::string sourceErrorsOnLine(uint32_t line) {
    if ((m_session == nullptr) || (m_session->getEventListener() == nullptr)) return "no error container";
    std::string description;
    for (const Event &error : m_session->getEventListener()->getEvents()) {
      if (isSourceError(error) && (lineOf(error) == line)) description += describeError(error);
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

// Every line of the file is legal, so nothing is fatal, a syntax error or an
// error, apart from the 23.3.1 elaboration error left out above. That
// includes the end of the file: the `celldefine of line 16 needs no
// `endcelldefine (22.10), so an error for a missing one, on any line or on
// none, fails this test.
TEST_F(CelldefineBasic2Test, CompilesWithoutErrors) {
  EXPECT_EQ(sourceErrors(), "");
}

// Line 16: `celldefine. It takes no argument and may appear anywhere
// (22.10).
TEST_F(CelldefineBasic2Test, Line16CelldefineIsAccepted) {
  EXPECT_EQ(sourceErrorsOnLine(16), "");
}

// ---------------------------------------------------------------------------
// The source file
// ---------------------------------------------------------------------------

// The file is read once. It has no `include (22.4) and no `define (22.5.1),
// and `celldefine is a compiler directive, not a macro usage (22.1).
TEST_F(CelldefineBasic2Test, SourceFileHasNoIncludesOrMacros) {
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

// The file has no design element, and `celldefine only tags modules as cells
// (22.10); it declares nothing. So nothing in the design comes from this
// file.
TEST_F(CelldefineBasic2Test, NoDesignObjectsDeclared) {
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
