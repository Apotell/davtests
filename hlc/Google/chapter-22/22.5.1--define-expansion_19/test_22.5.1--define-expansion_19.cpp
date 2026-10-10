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

// Test for tests/Google/chapter-22/22.5.1--define-expansion_19.sv
//
// Lines 1-7 are one-line "//" license comments, lines 8-9 are blank, lines
// 10-15 are one block comment carrying the test metadata, and the
// SystemVerilog text is:
//
//   16: `define wordsize 8
//   17: module top ();
//   18: logic [1:`wordsize] data;
//   19: endmodule
//
// This is the first example of IEEE 1800-2023 22.5.1, verbatim, placed in a
// module.
//
// Line 16 is a text_macro_definition (Syntax 22-2): text_macro_name is the
// simple identifier wordsize with no list_of_formal_arguments (no "(" follows
// it), and the macro text is 8, up to the newline.
//
// Line 18 uses wordsize inside the packed dimension of a declaration. 22.5.1:
// "For a macro without arguments, the text shall be substituted as is for
// every occurrence of `text_macro_identifier." A macro without arguments
// takes no parentheses, so the usage is legal, and line 18 becomes:
//
//   logic [1:8] data;
//
// That is a data_declaration (6.8, Syntax 6-3): no net type keyword appears,
// so data is a variable, not a net. Its data type is logic, which is
// "4-state data type, user-defined vector size, unsigned" (6.11, Table 6-8),
// with one packed dimension whose left bound is 1 and whose right bound is 8,
// the substituted macro text. Both bounds are simple decimal numbers, which
// are signed integers of at least 32 bits (5.7.1). The packed dimension makes
// data 8 bits wide (7.4.1).
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
// others with a vpiDirectiveType code); and vpiDecConst, vpiIntConst and
// vpiSize are codes of the LRM's own VPI object model (Clause 37, Annex K).
//
// What is checked
//   - No fatal, syntax or error diagnostics, and none of the preprocessor
//     diagnostics that a wrong reading of the definition or the usage would
//     produce; in particular no "parentheses needed" diagnostic.
//   - Exactly one SourceFile for the .sv, with no includes and no generic
//     directives.
//   - Exactly one PreprocMacroDefinition, wordsize, of type vpiPMDDefine, with
//     no formal arguments and the macro text 8, on line 16.
//   - Exactly one PreprocMacroInstance, wordsize, bound to that definition, on
//     line 18, with no actual arguments.
//   - Exactly one module, top, on lines 17-19, holding exactly one variable
//     and no net, process, continuous assignment or module instance, with one
//     null port.
//   - The variable is data, declared on line 18, of an unsigned logic type
//     with exactly one packed dimension, [1:8].
//   - The bounds 1 and 8 are not unsigned constants and are at least 32 bits
//     wide.
//   - The substituted bound 8 carries line 18, the line of the usage, not
//     line 16 where its text is defined. This rests on an interpretation of
//     the LRM rather than an explicit rule: Annex K defines vpiLineNo as the
//     "line number where the object is used", and 22.13 and 20.10 tie the
//     line a tool reports to `__LINE__, the "current input line number",
//     which while line 18 is expanded is 18.
//   - No other design object is created from this file.
//   - After elaboration only: one top-level instance named top, whose
//     variable data has vpiSize 8.
//
// What is NOT checked, and why
//   - The PreprocMacroInstance body, items and objects. The headers and
//     library do not document what these fields hold. The LRM-defined
//     substitution is checked where its result lives instead. Nearest
//     assertion: DataRangeIsOneToWordsize (the right bound is 8).
//   - How HLDB splits the macro text into tokens, whether it keeps white
//     space as tokens, and whether a CR from a CR LF line ending stays glued
//     to the last token. The LRM defines the macro text, not its storage, and
//     never defines CR LF as one newline (5.3), so the text is compared with
//     white space removed. Nearest assertion: the macro text check in
//     MacroDefinition.
//   - Whether 1 and 8 are vpiDecConst or vpiIntConst, and their exact size.
//     The LRM text ties neither choice down; it only makes them signed and at
//     least 32 bits (5.7.1). Nearest assertion: RangeBoundsAreSignedIntegers.
//   - The width of data before elaboration. The width is computed from the
//     bounds, which is reduction; it is asserted only on an elaborated design.
//     Nearest assertion: DataRangeIsOneToWordsize (the bounds themselves).
//   - The value data holds. It only exists while simulation time advances,
//     so it is permanently out of scope. Nearest assertion:
//     DataIsALogicVariable (its 4-state logic type).
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
//   - Only the top-level instance and the width of data are gated on
//     Design::getElaborated(): the instance tree only exists after
//     elaboration, and the width is reduced from the bounds. Everything else
//     is established by parsing and is asserted unconditionally.

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
#include <hldb/interface.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/port.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/preproc_macro_instance.h>
#include <hldb/program.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
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
constexpr std::string_view kSourceFileName = "22.5.1--define-expansion_19.sv";

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

// The variable named 'name' in 'variables', or nullptr.
const hldb::Variable *findVariable(const hldb::VariableCollection *variables, std::string_view name) {
  if (variables != nullptr) {
    for (const hldb::Variable *variable : *variables) {
      if (variable->getName() == name) return variable;
    }
  }
  return nullptr;
}

// The logic typespec 'variable' refers to, or nullptr when its type is not logic.
const hldb::LogicTypespec *logicTypespecOf(const hldb::Variable *variable) {
  const hldb::RefTypespec *const refTypespec = variable->getTypespec();
  if (refTypespec == nullptr) return nullptr;
  return ::any_cast<hldb::LogicTypespec>(refTypespec->getActual());
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

// The literal of 'constant' as written, for example "8": its vpiDecompile text
// (37.59), or, when that is empty, its value text after any "<format>:" prefix.
std::string_view literalOf(const hldb::Constant *constant) {
  const std::string_view decompile = constant->getDecompile();
  if (!decompile.empty()) return decompile;
  const std::string_view value = constant->getValue();
  const size_t colon = value.find(':');
  return (colon == std::string_view::npos) ? value : value.substr(colon + 1);
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

class DefineExpansion19Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.5.1--define-expansion_19.hlc"}); }
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

  // The variable data of top's definition, or nullptr.
  static const hldb::Variable *getData() {
    const hldb::Module *const top = getTopDefinition();
    return (top == nullptr) ? nullptr : findVariable(top->getVariables(), "data");
  }

  // The one packed dimension of data's logic type, or nullptr unless there is exactly one.
  static const hldb::Range *getDataRange() {
    const hldb::Variable *const data = getData();
    if (data == nullptr) return nullptr;
    const hldb::LogicTypespec *const logicTypespec = logicTypespecOf(data);
    if ((logicTypespec == nullptr) || (sizeOf(logicTypespec->getRanges()) != 1)) return nullptr;
    return logicTypespec->getRanges()->front();
  }
};

// The definition and the usage are both legal (22.5.1), and the substituted
// line logic [1:8] data; is a legal data_declaration (6.8).
TEST_F(DefineExpansion19Test, CompilesWithoutErrors) {
  ASSERT_NE(m_session, nullptr);
  const ErrorContainer *const errorContainer = m_session->getErrorContainer();
  ASSERT_NE(errorContainer, nullptr);

  const ErrorContainer::Stats stats = errorContainer->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0) << describeErrors(errorContainer, ErrorDefinition::FATAL);
  EXPECT_EQ(stats.nbSyntax, 0) << describeErrors(errorContainer, ErrorDefinition::SYNTAX);
  EXPECT_EQ(stats.nbError, 0) << describeErrors(errorContainer, ErrorDefinition::ERROR);
}

// Each of these diagnostics would mean HLC misread the definition or the usage.
TEST_F(DefineExpansion19Test, ReportsNoMacroDiagnostics) {
  // Line 16 matches Syntax 22-2 and the usage on line 18 matches Syntax 22-3.
  EXPECT_EQ(findError(ErrorDefinition::PP_SYNTAX_ERROR), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_SYNTAX_ERROR), nullptr);

  // wordsize has no formal arguments, so its usage takes no parentheses
  // (22.5.1), and no argument can be one too many, lack a default, or be
  // unused or undefined.
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_PARENTHESIS_NEEDED), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_TOO_MANY_ARGS_MACRO), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_NO_DEFAULT_VALUE), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_UNUSED_ARGUMENT), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_UNDEFINED_ARGUMENT), nullptr);

  // No "(" follows wordsize on line 16, so the text after the space is macro
  // text, not a formal argument list (22.5.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_HAS_SPACE_BEFORE_ARGS), nullptr);

  // wordsize is defined once, on line 16, before its use on line 18.
  EXPECT_EQ(findError(ErrorDefinition::PP_UNKOWN_MACRO), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MULTIPLY_DEFINED_MACRO), nullptr);

  // wordsize is not a compiler directive name (22.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_NAME_RESERVED), nullptr);

  // The macro text 8 does not refer to any macro.
  EXPECT_EQ(findError(ErrorDefinition::PP_RECURSIVE_MACRO_DEFINITION), nullptr);

  // `define is outside module top, and a macro usage may appear anywhere in
  // the compilation unit after its definition (22.5.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_IN_DESIGN_ELEMENT), nullptr);

  // The file is plain ASCII.
  EXPECT_EQ(findError(ErrorDefinition::PP_NON_ASCII_CONTENT), nullptr);
}

TEST_F(DefineExpansion19Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

TEST_F(DefineExpansion19Test, SourceFileHasNoIncludesOrDirectives) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  // There is no `include in the file (22.4).
  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);

  // `define is the only directive in the file (`wordsize is a macro usage,
  // not a directive). None of the directives HLDB records as generic
  // directives (those with a vpiDirectiveType code: `timescale,
  // `default_nettype, `celldefine, ...) occurs, and `define has no
  // vpiDirectiveType code, so a generic directive here would be one the
  // source does not contain.
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);
}

// Line 16: `define wordsize 8
TEST_F(DefineExpansion19Test, MacroDefinition) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);
  const hldb::PreprocMacroDefinitionCollection *const definitions = sourceFile->getPreprocMacroDefinitions();
  ASSERT_EQ(sizeOf(definitions), 1u);
  const hldb::PreprocMacroDefinition *const wordsize = definitions->front();

  // Line 16 is a `define (22.5.1), not an `undef (22.5.2) or an `undefineall
  // (22.5.3).
  EXPECT_EQ(wordsize->getType(), vpiPMDDefine);

  // text_macro_name is the simple identifier wordsize.
  EXPECT_EQ(wordsize->getName(), "wordsize");
  const hldb::Identifier *const nameObj = wordsize->getNameObj();
  ASSERT_NE(nameObj, nullptr);
  EXPECT_EQ(nameObj->getName(), "wordsize");
  EXPECT_EQ(nameObj->getStartLine(), 16u);

  // No "(" follows the name, so there is no list_of_formal_arguments.
  EXPECT_EQ(sizeOf(wordsize->getArguments()), 0u);

  // The macro text is 8, ending at the newline (22.5.1).
  EXPECT_EQ(strippedMacroText(wordsize), "8");

  // The whole directive, terminated by the newline, sits on line 16.
  EXPECT_EQ(wordsize->getStartLine(), 16u);
  EXPECT_EQ(wordsize->getEndLine(), 16u);
  EXPECT_TRUE(endsWith(wordsize->getFile(), kSourceFileName)) << wordsize->getFile();
}

// Line 18: logic [1:`wordsize] data;
TEST_F(DefineExpansion19Test, MacroUsage) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);
  const hldb::PreprocMacroInstanceCollection *const instances = sourceFile->getPreprocMacroInstances();
  ASSERT_EQ(sizeOf(instances), 1u);
  const hldb::PreprocMacroInstance *const usage = instances->front();

  // `text_macro_identifier is wordsize.
  EXPECT_EQ(usage->getName(), "wordsize");
  const hldb::Identifier *const nameObj = usage->getNameObj();
  ASSERT_NE(nameObj, nullptr);
  EXPECT_EQ(nameObj->getName(), "wordsize");
  EXPECT_EQ(nameObj->getStartLine(), 18u);

  // The usage binds to the only definition of wordsize, made on line 16.
  const hldb::PreprocMacroDefinitionCollection *const definitions = sourceFile->getPreprocMacroDefinitions();
  ASSERT_EQ(sizeOf(definitions), 1u);
  EXPECT_EQ(usage->getPreprocMacroDefinition(), definitions->front());

  // A macro without arguments is used without a list_of_actual_arguments.
  EXPECT_EQ(sizeOf(usage->getArguments()), 0u);

  EXPECT_EQ(usage->getStartLine(), 18u);
  EXPECT_TRUE(endsWith(usage->getFile(), kSourceFileName)) << usage->getFile();
}

// Lines 17-19: module top (); logic [1:8] data; endmodule
TEST_F(DefineExpansion19Test, TopModule) {
  ASSERT_NE(m_design, nullptr);
  ASSERT_EQ(countFromSourceFile(m_design->getAllModules()), 1u);
  const hldb::Module *const top = getTopDefinition();
  ASSERT_NE(top, nullptr);

  EXPECT_EQ(top->getDefName(), "top");
  EXPECT_EQ(top->getStartLine(), 17u);
  EXPECT_EQ(top->getEndLine(), 19u);

  // "endmodule" carries no ": top" end label.
  EXPECT_EQ(top->getEndLabel(), "");

  // The only module item is the data_declaration of line 18: one variable,
  // and no net (6.8: no net type keyword), process, continuous assignment or
  // module instance.
  EXPECT_EQ(sizeOf(top->getVariables()), 1u);
  EXPECT_EQ(sizeOf(top->getNets()), 0u);
  EXPECT_EQ(sizeOf(top->getProcesses()), 0u);
  EXPECT_EQ(sizeOf(top->getContAssigns()), 0u);
  EXPECT_EQ(sizeOf(top->getModules()), 0u);
}

// "module top ();" has one null port (37.14 details 9, 10 and 11).
TEST_F(DefineExpansion19Test, TopHasOneNullPort) {
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

// logic [1:8] data; declares the variable data (6.8) of type logic, which is
// unsigned unless declared signed (6.11, Table 6-8).
TEST_F(DefineExpansion19Test, DataIsALogicVariable) {
  const hldb::Variable *const data = getData();
  ASSERT_NE(data, nullptr) << "no variable data in module top";
  EXPECT_EQ(data->getName(), "data");
  EXPECT_EQ(data->getStartLine(), 18u);

  const hldb::LogicTypespec *const logicTypespec = logicTypespecOf(data);
  ASSERT_NE(logicTypespec, nullptr) << "data is declared logic";
  EXPECT_FALSE(logicTypespec->getSigned());
}

// The packed dimension [1:`wordsize] becomes [1:8]: left bound 1, right bound
// 8, the macro text substituted as is (22.5.1, 7.4.1).
TEST_F(DefineExpansion19Test, DataRangeIsOneToWordsize) {
  const hldb::Variable *const data = getData();
  ASSERT_NE(data, nullptr);
  const hldb::LogicTypespec *const logicTypespec = logicTypespecOf(data);
  ASSERT_NE(logicTypespec, nullptr);
  ASSERT_EQ(sizeOf(logicTypespec->getRanges()), 1u) << "data has one packed dimension";

  const hldb::Range *const range = getDataRange();
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const left = ::any_cast<hldb::Constant>(range->getLeftExpr());
  EXPECT_NE(left, nullptr) << "the left bound is the literal 1";
  if (left != nullptr) EXPECT_EQ(literalOf(left), "1");
  const hldb::Constant *const right = ::any_cast<hldb::Constant>(range->getRightExpr());
  EXPECT_NE(right, nullptr) << "the right bound is the substituted macro text 8";
  if (right != nullptr) EXPECT_EQ(literalOf(right), "8");
}

// 5.7.1: "Simple decimal numbers without the size and the base format shall
// be treated as signed integers", and an unsized number is at least 32 bits.
// vpiUIntConst marks an unsigned integer constant, so it would contradict the
// LRM for the bounds 1 and 8.
TEST_F(DefineExpansion19Test, RangeBoundsAreSignedIntegers) {
  const hldb::Range *const range = getDataRange();
  ASSERT_NE(range, nullptr);

  const std::vector<const hldb::Constant *> bounds = {::any_cast<hldb::Constant>(range->getLeftExpr()),
                                                      ::any_cast<hldb::Constant>(range->getRightExpr())};
  for (const hldb::Constant *bound : bounds) {
    EXPECT_NE(bound, nullptr) << "a bound is not a constant";
    if (bound == nullptr) continue;
    const int32_t constType = bound->getConstType();
    EXPECT_TRUE((constType == vpiDecConst) || (constType == vpiIntConst))
        << "bound " << literalOf(bound) << " has vpiConstType " << constType;
    EXPECT_GE(bound->getSize(), 32) << "bound " << literalOf(bound);
  }
}

// By interpretation of the LRM, not an explicit rule: Annex K defines
// vpiLineNo as the "line number where the object is used", and 22.13 and 20.10
// tie the line a tool reports to `__LINE__, the "current input line number",
// which while line 18 is expanded is 18. So the bound 8, substituted from the
// macro text of line 16, carries line 18.
TEST_F(DefineExpansion19Test, ExpandedCodeCarriesTheUsageLine) {
  const hldb::Range *const range = getDataRange();
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const right = ::any_cast<hldb::Constant>(range->getRightExpr());
  ASSERT_NE(right, nullptr) << "the right bound is the substituted macro text 8";
  EXPECT_EQ(right->getStartLine(), 18u) << "the substituted bound 8";
}

// top is the only module and is never instantiated, so nothing else is
// declared in the file, and top is the one top-level module (23.3.1).
TEST_F(DefineExpansion19Test, NoOtherDesignObjectsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllClasses()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);

  // The macro wordsize is preprocessor text, not a parameter; data lives in
  // top, not at the compilation-unit level.
  EXPECT_EQ(countFromSourceFile(m_design->getParameters()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParamAssigns()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getVariables()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getTypedefs()), 0u);

  // A top-level module is implicitly instantiated once, under its own name
  // (23.3.1). The instance tree only exists after elaboration, and the width
  // of data, 8 bits from [1:8] (7.4.1), is reduced from its bounds.
  if (m_design->getElaborated()) {
    const hldb::ModuleCollection *const topModules = m_design->getTopModules();
    ASSERT_EQ(countFromSourceFile(topModules), 1u);
    const hldb::Module *const topInstance = firstFromSourceFile(topModules);
    EXPECT_EQ(topInstance->getName(), "top");
    EXPECT_EQ(topInstance->getDefName(), "top");

    const hldb::Variable *const data = findVariable(topInstance->getVariables(), "data");
    ASSERT_NE(data, nullptr) << "no variable data in the top instance";
    const hldb::Any::vpi_property_value_t size = data->getVpiPropertyValue(vpiSize);
    ASSERT_TRUE(std::holds_alternative<int64_t>(size));
    EXPECT_EQ(std::get<int64_t>(size), 8);
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
