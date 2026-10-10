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

// Test for tests/Google/chapter-22/22.5.1--define-expansion_20.sv
//
// Lines 1-7 are one-line "//" license comments, lines 8-9 are blank, lines
// 10-15 are one block comment carrying the test metadata, and the
// SystemVerilog text is:
//
//   16: `define var_nand(dly) nand #dly
//   17: module top ();
//   18: `var_nand(2) g121 (q21, n10, n11);
//   19: `var_nand(5) g122 (q22, n10, n11);
//   20: endmodule
//
// This is the second example of IEEE 1800-2023 22.5.1 ("define a nand with
// variable delay"), placed in a module.
//
// Line 16 is a text_macro_definition (Syntax 22-2): text_macro_name is the
// simple identifier var_nand, immediately followed by "(", so (dly) is its
// list_of_formal_arguments, one formal argument dly without a default. The
// macro text is nand #dly, up to the newline; dly is used in it "in the same
// manner as an identifier" (22.5.1).
//
// Lines 18 and 19 are text_macro_usages (Syntax 22-3) with one actual
// argument each, 2 and 5, matching the one formal argument. Each formal
// argument is substituted by its actual argument (22.5.1), and the text after
// the closing parenthesis is ordinary source text, so the lines become:
//
//   18: nand #2 g121 (q21, n10, n11);
//   19: nand #5 g122 (q22, n10, n11);
//
// Each is a gate instantiation (28.3) of the n-input gate nand (28.4): "The
// first terminal in the terminal list shall connect to the output of the
// gate and all other terminals connect to its inputs", so q21 and q22 are
// outputs and n10 and n11 inputs of both gates. Each gate has one delay value,
// 2 and 5 ("If only one delay is specified, it shall specify both the rise
// and fall delays", 28.4); both are simple decimal numbers, which are signed
// integers of at least 32 bits (5.7.1). In the VPI object model a gate is a
// vpiGate primitive with vpiPrimType vpiNandPrim, its terminals carry
// vpiDirection and a vpiTermIndex starting at zero (37.35 detail 3), and
// "vpiSize shall return the number of inputs" (37.35 detail 1), 2 here.
//
// q21, n10, n11 and q22 are declared nowhere. 6.10: "If an identifier is used
// in the terminal list of a primitive instance ... and that identifier has
// not been declared previously ..., then an implicit scalar net of default
// net type shall be assumed." With no `default_nettype, "implicit nets are of
// type wire" (22.8). So top has four implicit scalar wires; n10 and n11 are
// shared by both gates and exist once each.
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
// others with a vpiDirectiveType code); and vpiGate, vpiNandPrim, vpiOutput,
// vpiInput, vpiWire, vpiSize, vpiDecConst and vpiIntConst are codes of the
// LRM's own VPI object model (Clause 37, Annex K).
//
// What is checked
//   - No fatal, syntax or error diagnostics, and none of the preprocessor
//     diagnostics that a wrong reading of the definition or the usages would
//     produce.
//   - Exactly one SourceFile for the .sv, with no includes and no generic
//     directives.
//   - Exactly one PreprocMacroDefinition, var_nand, of type vpiPMDDefine, with
//     the formal argument dly and the macro text nand #dly, on line 16.
//   - Exactly two PreprocMacroInstances of var_nand, bound to that
//     definition: one on line 18 with the actual argument 2, one on line 19
//     with the actual argument 5.
//   - Exactly one module, top, on lines 17-20, with one null port, holding
//     exactly two primitives and no variable, process, continuous assignment
//     or module instance.
//   - The primitives are the nand gates g121 and g122, each with one delay
//     (2 and 5, the substituted actual arguments) and three terminals: the
//     output q21 or q22 at index 0, and the inputs n10 and n11 at indexes 1
//     and 2.
//   - Each gate reports vpiSize 2, its number of inputs.
//   - The delays 2 and 5 are not unsigned constants and are at least 32 bits
//     wide.
//   - Before elaboration, top holds either no nets yet or exactly the four
//     implicit wires q21, n10, n11 and q22; after elaboration, the top
//     instance holds exactly those four, each scalar, and the gate terminals
//     bind to them. Wherever the four are present, each has vpiLineNo 0 and
//     the file under test as vpiFile (37.16 detail 9).
//   - g121 carries line 18 and g122 line 19, the lines of their usages, not
//     line 16 where nand #dly is defined. This rests on an interpretation of
//     the LRM rather than an explicit rule: Annex K defines vpiLineNo as the
//     "line number where the object is used", and 22.13 and 20.10 tie the
//     line a tool reports to `__LINE__, the "current input line number",
//     which while a usage is expanded is the usage's line.
//   - No other design object is created from this file.
//   - After elaboration only: one top-level instance named top.
//
// What is NOT checked, and why
//   - The PreprocMacroInstance body, items and objects. The headers and
//     library do not document what these fields hold. The LRM-defined
//     substitution is checked where its result lives instead. Nearest
//     assertions: GateG121 and GateG122.
//   - How HLDB splits the macro text into tokens, whether it keeps white
//     space as tokens, and whether a CR from a CR LF line ending stays glued
//     to the last token. The LRM defines the macro text, not its storage, and
//     never defines CR LF as one newline (5.3), so the text is compared with
//     white space removed. Nearest assertion: the macro text check in
//     MacroDefinition.
//   - The gate definition name (vpiDefName). The LRM lists the property but
//     does not say what it holds for a built-in gate. Nearest assertion:
//     TopHasTwoNandGates (vpiPrimType vpiNandPrim).
//   - When HLDB creates the implicit nets. 6.10 says they exist, not at which
//     stage a tool records them, so before elaboration "none yet" is accepted
//     besides the four wires. Nearest assertions: ImplicitNetsBeforeElaboration
//     and the elaborated part of NoOtherDesignObjectsDeclared.
//   - Whether 2 and 5 are vpiDecConst or vpiIntConst, and their exact size.
//     The LRM text ties neither choice down; it only makes them signed and at
//     least 32 bits (5.7.1). Nearest assertion: GateDelaysAreSignedIntegers.
//   - The rise and fall delays a simulator derives from each delay, and the
//     values the gates drive. They only exist while simulation time advances,
//     so they are permanently out of scope. Nearest assertions: the delay
//     checks in GateG121 and GateG122.
//   - The seven one-line comments and the block comment (5.4). The LRM gives
//     comments no meaning; how HLDB groups, classifies (vpiComment and
//     vpiDocumentComment are marked non-standard) and attaches them is a tool
//     convention. Nearest assertions: the line checks in MacroDefinition,
//     MacroUsages and TopModule.
//   - Columns. The LRM numbers lines (22.13) but not columns. Nearest
//     assertions: the line checks in MacroDefinition, MacroUsages and
//     TopModule.
//   - The SourceFile time unit and precision. There is no `timescale or
//     timeunit, so the default applies, and the default is
//     implementation-specific (3.14.2.3). Nearest assertion:
//     SourceFileHasNoIncludesOrDirectives (zero directives).
//   - The warning count. The LRM neither requires nor forbids warnings here.
//     Nearest assertion: CompilesWithoutErrors.
//   - Only the top-level instance, its implicit nets' sizes and the terminal
//     bindings are gated on Design::getElaborated(): the instance tree and
//     name binding only exist after elaboration. Everything else is
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
#include <hldb/gate.h>
#include <hldb/hldb_vpi_user.h>
#include <hldb/identifier.h>
#include <hldb/interface.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/port.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/preproc_macro_instance.h>
#include <hldb/prim_term.h>
#include <hldb/primitive.h>
#include <hldb/program.h>
#include <hldb/ref_obj.h>
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
constexpr std::string_view kSourceFileName = "22.5.1--define-expansion_20.sv";

// The implicit nets 6.10 requires: every terminal identifier, once each.
const std::vector<std::string_view> kImplicitNetNames = {"q21", "n10", "n11", "q22"};

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

// The object named 'name' in 'collection', or nullptr.
template <typename T>
const T *findByName(const std::vector<T *> *collection, std::string_view name) {
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (object->getName() == name) return object;
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

// The literal of 'constant' as written, for example "2": its vpiDecompile text
// (37.59), or, when that is empty, its value text after any "<format>:" prefix.
std::string_view literalOf(const hldb::Constant *constant) {
  const std::string_view decompile = constant->getDecompile();
  if (!decompile.empty()) return decompile;
  const std::string_view value = constant->getValue();
  const size_t colon = value.find(':');
  return (colon == std::string_view::npos) ? value : value.substr(colon + 1);
}

// The one delay of 'gate' as a constant, or nullptr unless it has exactly one constant delay.
const hldb::Constant *delayOf(const hldb::Primitive *gate) {
  if (sizeOf(gate->getDelays()) != 1) return nullptr;
  return ::any_cast<hldb::Constant>(gate->getDelays()->front());
}

// The name 'term' connects to, or empty when its expression is not a simple reference.
std::string_view terminalName(const hldb::PrimTerm *term) {
  const hldb::RefObj *const ref = ::any_cast<hldb::RefObj>(term->getExpr());
  return (ref == nullptr) ? std::string_view() : ref->getName();
}

// Checks the three terminals of a nand gate (28.4): 'output' at index 0, then
// the inputs n10 and n11 at indexes 1 and 2 (37.35 detail 3).
void expectNandTerminals(const hldb::Primitive *gate, std::string_view output) {
  const hldb::PrimTermCollection *const terms = gate->getPrimTerms();
  ASSERT_EQ(sizeOf(terms), 3u) << gate->getName() << " has three terminals";

  const std::vector<std::string_view> names = {output, "n10", "n11"};
  for (size_t index = 0; index < names.size(); ++index) {
    const hldb::PrimTerm *const term = terms->at(index);
    EXPECT_EQ(term->getTermIndex(), static_cast<int32_t>(index)) << gate->getName() << " terminal " << index;
    EXPECT_EQ(term->getDirection(), (index == 0) ? vpiOutput : vpiInput) << gate->getName() << " terminal " << index;
    EXPECT_EQ(terminalName(term), names[index]) << gate->getName() << " terminal " << index;
  }
}

// Checks that 'nets' holds exactly the four implicit wires of 6.10 and 22.8,
// each located as 37.16 detail 9 requires: "For implicit nets, vpiLineNo shall
// return 0, and vpiFile shall return the file name where the implicit net is
// first referenced."
void expectImplicitWires(const hldb::NetCollection *nets) {
  ASSERT_EQ(sizeOf(nets), kImplicitNetNames.size());
  for (std::string_view name : kImplicitNetNames) {
    const hldb::Net *const net = findByName(nets, name);
    EXPECT_NE(net, nullptr) << "no implicit net " << name;
    if (net == nullptr) continue;
    EXPECT_TRUE(net->getImplicitDecl()) << name << " is declared nowhere (6.10)";
    EXPECT_EQ(net->getNetType(), vpiWire) << name << " has the default net type wire (22.8)";

    const hldb::Any::vpi_property_value_t lineNo = net->getVpiPropertyValue(vpiLineNo);
    EXPECT_TRUE(std::holds_alternative<int64_t>(lineNo)) << name;
    if (std::holds_alternative<int64_t>(lineNo)) {
      EXPECT_EQ(std::get<int64_t>(lineNo), 0) << name << ": an implicit net has vpiLineNo 0 (37.16 detail 9)";
    }
    EXPECT_TRUE(endsWith(net->getFile(), kSourceFileName))
        << name << " is first referenced in the file under test (37.16 detail 9): " << net->getFile();
  }
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

class DefineExpansion20Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.5.1--define-expansion_20.hlc"}); }
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

  // The primitive named 'name' in top's definition, or nullptr.
  static const hldb::Primitive *getGate(std::string_view name) {
    const hldb::Module *const top = getTopDefinition();
    return (top == nullptr) ? nullptr : findByName(top->getPrimitives(), name);
  }

  // The macro instance whose usage starts on 'line', or nullptr.
  static const hldb::PreprocMacroInstance *getUsageOnLine(uint32_t line) {
    const hldb::SourceFile *const sourceFile = getSourceFile();
    if ((sourceFile == nullptr) || (sourceFile->getPreprocMacroInstances() == nullptr)) return nullptr;
    for (const hldb::PreprocMacroInstance *usage : *sourceFile->getPreprocMacroInstances()) {
      if (usage->getStartLine() == line) return usage;
    }
    return nullptr;
  }
};

// The definition and both usages are legal (22.5.1), and the substituted
// lines are legal gate instantiations (28.3, 28.4).
TEST_F(DefineExpansion20Test, CompilesWithoutErrors) {
  ASSERT_NE(m_session, nullptr);
  const ErrorContainer *const errorContainer = m_session->getErrorContainer();
  ASSERT_NE(errorContainer, nullptr);

  const ErrorContainer::Stats stats = errorContainer->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0) << describeErrors(errorContainer, ErrorDefinition::FATAL);
  EXPECT_EQ(stats.nbSyntax, 0) << describeErrors(errorContainer, ErrorDefinition::SYNTAX);
  EXPECT_EQ(stats.nbError, 0) << describeErrors(errorContainer, ErrorDefinition::ERROR);
}

// Each of these diagnostics would mean HLC misread the definition or a usage.
TEST_F(DefineExpansion20Test, ReportsNoMacroDiagnostics) {
  // Line 16 matches Syntax 22-2 and lines 18 and 19 match Syntax 22-3.
  EXPECT_EQ(findError(ErrorDefinition::PP_SYNTAX_ERROR), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_SYNTAX_ERROR), nullptr);

  // "(" follows var_nand with no white space, in the definition and in both
  // usages (22.5.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_HAS_SPACE_BEFORE_ARGS), nullptr);

  // Each usage gives one actual argument for the one formal argument, with
  // its parentheses.
  EXPECT_EQ(findError(ErrorDefinition::PP_TOO_MANY_ARGS_MACRO), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_NO_DEFAULT_VALUE), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_PARENTHESIS_NEEDED), nullptr);

  // dly is a formal argument and appears in the macro text, and each actual
  // argument has a formal argument to go to.
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_UNUSED_ARGUMENT), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_UNDEFINED_ARGUMENT), nullptr);

  // var_nand is defined once, on line 16, before its uses on lines 18 and 19.
  EXPECT_EQ(findError(ErrorDefinition::PP_UNKOWN_MACRO), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MULTIPLY_DEFINED_MACRO), nullptr);

  // var_nand is not a compiler directive name (22.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_NAME_RESERVED), nullptr);

  // The macro text nand #dly does not refer to any macro.
  EXPECT_EQ(findError(ErrorDefinition::PP_RECURSIVE_MACRO_DEFINITION), nullptr);

  // `define is outside module top, and a macro usage may appear anywhere in
  // the compilation unit after its definition (22.5.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_IN_DESIGN_ELEMENT), nullptr);

  // The file is plain ASCII.
  EXPECT_EQ(findError(ErrorDefinition::PP_NON_ASCII_CONTENT), nullptr);
}

TEST_F(DefineExpansion20Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

TEST_F(DefineExpansion20Test, SourceFileHasNoIncludesOrDirectives) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  // There is no `include in the file (22.4).
  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);

  // `define is the only directive in the file (`var_nand is a macro usage,
  // not a directive). None of the directives HLDB records as generic
  // directives (those with a vpiDirectiveType code: `timescale,
  // `default_nettype, `celldefine, ...) occurs, and `define has no
  // vpiDirectiveType code, so a generic directive here would be one the
  // source does not contain.
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);
}

// Line 16: `define var_nand(dly) nand #dly
TEST_F(DefineExpansion20Test, MacroDefinition) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);
  const hldb::PreprocMacroDefinitionCollection *const definitions = sourceFile->getPreprocMacroDefinitions();
  ASSERT_EQ(sizeOf(definitions), 1u);
  const hldb::PreprocMacroDefinition *const varNand = definitions->front();

  // Line 16 is a `define (22.5.1), not an `undef (22.5.2) or an `undefineall
  // (22.5.3).
  EXPECT_EQ(varNand->getType(), vpiPMDDefine);

  // text_macro_name is the simple identifier var_nand.
  EXPECT_EQ(varNand->getName(), "var_nand");
  const hldb::Identifier *const nameObj = varNand->getNameObj();
  ASSERT_NE(nameObj, nullptr);
  EXPECT_EQ(nameObj->getName(), "var_nand");
  EXPECT_EQ(nameObj->getStartLine(), 16u);

  // list_of_formal_arguments is (dly): one simple identifier.
  const hldb::IdentifierCollection *const arguments = varNand->getArguments();
  ASSERT_EQ(sizeOf(arguments), 1u);
  EXPECT_EQ(arguments->front()->getName(), "dly");

  // The macro text follows the formal argument list and ends at the newline;
  // the formal argument list is not part of it.
  EXPECT_EQ(strippedMacroText(varNand), "nand#dly");

  // The whole directive, terminated by the newline, sits on line 16.
  EXPECT_EQ(varNand->getStartLine(), 16u);
  EXPECT_EQ(varNand->getEndLine(), 16u);
  EXPECT_TRUE(endsWith(varNand->getFile(), kSourceFileName)) << varNand->getFile();
}

// Lines 18 and 19: `var_nand(2) ... and `var_nand(5) ...
TEST_F(DefineExpansion20Test, MacroUsages) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);
  ASSERT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 2u);
  const hldb::PreprocMacroDefinitionCollection *const definitions = sourceFile->getPreprocMacroDefinitions();
  ASSERT_EQ(sizeOf(definitions), 1u);

  const std::vector<uint32_t> lines = {18, 19};
  const std::vector<std::string_view> actuals = {"2", "5"};
  for (size_t index = 0; index < lines.size(); ++index) {
    const hldb::PreprocMacroInstance *const usage = getUsageOnLine(lines[index]);
    EXPECT_NE(usage, nullptr) << "no usage of var_nand on line " << lines[index];
    if (usage == nullptr) continue;

    // `text_macro_identifier is var_nand, and the usage binds to its only
    // definition, made on line 16.
    EXPECT_EQ(usage->getName(), "var_nand") << "line " << lines[index];
    const hldb::Identifier *const nameObj = usage->getNameObj();
    EXPECT_NE(nameObj, nullptr) << "line " << lines[index];
    if (nameObj != nullptr) EXPECT_EQ(nameObj->getName(), "var_nand") << "line " << lines[index];
    EXPECT_EQ(usage->getPreprocMacroDefinition(), definitions->front()) << "line " << lines[index];

    // One actual argument for the one formal argument dly.
    const hldb::IdentifierCollection *const arguments = usage->getArguments();
    EXPECT_EQ(sizeOf(arguments), 1u) << "line " << lines[index];
    if (sizeOf(arguments) == 1) EXPECT_EQ(stripWhiteSpace(arguments->front()->getName()), actuals[index]);

    EXPECT_TRUE(endsWith(usage->getFile(), kSourceFileName)) << usage->getFile();
  }
}

// Lines 17-20: module top (); ... endmodule
TEST_F(DefineExpansion20Test, TopModule) {
  ASSERT_NE(m_design, nullptr);
  ASSERT_EQ(countFromSourceFile(m_design->getAllModules()), 1u);
  const hldb::Module *const top = getTopDefinition();
  ASSERT_NE(top, nullptr);

  EXPECT_EQ(top->getDefName(), "top");
  EXPECT_EQ(top->getStartLine(), 17u);
  EXPECT_EQ(top->getEndLine(), 20u);

  // "endmodule" carries no ": top" end label.
  EXPECT_EQ(top->getEndLabel(), "");

  // The only module items are the two gate instantiations: two primitives,
  // and no variable, process, continuous assignment or module instance.
  EXPECT_EQ(sizeOf(top->getPrimitives()), 2u);
  EXPECT_EQ(sizeOf(top->getVariables()), 0u);
  EXPECT_EQ(sizeOf(top->getProcesses()), 0u);
  EXPECT_EQ(sizeOf(top->getContAssigns()), 0u);
  EXPECT_EQ(sizeOf(top->getModules()), 0u);
}

// "module top ();" has one null port (37.14 details 9, 10 and 11).
TEST_F(DefineExpansion20Test, TopHasOneNullPort) {
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

// Both lines expand to a nand gate instantiation (28.3, 28.4): a vpiGate
// primitive with vpiPrimType vpiNandPrim, named g121 and g122.
TEST_F(DefineExpansion20Test, TopHasTwoNandGates) {
  const hldb::Module *const top = getTopDefinition();
  ASSERT_NE(top, nullptr);
  ASSERT_EQ(sizeOf(top->getPrimitives()), 2u);

  const std::vector<std::string_view> names = {"g121", "g122"};
  for (std::string_view name : names) {
    const hldb::Primitive *const gate = getGate(name);
    EXPECT_NE(gate, nullptr) << "no gate " << name;
    if (gate == nullptr) continue;
    EXPECT_NE(::any_cast<hldb::Gate>(gate), nullptr) << name << " is a gate, not a UDP or switch";
    EXPECT_EQ(gate->getVpiType(), static_cast<uint32_t>(vpiGate)) << name;
    EXPECT_EQ(gate->getPrimType(), vpiNandPrim) << name;
  }
}

// Line 18 becomes: nand #2 g121 (q21, n10, n11);
TEST_F(DefineExpansion20Test, GateG121) {
  const hldb::Primitive *const gate = getGate("g121");
  ASSERT_NE(gate, nullptr);

  // One delay, the actual argument 2 substituted for dly (22.5.1).
  const hldb::Constant *const delay = delayOf(gate);
  ASSERT_NE(delay, nullptr) << "g121 has the one delay #2";
  EXPECT_EQ(literalOf(delay), "2");

  expectNandTerminals(gate, "q21");
}

// Line 19 becomes: nand #5 g122 (q22, n10, n11);
TEST_F(DefineExpansion20Test, GateG122) {
  const hldb::Primitive *const gate = getGate("g122");
  ASSERT_NE(gate, nullptr);

  // One delay, the actual argument 5 substituted for dly (22.5.1).
  const hldb::Constant *const delay = delayOf(gate);
  ASSERT_NE(delay, nullptr) << "g122 has the one delay #5";
  EXPECT_EQ(literalOf(delay), "5");

  expectNandTerminals(gate, "q22");
}

// 37.35 detail 1: "vpiSize shall return the number of inputs." Each nand has
// the two inputs n10 and n11.
TEST_F(DefineExpansion20Test, GatesReportTwoInputs) {
  const std::vector<std::string_view> names = {"g121", "g122"};
  for (std::string_view name : names) {
    const hldb::Primitive *const gate = getGate(name);
    EXPECT_NE(gate, nullptr) << "no gate " << name;
    if (gate == nullptr) continue;
    const hldb::Any::vpi_property_value_t size = gate->getVpiPropertyValue(vpiSize);
    EXPECT_TRUE(std::holds_alternative<int64_t>(size)) << name;
    if (std::holds_alternative<int64_t>(size)) EXPECT_EQ(std::get<int64_t>(size), 2) << name;
  }
}

// 5.7.1: "Simple decimal numbers without the size and the base format shall
// be treated as signed integers", and an unsized number is at least 32 bits.
// vpiUIntConst marks an unsigned integer constant, so it would contradict the
// LRM for the delays 2 and 5.
TEST_F(DefineExpansion20Test, GateDelaysAreSignedIntegers) {
  const std::vector<std::string_view> names = {"g121", "g122"};
  for (std::string_view name : names) {
    const hldb::Primitive *const gate = getGate(name);
    EXPECT_NE(gate, nullptr) << "no gate " << name;
    if (gate == nullptr) continue;
    const hldb::Constant *const delay = delayOf(gate);
    EXPECT_NE(delay, nullptr) << name << " has one constant delay";
    if (delay == nullptr) continue;
    const int32_t constType = delay->getConstType();
    EXPECT_TRUE((constType == vpiDecConst) || (constType == vpiIntConst))
        << name << " delay " << literalOf(delay) << " has vpiConstType " << constType;
    EXPECT_GE(delay->getSize(), 32) << name << " delay " << literalOf(delay);
  }
}

// 6.10 and 22.8: q21, n10, n11 and q22 are implicit scalar wires. Before
// elaboration the model may not have created them yet; if it has, they must
// be exactly those four.
TEST_F(DefineExpansion20Test, ImplicitNetsBeforeElaboration) {
  const hldb::Module *const top = getTopDefinition();
  ASSERT_NE(top, nullptr);
  const size_t netCount = sizeOf(top->getNets());
  ASSERT_TRUE((netCount == 0) || (netCount == kImplicitNetNames.size())) << "net count: " << netCount;
  if (netCount != 0) expectImplicitWires(top->getNets());
}

// By interpretation of the LRM, not an explicit rule: Annex K defines
// vpiLineNo as the "line number where the object is used", and 22.13 and 20.10
// tie the line a tool reports to `__LINE__, the "current input line number",
// which while a usage is expanded is the usage's line. So g121, expanded on
// line 18, and g122, expanded on line 19, carry those lines, not line 16.
TEST_F(DefineExpansion20Test, ExpandedCodeCarriesTheUsageLine) {
  const std::vector<std::string_view> names = {"g121", "g122"};
  const std::vector<uint32_t> lines = {18, 19};
  for (size_t index = 0; index < names.size(); ++index) {
    const hldb::Primitive *const gate = getGate(names[index]);
    EXPECT_NE(gate, nullptr) << "no gate " << names[index];
    if (gate == nullptr) continue;
    EXPECT_EQ(gate->getStartLine(), lines[index]) << names[index];
  }
}

// top is the only module and is never instantiated, so nothing else is
// declared in the file, and top is the one top-level module (23.3.1).
TEST_F(DefineExpansion20Test, NoOtherDesignObjectsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllClasses()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);

  // The formal argument dly is macro text, not a declaration.
  EXPECT_EQ(countFromSourceFile(m_design->getParameters()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParamAssigns()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getVariables()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getTypedefs()), 0u);

  // A top-level module is implicitly instantiated once, under its own name
  // (23.3.1). The instance tree and name binding only exist after
  // elaboration: the instance holds the four implicit scalar wires (6.10),
  // and the gate terminals bind to them.
  if (m_design->getElaborated()) {
    const hldb::ModuleCollection *const topModules = m_design->getTopModules();
    ASSERT_EQ(countFromSourceFile(topModules), 1u);
    const hldb::Module *const topInstance = firstFromSourceFile(topModules);
    EXPECT_EQ(topInstance->getName(), "top");
    EXPECT_EQ(topInstance->getDefName(), "top");

    expectImplicitWires(topInstance->getNets());
    for (std::string_view name : kImplicitNetNames) {
      const hldb::Net *const net = findByName(topInstance->getNets(), name);
      if (net == nullptr) continue;
      const hldb::Any::vpi_property_value_t size = net->getVpiPropertyValue(vpiSize);
      EXPECT_TRUE(std::holds_alternative<int64_t>(size)) << name;
      if (std::holds_alternative<int64_t>(size)) EXPECT_EQ(std::get<int64_t>(size), 1) << name << " is scalar";
    }

    const hldb::Primitive *const g121 = findByName(topInstance->getPrimitives(), "g121");
    ASSERT_NE(g121, nullptr);
    ASSERT_EQ(sizeOf(g121->getPrimTerms()), 3u);
    const hldb::RefObj *const output = ::any_cast<hldb::RefObj>(g121->getPrimTerms()->front()->getExpr());
    ASSERT_NE(output, nullptr);
    EXPECT_EQ(output->getActual(), findByName(topInstance->getNets(), "q21"));
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
