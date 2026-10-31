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

// Test for tests/Google/chapter-22/22.5.2--undef-nonexisting.sv
//
// The source file holds no design element at all. Lines 1-7 are one-line
// "//" license comments, lines 8-9 are blank, lines 10-15 are one block
// comment carrying the test metadata, and the only SystemVerilog text is:
//
//   16: `undef FOO
//   17: `undef BAR
//
// Both lines are undefine_compiler_directives (IEEE 1800-2023 Syntax 22-4:
// `undef text_macro_identifier), with no arguments and no macro text. Neither
// FOO nor BAR was defined before; nothing in the file defines anything.
// 22.5.2: "An attempt to undefine a text macro that was not previously
// defined using a `define compiler directive can issue a warning." So both
// lines are legal: a tool may warn about them but shall not treat them as
// errors. Undefining an undefined macro changes nothing ("An undefined text
// macro has no value, just as if it had never been defined").
//
// HLDB records an `undef as a PreprocMacroDefinition of type vpiPMDUndef.
//
// Every assertion below follows from the LRM; a red run means HLC deviates
// from it. Some checks read an LRM fact through an HLDB code: vpiPMDUndef says
// a line is an `undef (22.5.2), and an empty generic directive list says the
// file holds none of the directives HLDB records that way (`timescale,
// `default_nettype, `celldefine and the others with a vpiDirectiveType code).
//
// What is checked
//   - No fatal, syntax or error diagnostic outside the elaboration category:
//     undefining an undefined macro is legal.
//   - If HLC reports an "undef of an unknown macro" diagnostic, it is at most
//     a warning and points at line 16 or 17.
//   - None of the preprocessor diagnostics a wrong reading of either line
//     would produce.
//   - Exactly one SourceFile for the .sv, with no includes, no generic
//     directives and no macro instances.
//   - Exactly two PreprocMacroDefinitions, both of type vpiPMDUndef, with no
//     arguments and no macro text: FOO on line 16 and BAR on line 17. None of
//     type vpiPMDDefine.
//   - No design object of any kind is created from this file.
//
// What is NOT checked, and why
//   - Whether HLC warns about the two `undef lines at all. The LRM says a tool
//     "can issue a warning", so the warning is neither required nor
//     forbidden. Nearest assertion: UndefOfUnknownMacroIsAtMostAWarning
//     (when it is issued, it is not an error and is on the right lines).
//   - That FOO and BAR stay undefined. With no later use this is not
//     observable in the model. Nearest assertion:
//     SourceFileHasNoIncludesDirectivesOrMacroUses (zero macro instances).
//   - The order of the two entries in HLDB's definition list. They are told
//     apart by name instead. Nearest assertions: FooUndef and BarUndef.
//   - How HLDB stores an `undef's (absent) macro text. The directive has
//     none, so the stored text is compared with white space removed and must
//     be empty. Nearest assertions: FooUndef and BarUndef.
//   - The end lines of the two `undef entries. The LRM gives the directive no
//     end line: Annex K lists vpiEndLine, but Clause 37 uses it only for
//     assertion locations (37.49), and `undef is not in the VPI object model
//     at all. Where HLDB ends the span of a directive, on its own line or at
//     the start of the next one, depends on whether it counts the
//     terminating newline as part of the directive; that is an HLDB model
//     convention, not an LRM rule. Nearest assertions: the start line checks
//     in FooUndef and BarUndef.
//   - Elaboration-category diagnostics. The file has no module, and a design
//     shall contain at least one top-level module (23.3.1), so an elaborating
//     run may legitimately report that. Nearest assertion:
//     CompilesWithoutErrors (all other error-class diagnostics).
//   - The seven one-line comments and the block comment (5.4). The LRM gives
//     comments no meaning; how HLDB groups, classifies (vpiComment and
//     vpiDocumentComment are marked non-standard) and attaches them is a tool
//     convention. Nearest assertions: the line 16 and 17 checks, which only
//     hold if the 15 comment and blank lines before them were consumed as
//     whole lines.
//   - Columns. The LRM numbers lines (22.13) but not columns. Nearest
//     assertions: the line checks in FooUndef and BarUndef.
//   - The SourceFile time unit and precision. There is no `timescale or
//     timeunit, so the default applies, and the default is
//     implementation-specific (3.14.2.3). Nearest assertion:
//     SourceFileHasNoIncludesDirectivesOrMacroUses (zero directives).
//   - The warning count. The LRM allows warnings here. Nearest assertion:
//     UndefOfUnknownMacroIsAtMostAWarning.
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
#include <hldb/directive.h>
#include <hldb/hldb_vpi_user.h>
#include <hldb/identifier.h>
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
constexpr std::string_view kSourceFileName = "22.5.2--undef-nonexisting.sv";

// LRM 5.3 white space: spaces, tabs, newlines (CR LF on a CRLF checkout) and formfeeds.
constexpr std::string_view kWhiteSpace = " \t\r\n\f";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

// 'text' with every white space character removed. White space only
// separates tokens (LRM 5.2, 5.3), so it is not part of what is compared.
std::string stripWhiteSpace(std::string_view text) {
  std::string stripped;
  for (char c : text) {
    if (kWhiteSpace.find(c) == std::string_view::npos) stripped += c;
  }
  return stripped;
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

// The macro text of 'definition', its tokens concatenated with white space
// removed. This compares the text independently of how HLDB splits it into
// tokens and of whether white space is kept as tokens.
std::string strippedMacroText(const hldb::PreprocMacroDefinition *definition) {
  std::string text;
  if (const hldb::IdentifierCollection *const tokens = definition->getTokens()) {
    for (const hldb::Identifier *token : *tokens) text += stripWhiteSpace(token->getName());
  }
  return text;
}

// True for the severities that make a diagnostic an error.
bool isErrorClass(EventSeverity severity) {
  return (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
         (severity == EventSeverity::Error);
}

// The line a diagnostic points at, or 0 when it carries no location.
uint32_t lineOf(const Event &error) { return error.getArguments().empty() ? 0 : error.getArguments().front().m_line; }

// One diagnostic as " [<code> line <n>: <text>]", for failure messages, so a
// red run shows what was reported and where. <text> is HLC's message
// template, so its placeholders are not filled in.
std::string describeError(const Event &error) {
  const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
  std::string description = " [" + std::to_string(static_cast<int32_t>(error.getType()));
  description += " line " + std::to_string(lineOf(error));
  if (errorInfo != nullptr) description += ": " + std::string(errorInfo->m_lines[0].m_text);
  return description + "]";
}

// Every fatal, syntax or error diagnostic outside the elaboration category,
// described as by describeError(); empty when there is none.
std::string describeSourceErrors(const EventListener *eventListener) {
  std::string description;
  for (const Event &error : eventListener->getEvents()) {
    const EventDefinition *const errorInfo = hldb::findEventDefinition(hlc::getEventTable(), error.getType());
    if (errorInfo == nullptr) continue;
    if (isErrorClass(errorInfo->m_severity) && (errorInfo->m_category->m_name != "Elab")) {
      description += describeError(error);
    }
  }
  return description;
}
}  // namespace

class UndefNonexistingTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.5.2--undef-nonexisting.hlc"}); }
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

  // The definition-list entry named 'name', or nullptr unless there is exactly one.
  static const hldb::PreprocMacroDefinition *getEntry(std::string_view name) {
    const hldb::SourceFile *const sourceFile = getSourceFile();
    if ((sourceFile == nullptr) || (sourceFile->getPreprocMacroDefinitions() == nullptr)) return nullptr;
    const hldb::PreprocMacroDefinition *found = nullptr;
    for (const hldb::PreprocMacroDefinition *definition : *sourceFile->getPreprocMacroDefinitions()) {
      if (definition->getName() != name) continue;
      if (found != nullptr) return nullptr;
      found = definition;
    }
    return found;
  }
};

// 22.5.2: undefining a macro that was not previously defined "can issue a
// warning"; it is not an error. Both lines are well-formed (Syntax 22-4), so
// nothing in the file is fatal, a syntax error or an error.
// Elaboration-category diagnostics are left out: the file has no module, and
// a design shall contain at least one top-level module (23.3.1), so an
// elaborating run may legitimately report that.
TEST_F(UndefNonexistingTest, CompilesWithoutErrors) {
  ASSERT_NE(m_session, nullptr);
  const EventListener *const eventListener = m_session->getEventListener();
  ASSERT_NE(eventListener, nullptr);
  EXPECT_EQ(describeSourceErrors(eventListener), "");
}

// The warning 22.5.2 allows for undefining an undefined macro is optional;
// when HLC issues it (PpUndefUnkownMacro), it is at most a warning and
// points at one of the two `undef lines.
TEST_F(UndefNonexistingTest, UndefOfUnknownMacroIsAtMostAWarning) {
  ASSERT_NE(m_session, nullptr);
  const EventListener *const eventListener = m_session->getEventListener();
  ASSERT_NE(eventListener, nullptr);

  const EventDefinition *const errorInfo =
      hldb::findEventDefinition(hlc::getEventTable(), EventType::PpUndefUnkownMacro);
  ASSERT_NE(errorInfo, nullptr);

  std::string reportedAsError;
  std::string reportedOffTheUndefLines;
  for (const Event &error : eventListener->getEvents()) {
    if (error.getType() != EventType::PpUndefUnkownMacro) continue;
    if (isErrorClass(errorInfo->m_severity)) reportedAsError += describeError(error);
    const uint32_t line = lineOf(error);
    if ((line != 16u) && (line != 17u)) reportedOffTheUndefLines += describeError(error);
  }
  EXPECT_EQ(reportedAsError, "") << "an `undef of an undefined macro is legal (22.5.2)";
  EXPECT_EQ(reportedOffTheUndefLines, "") << "the `undef lines are 16 and 17";
}

TEST_F(UndefNonexistingTest, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

TEST_F(UndefNonexistingTest, SourceFileHasNoIncludesDirectivesOrMacroUses) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  // There is no `include in the file (22.4).
  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);

  // `undef is the only directive in the file. None of the directives HLDB
  // records as generic directives (those with a vpiDirectiveType code:
  // `timescale, `default_nettype, `celldefine, ...) occurs, and `undef has no
  // vpiDirectiveType code, so a generic directive here would be one the
  // source does not contain.
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);

  // A macro is substituted only where `text_macro_identifier appears
  // (22.5.1); neither `FOO nor `BAR does. The names after `undef are its
  // operands, not macro usages.
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// The file holds two `undef lines and no `define, so HLDB's definition list
// holds exactly two entries, both vpiPMDUndef, named FOO and BAR.
TEST_F(UndefNonexistingTest, TwoUndefsAndNoDefine) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);
  const hldb::PreprocMacroDefinitionCollection *const definitions = sourceFile->getPreprocMacroDefinitions();
  ASSERT_EQ(sizeOf(definitions), 2u);
  for (const hldb::PreprocMacroDefinition *definition : *definitions) {
    EXPECT_EQ(definition->getType(), vpiPMDUndef) << definition->getName() << " is an `undef (22.5.2)";
  }
  EXPECT_NE(getEntry("FOO"), nullptr);
  EXPECT_NE(getEntry("BAR"), nullptr);
}

// Line 16: `undef FOO. Syntax 22-4 gives an `undef only a
// text_macro_identifier: no arguments and no macro text.
TEST_F(UndefNonexistingTest, FooUndef) {
  const hldb::PreprocMacroDefinition *const foo = getEntry("FOO");
  ASSERT_NE(foo, nullptr);
  EXPECT_EQ(foo->getType(), vpiPMDUndef);

  const hldb::Identifier *const nameObj = foo->getNameObj();
  ASSERT_NE(nameObj, nullptr);
  EXPECT_EQ(nameObj->getName(), "FOO");
  EXPECT_EQ(nameObj->getStartLine(), 16u);

  EXPECT_EQ(sizeOf(foo->getArguments()), 0u);
  EXPECT_EQ(strippedMacroText(foo), "");

  EXPECT_EQ(foo->getStartLine(), 16u);
  EXPECT_TRUE(endsWith(foo->getFile(), kSourceFileName)) << foo->getFile();
}

// Line 17: `undef BAR, with the same shape as line 16.
TEST_F(UndefNonexistingTest, BarUndef) {
  const hldb::PreprocMacroDefinition *const bar = getEntry("BAR");
  ASSERT_NE(bar, nullptr);
  EXPECT_EQ(bar->getType(), vpiPMDUndef);

  const hldb::Identifier *const nameObj = bar->getNameObj();
  ASSERT_NE(nameObj, nullptr);
  EXPECT_EQ(nameObj->getName(), "BAR");
  EXPECT_EQ(nameObj->getStartLine(), 17u);

  EXPECT_EQ(sizeOf(bar->getArguments()), 0u);
  EXPECT_EQ(strippedMacroText(bar), "");

  EXPECT_EQ(bar->getStartLine(), 17u);
  EXPECT_TRUE(endsWith(bar->getFile(), kSourceFileName)) << bar->getFile();
}

// `undef is preprocessor text only (22.5.2), and the file has no design
// element, so nothing in the design comes from it.
TEST_F(UndefNonexistingTest, NoDesignObjectsDeclared) {
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
