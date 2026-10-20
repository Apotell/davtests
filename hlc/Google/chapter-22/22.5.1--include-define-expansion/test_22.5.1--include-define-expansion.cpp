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

// Test for tests/Google/chapter-22/22.5.1--include-define-expansion.sv
//
// The source file holds no design element at all. Lines 1-7 are one-line
// "//" license comments, lines 8-9 are blank, lines 10-15 are one block
// comment carrying the test metadata, and the SystemVerilog text is:
//
//   16: `define foo(the_file) `"the_file`"
//   17: `include `foo(dummy_include.sv)
//
// Line 16 is a text_macro_definition (IEEE 1800-2023 Syntax 22-2):
// text_macro_name is the simple identifier foo, immediately followed by "(",
// so (the_file) is its list_of_formal_arguments, one formal argument without
// a default. Its macro text is `"the_file`". 22.5.1: "A `" overrides the
// usual lexical meaning of " and indicates that the expansion shall include
// the quotation mark, substitution of actual arguments, and expansions of
// embedded macros", so the_file is used inside the `" string.
//
// Line 17 is an `include whose file name is given by a macro instead of a
// string literal, which 22.5.1 shows as legal: "Here is an example of the
// `include directive being followed by a macro, instead of by a string
// literal: `define home(filename) `"/home/mydir/filename`" `include
// `home(myfile)". `foo(dummy_include.sv) expands to "dummy_include.sv", so
// the line is `include "dummy_include.sv" (Syntax 22-1), with nothing else on
// the line (22.4: "Only white space or a comment may appear on the same line
// as the `include compiler directive"). The relative path is searched in the
// compiler's current working directory (22.4); the run uses the fixture's own
// folder, tests/Google/chapter-22, where dummy_include.sv exists.
//
// dummy_include.sv holds only comments (seven one-line comments and a block
// comment). 22.4: "The result is as though the contents of the included
// source file appear in place of the `include compiler directive", so the
// inclusion adds no SystemVerilog text, and the design stays empty.
//
// Every assertion below follows from the LRM; a red run means HLC deviates
// from it. Some checks read an LRM fact through an HLDB code: vpiPMDDefine
// says line 16 is a `define (not an `undef, 22.5.2, or an `undefineall,
// 22.5.3); vpiIncludes is HLDB's record of the files an `include brought in;
// and an empty generic directive list says the file holds none of the
// directives HLDB records that way (`timescale, `default_nettype,
// `celldefine and the others with a vpiDirectiveType code).
//
// What is checked
//   - No fatal, syntax or error diagnostic outside the elaboration category,
//     and none of the preprocessor diagnostics a wrong reading of either line
//     would produce; in particular no "cannot open include file", "invalid
//     include file name" or "unterminated string" diagnostic.
//   - Exactly one SourceFile for the .sv, which records exactly one included
//     file, dummy_include.sv, and no generic directives.
//   - Exactly one PreprocMacroDefinition, foo, of type vpiPMDDefine, with the
//     formal argument the_file and the macro text `"the_file`", on line 16.
//   - Exactly one PreprocMacroInstance, foo, bound to that definition, on line
//     17, with the actual argument dummy_include.sv.
//   - No design object of any kind is created from this file or from
//     dummy_include.sv.
//
// What is NOT checked, and why
//   - How HLDB records the included file's name: as written or as a resolved
//     path. The LRM fixes which file is included, not how a tool spells its
//     path, so the name is matched by its ending. Nearest assertion:
//     IncludesDummyIncludeFile.
//   - Whether the included file also appears as a separate SourceFile of the
//     design. The LRM does not define a tool's file list. Nearest assertion:
//     IncludesDummyIncludeFile.
//   - The comments of either file (5.4). The LRM gives comments no meaning;
//     how HLDB groups, classifies (vpiComment and vpiDocumentComment are
//     marked non-standard) and attaches them is a tool convention. Nearest
//     assertions: the line checks in MacroDefinition and MacroUsage.
//   - The PreprocMacroInstance body, items and objects. The headers and
//     library do not document what these fields hold. The LRM-defined
//     expansion, the file name "dummy_include.sv", is checked where its result
//     lives instead. Nearest assertion: IncludesDummyIncludeFile.
//   - Informational diagnostics such as "processing include file". The LRM
//     does not define them. Nearest assertion: IncludesDummyIncludeFile.
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
//   - Line numbering inside the included file and after the `include (22.13).
//     The included file has no object that could carry a line. Nearest
//     assertions: the line 16 and 17 checks.
//   - Elaboration-category diagnostics. The design has no module, and a design
//     shall contain at least one top-level module (23.3.1), so an elaborating
//     run may legitimately report that. Nearest assertion:
//     CompilesWithoutErrors (all other error-class diagnostics).
//   - Columns. The LRM numbers lines (22.13) but not columns. Nearest
//     assertions: the line checks in MacroDefinition and MacroUsage.
//   - The SourceFile time unit and precision. There is no `timescale or
//     timeunit, so the default applies, and the default is
//     implementation-specific (3.14.2.3). Nearest assertion:
//     SourceFileHasNoDirectives (zero directives).
//   - The warning count. The LRM neither requires nor forbids warnings here.
//     Nearest assertion: CompilesWithoutErrors.
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
#include <hldb/include_stmt.h>
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
constexpr std::string_view kSourceFileName = "22.5.1--include-define-expansion.sv";

// The file `foo(dummy_include.sv) names, in the fixture's own folder.
constexpr std::string_view kIncludedFileName = "dummy_include.sv";

// LRM 5.3 white space: spaces, tabs, newlines (CR LF on a CRLF checkout) and formfeeds.
constexpr std::string_view kWhiteSpace = " \t\r\n\f";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

// True when 'path' names the file 'fileName': the whole path, or its last
// component after a slash or backslash.
bool namesFile(std::string_view path, std::string_view fileName) {
  if (!endsWith(path, fileName)) return false;
  if (path.size() == fileName.size()) return true;
  const char separator = path[path.size() - fileName.size() - 1];
  return (separator == '/') || (separator == '\\');
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

// Number of objects in 'collection' whose source location is the file 'fileName'.
template <typename T>
size_t countFromFile(const std::vector<T *> *collection, std::string_view fileName) {
  size_t count = 0;
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (namesFile(object->getFile(), fileName)) ++count;
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

class IncludeDefineExpansionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.5.1--include-define-expansion.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // All SourceFile nodes recorded for the file under test.
  static std::vector<const hldb::SourceFile *> findSourceFiles() {
    std::vector<const hldb::SourceFile *> sourceFiles;
    if (m_design != nullptr) {
      if (const hldb::SourceFileCollection *const collection = m_design->getSourceFiles()) {
        for (const hldb::SourceFile *sourceFile : *collection) {
          if (namesFile(sourceFile->getName(), kSourceFileName)) sourceFiles.emplace_back(sourceFile);
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

// Line 16 is a well-formed text_macro_definition (Syntax 22-2) and line 17 a
// well-formed `include once the macro is substituted (22.4, 22.5.1), naming a
// file that exists in the working directory; the included file holds only
// comments. Nothing is fatal, a syntax error or an error. Elaboration-category
// diagnostics are left out: the design has no module, and a design shall
// contain at least one top-level module (23.3.1), so an elaborating run may
// legitimately report that.
TEST_F(IncludeDefineExpansionTest, CompilesWithoutErrors) {
  ASSERT_NE(m_session, nullptr);
  const ErrorContainer *const errorContainer = m_session->getErrorContainer();
  ASSERT_NE(errorContainer, nullptr);
  EXPECT_EQ(describeSourceErrors(errorContainer), "");
}

// Each of these diagnostics would mean HLC misread one of the two lines.
TEST_F(IncludeDefineExpansionTest, ReportsNoMacroOrIncludeDiagnostics) {
  // Line 16 matches Syntax 22-2 and line 17 Syntax 22-3 and 22-1.
  EXPECT_EQ(findError(ErrorDefinition::PP_SYNTAX_ERROR), nullptr);

  EXPECT_EQ(findError(ErrorDefinition::PP_CANNOT_OPEN_INCLUDE_FILE), nullptr);

  // `" overrides the usual meaning of " (22.5.1), so the macro text holds no
  // ordinary string literal that could be left unterminated, and the text
  // between the two `" does take the actual argument, so the_file is used.
  EXPECT_EQ(findError(ErrorDefinition::PP_UNTERMINATED_STRING), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_UNUSED_ARGUMENT), nullptr);

  EXPECT_EQ(findError(ErrorDefinition::PP_TOO_MANY_ARGS_MACRO), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_NO_DEFAULT_VALUE), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_PARENTHESIS_NEEDED), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_UNDEFINED_ARGUMENT), nullptr);

  // foo is defined once, on line 16, before its use on line 17.
  EXPECT_EQ(findError(ErrorDefinition::PP_UNKOWN_MACRO), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MULTIPLY_DEFINED_MACRO), nullptr);

  // foo is not a compiler directive name (22.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_NAME_RESERVED), nullptr);

  // The macro text of foo refers to no macro.
  EXPECT_EQ(findError(ErrorDefinition::PP_RECURSIVE_MACRO_DEFINITION), nullptr);

  // The file has no design element, so neither directive is inside one.
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_IN_DESIGN_ELEMENT), nullptr);
}

TEST_F(IncludeDefineExpansionTest, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

// 22.4: `include inserts "the entire contents of a source file"; the file
// named by the expansion of `foo(dummy_include.sv) is dummy_include.sv, and it
// is the only file the source includes.
TEST_F(IncludeDefineExpansionTest, IncludesDummyIncludeFile) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);
  const hldb::SourceFileCollection *const includes = sourceFile->getIncludes();
  ASSERT_EQ(sizeOf(includes), 1u) << "line 17 includes exactly one file";
  EXPECT_TRUE(namesFile(includes->front()->getName(), kIncludedFileName)) << includes->front()->getName();

  // dummy_include.sv holds no `include of its own.
  EXPECT_EQ(sizeOf(includes->front()->getIncludes()), 0u);
}

TEST_F(IncludeDefineExpansionTest, SourceFileHasNoDirectives) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  // `define and `include are the only directives in the file (`foo is a macro
  // usage, not a directive). None of the directives HLDB records as generic
  // directives (those with a vpiDirectiveType code: `timescale,
  // `default_nettype, `celldefine, ...) occurs, and neither `define nor
  // `include has a vpiDirectiveType code, so a generic directive here would
  // be one the source does not contain.
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);
}

// Line 16: `define foo(the_file) `"the_file`"
TEST_F(IncludeDefineExpansionTest, MacroDefinition) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);
  const hldb::PreprocMacroDefinitionCollection *const definitions = sourceFile->getPreprocMacroDefinitions();
  ASSERT_EQ(sizeOf(definitions), 1u);
  const hldb::PreprocMacroDefinition *const foo = definitions->front();

  // Line 16 is a `define (22.5.1), not an `undef (22.5.2) or an `undefineall
  // (22.5.3).
  EXPECT_EQ(foo->getType(), vpiPMDDefine);

  // text_macro_name is the simple identifier foo.
  EXPECT_EQ(foo->getName(), "foo");
  const hldb::Identifier *const nameObj = foo->getNameObj();
  ASSERT_NE(nameObj, nullptr);
  EXPECT_EQ(nameObj->getName(), "foo");
  EXPECT_EQ(nameObj->getStartLine(), 16u);

  // list_of_formal_arguments is (the_file): one simple identifier.
  const hldb::IdentifierCollection *const arguments = foo->getArguments();
  ASSERT_EQ(sizeOf(arguments), 1u);
  EXPECT_EQ(arguments->front()->getName(), "the_file");

  // The macro text follows the formal argument list and ends at the newline:
  // `"the_file`", kept as written, with its two `" intact.
  EXPECT_EQ(strippedMacroText(foo), "`\"the_file`\"");

  // The `define directive starts on line 16. Its end line is not checked;
  // see the header.
  EXPECT_EQ(foo->getStartLine(), 16u);
  EXPECT_TRUE(namesFile(foo->getFile(), kSourceFileName)) << foo->getFile();
}

// Line 17: `include `foo(dummy_include.sv)
TEST_F(IncludeDefineExpansionTest, MacroUsage) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);
  const hldb::PreprocMacroInstanceCollection *const instances = sourceFile->getPreprocMacroInstances();
  ASSERT_EQ(sizeOf(instances), 1u);
  const hldb::PreprocMacroInstance *const usage = instances->front();

  // `text_macro_identifier is foo.
  EXPECT_EQ(usage->getName(), "foo");
  const hldb::Identifier *const nameObj = usage->getNameObj();
  ASSERT_NE(nameObj, nullptr);
  EXPECT_EQ(nameObj->getName(), "foo");
  EXPECT_EQ(nameObj->getStartLine(), 17u);

  // The usage binds to the only definition of foo, made on line 16.
  const hldb::PreprocMacroDefinitionCollection *const definitions = sourceFile->getPreprocMacroDefinitions();
  ASSERT_EQ(sizeOf(definitions), 1u);
  EXPECT_EQ(usage->getPreprocMacroDefinition(), definitions->front());

  // One actual argument, dummy_include.sv, for the one formal argument.
  const hldb::IdentifierCollection *const arguments = usage->getArguments();
  ASSERT_EQ(sizeOf(arguments), 1u);
  EXPECT_EQ(stripWhiteSpace(arguments->front()->getName()), "dummy_include.sv");

  EXPECT_EQ(usage->getStartLine(), 17u);
  EXPECT_TRUE(namesFile(usage->getFile(), kSourceFileName)) << usage->getFile();
}

// A text macro and an `include of a file of comments declare nothing (22.4,
// 22.5.1), and neither file has a design element, so nothing in the design
// comes from either. The source holds no library map include_statement
// (33.3.1) either.
TEST_F(IncludeDefineExpansionTest, NoDesignObjectsDeclared) {
  ASSERT_NE(m_design, nullptr);

  const std::vector<std::string_view> fileNames = {kSourceFileName, kIncludedFileName};
  for (std::string_view fileName : fileNames) {
    EXPECT_EQ(countFromFile(m_design->getAllModules(), fileName), 0u) << fileName;
    EXPECT_EQ(countFromFile(m_design->getTopModules(), fileName), 0u) << fileName;
    EXPECT_EQ(countFromFile(m_design->getAllInterfaces(), fileName), 0u) << fileName;
    EXPECT_EQ(countFromFile(m_design->getAllPrograms(), fileName), 0u) << fileName;
    EXPECT_EQ(countFromFile(m_design->getAllPackages(), fileName), 0u) << fileName;
    EXPECT_EQ(countFromFile(m_design->getAllClasses(), fileName), 0u) << fileName;
    EXPECT_EQ(countFromFile(m_design->getAllUdps(), fileName), 0u) << fileName;
    EXPECT_EQ(countFromFile(m_design->getConfigs(), fileName), 0u) << fileName;
    EXPECT_EQ(countFromFile(m_design->getCheckerDecls(), fileName), 0u) << fileName;
    EXPECT_EQ(countFromFile(m_design->getParameters(), fileName), 0u) << fileName;
    EXPECT_EQ(countFromFile(m_design->getParamAssigns(), fileName), 0u) << fileName;
    EXPECT_EQ(countFromFile(m_design->getVariables(), fileName), 0u) << fileName;
    EXPECT_EQ(countFromFile(m_design->getTypedefs(), fileName), 0u) << fileName;
    EXPECT_EQ(countFromFile(m_design->getIncludeStmts(), fileName), 0u) << fileName;
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
