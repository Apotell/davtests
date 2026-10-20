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

// Test for tests/Google/chapter-22/22.7--timescale-basic-1.sv
//
// The source file holds no design element at all. Lines 1-7 are one-line
// "//" license comments, lines 8-9 are blank, lines 10-15 are one block
// comment carrying the test metadata, and the only SystemVerilog text is:
//
//   16: `timescale 1 ns / 1 ps
//
// This is the first example of IEEE 1800-2023 22.7, verbatim.
//
// Line 16 is a timescale_compiler_directive (Syntax 22-6: `timescale
// time_unit / time_precision) with the time_unit 1 ns and the time_precision
// 1 ps. 22.7: "the valid integers are 1, 10, and 100" and "the valid
// character strings are s, ms, us, ns, ps, and fs", so both arguments are
// valid; "The time_precision argument shall be at least as precise as the
// time_unit argument", and 1 ps is a thousandth of 1 ns, so the pair is
// legal. "It shall be illegal for the `timescale directive to be specified
// within a design element"; the file has no design element, so line 16 is
// outside one.
//
// The directive "specifies the default unit of measurement for time and
// delay values and the degree of accuracy for delays in all design elements
// that follow this directive" (22.7). No design element follows it, so it
// applies to nothing, and nothing in the design shows its values.
//
// `timescale is a compiler directive (22.1), not a text macro usage, and the
// file defines no macro.
//
// Every assertion below follows from the LRM; a red run means HLC deviates
// from it.
//
// What is checked
//   - No fatal, syntax or error diagnostic outside the elaboration category,
//     and none of the diagnostics a wrong reading of line 16 would produce;
//     in particular no invalid timescale value, unit or scale, and no
//     "directive in design element".
//   - Exactly one SourceFile for the .sv, with no includes, no macro
//     definitions and no macro instances.
//   - No design object of any kind is created from this file.
//
// What is NOT checked, and why
//   - A directive object for line 16 in the SourceFile's directive list, and
//     its line. The LRM requires a `timescale to take effect on the design
//     elements that follow it (22.7); it does not require a tool to keep a
//     record of the directive itself, and the directive is not in the VPI
//     object model. HLDB defines such a record (a generic directive of type
//     vpiDirectiveTypeTimeScale), but whether HLC fills it in is an HLDB
//     model question, not an LRM rule, so the size of the directive list is
//     not asserted either way. Nearest assertion:
//     ReportsNoTimescaleDiagnostics (line 16 is read as a valid `timescale).
//   - The time unit 1 ns and the time precision 1 ps as stored values. The
//     LRM gives them effect only on the design elements that follow the
//     directive (22.7), and none does. The SourceFile time unit and precision
//     are an HLDB convention: if they stand for the compilation-unit scope,
//     3.14.2.3 says "The time unit of the compilation-unit scope can only be
//     set by a timeunit declaration, not a `timescale" directive. Nearest
//     assertion: ReportsNoTimescaleDiagnostics (the values are accepted as
//     valid).
//   - Delay scaling and rounding (3.14, 22.7). They only exist while
//     simulation time advances, and the file has no delay anyway. Nearest
//     assertion: ReportsNoTimescaleDiagnostics.
//   - The informational diagnostics a tool may give about timescales. The
//     LRM neither requires nor forbids them. Nearest assertion:
//     CompilesWithoutErrors.
//   - Elaboration-category diagnostics. The file has no module, and a design
//     shall contain at least one top-level module (23.3.1), so an elaborating
//     run may legitimately report that. Nearest assertion:
//     CompilesWithoutErrors (all other error-class diagnostics).
//   - The seven one-line comments and the block comment (5.4). The LRM gives
//     comments no meaning; how HLDB groups, classifies (vpiComment and
//     vpiDocumentComment are marked non-standard) and attaches them is a tool
//     convention. Nearest assertion: CompilesWithoutErrors (the comments are
//     consumed without an error).
//   - The warning count. The LRM neither requires nor forbids warnings for a
//     file without design elements. Nearest assertion: CompilesWithoutErrors.
//   - No check is gated on Design::getElaborated(). The 23.3.1 error an
//     elaborating run may report is excluded, not asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/ErrorReporting/Location.h>
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
constexpr std::string_view kSourceFileName = "22.7--timescale-basic-1.sv";

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

// One diagnostic as " [<code> line <n>: <text>]", for failure messages, so a
// red run shows what was reported and where. <text> is HLC's message
// template, so its placeholders are not filled in.
std::string describeError(const Error &error) {
  const ErrorDefinition::ErrorMap &errorInfoMap = ErrorDefinition::getErrorInfoMap();
  const ErrorDefinition::ErrorMap::const_iterator errorInfo = errorInfoMap.find(error.getType());
  std::string description = " [" + std::to_string(static_cast<int32_t>(error.getType()));
  description += " line " + std::to_string(lineOf(error));
  if (errorInfo != errorInfoMap.end()) description += ": " + std::string(errorInfo->second.m_errorText);
  return description + "]";
}

// Every fatal, syntax or error diagnostic outside the elaboration category,
// described as by describeError(); empty when there is none.
std::string describeSourceErrors(const ErrorContainer *errorContainer) {
  const ErrorDefinition::ErrorMap &errorInfoMap = ErrorDefinition::getErrorInfoMap();
  std::string description;
  for (const Error &error : errorContainer->getErrors()) {
    const ErrorDefinition::ErrorMap::const_iterator errorInfo = errorInfoMap.find(error.getType());
    if (errorInfo == errorInfoMap.end()) continue;
    const ErrorDefinition::ErrorSeverity severity = errorInfo->second.m_severity;
    const bool isErrorClass = (severity == ErrorDefinition::FATAL) || (severity == ErrorDefinition::SYNTAX) ||
                              (severity == ErrorDefinition::ERROR);
    if (isErrorClass && (errorInfo->second.m_category != ErrorDefinition::ELAB)) description += describeError(error);
  }
  return description;
}
}  // namespace

class TimescaleBasic1Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.7--timescale-basic-1.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // All SourceFile nodes recorded for the file under test.
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

  // The SourceFile node for the file under test, or nullptr unless there is exactly one.
  static const hldb::SourceFile *getSourceFile() {
    const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
    return (sourceFiles.size() == 1) ? sourceFiles.front() : nullptr;
  }
};

// The file is a well-formed, legal timescale_compiler_directive (Syntax 22-6,
// 22.7) and comments; nothing in it is fatal, a syntax error or an error.
// Elaboration-category diagnostics are left out: the file has no module, and
// a design shall contain at least one top-level module (23.3.1), so an
// elaborating run may legitimately report that.
TEST_F(TimescaleBasic1Test, CompilesWithoutErrors) {
  ASSERT_NE(m_session, nullptr);
  const ErrorContainer *const errorContainer = m_session->getErrorContainer();
  ASSERT_NE(errorContainer, nullptr);
  EXPECT_EQ(describeSourceErrors(errorContainer), "");
}

// Each of these diagnostics would mean HLC misread line 16.
TEST_F(TimescaleBasic1Test, ReportsNoTimescaleDiagnostics) {
  // Line 16 matches Syntax 22-6.
  EXPECT_EQ(findError(ErrorDefinition::PP_SYNTAX_ERROR), nullptr);

  // 1 is a valid integer, ns and ps are valid units, and 1 ps is at least as
  // precise as 1 ns (22.7).
  EXPECT_EQ(findError(ErrorDefinition::PA_TIMESCALE_INVALID_VALUE), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PA_TIMESCALE_INVALID_SCALE), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_VALUE), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_ILLEGAL_TIMESCALE), nullptr);

  // The file has no design element, so the `timescale is not inside one
  // (22.7).
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_IN_DESIGN_ELEMENT), nullptr);

  // `timescale is a compiler directive (22.1), not a text macro usage.
  EXPECT_EQ(findError(ErrorDefinition::PP_UNKOWN_MACRO), nullptr);
}

TEST_F(TimescaleBasic1Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

TEST_F(TimescaleBasic1Test, SourceFileHasNoIncludesOrMacros) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  // There is no `include in the file (22.4).
  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);

  // There is no `define (22.5.1), `undef (22.5.2) or `undefineall (22.5.3),
  // and `timescale is a compiler directive (22.1), not a macro usage.
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// A `timescale only sets the time unit and precision of the design elements
// that follow it (22.7); it declares nothing, and the file has no design
// element, so nothing in the design comes from it.
TEST_F(TimescaleBasic1Test, NoDesignObjectsDeclared) {
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
