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

// Test for tests/Google/chapter-22/22.5.3--undefineall-and-redefine.sv
//
// The source file holds no design element at all. Lines 1-7 are one-line
// "//" license comments, lines 8-9 are blank, lines 10-15 are one block
// comment carrying the test metadata, and the only SystemVerilog text is:
//
//   16: `define FOO "foo"
//   17: `undefineall
//   18: `define FOO 5
//   19: `undefineall
//   20: `define FOO(x,y) (x + y)
//
// IEEE 1800-2023 22.5.3: "The `undefineall directive shall undefine all text
// macros previously defined by `define compiler directives within the
// compilation unit. This directive takes no arguments and may appear anywhere
// in the source description." So:
//   - Line 16 defines FOO with no formal arguments and the macro text "foo",
//     a string literal (5.9).
//   - Line 17 undefines every macro, FOO included.
//   - Line 18 defines FOO again, now with the macro text 5. FOO is undefined
//     at that point, so this is a fresh definition, not a redefinition of a
//     defined macro.
//   - Line 19 undefines every macro again.
//   - Line 20 defines FOO a third time, again while it is undefined: "(" follows
//     FOO immediately, so (x,y) is its list_of_formal_arguments, and the macro
//     text is (x + y), which uses both.
// Each `define is a text_macro_definition (Syntax 22-2); each `undefineall
// carries no argument. FOO is never used, so no substitution takes place.
//
// HLDB records `define and `undefineall alike as PreprocMacroDefinitions and
// tells them apart by type: vpiPMDDefine and vpiPMDUndefineAll.
//
// Every assertion below follows from the LRM; a red run means HLC deviates
// from it. Some checks read an LRM fact through an HLDB code: vpiPMDDefine
// and vpiPMDUndefineAll say which of `define (22.5.1) and `undefineall
// (22.5.3) a line is, and an empty generic directive list says the file holds
// none of the directives HLDB records that way (`timescale, `default_nettype,
// `celldefine and the others with a vpiDirectiveType code).
//
// What is checked
//   - No fatal, syntax or error diagnostic outside the elaboration category,
//     and none of the preprocessor diagnostics a wrong reading of the lines
//     would produce; in particular no "multiply defined macro" diagnostic,
//     since every `define of FOO follows an `undefineall.
//   - Exactly one SourceFile for the .sv, with no includes, no generic
//     directives and no macro instances.
//   - Exactly five PreprocMacroDefinitions: three of type vpiPMDDefine, on
//     lines 16, 18 and 20, and two of type vpiPMDUndefineAll, on lines 17
//     and 19.
//   - The three `define of FOO each keep their own arguments and macro text:
//     none and "foo", none and 5, x and y and (x + y).
//   - Neither `undefineall has arguments or macro text.
//   - No design object of any kind is created from this file.
//
// What is NOT checked, and why
//   - Which definition of FOO is in effect at each point. With no use of FOO
//     this is not observable in the model. Nearest assertions: the three FOO
//     definition tests and the two `undefineall tests, which fix what each
//     line recorded and in which order the lines come.
//   - The name HLDB gives an `undefineall entry. The directive has no
//     operand ("This directive takes no arguments", 22.5.3), so the LRM
//     gives it no name.
//     Nearest assertion: UndefineAllEntries (type, line and no arguments).
//   - The order of the entries in HLDB's definition list. They are told apart
//     by line instead. Nearest assertion: FiveEntriesThreeDefinesTwoUndefineAlls.
//   - How HLDB splits the macro text into tokens, whether it keeps white
//     space as tokens, and whether a CR from a CR LF line ending stays glued
//     to the last token. The LRM defines the macro text, not its storage, and
//     never defines CR LF as one newline (5.3), so the text is compared with
//     white space removed. Nearest assertions: the macro text checks.
//   - The end lines of the five entries. The LRM gives `define and
//     `undefineall no end line: Annex K lists vpiEndLine, but Clause 37 uses
//     it only for assertion locations (37.49), and neither directive is in
//     the VPI object model at all. Where HLDB ends the span of a directive,
//     on its own line or at the start of the next one, depends on whether it
//     counts the terminating newline as part of the directive; that is an
//     HLDB model convention, not an LRM rule. Nearest assertions: the start
//     line lookups in FirstFooDefinition, SecondFooDefinition,
//     ThirdFooDefinition and UndefineAllEntries, which find each entry by
//     the line it starts on.
//   - Elaboration-category diagnostics. The file has no module, and a design
//     shall contain at least one top-level module (23.3.1), so an elaborating
//     run may legitimately report that. Nearest assertion:
//     CompilesWithoutErrors (all other error-class diagnostics).
//   - The seven one-line comments and the block comment (5.4). The LRM gives
//     comments no meaning; how HLDB groups, classifies (vpiComment and
//     vpiDocumentComment are marked non-standard) and attaches them is a tool
//     convention. Nearest assertions: the line 16 to 20 checks, which only
//     hold if the 15 comment and blank lines before them were consumed as
//     whole lines.
//   - Columns. The LRM numbers lines (22.13) but not columns. Nearest
//     assertions: the same line checks.
//   - The SourceFile time unit and precision. There is no `timescale or
//     timeunit, so the default applies, and the default is
//     implementation-specific (3.14.2.3). Nearest assertion:
//     SourceFileHasNoIncludesDirectivesOrMacroUses (zero directives).
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
constexpr std::string_view kSourceFileName = "22.5.3--undefineall-and-redefine.sv";

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
    const EventSeverity severity = errorInfo->m_severity;
    const bool isErrorClass = (severity == EventSeverity::Fatal) || (severity == EventSeverity::Syntax) ||
                              (severity == EventSeverity::Error);
    if (isErrorClass && (errorInfo->m_category->m_name != "Elab")) description += describeError(error);
  }
  return description;
}
}  // namespace

class UndefineAllAndRedefineTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.5.3--undefineall-and-redefine.hlc"}); }
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

  // Number of definition-list entries of type 'type' (vpiPMDDefine, ...).
  static size_t countEntriesOfType(int32_t type) {
    const hldb::SourceFile *const sourceFile = getSourceFile();
    if ((sourceFile == nullptr) || (sourceFile->getPreprocMacroDefinitions() == nullptr)) return 0;
    size_t count = 0;
    for (const hldb::PreprocMacroDefinition *definition : *sourceFile->getPreprocMacroDefinitions()) {
      if (definition->getType() == type) ++count;
    }
    return count;
  }

  // The definition-list entry starting on 'line', or nullptr unless there is exactly one.
  static const hldb::PreprocMacroDefinition *getEntryOnLine(uint32_t line) {
    const hldb::SourceFile *const sourceFile = getSourceFile();
    if ((sourceFile == nullptr) || (sourceFile->getPreprocMacroDefinitions() == nullptr)) return nullptr;
    const hldb::PreprocMacroDefinition *found = nullptr;
    for (const hldb::PreprocMacroDefinition *definition : *sourceFile->getPreprocMacroDefinitions()) {
      if (definition->getStartLine() != line) continue;
      if (found != nullptr) return nullptr;
      found = definition;
    }
    return found;
  }
};

// The file is three well-formed text_macro_definitions (Syntax 22-2), two
// `undefineall directives and comments; nothing in it is fatal, a syntax error
// or an error. Elaboration-category diagnostics are left out: the file has no
// module, and a design shall contain at least one top-level module (23.3.1),
// so an elaborating run may legitimately report that.
TEST_F(UndefineAllAndRedefineTest, CompilesWithoutErrors) {
  ASSERT_NE(m_session, nullptr);
  const EventListener *const eventListener = m_session->getEventListener();
  ASSERT_NE(eventListener, nullptr);
  EXPECT_EQ(describeSourceErrors(eventListener), "");
}

TEST_F(UndefineAllAndRedefineTest, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

TEST_F(UndefineAllAndRedefineTest, SourceFileHasNoIncludesDirectivesOrMacroUses) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  // There is no `include in the file (22.4).
  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);

  // `define and `undefineall are the only directives in the file. None of the
  // directives HLDB records as generic directives (those with a
  // vpiDirectiveType code: `timescale, `default_nettype, `celldefine, ...)
  // occurs, and neither `define nor `undefineall has a vpiDirectiveType code,
  // so a generic directive here would be one the source does not contain.
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);

  // A macro is substituted only where `text_macro_identifier appears
  // (22.5.1); `FOO does not appear in the file.
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// Three `define lines (16, 18, 20) and two `undefineall lines (17, 19).
TEST_F(UndefineAllAndRedefineTest, FiveEntriesThreeDefinesTwoUndefineAlls) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 5u);
  EXPECT_EQ(countEntriesOfType(vpiPMDDefine), 3u);
  EXPECT_EQ(countEntriesOfType(vpiPMDUndefineAll), 2u);
}

// Line 16: `define FOO "foo"
TEST_F(UndefineAllAndRedefineTest, FirstFooDefinition) {
  const hldb::PreprocMacroDefinition *const foo = getEntryOnLine(16);
  ASSERT_NE(foo, nullptr);
  EXPECT_EQ(foo->getType(), vpiPMDDefine);
  EXPECT_EQ(foo->getName(), "FOO");
  const hldb::Identifier *const nameObj = foo->getNameObj();
  ASSERT_NE(nameObj, nullptr);
  EXPECT_EQ(nameObj->getName(), "FOO");

  // No "(" follows FOO; the macro text is the string literal "foo" (5.9).
  EXPECT_EQ(sizeOf(foo->getArguments()), 0u);
  EXPECT_EQ(strippedMacroText(foo), "\"foo\"");
}

// Line 18: `define FOO 5, a fresh definition after the `undefineall of line 17.
TEST_F(UndefineAllAndRedefineTest, SecondFooDefinition) {
  const hldb::PreprocMacroDefinition *const foo = getEntryOnLine(18);
  ASSERT_NE(foo, nullptr);
  EXPECT_EQ(foo->getType(), vpiPMDDefine);
  EXPECT_EQ(foo->getName(), "FOO");

  // No "(" follows FOO; the macro text is 5, not the earlier "foo".
  EXPECT_EQ(sizeOf(foo->getArguments()), 0u);
  EXPECT_EQ(strippedMacroText(foo), "5");
}

// Line 20: `define FOO(x,y) (x + y), a fresh definition after the
// `undefineall of line 19, now with formal arguments.
TEST_F(UndefineAllAndRedefineTest, ThirdFooDefinition) {
  const hldb::PreprocMacroDefinition *const foo = getEntryOnLine(20);
  ASSERT_NE(foo, nullptr);
  EXPECT_EQ(foo->getType(), vpiPMDDefine);
  EXPECT_EQ(foo->getName(), "FOO");

  // "(" follows FOO immediately, so (x,y) is its list_of_formal_arguments
  // (22.5.1); the macro text follows the list after a space.
  const hldb::IdentifierCollection *const arguments = foo->getArguments();
  ASSERT_EQ(sizeOf(arguments), 2u);
  EXPECT_EQ(arguments->at(0)->getName(), "x");
  EXPECT_EQ(arguments->at(1)->getName(), "y");
  EXPECT_EQ(strippedMacroText(foo), "(x+y)");
}

// Lines 17 and 19: `undefineall, which "takes no arguments" (22.5.3) and has
// no macro text.
TEST_F(UndefineAllAndRedefineTest, UndefineAllEntries) {
  const std::vector<uint32_t> lines = {17, 19};
  for (uint32_t line : lines) {
    const hldb::PreprocMacroDefinition *const undefineAll = getEntryOnLine(line);
    EXPECT_NE(undefineAll, nullptr) << "no entry on line " << line;
    if (undefineAll == nullptr) continue;
    EXPECT_EQ(undefineAll->getType(), vpiPMDUndefineAll) << "line " << line;
    EXPECT_EQ(sizeOf(undefineAll->getArguments()), 0u) << "line " << line;
    EXPECT_EQ(strippedMacroText(undefineAll), "") << "line " << line;
    EXPECT_TRUE(endsWith(undefineAll->getFile(), kSourceFileName)) << undefineAll->getFile();
  }
}

// Text macros and `undefineall are preprocessor text only (22.5.1, 22.5.3):
// FOO is not a parameter, variable or function. The file also has no design
// element, so nothing in the design comes from it.
TEST_F(UndefineAllAndRedefineTest, NoDesignObjectsDeclared) {
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
