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

// Test for tests/Google/chapter-22/22.5.2--undef-basic.sv
//
// The source file holds no design element at all. Lines 1-7 are one-line
// "//" license comments, lines 8-9 are blank, lines 10-15 are one block
// comment carrying the test metadata, and the only SystemVerilog text is:
//
//   16: `define FOO "foo"
//   17: `undef FOO
//
// Line 16 is a text_macro_definition (IEEE 1800-2023 Syntax 22-2):
// text_macro_name is the simple identifier FOO with no formal arguments (no
// "(" follows it), and the macro text is the string literal "foo" (5.9).
//
// Line 17 is an undefine_compiler_directive (Syntax 22-4: `undef
// text_macro_identifier). 22.5.2: "The directive `undef shall undefine the
// specified text macro if previously defined by a `define compiler directive
// within the compilation unit." FOO was defined on line 16, so line 17
// undefines it. The warning 22.5.2 allows ("An attempt to undefine a text
// macro that was not previously defined using a `define compiler directive
// can issue a warning") does not apply. The `undef has no macro text and no
// arguments. FOO is never used, so no substitution takes place.
//
// HLDB records a `define and an `undef alike as a PreprocMacroDefinition and
// tells them apart by its type: vpiPMDDefine for line 16, vpiPMDUndef for
// line 17.
//
// Every assertion below follows from the LRM; a red run means HLC deviates
// from it. Some checks read an LRM fact through an HLDB code: vpiPMDDefine
// and vpiPMDUndef say which of `define (22.5.1) and `undef (22.5.2) a line
// is, and an empty generic directive list says the file holds none of the
// directives HLDB records that way (`timescale, `default_nettype,
// `celldefine and the others with a vpiDirectiveType code).
//
// What is checked
//   - No fatal, syntax or error diagnostic outside the elaboration category,
//     and none of the preprocessor diagnostics a wrong reading of either line
//     would produce; in particular no "undef of an unknown macro" diagnostic,
//     since FOO is defined before it is undefined.
//   - Exactly one SourceFile for the .sv, with no includes, no generic
//     directives and no macro instances.
//   - Exactly two PreprocMacroDefinitions, both named FOO: the `define of
//     line 16 (vpiPMDDefine, no arguments, macro text "foo") and the `undef
//     of line 17 (vpiPMDUndef, no arguments, no macro text).
//   - No design object of any kind is created from this file.
//
// What is NOT checked, and why
//   - That FOO "has no value, just as if it had never been defined" after
//     line 17 (22.5.2). With no later use of FOO this is not observable in the
//     model. Nearest assertion: FooUndef (the `undef is recorded, after the
//     `define).
//   - Macro expansion results. FOO is never used, so there is no substituted
//     text. Nearest assertions: SourceFileHasNoIncludesDirectivesOrMacroUses
//     (zero macro instances) and FooDefinition (the stored macro text).
//   - How HLDB splits the macro text into tokens, whether it keeps white
//     space as tokens, and whether a CR from a CR LF line ending stays glued
//     to the last token. The LRM defines the macro text, not its storage, and
//     never defines CR LF as one newline (5.3), so the text is compared with
//     white space removed. Nearest assertions: the macro text checks in
//     FooDefinition and FooUndef.
//   - The end lines of the `define and `undef entries. The LRM gives these
//     directives no end line: Annex K lists vpiEndLine, but Clause 37 uses it
//     only for assertion locations (37.49), and neither directive is in the
//     VPI object model at all. Where HLDB ends the span of a directive, on
//     its own line or at the start of the next one, depends on whether it
//     counts the terminating newline as part of the directive; that is an
//     HLDB model convention, not an LRM rule. Nearest assertions: the start
//     line checks in FooDefinition and FooUndef.
//   - The order of the two entries in HLDB's definition list. They are told
//     apart by type and line instead. Nearest assertion:
//     DefineAndUndefAreBothRecorded.
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
//     assertions: the line checks in FooDefinition and FooUndef.
//   - The SourceFile time unit and precision. There is no `timescale or
//     timeunit, so the default applies, and the default is
//     implementation-specific (3.14.2.3). Nearest assertion:
//     SourceFileHasNoIncludesDirectivesOrMacroUses (zero directives).
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
constexpr std::string_view kSourceFileName = "22.5.2--undef-basic.sv";

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

class UndefBasicTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.5.2--undef-basic.hlc"}); }
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

  // The definition-list entry of type 'type' (vpiPMDDefine, vpiPMDUndef, ...),
  // or nullptr unless there is exactly one.
  static const hldb::PreprocMacroDefinition *getEntryOfType(int32_t type) {
    const hldb::SourceFile *const sourceFile = getSourceFile();
    if ((sourceFile == nullptr) || (sourceFile->getPreprocMacroDefinitions() == nullptr)) return nullptr;
    const hldb::PreprocMacroDefinition *found = nullptr;
    for (const hldb::PreprocMacroDefinition *definition : *sourceFile->getPreprocMacroDefinitions()) {
      if (definition->getType() != type) continue;
      if (found != nullptr) return nullptr;
      found = definition;
    }
    return found;
  }
};

// The file is a well-formed text_macro_definition (Syntax 22-2), a
// well-formed undefine_compiler_directive (Syntax 22-4) and comments; nothing
// in it is fatal, a syntax error or an error. Elaboration-category
// diagnostics are left out: the file has no module, and a design shall
// contain at least one top-level module (23.3.1), so an elaborating run may
// legitimately report that.
TEST_F(UndefBasicTest, CompilesWithoutErrors) {
  ASSERT_NE(m_session, nullptr);
  const ErrorContainer *const errorContainer = m_session->getErrorContainer();
  ASSERT_NE(errorContainer, nullptr);
  EXPECT_EQ(describeSourceErrors(errorContainer), "");
}

// Each of these diagnostics would mean HLC misread one of the two lines.
TEST_F(UndefBasicTest, ReportsNoMacroDiagnostics) {
  // Line 16 matches Syntax 22-2 and line 17 Syntax 22-4.
  EXPECT_EQ(findError(ErrorDefinition::PP_SYNTAX_ERROR), nullptr);

  // FOO is defined on line 16 before it is undefined on line 17, so the
  // warning 22.5.2 allows for undefining a macro that was not previously
  // defined does not apply.
  EXPECT_EQ(findError(ErrorDefinition::PP_UNDEF_UNKOWN_MACRO), nullptr);

  // FOO is defined once, and is not a compiler directive name (22.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_MULTIPLY_DEFINED_MACRO), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_NAME_RESERVED), nullptr);

  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_UNUSED_ARGUMENT), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_UNDEFINED_ARGUMENT), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_UNKOWN_MACRO), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_PARENTHESIS_NEEDED), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_TOO_MANY_ARGS_MACRO), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_NO_DEFAULT_VALUE), nullptr);

  // "foo" is closed on line 16 (5.9) and refers to no macro.
  EXPECT_EQ(findError(ErrorDefinition::PP_UNTERMINATED_STRING), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_RECURSIVE_MACRO_DEFINITION), nullptr);

  // The file has no design element, so neither directive is inside one.
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_IN_DESIGN_ELEMENT), nullptr);
}

TEST_F(UndefBasicTest, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

TEST_F(UndefBasicTest, SourceFileHasNoIncludesDirectivesOrMacroUses) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  // There is no `include in the file (22.4).
  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);

  // `define and `undef are the only directives in the file. None of the
  // directives HLDB records as generic directives (those with a
  // vpiDirectiveType code: `timescale, `default_nettype, `celldefine, ...)
  // occurs, and neither `define nor `undef has a vpiDirectiveType code, so a
  // generic directive here would be one the source does not contain.
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);

  // A macro is substituted only where `text_macro_identifier appears
  // (22.5.1); `FOO does not appear in the file. The FOO of line 17 is the
  // operand of `undef, not a macro usage.
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// The file holds one `define and one `undef of FOO, and nothing else that
// HLDB records in its definition list.
TEST_F(UndefBasicTest, DefineAndUndefAreBothRecorded) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);
  const hldb::PreprocMacroDefinitionCollection *const definitions = sourceFile->getPreprocMacroDefinitions();
  ASSERT_EQ(sizeOf(definitions), 2u);
  for (const hldb::PreprocMacroDefinition *definition : *definitions) {
    EXPECT_EQ(definition->getName(), "FOO");
  }
  EXPECT_NE(getEntryOfType(vpiPMDDefine), nullptr) << "line 16 is one `define (22.5.1)";
  EXPECT_NE(getEntryOfType(vpiPMDUndef), nullptr) << "line 17 is one `undef (22.5.2)";
}

// Line 16: `define FOO "foo"
TEST_F(UndefBasicTest, FooDefinition) {
  const hldb::PreprocMacroDefinition *const foo = getEntryOfType(vpiPMDDefine);
  ASSERT_NE(foo, nullptr);

  // text_macro_name is the simple identifier FOO.
  EXPECT_EQ(foo->getName(), "FOO");
  const hldb::Identifier *const nameObj = foo->getNameObj();
  ASSERT_NE(nameObj, nullptr);
  EXPECT_EQ(nameObj->getName(), "FOO");
  EXPECT_EQ(nameObj->getStartLine(), 16u);

  // No "(" follows FOO, so there is no list_of_formal_arguments; the macro
  // text is the string literal "foo", double quotes included (5.9).
  EXPECT_EQ(sizeOf(foo->getArguments()), 0u);
  EXPECT_EQ(strippedMacroText(foo), "\"foo\"");

  // The `define directive starts on line 16. Its end line is not checked;
  // see the header.
  EXPECT_EQ(foo->getStartLine(), 16u);
  EXPECT_TRUE(endsWith(foo->getFile(), kSourceFileName)) << foo->getFile();
}

// Line 17: `undef FOO. Syntax 22-4 gives an `undef only a
// text_macro_identifier: no arguments and no macro text.
TEST_F(UndefBasicTest, FooUndef) {
  const hldb::PreprocMacroDefinition *const undef = getEntryOfType(vpiPMDUndef);
  ASSERT_NE(undef, nullptr);

  // The text_macro_identifier is FOO.
  EXPECT_EQ(undef->getName(), "FOO");
  const hldb::Identifier *const nameObj = undef->getNameObj();
  ASSERT_NE(nameObj, nullptr);
  EXPECT_EQ(nameObj->getName(), "FOO");
  EXPECT_EQ(nameObj->getStartLine(), 17u);

  EXPECT_EQ(sizeOf(undef->getArguments()), 0u);
  EXPECT_EQ(strippedMacroText(undef), "");

  // The `undef directive starts on line 17, after the `define of line 16.
  // Its end line is not checked; see the header.
  EXPECT_EQ(undef->getStartLine(), 17u);
  EXPECT_TRUE(endsWith(undef->getFile(), kSourceFileName)) << undef->getFile();
}

// A text macro and its `undef are preprocessor text only (22.5.1, 22.5.2):
// FOO is not a parameter or a string variable. The file also has no design
// element, so nothing in the design comes from it.
TEST_F(UndefBasicTest, NoDesignObjectsDeclared) {
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
