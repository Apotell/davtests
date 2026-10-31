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

// Test for tests/Google/chapter-22/22.5.1--define-expansion_22.sv
//
// The source file holds no design element at all. Lines 1-7 are one-line
// "//" license comments, lines 8-9 are blank, lines 10-15 are one block
// comment carrying the test metadata, and the only SystemVerilog text is:
//
//   16: `define max(a,b)((a) > (b) ? (a) : (b))
//
// This is the definition of the IEEE 1800-2023 22.5.1 example on literal
// argument substitution; the example's usage (n = `max(p+q, r+s);) is not in
// the file.
//
// Line 16 is a text_macro_definition (Syntax 22-2):
//   - text_macro_name is the simple identifier max. "If formal arguments are
//     used, the list of formal argument names shall be enclosed in
//     parentheses immediately following the name of the macro" (22.5.1), and
//     "(" does follow max immediately, so (a,b) is its
//     list_of_formal_arguments: formal arguments a and b, in that order,
//     neither with a default. max is neither a keyword nor a compiler
//     directive name (22.1), so it is a legal macro name.
//   - The list ends at its own closing parenthesis. Everything after it up to
//     the newline is the macro text (22.5.1), even though no white space
//     separates the two: ((a) > (b) ? (a) : (b)). Both formal arguments are
//     used in it, each twice, "in the same manner as an identifier".
//   - `define may appear outside design elements (22.5.1), so the line is
//     legal.
//   - max is never used (no `max in the file), so no substitution takes place
//     (22.5.1).
//   - A text macro is preprocessor text only; it declares no parameter,
//     variable, function or design element (22.5.1, 3.12.1).
//
// Every assertion below follows from the LRM; a red run means HLC deviates
// from it. Two checks read an LRM fact through an HLDB code: vpiPMDDefine
// says line 16 is a `define (not an `undef, 22.5.2, or an `undefineall,
// 22.5.3), and an empty generic directive list says the file holds none of
// the directives HLDB records that way (`timescale, `default_nettype,
// `celldefine and the others with a vpiDirectiveType code).
//
// What is checked
//   - No fatal, syntax or error diagnostic outside the elaboration category,
//     and none of the preprocessor diagnostics a wrong reading of the line
//     would produce.
//   - Exactly one SourceFile for the .sv, with no includes, no generic
//     directives and no macro instances.
//   - Exactly one PreprocMacroDefinition, max, of type vpiPMDDefine, with the
//     formal arguments a and b and the macro text ((a) > (b) ? (a) : (b)),
//     located on line 16: the formal argument list ends at its own closing
//     parenthesis, and none of it leaks into the macro text.
//   - No design object of any kind is created from this file.
//
// What is NOT checked, and why
//   - Macro expansion results. No macro is used, so there is no substituted
//     text and no expression; the LRM's expansion n = ((p+q) > (r+s) ? (p+q)
//     : (r+s)) belongs to a usage this file does not contain. Nearest
//     assertions: MacroDefinitionIsNeverUsed (zero PreprocMacroInstances)
//     and MacroDefinition (the stored macro text).
//   - That an expression passed as an actual argument would be evaluated
//     twice, as 22.5.1 warns. Evaluation only happens while simulation time
//     advances, so it is permanently out of scope. Nearest assertion:
//     MacroDefinition (each formal argument appears twice in the macro text).
//   - How HLDB splits the macro text into tokens, whether it keeps white
//     space as tokens, and whether a CR from a CR LF line ending stays glued
//     to the last token. The LRM defines the macro text, not its storage, and
//     never defines CR LF as one newline (5.3), so the text is compared with
//     white space removed. Nearest assertion: the macro text check in
//     MacroDefinition.
//   - The end line of the definition. The LRM gives a macro definition no
//     end line: Annex K lists vpiEndLine, but Clause 37 uses it only for
//     assertion locations (37.49), and macro definitions are not in the VPI
//     object model at all. Where HLDB ends the span of a directive, on its
//     own line or at the start of the next one, depends on whether it counts
//     the terminating newline as part of the directive; that is an HLDB
//     model convention, not an LRM rule. Nearest assertion: the start line
//     check in MacroDefinition.
//   - That the definition stays in effect to the end of the compilation unit
//     (22.2). With no later use this is not observable in the model. Nearest
//     assertion: MacroDefinitionIsNeverUsed.
//   - Elaboration-category diagnostics. The file has no module, and a design
//     shall contain at least one top-level module (23.3.1), so an elaborating
//     run may legitimately report that. Nearest assertion:
//     CompilesWithoutErrors (all other error-class diagnostics).
//   - The seven one-line comments and the block comment (5.4). The LRM gives
//     comments no meaning; how HLDB groups, classifies (vpiComment and
//     vpiDocumentComment are marked non-standard) and attaches them is a tool
//     convention. Nearest assertions: the line 16 checks in MacroDefinition,
//     which only hold if the 15 comment and blank lines before it were
//     consumed as whole lines.
//   - Columns. The LRM numbers lines (22.13) but not columns. Nearest
//     assertions: the line checks in MacroDefinition.
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
constexpr std::string_view kSourceFileName = "22.5.1--define-expansion_22.sv";

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

class DefineExpansion22Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.5.1--define-expansion_22.hlc"}); }
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

// The file is one well-formed text_macro_definition (Syntax 22-2) and
// comments; nothing in it is fatal, a syntax error or an error.
// Elaboration-category diagnostics are left out: the file has no module, and
// a design shall contain at least one top-level module (23.3.1), so an
// elaborating run may legitimately report that.
TEST_F(DefineExpansion22Test, CompilesWithoutErrors) {
  ASSERT_NE(m_session, nullptr);
  const EventListener *const eventListener = m_session->getEventListener();
  ASSERT_NE(eventListener, nullptr);
  EXPECT_EQ(describeSourceErrors(eventListener), "");
}

TEST_F(DefineExpansion22Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

TEST_F(DefineExpansion22Test, SourceFileHasNoIncludesDirectivesOrMacroUses) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  // There is no `include in the file (22.4).
  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);

  // `define is the only directive in the file. None of the directives HLDB
  // records as generic directives (those with a vpiDirectiveType code:
  // `timescale, `default_nettype, `celldefine, ...) occurs, and `define has no
  // vpiDirectiveType code, so a generic directive here would be one the
  // source does not contain.
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);
}

// A macro is substituted only where `text_macro_identifier appears (22.5.1).
// `max does not appear in the file.
TEST_F(DefineExpansion22Test, MacroDefinitionIsNeverUsed) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// Line 16: `define max(a,b)((a) > (b) ? (a) : (b))
TEST_F(DefineExpansion22Test, MacroDefinition) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);
  const hldb::PreprocMacroDefinitionCollection *const definitions = sourceFile->getPreprocMacroDefinitions();
  ASSERT_EQ(sizeOf(definitions), 1u);
  const hldb::PreprocMacroDefinition *const maxMacro = definitions->front();

  // Line 16 is a `define (22.5.1), not an `undef (22.5.2) or an `undefineall
  // (22.5.3).
  EXPECT_EQ(maxMacro->getType(), vpiPMDDefine);

  // text_macro_name is the simple identifier max.
  EXPECT_EQ(maxMacro->getName(), "max");
  const hldb::Identifier *const nameObj = maxMacro->getNameObj();
  ASSERT_NE(nameObj, nullptr);
  EXPECT_EQ(nameObj->getName(), "max");
  EXPECT_EQ(nameObj->getStartLine(), 16u);

  // list_of_formal_arguments is (a,b): two simple identifiers, in this order.
  // It ends at its own closing parenthesis, so nothing of the macro text that
  // follows belongs to it.
  const hldb::IdentifierCollection *const arguments = maxMacro->getArguments();
  ASSERT_EQ(sizeOf(arguments), 2u);
  EXPECT_EQ(arguments->at(0)->getName(), "a");
  EXPECT_EQ(arguments->at(1)->getName(), "b");

  // The macro text starts right after that closing parenthesis, with no white
  // space between, and ends at the newline (22.5.1); the formal argument list
  // is not part of it. a and b each appear in it twice.
  EXPECT_EQ(strippedMacroText(maxMacro), "((a)>(b)?(a):(b))");

  // The `define directive starts on line 16. Its end line is not checked;
  // see the header.
  EXPECT_EQ(maxMacro->getStartLine(), 16u);
  EXPECT_TRUE(endsWith(maxMacro->getFile(), kSourceFileName)) << maxMacro->getFile();
}

// A text macro is preprocessor text only (22.5.1): max is not a function, a
// parameter or a variable. The file also has no design element, so nothing
// in the design comes from it.
TEST_F(DefineExpansion22Test, NoDesignObjectsDeclared) {
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
