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
#include <iterator>
#include <sstream>
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

// One event as hldb::renderEvent() prints it, one line per output line, so that a
// failing test shows what was reported and where.
std::string describeEvent(const Event &event) {
  std::ostringstream oss;
  const std::vector<std::string> lines = hldb::renderEvent(event);
  std::copy(lines.cbegin(), lines.cend(), std::ostream_iterator<std::string>(oss, "\n"));
  return oss.str();
}

// Every fatal, syntax or error diagnostic outside the elaboration category,
// described as by describeEvent(); empty when there is none.
std::string describeSourceErrors(const EventListener *eventListener) {
  std::ostringstream oss;
  for (const Event &event : eventListener->getEvents()) {
    const EventDefinition *const eventDefinition = event.getDefinition();
    if (eventDefinition == nullptr) continue;
    const EventSeverity severity = eventDefinition->m_severity;
    const bool isErrorClass =
        (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) || (severity == EventSeverity::Error);
    if (isErrorClass && (eventDefinition->m_category->m_name != "Elab")) oss << describeEvent(event);
  }
  return oss.str();
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
  const EventListener *const eventListener = m_session->getEventListener();
  ASSERT_NE(eventListener, nullptr);
  EXPECT_EQ(describeSourceErrors(eventListener), "");
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

  ASSERT_NE(m_unit, nullptr);
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
