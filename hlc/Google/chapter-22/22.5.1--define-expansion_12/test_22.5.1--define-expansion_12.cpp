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

// Test for tests/Google/chapter-22/22.5.1--define-expansion_12.sv
//
// Lines 1-7 are one-line "//" license comments, lines 8-9 are blank, lines
// 10-16 are one block comment carrying the test metadata (its
// ":should_fail_because:" quotes 22.5.1 on omitted arguments without a
// default), and the SystemVerilog text is:
//
//   17: `define MACRO1(a=5,b="B",c) initial $display(a,,b,,c);
//   18: module top ();
//   19: `MACRO1 ( 1 ) // ILLEGAL: b and c omitted, no default for c
//   20: endmodule
//
// Line 17 is a text_macro_definition (IEEE 1800-2023 Syntax 22-2):
//   - text_macro_name is the simple identifier MACRO1, immediately followed
//     by "(", so (a=5,b="B",c) is its list_of_formal_arguments: formal
//     arguments a, b and c, in that order. a has the default text 5, b the
//     default text "B", and c has no default.
//   - The macro text is everything after the formal argument list up to the
//     newline (22.5.1): initial $display(a,,b,,c);
//
// Line 19 is a text_macro_usage (Syntax 22-3) of MACRO1 inside module top,
// followed by a one-line comment that is not part of it (5.4). White space
// between the macro name and "(" is allowed in a usage (22.5.1). It gives one
// actual argument, 1, for three formal arguments, so b and c are omitted.
// 22.5.1: "If fewer actual arguments are specified than the number of formal
// arguments and all the remaining formal arguments have defaults, then the
// defaults are substituted for the additional formal arguments. It shall be
// an error if any of the remaining formal arguments does not have a default
// specified." b has a default but c does not, so the usage is an error. The
// LRM gives this exact usage as an example: "`MACRO1 ( 1 ) // ILLEGAL: b and
// c omitted, no default for c". So the file has exactly one LRM error, on
// line 19; everything else is legal.
//
// "module top ();" has a list_of_ports holding one null port: 37.14 detail 10
// names "module M();" as the null port case, with vpiLowConn NULL; detail 11
// gives a null port vpiSize 0, and detail 9 gives the first port vpiPortIndex 0.
//
// top is never instantiated, so it is a top-level module, implicitly
// instantiated once under its own name (23.3.1).
//
// Every assertion below follows from the LRM, except the parts labelled as
// HLC error recovery (in ErrorsOnlyOnTheIllegalLine and TopModule); a red
// run of any other check means HLC deviates from the LRM. Two checks read an LRM fact through an HLDB
// code: vpiPMDDefine says line 17 is a `define (not an `undef, 22.5.2, or an
// `undefineall, 22.5.3), and an empty generic directive list says the file
// holds none of the directives HLDB records that way (`timescale,
// `default_nettype, `celldefine and the others with a vpiDirectiveType code).
//
// What is checked
//   - At least one error-class diagnostic (fatal, syntax or error) points at
//     line 19.
//   - HLC diagnoses it as the missing default: PP_MACRO_NO_DEFAULT_VALUE, the
//     HLC code for exactly this 22.5.1 rule, is reported on line 19 with an
//     error-class severity.
//   - Every error-class diagnostic points at line 19. For lines 1-18 this is
//     an LRM fact: legal text draws no error. For a diagnostic past line 19
//     or without a location it is an HLC error-recovery requirement, not an
//     LRM rule: such a diagnostic would be a cascade from the illegal line.
//   - None of the preprocessor diagnostics that a misreading of the
//     definition or of the usage would produce.
//   - Exactly one SourceFile for the .sv, with no includes and no generic
//     directives.
//   - Exactly one PreprocMacroDefinition, MACRO1, of type vpiPMDDefine, with
//     formal arguments a, b and c and the macro text above, on line 17. The
//     keyword initial stays separated from $display.
//   - Exactly one module, top, on lines 18-20, with one null port. The null
//     port is an LRM fact (37.14). That the module is still recorded whole,
//     with its lines and end label, around the illegal line 19 is HLC
//     error recovery, not an LRM rule: the LRM does not define what a tool
//     does after reporting the illegal usage.
//   - No other design object is created from this file.
//   - After elaboration only: one top-level instance named top.
//
// What is NOT checked, and why
//   - Whether HLDB records a PreprocMacroInstance for `MACRO1 ( 1 ), and with
//     which arguments, body, items and objects. A PreprocMacroInstance models
//     a substitution, and the LRM defines no substitution for an illegal
//     usage, so recording one and recording none are both consistent with
//     the LRM. Nearest assertions: ReportsAnErrorOnTheIllegalLine and
//     MacroDefinition (MACRO1 has three formal arguments).
//   - The contents of module top: its processes, statements and $display
//     arguments. They could only come from an expansion of the illegal usage,
//     which the LRM does not define. Nearest assertions: TopModule (the
//     module itself, its lines and end label) and TopHasOneNullPort.
//   - How many diagnostics line 19 draws, and of which severity, beyond the
//     required error. The LRM requires an error for the illegal usage but
//     does not limit how many diagnostics it draws or of which kind. Nearest
//     assertions: ReportsAnErrorOnTheIllegalLine and
//     ErrorsOnlyOnTheIllegalLine.
//   - Which omitted argument the diagnostic names (it should be c, which has
//     no default, not b, which has one) and the diagnostic's column. The LRM
//     requires the error but not its message format. Nearest assertion:
//     DiagnosesTheMissingDefault.
//   - Where HLDB stores the default texts 5 and "B". The headers and library
//     expose only an undocumented "buddy" field next to a formal argument,
//     and the illegal usage defines no expansion through which a default
//     could be observed. Nearest assertion: MacroDefinition (the formal
//     argument names).
//   - How HLDB splits the macro text into tokens, whether it keeps white
//     space as tokens, and whether a CR from a CR LF line ending stays glued
//     to the last token. The LRM defines the macro text, not its storage, and
//     never defines CR LF as one newline (5.3), so the text is compared with
//     white space removed, plus a separate check that initial is not joined
//     to $display. Nearest assertions: the macro text checks in
//     MacroDefinition.
//   - The seven one-line comments, the block comment and the trailing
//     one-line comment on line 19 (5.4). The LRM gives comments no meaning;
//     how HLDB groups, classifies (vpiComment and vpiDocumentComment are
//     marked non-standard) and attaches them is a tool convention. Nearest
//     assertions: the line checks in MacroDefinition and TopModule.
//   - Columns. The LRM numbers lines (22.13) but not columns. Nearest
//     assertions: the line checks in MacroDefinition and TopModule.
//   - The SourceFile time unit and precision. There is no `timescale or
//     timeunit, so the default applies, and the default is
//     implementation-specific (3.14.2.3). Nearest assertion:
//     SourceFileHasNoIncludesOrDirectives (zero directives).
//   - The warning count. The LRM neither requires nor forbids warnings here.
//     Nearest assertion: ErrorsOnlyOnTheIllegalLine.
//   - Only the top-level instance check is gated on Design::getElaborated():
//     the instance tree only exists after elaboration. Elaboration-category
//     diagnostics are not excluded from ErrorsOnlyOnTheIllegalLine, because
//     top exists and so a design with a top-level module (23.3.1) gives an
//     elaborating run nothing legitimate to report off line 19.

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
#include <hldb/port.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/program.h>
#include <hldb/source_file.h>
#include <hldb/typedef.h>
#include <hldb/udp_defn.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "22.5.1--define-expansion_12.sv";

// The line of the illegal usage `MACRO1 ( 1 ).
constexpr uint32_t kUsageLine = 19;

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

// The first object in 'collection' whose source location is the file under test, or nullptr.
template <typename T>
const T *firstFromSourceFile(const std::vector<T *> *collection) {
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (endsWith(object->getFile(), kSourceFileName)) return object;
    }
  }
  return nullptr;
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

// True when 'error' has an error-class severity in HLC's error table.
bool isErrorClass(const Error &error) {
  const ErrorDefinition::ErrorMap &errorInfoMap = ErrorDefinition::getErrorInfoMap();
  const ErrorDefinition::ErrorMap::const_iterator errorInfo = errorInfoMap.find(error.getType());
  return (errorInfo != errorInfoMap.end()) && isErrorClass(errorInfo->second.m_severity);
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

// Every diagnostic, described as by describeError(); for failure messages.
std::string describeAllErrors(const ErrorContainer *errorContainer) {
  std::string description;
  for (const Error &error : errorContainer->getErrors()) description += describeError(error);
  return description;
}
}  // namespace

class DefineExpansion12Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.5.1--define-expansion_12.hlc"}); }
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

  // The module definition of top from the file under test, or nullptr unless there is exactly one.
  static const hldb::Module *getTopDefinition() {
    if ((m_design == nullptr) || (countFromSourceFile(m_design->getAllModules()) != 1)) return nullptr;
    return firstFromSourceFile(m_design->getAllModules());
  }
};

// 22.5.1: "It shall be an error if any of the remaining formal arguments does
// not have a default specified." `MACRO1 ( 1 ) on line 19 omits b and c, and c
// has no default. The LRM requires an error there; it does not say which
// kind, so any error-class diagnostic on line 19 satisfies it.
TEST_F(DefineExpansion12Test, ReportsAnErrorOnTheIllegalLine) {
  ASSERT_NE(m_session, nullptr);
  const ErrorContainer *const errorContainer = m_session->getErrorContainer();
  ASSERT_NE(errorContainer, nullptr);

  size_t errorsOnUsageLine = 0;
  for (const Error &error : errorContainer->getErrors()) {
    if (isErrorClass(error) && (lineOf(error) == kUsageLine)) ++errorsOnUsageLine;
  }
  EXPECT_GE(errorsOnUsageLine, 1u) << "no error on line " << kUsageLine << ";" << describeAllErrors(errorContainer);
}

// HLC has a dedicated code for this 22.5.1 rule, PP_MACRO_NO_DEFAULT_VALUE: an
// omitted formal argument without a default. Reporting the usage under it
// identifies the LRM condition that makes line 19 illegal.
TEST_F(DefineExpansion12Test, DiagnosesTheMissingDefault) {
  const Error *const error = findError(ErrorDefinition::PP_MACRO_NO_DEFAULT_VALUE);
  ASSERT_NE(error, nullptr) << "`MACRO1 ( 1 ) omits c, which has no default (22.5.1)";
  EXPECT_EQ(lineOf(*error), kUsageLine);

  // The LRM calls this an error, not a warning; it does not say which kind of
  // error, so any error-class severity is accepted.
  const ErrorDefinition::ErrorMap &errorInfoMap = ErrorDefinition::getErrorInfoMap();
  const ErrorDefinition::ErrorMap::const_iterator errorInfo =
      errorInfoMap.find(ErrorDefinition::PP_MACRO_NO_DEFAULT_VALUE);
  ASSERT_NE(errorInfo, errorInfoMap.end());
  EXPECT_TRUE(isErrorClass(errorInfo->second.m_severity)) << "severity " << errorInfo->second.m_severity;
}

// Line 19 is the one illegal line (22.5.1); the LRM requires an error there
// but does not limit how many diagnostics, or of which severity, it draws, so
// any number of them on line 19 is accepted. Off line 19 this check mixes two
// kinds of fact:
//   - LRM: lines 1-16 are comments and blank lines, line 17 is a well-formed
//     `define (Syntax 22-2) and line 18 a well-formed module header, so an
//     error pointing at them would mean HLC rejected legal text.
//   - HLC error recovery, not an LRM rule: the LRM gives an illegal usage no
//     meaning and does not define what a tool does after reporting it, so an
//     error past line 19 or without a location is a cascade, not a
//     conformance failure.
TEST_F(DefineExpansion12Test, ErrorsOnlyOnTheIllegalLine) {
  ASSERT_NE(m_session, nullptr);
  const ErrorContainer *const errorContainer = m_session->getErrorContainer();
  ASSERT_NE(errorContainer, nullptr);

  std::string offLine;
  for (const Error &error : errorContainer->getErrors()) {
    if (isErrorClass(error) && (lineOf(error) != kUsageLine)) offLine += describeError(error);
  }
  EXPECT_EQ(offLine, "") << "error-class diagnostics off line " << kUsageLine;
}

// Each of these diagnostics would mean HLC misread the definition or the usage.
TEST_F(DefineExpansion12Test, ReportsNoMisreadingDiagnostics) {
  // One actual argument for three formal arguments is too few, not too many.
  EXPECT_EQ(findError(ErrorDefinition::PP_TOO_MANY_ARGS_MACRO), nullptr);

  // In the definition "(" follows MACRO1 with no white space; in the usage
  // white space before "(" is allowed (22.5.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_HAS_SPACE_BEFORE_ARGS), nullptr);

  // The usage has its parentheses (22.5.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_PARENTHESIS_NEEDED), nullptr);

  // a, b and c all appear in the macro text.
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_UNUSED_ARGUMENT), nullptr);

  // MACRO1 is defined once, on line 17, before its use on line 19.
  EXPECT_EQ(findError(ErrorDefinition::PP_UNKOWN_MACRO), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MULTIPLY_DEFINED_MACRO), nullptr);

  // MACRO1 is not a compiler directive name (22.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_NAME_RESERVED), nullptr);

  // The default "B" is closed on line 17 (5.9).
  EXPECT_EQ(findError(ErrorDefinition::PP_UNTERMINATED_STRING), nullptr);

  // The macro text of MACRO1 does not refer to any macro.
  EXPECT_EQ(findError(ErrorDefinition::PP_RECURSIVE_MACRO_DEFINITION), nullptr);

  // `define is outside module top, and a macro usage may appear anywhere in
  // the compilation unit after its definition (22.5.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_IN_DESIGN_ELEMENT), nullptr);

  // The file is plain ASCII.
  EXPECT_EQ(findError(ErrorDefinition::PP_NON_ASCII_CONTENT), nullptr);
}

TEST_F(DefineExpansion12Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

TEST_F(DefineExpansion12Test, SourceFileHasNoIncludesOrDirectives) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  // There is no `include in the file (22.4).
  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);

  // `define is the only directive in the file (`MACRO1 is a macro usage, not
  // a directive). None of the directives HLDB records as generic directives
  // (those with a vpiDirectiveType code: `timescale, `default_nettype,
  // `celldefine, ...) occurs, and `define has no vpiDirectiveType code, so a
  // generic directive here would be one the source does not contain.
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);
}

// Line 17: `define MACRO1(a=5,b="B",c) initial $display(a,,b,,c);
TEST_F(DefineExpansion12Test, MacroDefinition) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);
  const hldb::PreprocMacroDefinitionCollection *const definitions = sourceFile->getPreprocMacroDefinitions();
  ASSERT_EQ(sizeOf(definitions), 1u);
  const hldb::PreprocMacroDefinition *const macro1 = definitions->front();

  // Line 17 is a `define (22.5.1), not an `undef (22.5.2) or an `undefineall
  // (22.5.3).
  EXPECT_EQ(macro1->getType(), vpiPMDDefine);

  // text_macro_name is the simple identifier MACRO1.
  EXPECT_EQ(macro1->getName(), "MACRO1");
  const hldb::Identifier *const nameObj = macro1->getNameObj();
  ASSERT_NE(nameObj, nullptr);
  EXPECT_EQ(nameObj->getName(), "MACRO1");
  EXPECT_EQ(nameObj->getStartLine(), 17u);

  // formal_argument ::= simple_identifier [ = default_text ]: the names are
  // a, b and c, in this order; the default texts are not part of the names.
  const hldb::IdentifierCollection *const arguments = macro1->getArguments();
  ASSERT_EQ(sizeOf(arguments), 3u);
  EXPECT_EQ(arguments->at(0)->getName(), "a");
  EXPECT_EQ(arguments->at(1)->getName(), "b");
  EXPECT_EQ(arguments->at(2)->getName(), "c");

  // The macro text follows the formal argument list and ends at the newline;
  // neither the formal argument list nor its defaults are part of it.
  EXPECT_EQ(strippedMacroText(macro1), "initial$display(a,,b,,c);");

  // The white space between initial and $display must survive: joined, they
  // would read as the single identifier initial$display (5.6).
  const std::string_view first = firstToken(macro1);
  EXPECT_TRUE(startsWithSeparatedWord(first, "initial")) << "first macro text token: " << first;

  // The whole directive, terminated by the newline, sits on line 17.
  EXPECT_EQ(macro1->getStartLine(), 17u);
  EXPECT_EQ(macro1->getEndLine(), 17u);
  EXPECT_TRUE(endsWith(macro1->getFile(), kSourceFileName)) << macro1->getFile();
}

// Lines 18-20: module top (); ... endmodule. Only what does not depend on the
// illegal usage on line 19 is checked. HLC error recovery, not an LRM rule:
// lines 18 and 20 are legal, but the LRM does not define what a tool does
// after reporting line 19, so a module that is missing or cut short here is
// a recovery failure, not a conformance failure.
TEST_F(DefineExpansion12Test, TopModule) {
  ASSERT_NE(m_design, nullptr);
  ASSERT_EQ(countFromSourceFile(m_design->getAllModules()), 1u);
  const hldb::Module *const top = getTopDefinition();
  ASSERT_NE(top, nullptr);

  EXPECT_EQ(top->getDefName(), "top");
  EXPECT_EQ(top->getStartLine(), 18u);
  EXPECT_EQ(top->getEndLine(), 20u);

  // "endmodule" carries no ": top" end label.
  EXPECT_EQ(top->getEndLabel(), "");
}

// "module top ();" has one null port (37.14 details 9, 10 and 11).
TEST_F(DefineExpansion12Test, TopHasOneNullPort) {
  const hldb::Module *const top = getTopDefinition();
  ASSERT_NE(top, nullptr);

  const hldb::PortCollection *const ports = top->getPorts();
  ASSERT_EQ(sizeOf(ports), 1u) << "\"module M();\" has one null port (37.14 detail 10)";
  const hldb::Port *const port = ports->front();

  EXPECT_EQ(port->getPortIndex(), 0);
  EXPECT_EQ(port->getLowConn(), nullptr);

  const hldb::Any::vpi_property_value_t size = port->getVpiPropertyValue(vpiSize);
  ASSERT_TRUE(std::holds_alternative<int64_t>(size));
  EXPECT_EQ(std::get<int64_t>(size), 0);
}

// top is the only module and is never instantiated, so nothing else is
// declared in the file, and top is the one top-level module (23.3.1).
TEST_F(DefineExpansion12Test, NoOtherDesignObjectsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllClasses()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);

  // The formal arguments a, b and c are macro text, not declarations.
  EXPECT_EQ(countFromSourceFile(m_design->getParameters()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParamAssigns()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getVariables()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getTypedefs()), 0u);

  // A top-level module is implicitly instantiated once, under its own name
  // (23.3.1). The instance tree only exists after elaboration.
  if (m_design->getElaborated()) {
    const hldb::ModuleCollection *const topModules = m_design->getTopModules();
    ASSERT_EQ(countFromSourceFile(topModules), 1u);
    const hldb::Module *const topInstance = firstFromSourceFile(topModules);
    EXPECT_EQ(topInstance->getName(), "top");
    EXPECT_EQ(topInstance->getDefName(), "top");
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
