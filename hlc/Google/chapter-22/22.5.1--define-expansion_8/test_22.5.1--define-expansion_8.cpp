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

// Test for tests/Google/chapter-22/22.5.1--define-expansion_8.sv
//
// The source file holds no design element at all. Lines 1-7 are one-line
// "//" license comments, lines 8-9 are blank, lines 10-16 are one block
// comment carrying the test metadata (":should_fail_because: It shall be an
// error to specify more actual arguments than the number of formal
// arguments."), and the only SystemVerilog text is:
//
//   17: `define D(x,y) initial $display("start", x , y, "end");
//   18: `D(,,)
//
// Line 17 is a text_macro_definition (IEEE 1800-2023 Syntax 22-2):
//   - text_macro_name is the simple identifier D, immediately followed by
//     "(" (no white space), so (x,y) is its list_of_formal_arguments: two
//     formal arguments x and y, in that order, neither with a default.
//   - The macro text is everything after the formal argument list up to the
//     newline (22.5.1): initial $display("start", x , y, "end"); in which both
//     formal arguments are used "in the same manner as an identifier".
//
// Line 18 is a text_macro_usage (Syntax 22-3) of D, defined on the line
// before it. Its list_of_actual_arguments is split by two commas into three
// actual arguments, each empty, which 22.5.1 permits for an individual
// argument ("An actual argument may be empty or white space only"). But D has
// only two formal arguments, and 22.5.1 states: "It shall be an error to
// specify more actual arguments than the number of formal arguments." The
// LRM gives this exact usage as an example: "`D(,,) // illegal, more actual
// than formal arguments". So the file has exactly one LRM error, on line 18.
//
// Every assertion below follows from the LRM, except the one part of
// ErrorsOnlyOnTheIllegalLine labelled as HLC error recovery; a red run means
// HLC deviates from the LRM. Two checks read an LRM fact through an HLDB
// code: vpiPMDDefine says line 17 is a `define (not an `undef, 22.5.2, or an
// `undefineall, 22.5.3), and an empty generic directive list says the file
// holds none of the directives HLDB records that way (`timescale,
// `default_nettype, `celldefine and the others with a vpiDirectiveType code).
//
// What is checked
//   - PP_TOO_MANY_ARGS_MACRO is reported on line 18 with an error-class
//     severity (fatal, syntax or error), not as a warning.
//   - Every fatal, syntax or error diagnostic outside the elaboration
//     category points at line 18. For lines 1-17 (comments, blank lines and
//     a well-formed `define) this is an LRM fact: legal text draws no error.
//     For a diagnostic past line 18 or without a location it is an HLC
//     error-recovery requirement, not an LRM rule: such a diagnostic would be
//     a cascade from the illegal line.
//   - None of the preprocessor diagnostics that a wrong reading of either
//     line would produce is reported.
//   - Exactly one SourceFile for the .sv, with no includes and no generic
//     directives.
//   - Exactly one PreprocMacroDefinition, D, of type vpiPMDDefine, with
//     formal arguments x and y and the macro text above, on line 17. The
//     keyword initial stays separated from $display.
//   - No design object of any kind is created from this file.
//
// What is NOT checked, and why
//   - How HLDB splits the macro text into tokens, and whether it keeps white
//     space as tokens. The LRM defines the macro text, not its storage, so
//     the text is compared with white space removed, plus a separate check
//     that initial is not joined to $display. Nearest assertions: the macro
//     text checks in DDefinition.
//   - Which error-class severity PP_TOO_MANY_ARGS_MACRO has. The LRM only
//     says "error". Nearest assertion: the error-class check in
//     ReportsTooManyActualArguments.
//   - Whether HLDB records a PreprocMacroInstance for `D(,,), and with which
//     arguments, body, items and objects. A PreprocMacroInstance models a
//     substitution, and the LRM defines no substitution for an illegal
//     usage, so recording one (with three empty actual arguments) and
//     recording none are both consistent with the LRM; which one HLDB does is
//     a tool convention. Nearest assertions: ReportsTooManyActualArguments
//     (the usage on line 18 is diagnosed) and DDefinition (D has two formal
//     arguments).
//   - What the macro text would print. $display output only exists while
//     simulation time advances, so it is permanently out of scope. Nearest
//     assertion: DDefinition (the stored macro text).
//   - The symbol text and column carried by the PP_TOO_MANY_ARGS_MACRO
//     diagnostic. The LRM requires the error but not its message format.
//     Nearest assertion: the line 18 check in ReportsTooManyActualArguments.
//   - The seven one-line comments and the block comment (5.4). The LRM gives
//     comments no meaning; how HLDB groups, classifies (vpiComment and
//     vpiDocumentComment are marked non-standard) and attaches them is a tool
//     convention. Nearest assertions: the line 17 checks in DDefinition and
//     the line 18 check in ReportsTooManyActualArguments, which only hold if
//     the 16 comment and blank lines before them were consumed as whole
//     lines.
//   - Columns. The LRM numbers lines (22.13) but not columns, and the column
//     base and node anchor are tool conventions. Nearest assertions: the line
//     checks in DDefinition and ReportsTooManyActualArguments.
//   - The SourceFile time unit and precision. There is no `timescale or
//     timeunit, so the default applies, and the default is
//     implementation-specific (3.14.2.3). Nearest assertion:
//     SourceFileHasNoIncludesOrDirectives (zero directives).
//   - How many diagnostics line 18 draws, and of which severity, including
//     how often PP_TOO_MANY_ARGS_MACRO itself is reported. The LRM requires
//     an error for the illegal usage but does not limit how many diagnostics
//     it draws or of which kind, so for example an extra syntax diagnostic on
//     line 18 is not checked either way. Nearest assertions:
//     ReportsTooManyActualArguments and ErrorsOnlyOnTheIllegalLine.
//   - Elaboration-category diagnostics. The file has no module, and a design
//     shall contain at least one top-level module (23.3.1), so an elaborating
//     run may legitimately report that. Nearest assertion:
//     ErrorsOnlyOnTheIllegalLine (all other error-class diagnostics).
//   - The warning count. The LRM neither requires nor forbids warnings here.
//     Nearest assertion: ErrorsOnlyOnTheIllegalLine.
//   - Whether a CR from a CR LF line ending stays glued to the last macro
//     text token. 22.5.1 ends the macro text at the newline, but the LRM
//     never defines CR LF as one newline (5.3 only says newlines are white
//     space), so a stored trailing CR is not clearly an LRM deviation.
//     Nearest assertion: the macro text check in DDefinition, which ignores
//     white space.
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
constexpr std::string_view kSourceFileName = "22.5.1--define-expansion_8.sv";

// The line of the illegal usage `D(,,).
constexpr uint32_t kUsageLine = 18;

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

// The first macro text token of 'definition' that carries text other than
// white space, without its leading white space; empty when there is none.
std::string_view firstToken(const hldb::PreprocMacroDefinition *definition) {
  if (const hldb::IdentifierCollection *const tokens = definition->getTokens()) {
    for (const hldb::Identifier *token : *tokens) {
      std::string_view text = token->getName();
      const size_t start = text.find_first_not_of(kWhiteSpace);
      if (start == std::string_view::npos) continue;
      text.remove_prefix(start);
      return text;
    }
  }
  return {};
}

// True when 'text' starts with 'word' and 'word' is kept apart from what
// follows: 'text' is 'word' itself (a token of its own), or white space follows
// 'word'. Joined to following characters, 'word' would become part of a
// different identifier, since $ is an identifier character (5.6).
bool startsWithSeparatedWord(std::string_view text, std::string_view word) {
  if (text == word) return true;
  return (text.size() > word.size()) && (text.substr(0, word.size()) == word) &&
         (kWhiteSpace.find(text[word.size()]) != std::string_view::npos);
}

// True for the severities the LRM's "error" can map to.
bool isErrorClass(ErrorDefinition::ErrorSeverity severity) {
  return (severity == ErrorDefinition::FATAL) || (severity == ErrorDefinition::SYNTAX) ||
         (severity == ErrorDefinition::ERROR);
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
}  // namespace

class DefineExpansion8Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.5.1--define-expansion_8.hlc"}); }
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

// 22.5.1: "It shall be an error to specify more actual arguments than the
// number of formal arguments." `D(,,) on line 18 passes three actual
// arguments to D, which has two formal arguments.
TEST_F(DefineExpansion8Test, ReportsTooManyActualArguments) {
  const Error *const error = findError(ErrorDefinition::PP_TOO_MANY_ARGS_MACRO);
  ASSERT_NE(error, nullptr) << "`D(,,) passes 3 actual arguments to a macro with 2 formal arguments";
  ASSERT_FALSE(error->getLocations().empty());
  EXPECT_EQ(error->getLocations().front().m_line, kUsageLine);

  // The LRM calls this an error, not a warning; it does not say which kind of
  // error, so any error-class severity is accepted.
  const ErrorDefinition::ErrorMap &errorInfoMap = ErrorDefinition::getErrorInfoMap();
  const ErrorDefinition::ErrorMap::const_iterator errorInfo =
      errorInfoMap.find(ErrorDefinition::PP_TOO_MANY_ARGS_MACRO);
  ASSERT_NE(errorInfo, errorInfoMap.end());
  EXPECT_TRUE(isErrorClass(errorInfo->second.m_severity)) << "severity " << errorInfo->second.m_severity;
}

// Line 18 is the one illegal line (22.5.1); the LRM requires an error there
// but does not limit how many diagnostics, or of which severity, it draws, so
// any number of them on line 18 is accepted. Off line 18 this check mixes two
// kinds of fact:
//   - LRM: lines 1-16 are comments and blank lines and line 17 is a
//     well-formed `define (Syntax 22-2), so an error pointing at them would
//     mean HLC rejected legal text.
//   - HLC error recovery, not an LRM rule: the LRM gives an illegal usage no
//     meaning and does not define what a tool does after reporting it, so an
//     error past line 18 or without a location is a cascade, not a
//     conformance failure.
// Elaboration-category diagnostics are left out: the file has no module, and
// a design shall contain at least one top-level module (23.3.1), so an
// elaborating run may legitimately report that.
TEST_F(DefineExpansion8Test, ErrorsOnlyOnTheIllegalLine) {
  ASSERT_NE(m_session, nullptr);
  const ErrorContainer *const errorContainer = m_session->getErrorContainer();
  ASSERT_NE(errorContainer, nullptr);

  const ErrorDefinition::ErrorMap &errorInfoMap = ErrorDefinition::getErrorInfoMap();
  std::string offLine;
  for (const Error &error : errorContainer->getErrors()) {
    const ErrorDefinition::ErrorMap::const_iterator errorInfo = errorInfoMap.find(error.getType());
    if (errorInfo == errorInfoMap.end()) continue;
    if (!isErrorClass(errorInfo->second.m_severity) || (errorInfo->second.m_category == ErrorDefinition::ELAB)) {
      continue;
    }
    if (lineOf(error) != kUsageLine) offLine += describeError(error);
  }
  EXPECT_EQ(offLine, "") << "error-class diagnostics off line " << kUsageLine;
}

// Each of these diagnostics would mean HLC misread one of the two lines.
TEST_F(DefineExpansion8Test, ReportsNoOtherMacroDiagnostics) {
  // Both lines match Syntax 22-2 and Syntax 22-3.
  EXPECT_EQ(findError(ErrorDefinition::PP_SYNTAX_ERROR), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_SYNTAX_ERROR), nullptr);

  // "(" follows D with no white space on line 17, so (x,y) is the formal
  // argument list (22.5.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_HAS_SPACE_BEFORE_ARGS), nullptr);

  // Both formal arguments x and y appear in the macro text.
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_UNUSED_ARGUMENT), nullptr);

  // D is defined on line 17, before its use on line 18, and only once.
  EXPECT_EQ(findError(ErrorDefinition::PP_UNKOWN_MACRO), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MULTIPLY_DEFINED_MACRO), nullptr);

  // The usage has its parentheses (22.5.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_PARENTHESIS_NEEDED), nullptr);

  // Too many arguments is not too few: the empty actual arguments that map to
  // x and y are legal and are substituted by nothing, as neither has a
  // default (22.5.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_NO_DEFAULT_VALUE), nullptr);

  // D is not a compiler directive name (22.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_NAME_RESERVED), nullptr);

  // "start" and "end" are each closed on line 17 (5.9).
  EXPECT_EQ(findError(ErrorDefinition::PP_UNTERMINATED_STRING), nullptr);

  // The macro text of D does not refer to D or to any other macro.
  EXPECT_EQ(findError(ErrorDefinition::PP_RECURSIVE_MACRO_DEFINITION), nullptr);

  // Both directives are outside any design element.
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_IN_DESIGN_ELEMENT), nullptr);

  // The file is plain ASCII.
  EXPECT_EQ(findError(ErrorDefinition::PP_NON_ASCII_CONTENT), nullptr);
}

TEST_F(DefineExpansion8Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

TEST_F(DefineExpansion8Test, SourceFileHasNoIncludesOrDirectives) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  // There is no `include in the file (22.4).
  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);

  // `define is the only directive in the file (`D is a macro usage, not a
  // directive). None of the directives HLDB records as generic directives
  // (those with a vpiDirectiveType code: `timescale, `default_nettype,
  // `celldefine, ...) occurs, and `define has no vpiDirectiveType code, so a
  // generic directive here would be one the source does not contain.
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);
}

// Line 17: `define D(x,y) initial $display("start", x , y, "end");
TEST_F(DefineExpansion8Test, DDefinition) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);
  const hldb::PreprocMacroDefinitionCollection *const definitions = sourceFile->getPreprocMacroDefinitions();
  ASSERT_EQ(sizeOf(definitions), 1u);
  const hldb::PreprocMacroDefinition *const d = definitions->front();

  // Line 17 is a `define (22.5.1), not an `undef (22.5.2) or an `undefineall
  // (22.5.3).
  EXPECT_EQ(d->getType(), vpiPMDDefine);

  // text_macro_name is the simple identifier D.
  EXPECT_EQ(d->getName(), "D");
  const hldb::Identifier *const nameObj = d->getNameObj();
  ASSERT_NE(nameObj, nullptr);
  EXPECT_EQ(nameObj->getName(), "D");
  EXPECT_EQ(nameObj->getStartLine(), 17u);

  // list_of_formal_arguments is (x,y): two simple identifiers, in this order.
  const hldb::IdentifierCollection *const arguments = d->getArguments();
  ASSERT_EQ(sizeOf(arguments), 2u);
  EXPECT_EQ(arguments->at(0)->getName(), "x");
  EXPECT_EQ(arguments->at(1)->getName(), "y");

  // The macro text follows the formal argument list and ends at the newline;
  // the formal argument list is not part of it.
  EXPECT_EQ(strippedMacroText(d), "initial$display(\"start\",x,y,\"end\");");

  // The white space between initial and $display must survive: joined, they
  // would read as the single identifier initial$display (5.6).
  const std::string_view first = firstToken(d);
  EXPECT_TRUE(startsWithSeparatedWord(first, "initial")) << "first macro text token: " << first;

  // The whole directive, terminated by the newline, sits on line 17.
  EXPECT_EQ(d->getStartLine(), 17u);
  EXPECT_EQ(d->getEndLine(), 17u);
  EXPECT_TRUE(endsWith(d->getFile(), kSourceFileName)) << d->getFile();
}

// The file has no design element, and the illegal usage has no expansion, so
// nothing in the design comes from it.
TEST_F(DefineExpansion8Test, NoDesignObjectsDeclared) {
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
