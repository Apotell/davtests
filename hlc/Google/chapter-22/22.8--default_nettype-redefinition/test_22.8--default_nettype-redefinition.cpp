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

// Test for tests/Google/chapter-22/22.8--default_nettype-redefinition.sv
//
// THE SOURCE
//
// Lines 1-15 are license and metadata comments. The SystemVerilog text is
// eleven compiler directives and no design element:
//
//   16: `default_nettype wire
//   17: `default_nettype tri
//   18: `default_nettype tri0
//   19: `default_nettype tri1
//   20: `default_nettype wand
//   21: `default_nettype triand
//   22: `default_nettype wor
//   23: `default_nettype trior
//   24: `default_nettype trireg
//   25: `default_nettype uwire
//   26: `default_nettype none
//
// The repository stores the file with LF line endings; a Windows checkout
// (core.autocrlf) turns them into CR LF. 5.3 lists "newlines" as white space
// but does not name the carriage return, so this test treats CR LF as one
// line ending and asserts nothing about the CR itself.
//
// WHAT THE LRM SAYS ABOUT IT
//
// Each line is a default_nettype_compiler_directive (Syntax 22-7):
//
//   default_nettype_value ::= wire | tri | tri0 | tri1 | wand | triand | wor
//                             | trior | trireg | uwire | none
//
//   Lines 16-26 use every default_nettype_value exactly once, in the order of
//   Syntax 22-7, so each line is legal on its own.
//
// Writing the directive eleven times is legal too: "Multiple
//   `default_nettype directives are allowed. The latest occurrence of this
//   directive in the source controls the type of nets that will be implicitly
//   declared" (22.8). Each line supersedes the one before it (22.2), so after
//   line 26 the default net type is none.
//
// `default_nettype "can be used only outside design elements" (22.8). The
//   file has no design element, so all eleven lines are outside one. For the
//   same reason no design element follows them: the file declares no net,
//   implicit or explicit (6.10), so none of the eleven net types, and not the
//   final none either, takes effect on anything in the design.
//
// `default_nettype is a compiler directive (22.1), not a usage of a text
//   macro (22.5.1), and the file defines no macro.
//
// The file has no module, and "A design shall contain at least one top-level
//   module" (23.3.1). An elaborating run may report that, so the error checks
//   below leave HLC's elaboration-category diagnostics out.
//
// Every expected value comes from the LRM, so a red test means HLC deviates
// from it. PpIllegalDirectiveValue is HLC's code for a directive argument
// it does not accept, and PpIllegalDirectiveInDesignElement its code for
// the 22.8 placement rule.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors                     no fatal, syntax or error
//                                             diagnostic (outside elaboration)
//   Line16WireIsAccepted                      no such diagnostic on line 16
//   Line17TriIsAccepted                         ... on line 17
//   Line18Tri0IsAccepted                        ... on line 18
//   Line19Tri1IsAccepted                        ... on line 19
//   Line20WandIsAccepted                        ... on line 20
//   Line21TriandIsAccepted                      ... on line 21
//   Line22WorIsAccepted                         ... on line 22
//   Line23TriorIsAccepted                       ... on line 23
//   Line24TriregIsAccepted                      ... on line 24
//   Line25UwireIsAccepted                       ... on line 25
//   Line26NoneIsAccepted                        ... on line 26
//   ReportsNoDirectiveMisreadingDiagnostics   no diagnostic of any severity
//                                             that misreads lines 16-26
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
//   - Which default net type is in effect after each line, and that none is
//     in effect at the end. The LRM gives `default_nettype effect only on the
//     nets implicitly declared after it (22.8, 6.10), and the file declares
//     no net and no design element. The default net type the LRM does model,
//     vpiDefNetType, is a property of an instance (37.10), and there is none.
//     Nearest assertions: the eleven LineNN tests (each value is read as
//     legal) and NoDesignObjectsDeclared.
//   - The "all nets shall be explicitly declared" error that none enables
//     (22.8). It needs a net that is not explicitly declared, and the file has
//     no net at all. Nearest assertion: CompilesWithoutErrors (no error is
//     reported).
//   - Directive objects for lines 16-26 in any directive list (SourceFile or
//     Design). The LRM requires the directives to take effect, not to be
//     recorded, and directives are not VPI objects, so neither the contents
//     nor the size of such a list is asserted. Nearest assertions: the eleven
//     LineNN tests.
//   - Elaboration-category diagnostics. The 23.3.1 error for a design without
//     a top-level module is legitimate here. Nearest assertion:
//     CompilesWithoutErrors (every other error-class diagnostic).
//   - The comments on lines 1-15 (5.4) and columns. The LRM gives comments no
//     meaning, how HLDB keeps them is a tool convention, and the LRM numbers
//     lines (22.13) but not columns. Nearest assertions: the eleven LineNN
//     tests, whose line numbers hold only if lines 1-15 were read as whole
//     lines.
//   - Warnings and informational diagnostics, for example about a directive
//     that is redefined before anything uses it. The LRM allows multiple
//     `default_nettype directives and neither requires nor forbids a warning.
//     Nearest assertions: CompilesWithoutErrors and
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
constexpr std::string_view kSourceFileName = "22.8--default_nettype-redefinition.sv";

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

class DefaultNettypeRedefinitionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.8--default_nettype-redefinition.hlc"}); }
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
// error, apart from the 23.3.1 elaboration error left out above.
TEST_F(DefaultNettypeRedefinitionTest, CompilesWithoutErrors) {
  EXPECT_EQ(sourceErrors(), "");
}

// Lines 16-26, one test per line. Each value is a default_nettype_value of
// Syntax 22-7, each line is outside any design element (22.8), and every
// line after 16 is a redefinition, which 22.8 allows ("Multiple
// `default_nettype directives are allowed").

// Line 16: `default_nettype wire
TEST_F(DefaultNettypeRedefinitionTest, Line16WireIsAccepted) {
  EXPECT_EQ(sourceErrorsOnLine(16), "");
}

// Line 17: `default_nettype tri
TEST_F(DefaultNettypeRedefinitionTest, Line17TriIsAccepted) {
  EXPECT_EQ(sourceErrorsOnLine(17), "");
}

// Line 18: `default_nettype tri0
TEST_F(DefaultNettypeRedefinitionTest, Line18Tri0IsAccepted) {
  EXPECT_EQ(sourceErrorsOnLine(18), "");
}

// Line 19: `default_nettype tri1
TEST_F(DefaultNettypeRedefinitionTest, Line19Tri1IsAccepted) {
  EXPECT_EQ(sourceErrorsOnLine(19), "");
}

// Line 20: `default_nettype wand
TEST_F(DefaultNettypeRedefinitionTest, Line20WandIsAccepted) {
  EXPECT_EQ(sourceErrorsOnLine(20), "");
}

// Line 21: `default_nettype triand
TEST_F(DefaultNettypeRedefinitionTest, Line21TriandIsAccepted) {
  EXPECT_EQ(sourceErrorsOnLine(21), "");
}

// Line 22: `default_nettype wor
TEST_F(DefaultNettypeRedefinitionTest, Line22WorIsAccepted) {
  EXPECT_EQ(sourceErrorsOnLine(22), "");
}

// Line 23: `default_nettype trior
TEST_F(DefaultNettypeRedefinitionTest, Line23TriorIsAccepted) {
  EXPECT_EQ(sourceErrorsOnLine(23), "");
}

// Line 24: `default_nettype trireg
TEST_F(DefaultNettypeRedefinitionTest, Line24TriregIsAccepted) {
  EXPECT_EQ(sourceErrorsOnLine(24), "");
}

// Line 25: `default_nettype uwire
TEST_F(DefaultNettypeRedefinitionTest, Line25UwireIsAccepted) {
  EXPECT_EQ(sourceErrorsOnLine(25), "");
}

// Line 26: `default_nettype none. none is a default_nettype_value too; it
// only makes later implicit nets an error (22.8), and none follow.
TEST_F(DefaultNettypeRedefinitionTest, Line26NoneIsAccepted) {
  EXPECT_EQ(sourceErrorsOnLine(26), "");
}

// ---------------------------------------------------------------------------
// The source file
// ---------------------------------------------------------------------------

// The file is read once. It has no `include (22.4) and no `define (22.5.1),
// and `default_nettype is a compiler directive, not a macro usage (22.1).
TEST_F(DefaultNettypeRedefinitionTest, SourceFileHasNoIncludesOrMacros) {
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

// The file has no design element, and a `default_nettype only sets the type
// of nets declared implicitly later (22.8); it declares nothing itself. So
// nothing in the design comes from this file, and with no module there is
// nowhere a net, implicit or explicit, could be.
TEST_F(DefaultNettypeRedefinitionTest, NoDesignObjectsDeclared) {
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
