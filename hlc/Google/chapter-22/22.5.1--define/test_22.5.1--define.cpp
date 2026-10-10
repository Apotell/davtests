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

// Test for tests/Google/chapter-22/22.5.1--define.sv
//
// The source file holds no design element at all. Lines 1-7 are one-line
// "//" license comments, lines 8-9 are blank, lines 10-15 are one block
// comment carrying the test metadata, and the only SystemVerilog text is:
//
//   16: `define FOUR 5
//   17: `define SOMESTRING "somestring"
//
// Both lines are text_macro_definitions (IEEE 1800-2023 Syntax 22-2):
//   - text_macro_name is a simple identifier with no list_of_formal_arguments
//     (no "(" follows the name), so each macro has zero formal arguments.
//   - The macro text is everything after the name up to the newline that is
//     not preceded by a backslash (22.5.1); the newline itself is not part of
//     it. For FOUR the macro text is 5; for SOMESTRING it is the string
//     literal "somestring", double quotes included (5.9). The name FOUR
//     carries no meaning: the preprocessor stores the text 5 verbatim.
//   - `define may appear outside design elements (22.5.1), and FOUR and
//     SOMESTRING are not compiler directive names (22.1), so both are legal.
//   - Neither macro is ever used (no `FOUR or `SOMESTRING in the file), so no
//     substitution takes place (22.5.1).
//   - A text macro is preprocessor text only; it declares no parameter,
//     variable, type or design element (22.5.1, 3.12.1).
//
// Every assertion below follows from the LRM; a red run means HLC deviates
// from it. Two checks read an LRM fact through an HLDB code: vpiPMDDefine
// says each line is a `define (not an `undef, 22.5.2, or an `undefineall,
// 22.5.3), and an empty generic directive list says the file holds none of
// the directives HLDB records that way (`timescale, `default_nettype,
// `celldefine and the others with a vpiDirectiveType code).
//
// What is checked
//   - No fatal, syntax or error diagnostic outside the elaboration category,
//     and none of the preprocessor diagnostics a wrong reading of these two
//     lines would produce.
//   - Exactly one SourceFile for the .sv, with no includes, no generic
//     directives and no macro instances.
//   - Exactly two PreprocMacroDefinitions, FOUR and SOMESTRING, each of type
//     vpiPMDDefine, with zero arguments and macro text 5 and "somestring",
//     located on lines 16 and 17.
//   - No design object of any kind is created from this file.
//
// What is NOT checked, and why
//   - How HLDB splits the macro text into tokens, and whether it keeps white
//     space as tokens. The LRM defines the macro text, not its storage, so
//     the text is compared with white space removed. Nearest assertions: the
//     macro text checks in FourDefinition and SomestringDefinition.
//   - Macro expansion results. No macro is used, so there is no substituted
//     text, no expression to reduce and no string to print; what `SOMESTRING
//     would display at run time is a simulation fact and permanently out of
//     scope. Nearest assertions: MacroDefinitionsAreNeverUsed (zero
//     PreprocMacroInstances) and SomestringDefinition (the stored macro text
//     is the quoted string literal).
//   - That each definition stays in effect to the end of the compilation unit
//     (22.2). With no later use this is not observable in the model. Nearest
//     assertion: MacroDefinitionsAreNeverUsed.
//   - Elaboration-category diagnostics. The file has no module, and a design
//     shall contain at least one top-level module (23.3.1), so an elaborating
//     run may legitimately report that. Nearest assertion:
//     CompilesWithoutErrors (all other error-class diagnostics).
//   - The seven one-line comments and the block comment (5.4). The LRM gives
//     comments no meaning; how HLDB groups, classifies (vpiComment and
//     vpiDocumentComment are marked non-standard) and attaches them is a tool
//     convention. Nearest assertions: the start lines 16 and 17 in
//     FourDefinition and SomestringDefinition, which only hold if the 15
//     comment and blank lines before them were consumed as whole lines.
//   - Columns. The LRM numbers lines (22.13) but not columns, and the column
//     base and node anchor are tool conventions. Nearest assertions: the start
//     and end line checks in FourDefinition and SomestringDefinition.
//   - The SourceFile time unit and precision. There is no `timescale or
//     timeunit, so the default applies, and the default is
//     implementation-specific (3.14.2.3). Nearest assertion:
//     SourceFileHasNoIncludesDirectivesOrMacroUses (zero directives).
//   - The warning count. The LRM neither requires nor forbids warnings for a
//     file without design elements. Nearest assertion: CompilesWithoutErrors.
//   - Whether a CR from a CR LF line ending stays glued to the last macro
//     text token. 22.5.1 ends the macro text at the newline, but the LRM
//     never defines CR LF as one newline (5.3 only says newlines are white
//     space), so a stored trailing CR is not clearly an LRM deviation.
//     Nearest assertions: the macro text checks in FourDefinition and
//     SomestringDefinition, which ignore white space.
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
constexpr std::string_view kSourceFileName = "22.5.1--define.sv";

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

// Number of macro definitions in 'sourceFile' named 'name'.
size_t countDefinitions(const hldb::SourceFile *sourceFile, std::string_view name) {
  size_t count = 0;
  if (const hldb::PreprocMacroDefinitionCollection *const definitions = sourceFile->getPreprocMacroDefinitions()) {
    for (const hldb::PreprocMacroDefinition *definition : *definitions) {
      if (definition->getName() == name) ++count;
    }
  }
  return count;
}

// The first macro definition in 'sourceFile' named 'name', or nullptr.
const hldb::PreprocMacroDefinition *findDefinition(const hldb::SourceFile *sourceFile, std::string_view name) {
  if (const hldb::PreprocMacroDefinitionCollection *const definitions = sourceFile->getPreprocMacroDefinitions()) {
    for (const hldb::PreprocMacroDefinition *definition : *definitions) {
      if (definition->getName() == name) return definition;
    }
  }
  return nullptr;
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

class DefineTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.5.1--define.hlc"}); }
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

// The file is two well-formed text_macro_definitions (Syntax 22-2) and
// comments; nothing in it is fatal, a syntax error or an error.
// Elaboration-category diagnostics are left out: the file has no module, and
// a design shall contain at least one top-level module (23.3.1), so an
// elaborating run may legitimately report that.
TEST_F(DefineTest, CompilesWithoutErrors) {
  ASSERT_NE(m_session, nullptr);
  const ErrorContainer *const errorContainer = m_session->getErrorContainer();
  ASSERT_NE(errorContainer, nullptr);
  EXPECT_EQ(describeSourceErrors(errorContainer), "");
}

// Each of these diagnostics would mean HLC misread one of the two lines.
TEST_F(DefineTest, ReportsNoMacroDiagnostics) {
  // Both lines match Syntax 22-2.
  EXPECT_EQ(findError(ErrorDefinition::PP_SYNTAX_ERROR), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_SYNTAX_ERROR), nullptr);

  // No "(" follows FOUR or SOMESTRING, so the text after the space is macro
  // text, not a formal argument list (22.5.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_HAS_SPACE_BEFORE_ARGS), nullptr);

  // Neither macro has formal arguments, so no argument can be unused or
  // undefined.
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_UNUSED_ARGUMENT), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_UNDEFINED_ARGUMENT), nullptr);

  // FOUR and SOMESTRING are each defined once.
  EXPECT_EQ(findError(ErrorDefinition::PP_MULTIPLY_DEFINED_MACRO), nullptr);

  // Neither name is a compiler directive name (22.1), so neither redefines one (22.5.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_NAME_RESERVED), nullptr);

  // "somestring" is closed by its second double quote on line 17 (5.9).
  EXPECT_EQ(findError(ErrorDefinition::PP_UNTERMINATED_STRING), nullptr);

  // Neither macro text refers to a macro, and no macro is used anywhere.
  EXPECT_EQ(findError(ErrorDefinition::PP_RECURSIVE_MACRO_DEFINITION), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_UNKOWN_MACRO), nullptr);

  // `define is legal both inside and outside design elements (22.5.1); here it is outside.
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_IN_DESIGN_ELEMENT), nullptr);

  // The file is plain ASCII.
  EXPECT_EQ(findError(ErrorDefinition::PP_NON_ASCII_CONTENT), nullptr);
}

TEST_F(DefineTest, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

TEST_F(DefineTest, SourceFileHasNoIncludesDirectivesOrMacroUses) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  // There is no `include in the file (22.4).
  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);

  // `define is the only directive in the file, and HLDB models it as a
  // PreprocMacroDefinition (there is no vpiDirectiveType code for it), so no
  // generic directive is recorded.
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);
}

// A macro is substituted only where `text_macro_identifier appears (22.5.1).
// Neither `FOUR nor `SOMESTRING appears in the file.
TEST_F(DefineTest, MacroDefinitionsAreNeverUsed) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// Two `define lines, two distinct names, no `undef or `undefineall.
TEST_F(DefineTest, ExactlyTwoDefineDefinitions) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  const hldb::PreprocMacroDefinitionCollection *const definitions = sourceFile->getPreprocMacroDefinitions();
  ASSERT_EQ(sizeOf(definitions), 2u);
  // Both lines are `define (22.5.1), not `undef (22.5.2) or `undefineall
  // (22.5.3).
  for (const hldb::PreprocMacroDefinition *definition : *definitions) {
    EXPECT_EQ(definition->getType(), vpiPMDDefine) << definition->getName();
  }

  EXPECT_EQ(countDefinitions(sourceFile, "FOUR"), 1u);
  EXPECT_EQ(countDefinitions(sourceFile, "SOMESTRING"), 1u);
}

// Line 16: `define FOUR 5
TEST_F(DefineTest, FourDefinition) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);
  const hldb::PreprocMacroDefinition *const four = findDefinition(sourceFile, "FOUR");
  ASSERT_NE(four, nullptr);

  EXPECT_EQ(four->getType(), vpiPMDDefine);

  // text_macro_name is the simple identifier FOUR.
  EXPECT_EQ(four->getName(), "FOUR");
  const hldb::Identifier *const nameObj = four->getNameObj();
  ASSERT_NE(nameObj, nullptr);
  EXPECT_EQ(nameObj->getName(), "FOUR");
  EXPECT_EQ(nameObj->getStartLine(), 16u);

  // No "(" follows the name, so there is no list_of_formal_arguments.
  EXPECT_EQ(sizeOf(four->getArguments()), 0u);

  // The macro text is 5, stored verbatim; the name FOUR does not influence
  // it (22.5.1).
  EXPECT_EQ(strippedMacroText(four), "5");

  // The whole directive, terminated by the newline, sits on line 16.
  EXPECT_EQ(four->getStartLine(), 16u);
  EXPECT_EQ(four->getEndLine(), 16u);
  EXPECT_TRUE(endsWith(four->getFile(), kSourceFileName)) << four->getFile();
}

// Line 17: `define SOMESTRING "somestring"
TEST_F(DefineTest, SomestringDefinition) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);
  const hldb::PreprocMacroDefinition *const somestring = findDefinition(sourceFile, "SOMESTRING");
  ASSERT_NE(somestring, nullptr);

  EXPECT_EQ(somestring->getType(), vpiPMDDefine);

  // text_macro_name is the simple identifier SOMESTRING.
  EXPECT_EQ(somestring->getName(), "SOMESTRING");
  const hldb::Identifier *const nameObj = somestring->getNameObj();
  ASSERT_NE(nameObj, nullptr);
  EXPECT_EQ(nameObj->getName(), "SOMESTRING");
  EXPECT_EQ(nameObj->getStartLine(), 17u);

  // No "(" follows the name, so there is no list_of_formal_arguments.
  EXPECT_EQ(sizeOf(somestring->getArguments()), 0u);

  // The macro text is the quoted_string "somestring" (5.9); its double quotes
  // belong to it.
  EXPECT_EQ(strippedMacroText(somestring), "\"somestring\"");

  // The whole directive, terminated by the newline, sits on line 17.
  EXPECT_EQ(somestring->getStartLine(), 17u);
  EXPECT_EQ(somestring->getEndLine(), 17u);
  EXPECT_TRUE(endsWith(somestring->getFile(), kSourceFileName)) << somestring->getFile();
}

// A text macro is preprocessor text only (22.5.1): FOUR is not a parameter
// and SOMESTRING is not a string variable. The file also has no design
// element, so nothing in the design comes from it.
TEST_F(DefineTest, NoDesignObjectsDeclared) {
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
