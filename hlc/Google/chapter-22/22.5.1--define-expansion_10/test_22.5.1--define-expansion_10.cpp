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

// Test for tests/Google/chapter-22/22.5.1--define-expansion_10.sv
//
// Lines 1-7 are one-line "//" license comments, lines 8-9 are blank, lines
// 10-15 are one block comment carrying the test metadata, and the
// SystemVerilog text is:
//
//   16: `define MACRO1(a=5,b="B",c) initial $display(a,,b,,c);
//   17: module top ();
//   18: `MACRO1 ( 1 , , 3 )
//   19: endmodule
//
// Line 16 is a text_macro_definition (IEEE 1800-2023 Syntax 22-2):
//   - text_macro_name is the simple identifier MACRO1, immediately followed
//     by "(", so (a=5,b="B",c) is its list_of_formal_arguments: formal
//     arguments a, b and c, in that order. a has the default text 5, b the
//     default text "B", and c has no default.
//   - The macro text is everything after the formal argument list up to the
//     newline (22.5.1): initial $display(a,,b,,c);
//
// Line 18 is a text_macro_usage (Syntax 22-3) of MACRO1 inside module top.
// White space between the macro name and "(" is allowed in a usage (22.5.1).
// Its three actual arguments are: 1, white space only, and 3. Three actual
// arguments for three formal arguments is legal. An actual argument that is
// white space only is replaced by the default when one is specified (22.5.1),
// so a becomes 1, b becomes its default "B" and c becomes 3. The LRM gives
// this exact usage as an example: "`MACRO1 ( 1 , , 3 ) // argument b omitted,
// replaced by default // expands to '$display(1,,"B",,3);'". So line 18
// expands to:
//
//   initial $display(1,,"B",,3);
//
// After expansion, module top holds one initial procedure (9.2.1) whose
// statement is a call of the built-in system task $display (21.2.1), a
// vpiSysTaskCall that is not user-defined (37.42). The call has five
// arguments; the second and fourth are empty, and an empty argument is
// represented as a vpiOperation with vpiOpType vpiNullOp (37.42, detail 8).
// 1 and 3 are simple decimal numbers, which are signed integers of at least
// 32 bits (5.7.1). "B" is a string literal (5.9), a vpiConstant whose
// vpiConstType is vpiStringConst (37.42, detail 8 example).
//
// "module top ();" has a list_of_ports holding one null port: 37.14 detail 10
// names "module M();" as the null port case, with vpiLowConn NULL; detail 11
// gives a null port vpiSize 0, and detail 9 gives the first port vpiPortIndex 0.
//
// top is never instantiated, so it is a top-level module, implicitly
// instantiated once under its own name (23.3.1).
//
// Every assertion below follows from the LRM (ExpandedCodeCarriesTheUsageLine
// by an interpretation, stated with it); a red run means HLC deviates from
// it. Some checks read an LRM fact through a code: vpiPMDDefine says line
// 16 is a `define (not an `undef, 22.5.2, or an `undefineall, 22.5.3); an
// empty generic directive list says the file holds none of the directives
// HLDB records that way (`timescale, `default_nettype, `celldefine and the
// others with a vpiDirectiveType code); and vpiInitial, vpiSysTaskCall,
// vpiNullOp, vpiStringConst, vpiDecConst and vpiIntConst are codes of the
// LRM's own VPI object model (Clause 37, Annex K).
//
// Each LRM rule HLC might break has its own test, and no test depends on
// another rule's count: the non-empty $display arguments are checked
// whether or not the empty ones are represented.
//
// What is checked
//   - No fatal, syntax or error diagnostics, and none of the preprocessor
//     diagnostics that a wrong reading of the definition or the usage would
//     produce.
//   - Exactly one SourceFile for the .sv, with no includes and no generic
//     directives.
//   - Exactly one PreprocMacroDefinition, MACRO1, of type vpiPMDDefine, with
//     formal arguments a, b and c and the macro text above, on line 16. The
//     keyword initial stays separated from $display.
//   - Exactly one PreprocMacroInstance, MACRO1, bound to that definition, on
//     line 18, with the actual arguments 1, (white space only) and 3.
//   - Exactly one module, top, on lines 17-19, with one null port.
//   - top holds exactly one process, an initial procedure, whose statement is
//     a $display system task call that is not user-defined.
//   - The $display call has five arguments, the second and fourth being
//     vpiNullOp operations.
//   - Its non-empty arguments are 1, "B" and 3, in order. The "B" is the
//     default of b, which proves the default substitution.
//   - 1 and 3 are not unsigned constants and are at least 32 bits wide.
//   - "B" is a vpiStringConst.
//   - The initial procedure and the $display call carry line 18, the line of
//     the usage, not line 16 where their text is defined. This rests on an
//     interpretation of the LRM rather than an explicit rule: Annex K
//     defines vpiLineNo as the "line number where the object is used", and
//     22.13 and 20.10 tie the line a tool reports to `__LINE__, the "current
//     input line number", which while line 18 is expanded is 18.
//   - No other design object is created from this file.
//   - After elaboration only: one top-level instance named top.
//
// What is NOT checked, and why
//   - Where HLDB stores the default texts 5 and "B" of the formal arguments.
//     A formal argument is an Identifier, whose name is the simple
//     identifier; the headers and library expose only an undocumented
//     "buddy" field next to it, so reading a default from it would be
//     guessing a tool convention. a's default 5 is not used by this usage (a
//     receives 1), so it has no observable effect in this file. Nearest
//     assertion: DisplayNonEmptyArgumentsAreActualsAndDefault (the second
//     non-empty $display argument is "B", the default of b).
//   - The PreprocMacroInstance body, items and objects. The headers and
//     library do not document what these fields hold. The LRM-defined
//     expansion is checked where its result lives instead. Nearest assertion:
//     DisplayNonEmptyArgumentsAreActualsAndDefault.
//   - Whether the instance records the second actual argument as written
//     (white space only) or the default substituted for it ("B"). Both
//     reflect 22.5.1; MacroUsage accepts either and nothing else.
//   - How HLDB splits the macro text into tokens, whether it keeps white
//     space as tokens, and whether a CR from a CR LF line ending stays glued
//     to the last token. The LRM defines the macro text, not its storage, and
//     never defines CR LF as one newline (5.3), so the text is compared with
//     white space removed, plus a separate check that initial is not joined
//     to $display. Nearest assertions: the macro text checks in
//     MacroDefinition.
//   - How a string literal's value is stored: with or without its double
//     quotes. Both denote the string B (5.9). Nearest assertion:
//     DisplayNonEmptyArgumentsAreActualsAndDefault, which compares the
//     unquoted text.
//   - The size of "B". 5.9 gives a string literal 8 bits per character only
//     when it is used as an operand in an expression, which a $display
//     argument is not required to be. Nearest assertion:
//     DisplayStringLiteralIsAStringConstant.
//   - What $display prints (a space for each empty argument, 21.2.1, and the
//     trailing newline). That output only exists while simulation time
//     advances, so it is permanently out of scope. Nearest assertions:
//     DisplayEmptyArgumentsAreNullOperations and
//     DisplayNonEmptyArgumentsAreActualsAndDefault.
//   - Whether 1 and 3 are vpiDecConst or vpiIntConst, and their exact size.
//     The LRM text ties neither choice down; it only makes them signed and at
//     least 32 bits (5.7.1). Nearest assertion:
//     DisplayIntegerLiteralsAreSignedIntegers.
//   - The seven one-line comments and the block comment (5.4). The LRM gives
//     comments no meaning; how HLDB groups, classifies (vpiComment and
//     vpiDocumentComment are marked non-standard) and attaches them is a tool
//     convention. Nearest assertions: the line checks in MacroDefinition,
//     MacroUsage and TopModule.
//   - Columns. The LRM numbers lines (22.13) but not columns. Nearest
//     assertions: the line checks in MacroDefinition, MacroUsage and
//     TopModule.
//   - The SourceFile time unit and precision. There is no `timescale or
//     timeunit, so the default applies, and the default is
//     implementation-specific (3.14.2.3). Nearest assertion:
//     SourceFileHasNoIncludesOrDirectives (zero directives).
//   - The warning count. The LRM neither requires nor forbids warnings here.
//     Nearest assertion: CompilesWithoutErrors.
//   - Only the top-level instance check is gated on Design::getElaborated():
//     the instance tree only exists after elaboration. Everything else is
//     established by parsing and is asserted unconditionally.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/ErrorReporting/Location.h>
#include <hlc/Tests/Test.h>

#include <hldb/checker_decl.h>
#include <hldb/class_defn.h>
#include <hldb/config_decl.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/directive.h>
#include <hldb/hldb_vpi_user.h>
#include <hldb/identifier.h>
#include <hldb/initial.h>
#include <hldb/interface.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/port.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/preproc_macro_instance.h>
#include <hldb/process_stmt.h>
#include <hldb/program.h>
#include <hldb/source_file.h>
#include <hldb/sys_func_call.h>
#include <hldb/sys_task_call.h>
#include <hldb/tf_call.h>
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
constexpr std::string_view kSourceFileName = "22.5.1--define-expansion_10.sv";

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

// 'text' without one pair of enclosing double quotes, if it has them.
std::string_view unquote(std::string_view text) {
  if ((text.size() >= 2) && (text.front() == '"') && (text.back() == '"')) return text.substr(1, text.size() - 2);
  return text;
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

// The literal of 'constant' as written, for example "1": its vpiDecompile text
// (37.59), or, when that is empty, its value text after any "<format>:" prefix.
std::string_view literalOf(const hldb::Constant *constant) {
  const std::string_view decompile = constant->getDecompile();
  if (!decompile.empty()) return decompile;
  const std::string_view value = constant->getValue();
  const size_t colon = value.find(':');
  return (colon == std::string_view::npos) ? value : value.substr(colon + 1);
}

// The arguments of 'call' that are not empty arguments, in order. An empty
// argument is a vpiOperation with vpiOpType vpiNullOp (37.42 detail 8); a null
// entry is skipped as well. This lets the non-empty arguments be checked
// whether or not the empty ones are represented.
std::vector<const hldb::Any *> nonEmptyArguments(const hldb::TFCall *call) {
  std::vector<const hldb::Any *> result;
  if (const hldb::AnyCollection *const arguments = call->getArguments()) {
    for (const hldb::Any *argument : *arguments) {
      if (argument == nullptr) continue;
      const hldb::Operation *const operation = ::any_cast<hldb::Operation>(argument);
      if ((operation != nullptr) && (operation->getOpType() == vpiNullOp)) continue;
      result.emplace_back(argument);
    }
  }
  return result;
}

// The constant among 'arguments' whose literal, without enclosing double
// quotes, is 'text'; nullptr when there is none.
const hldb::Constant *findLiteral(const std::vector<const hldb::Any *> &arguments, std::string_view text) {
  for (const hldb::Any *argument : arguments) {
    const hldb::Constant *const constant = ::any_cast<hldb::Constant>(argument);
    if ((constant != nullptr) && (unquote(literalOf(constant)) == text)) return constant;
  }
  return nullptr;
}

// The line a diagnostic points at, or 0 when it carries no location.
uint32_t lineOf(const Error &error) { return error.getLocations().empty() ? 0 : error.getLocations().front().m_line; }

// Every diagnostic of 'severity' as " [<code> line <n>: <text>]", for failure
// messages, so a red run shows what was reported and where. <text> is HLC's
// message template, so its placeholders are not filled in.
std::string describeErrors(const ErrorContainer *errorContainer, ErrorDefinition::ErrorSeverity severity) {
  const ErrorDefinition::ErrorMap &errorInfoMap = ErrorDefinition::getErrorInfoMap();
  std::string description;
  for (const Error &error : errorContainer->getErrors()) {
    const ErrorDefinition::ErrorMap::const_iterator errorInfo = errorInfoMap.find(error.getType());
    if ((errorInfo == errorInfoMap.end()) || (errorInfo->second.m_severity != severity)) continue;
    description += " [" + std::to_string(static_cast<int32_t>(error.getType()));
    description += " line " + std::to_string(lineOf(error));
    description += ": " + std::string(errorInfo->second.m_errorText) + "]";
  }
  return description;
}
}  // namespace

class DefineExpansion10Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.5.1--define-expansion_10.hlc"}); }
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

  // The statement of top's only process, or nullptr unless top has exactly one process.
  static const hldb::Any *getInitialStatement() {
    const hldb::Module *const top = getTopDefinition();
    if ((top == nullptr) || (sizeOf(top->getProcesses()) != 1)) return nullptr;
    return top->getProcesses()->front()->getStmt();
  }

  // The $display call made by top's only process, or nullptr.
  static const hldb::TFCall *getDisplayCall() { return ::any_cast<hldb::TFCall>(getInitialStatement()); }
};

// The definition and the usage are both legal (22.5.1), and the expanded
// text initial $display(1,,"B",,3); is a legal module item (9.2.1, 21.2.1).
TEST_F(DefineExpansion10Test, CompilesWithoutErrors) {
  ASSERT_NE(m_session, nullptr);
  const ErrorContainer *const errorContainer = m_session->getErrorContainer();
  ASSERT_NE(errorContainer, nullptr);

  const ErrorContainer::Stats stats = errorContainer->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0) << describeErrors(errorContainer, ErrorDefinition::FATAL);
  EXPECT_EQ(stats.nbSyntax, 0) << describeErrors(errorContainer, ErrorDefinition::SYNTAX);
  EXPECT_EQ(stats.nbError, 0) << describeErrors(errorContainer, ErrorDefinition::ERROR);
}

// Each of these diagnostics would mean HLC misread the definition or the usage.
TEST_F(DefineExpansion10Test, ReportsNoMacroDiagnostics) {
  // Line 16 matches Syntax 22-2 and line 18 matches Syntax 22-3.
  EXPECT_EQ(findError(ErrorDefinition::PP_SYNTAX_ERROR), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_SYNTAX_ERROR), nullptr);

  // In the definition "(" follows MACRO1 with no white space; in the usage
  // white space before "(" is allowed (22.5.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_HAS_SPACE_BEFORE_ARGS), nullptr);

  // Three actual arguments for three formal arguments.
  EXPECT_EQ(findError(ErrorDefinition::PP_TOO_MANY_ARGS_MACRO), nullptr);

  // Only b is omitted, and b has the default "B" (22.5.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_NO_DEFAULT_VALUE), nullptr);

  // The usage has its parentheses (22.5.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_PARENTHESIS_NEEDED), nullptr);

  // a, b and c are all formal arguments and all appear in the macro text, and
  // each actual argument has a formal argument to go to.
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_UNUSED_ARGUMENT), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_UNDEFINED_ARGUMENT), nullptr);

  // MACRO1 is defined once, on line 16, before its use on line 18.
  EXPECT_EQ(findError(ErrorDefinition::PP_UNKOWN_MACRO), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MULTIPLY_DEFINED_MACRO), nullptr);

  // MACRO1 is not a compiler directive name (22.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_NAME_RESERVED), nullptr);

  // The default "B" is closed on line 16 (5.9).
  EXPECT_EQ(findError(ErrorDefinition::PP_UNTERMINATED_STRING), nullptr);

  // The macro text of MACRO1 does not refer to any macro.
  EXPECT_EQ(findError(ErrorDefinition::PP_RECURSIVE_MACRO_DEFINITION), nullptr);

  // `define is outside module top, and a macro usage may appear anywhere in
  // the compilation unit after its definition (22.5.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_IN_DESIGN_ELEMENT), nullptr);

  // The file is plain ASCII.
  EXPECT_EQ(findError(ErrorDefinition::PP_NON_ASCII_CONTENT), nullptr);
}

TEST_F(DefineExpansion10Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

TEST_F(DefineExpansion10Test, SourceFileHasNoIncludesOrDirectives) {
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

// Line 16: `define MACRO1(a=5,b="B",c) initial $display(a,,b,,c);
TEST_F(DefineExpansion10Test, MacroDefinition) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);
  const hldb::PreprocMacroDefinitionCollection *const definitions = sourceFile->getPreprocMacroDefinitions();
  ASSERT_EQ(sizeOf(definitions), 1u);
  const hldb::PreprocMacroDefinition *const macro1 = definitions->front();

  // Line 16 is a `define (22.5.1), not an `undef (22.5.2) or an `undefineall
  // (22.5.3).
  EXPECT_EQ(macro1->getType(), vpiPMDDefine);

  // text_macro_name is the simple identifier MACRO1.
  EXPECT_EQ(macro1->getName(), "MACRO1");
  const hldb::Identifier *const nameObj = macro1->getNameObj();
  ASSERT_NE(nameObj, nullptr);
  EXPECT_EQ(nameObj->getName(), "MACRO1");
  EXPECT_EQ(nameObj->getStartLine(), 16u);

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

  // The whole directive, terminated by the newline, sits on line 16.
  EXPECT_EQ(macro1->getStartLine(), 16u);
  EXPECT_EQ(macro1->getEndLine(), 16u);
  EXPECT_TRUE(endsWith(macro1->getFile(), kSourceFileName)) << macro1->getFile();
}

// Line 18: `MACRO1 ( 1 , , 3 )
TEST_F(DefineExpansion10Test, MacroUsage) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);
  const hldb::PreprocMacroInstanceCollection *const instances = sourceFile->getPreprocMacroInstances();
  ASSERT_EQ(sizeOf(instances), 1u);
  const hldb::PreprocMacroInstance *const usage = instances->front();

  // `text_macro_identifier is MACRO1.
  EXPECT_EQ(usage->getName(), "MACRO1");
  const hldb::Identifier *const nameObj = usage->getNameObj();
  ASSERT_NE(nameObj, nullptr);
  EXPECT_EQ(nameObj->getName(), "MACRO1");
  EXPECT_EQ(nameObj->getStartLine(), 18u);

  // The usage binds to the only definition of MACRO1, made on line 16.
  const hldb::PreprocMacroDefinitionCollection *const definitions = sourceFile->getPreprocMacroDefinitions();
  ASSERT_EQ(sizeOf(definitions), 1u);
  EXPECT_EQ(usage->getPreprocMacroDefinition(), definitions->front());

  // list_of_actual_arguments holds three actual arguments. The first is 1 and
  // the third is 3. The second is white space only, so the default "B" of b
  // is substituted for it (22.5.1); the model may record either the actual
  // as written or that default.
  const hldb::IdentifierCollection *const arguments = usage->getArguments();
  ASSERT_EQ(sizeOf(arguments), 3u);
  EXPECT_EQ(stripWhiteSpace(arguments->at(0)->getName()), "1");
  const std::string secondArgument = stripWhiteSpace(arguments->at(1)->getName());
  EXPECT_TRUE((secondArgument == "") || (secondArgument == "\"B\"")) << "second actual argument: " << secondArgument;
  EXPECT_EQ(stripWhiteSpace(arguments->at(2)->getName()), "3");

  EXPECT_EQ(usage->getStartLine(), 18u);
  EXPECT_TRUE(endsWith(usage->getFile(), kSourceFileName)) << usage->getFile();
}

// Lines 17-19: module top (); ... endmodule
TEST_F(DefineExpansion10Test, TopModule) {
  ASSERT_NE(m_design, nullptr);
  ASSERT_EQ(countFromSourceFile(m_design->getAllModules()), 1u);
  const hldb::Module *const top = getTopDefinition();
  ASSERT_NE(top, nullptr);

  EXPECT_EQ(top->getDefName(), "top");
  EXPECT_EQ(top->getStartLine(), 17u);
  EXPECT_EQ(top->getEndLine(), 19u);

  // "endmodule" carries no ": top" end label.
  EXPECT_EQ(top->getEndLabel(), "");

  // The expansion of line 18 is the only module item: one initial procedure,
  // and no continuous assignment, net or module instance.
  EXPECT_EQ(sizeOf(top->getProcesses()), 1u);
  EXPECT_EQ(sizeOf(top->getContAssigns()), 0u);
  EXPECT_EQ(sizeOf(top->getNets()), 0u);
  EXPECT_EQ(sizeOf(top->getModules()), 0u);
}

// "module top ();" has one null port (37.14 details 9, 10 and 11).
TEST_F(DefineExpansion10Test, TopHasOneNullPort) {
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

// Line 18 expands to: initial $display(1,,"B",,3);
// initial_construct ::= initial statement_or_null (9.2.1); the statement is
// the $display call itself, not a block. $display is a built-in system task
// (21.2.1), so the call is a vpiSysTaskCall whose vpiUserDefn is false (37.42).
TEST_F(DefineExpansion10Test, TopInitialIsADisplaySystemTaskCall) {
  const hldb::Module *const top = getTopDefinition();
  ASSERT_NE(top, nullptr);
  const hldb::ProcessCollection *const processes = top->getProcesses();
  ASSERT_EQ(sizeOf(processes), 1u);
  EXPECT_EQ(processes->front()->getVpiType(), static_cast<uint32_t>(vpiInitial));

  const hldb::Any *const stmt = getInitialStatement();
  ASSERT_NE(stmt, nullptr);
  EXPECT_EQ(stmt->getVpiType(), static_cast<uint32_t>(vpiSysTaskCall));

  const hldb::TFCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$display");

  if (const hldb::SysTaskCall *const taskCall = ::any_cast<hldb::SysTaskCall>(stmt)) {
    EXPECT_FALSE(taskCall->getUserDefn());
  } else if (const hldb::SysFuncCall *const funcCall = ::any_cast<hldb::SysFuncCall>(stmt)) {
    EXPECT_FALSE(funcCall->getUserDefn());
  } else {
    ADD_FAILURE() << "the statement is neither a system task call nor a system function call";
  }
}

// $display(1,,"B",,3) has five arguments; the second and fourth are empty, and
// an empty argument is a vpiOperation with vpiOpType vpiNullOp (37.42
// detail 8). The non-empty arguments are checked separately, in
// DisplayNonEmptyArgumentsAreActualsAndDefault.
TEST_F(DefineExpansion10Test, DisplayEmptyArgumentsAreNullOperations) {
  const hldb::TFCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr);
  const hldb::AnyCollection *const arguments = call->getArguments();
  ASSERT_EQ(sizeOf(arguments), 5u) << "$display(1,,\"B\",,3) has two empty arguments (37.42 detail 8)";

  const hldb::Operation *const second = ::any_cast<hldb::Operation>(arguments->at(1));
  EXPECT_NE(second, nullptr) << "argument 2 is empty";
  if (second != nullptr) EXPECT_EQ(second->getOpType(), vpiNullOp);

  const hldb::Operation *const fourth = ::any_cast<hldb::Operation>(arguments->at(3));
  EXPECT_NE(fourth, nullptr) << "argument 4 is empty";
  if (fourth != nullptr) EXPECT_EQ(fourth->getOpType(), vpiNullOp);
}

// 22.5.1: a receives the actual 1, b's actual argument is white space only so
// its default "B" is substituted, and c receives 3. Checked on the non-empty
// arguments only, so the result does not depend on how empty arguments are
// represented.
TEST_F(DefineExpansion10Test, DisplayNonEmptyArgumentsAreActualsAndDefault) {
  const hldb::TFCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr);
  const std::vector<const hldb::Any *> arguments = nonEmptyArguments(call);
  ASSERT_EQ(arguments.size(), 3u);

  const std::vector<std::string_view> expected = {"1", "B", "3"};
  for (size_t index = 0; index < expected.size(); ++index) {
    const hldb::Constant *const literal = ::any_cast<hldb::Constant>(arguments[index]);
    EXPECT_NE(literal, nullptr) << "non-empty argument " << (index + 1);
    if (literal != nullptr)
      EXPECT_EQ(unquote(literalOf(literal)), expected[index]) << "non-empty argument " << (index + 1);
  }
}

// 5.7.1: "Simple decimal numbers without the size and the base format shall
// be treated as signed integers", and an unsized number is at least 32 bits.
// vpiUIntConst marks an unsigned integer constant, so it would contradict the
// LRM for 1 and 3. Both are actual arguments, so they are found by value,
// independently of the default substitution and of the empty arguments.
TEST_F(DefineExpansion10Test, DisplayIntegerLiteralsAreSignedIntegers) {
  const hldb::TFCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr);
  const std::vector<const hldb::Any *> arguments = nonEmptyArguments(call);

  const std::vector<std::string_view> integerLiterals = {"1", "3"};
  for (std::string_view text : integerLiterals) {
    const hldb::Constant *const literal = findLiteral(arguments, text);
    EXPECT_NE(literal, nullptr) << "no $display argument " << text;
    if (literal == nullptr) continue;
    const int32_t constType = literal->getConstType();
    EXPECT_TRUE((constType == vpiDecConst) || (constType == vpiIntConst))
        << "argument " << text << " has vpiConstType " << constType;
    EXPECT_GE(literal->getSize(), 32) << "argument " << text;
  }
}

// "B" is a string literal (5.9); in the VPI object model a string literal
// argument is a vpiConstant with vpiConstType vpiStringConst (37.42 detail 8
// example). It only exists if the default of b was substituted.
TEST_F(DefineExpansion10Test, DisplayStringLiteralIsAStringConstant) {
  const hldb::TFCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr);
  const hldb::Constant *const literal = findLiteral(nonEmptyArguments(call), "B");
  ASSERT_NE(literal, nullptr) << "no $display argument \"B\" (the default of b)";
  EXPECT_EQ(literal->getConstType(), vpiStringConst);
}

// By interpretation of the LRM, not an explicit rule: Annex K defines
// vpiLineNo as the "line number where the object is used", and 22.13 and 20.10
// tie the line a tool reports to `__LINE__, the "current input line number",
// which while line 18 is expanded is 18. So the initial procedure and the
// $display call produced by the expansion carry line 18, not line 16 where
// their text is defined.
TEST_F(DefineExpansion10Test, ExpandedCodeCarriesTheUsageLine) {
  const hldb::Module *const top = getTopDefinition();
  ASSERT_NE(top, nullptr);
  ASSERT_EQ(sizeOf(top->getProcesses()), 1u);
  EXPECT_EQ(top->getProcesses()->front()->getStartLine(), 18u) << "the initial procedure";

  const hldb::TFCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getStartLine(), 18u) << "the $display call";
}

// top is the only module and is never instantiated, so the macro expansion
// creates nothing else, and top is the one top-level module (23.3.1).
TEST_F(DefineExpansion10Test, NoOtherDesignObjectsDeclared) {
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
    EXPECT_EQ(sizeOf(topInstance->getProcesses()), 1u);
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
