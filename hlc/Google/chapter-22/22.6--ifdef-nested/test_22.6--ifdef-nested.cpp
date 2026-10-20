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

// Test for tests/Google/chapter-22/22.6--ifdef-nested.sv
//
// Lines 1-7 are one-line "//" license comments, lines 8-9 are blank, lines
// 10-15 are one block comment carrying the test metadata, and the
// SystemVerilog text is:
//
//   16: module test(out);
//   17: output out;
//   18: `define wow
//   19: `define nest_one
//   20: `define second_nest
//   21: `define nest_two
//   22: `ifdef wow
//   23:   initial $display("wow is defined");
//   24:   `ifdef nest_one
//   25:     initial $display("nest_one is defined");
//   26:     `ifdef nest_two
//   27:       initial $display("nest_two is defined");
//   28:     `else
//   29:       initial $display("nest_two is not defined");
//   30:     `endif
//   31:   `else
//   32:     initial $display("nest_one is not defined");
//   33:   `endif
//   34: `else
//   35:   initial $display("wow is not defined");
//   36:   `ifdef second_nest
//   37:     initial $display("second_nest is defined");
//   38:   `else
//   39:     initial $display("second_nest is not defined");
//   40:   `endif
//   41: `endif
//   42: endmodule
//
// This is Example 2 of IEEE 1800-2023 22.6, "usage of nested conditional
// compilation directives".
//
// Lines 18-21 are text_macro_definitions (Syntax 22-2) of wow, nest_one,
// second_nest and nest_two, each without formal arguments and with blank
// macro text: "The macro text can be blank, in which case the text macro is
// defined to be empty" (22.5.1). `define "can be used both inside and
// outside design elements" (22.5.1), here inside module test.
//
// Lines 22-41 are one conditional_compilation_directive (Syntax 22-5),
// `ifdef wow, `else, `endif, with three constructs nested in it: `ifdef
// nest_one (lines 24-33) in its first block of text, `ifdef nest_two (lines
// 26-30) in that one's first block, and `ifdef second_nest (lines 36-40) in
// the outer `else block. 22.6: "Nesting of conditional compilation
// constructs shall be permitted", and "Until the inner conditional
// compilation construct is terminated with `endif, all compiler directives
// apply only to the inner construct and shall not terminate the
// block_of_text in the outer construct."
//
// All four names are defined before line 22, so by 22.6 ("it succeeds if
// that identifier is currently defined as a text macro name"; "If the
// evaluation succeeds, the associated block_of_text ... is processed ...,
// and if there are subsequent `elsif or `else directives before the next
// `endif, these directives and their blocks of text are ignored"):
//   - `ifdef wow (line 22) succeeds: lines 23-33 are processed and the `else
//     block, lines 35-40 with the whole second_nest construct, is ignored.
//   - `ifdef nest_one (line 24) succeeds: lines 25-30 are processed and line
//     32 is ignored.
//   - `ifdef nest_two (line 26) succeeds: line 27 is processed and line 29 is
//     ignored.
// second_nest is defined too, but its construct lies in an ignored block, so
// neither line 37 nor line 39 is processed. The module therefore compiles
// as:
//
//   module test(out);
//     output out;
//     initial $display("wow is defined");
//     initial $display("nest_one is defined");
//     initial $display("nest_two is defined");
//   endmodule
//
// The names after `ifdef are tested, not substituted: none of them is a
// macro usage.
//
// test has a non-ANSI port list (23.2.2.1) of one port, out (vpiPortIndex 0,
// 37.14 detail 9), declared output. "An implicit data type declaration
// implies a net unless the var keyword is used" (23.2.2.3), and with no
// `default_nettype the default net type is wire (22.8), so out is a wire
// net.
//
// Each of the three processed module items is an initial procedure (9.2.1)
// whose statement is a call of the built-in system task $display (21.2.1), a
// vpiSysTaskCall that is not user-defined (37.42), with one string literal
// argument (5.9), a vpiConstant with vpiConstType vpiStringConst (37.42
// detail 8 example).
//
// test is never instantiated, so it is a top-level module, implicitly
// instantiated once under its own name (23.3.1).
//
// Every assertion below follows from the LRM; a red run means HLC deviates
// from it. Some checks read an LRM fact through a code: vpiPMDDefine says
// lines 18-21 are `define (not `undef, 22.5.2, or `undefineall, 22.5.3); an
// empty generic directive list says the file holds none of the directives
// HLDB records that way (`timescale, `default_nettype, `celldefine and the
// others with a vpiDirectiveType code); and vpiOutput, vpiWire, vpiInitial,
// vpiSysTaskCall and vpiStringConst are codes of the LRM's own VPI object
// model (Clause 37, Annex K).
//
// What is checked
//   - No fatal, syntax or error diagnostic, and none of the preprocessor
//     diagnostics a wrong reading of the definitions, of the nested
//     conditional directives or of the string literals in the ignored blocks
//     would produce; in particular no "directive in design element"
//     diagnostic for the `define lines inside test.
//   - Exactly one SourceFile for the .sv, with no includes, no generic
//     directives and no macro instances.
//   - Exactly four PreprocMacroDefinitions, all vpiPMDDefine: wow, nest_one,
//     second_nest and nest_two on lines 18, 19, 20 and 21, each without
//     formal arguments and with blank macro text.
//   - Exactly one module, test, on lines 16-42, with the one port out
//     (output, index 0) and exactly three processes, and no variable,
//     continuous assignment, primitive or module instance.
//   - The three processes are the initial procedures of lines 23, 25 and 27,
//     each a $display system task call, not user-defined, on its line, with
//     one argument; none of the initial procedures of the ignored lines 29,
//     32, 35, 37 and 39 exists.
//   - The three arguments are string constants with the texts "wow is
//     defined", "nest_one is defined" and "nest_two is defined".
//   - Before elaboration, test holds either no net yet or exactly the wire
//     net out; after elaboration, the top-level instance test holds exactly
//     that net and the three processes.
//   - No other design object is created from this file.
//
// What is NOT checked, and why
//   - The PreprocMacroCondition objects of the twelve conditional
//     directives. The headers expose no collection that holds them, so
//     reading them would guess a tool convention. The effect of each
//     condition is checked where it lives instead. Nearest assertions:
//     ProcessedBlocksAreCompiled, IgnoredBlocksAreNotCompiled and
//     DisplayArgumentTexts.
//   - Whether HLC warns that a macro is defined but never used. The LRM
//     neither requires nor forbids that. Nearest assertion:
//     CompilesWithoutErrors.
//   - What the three $display calls print, and in which order. Printing only
//     exists while simulation time advances, and "There shall be no implied
//     order of execution between initial and always procedures" (9.2), so
//     this is permanently out of scope. Nearest assertion:
//     DisplayArgumentTexts, which fixes the arguments they print from.
//   - The order of the three processes in HLDB's process list. The LRM gives
//     module items no order in the model, so each process is found by its
//     line. Nearest assertion: ProcessedBlocksAreCompiled.
//   - When HLDB records the net declared by the port declaration. 23.2.2.3
//     says it is a net, not at which stage a tool records it, so before
//     elaboration "none yet" is accepted besides the one wire. Nearest
//     assertions: PortNetBeforeElaboration and the elaborated part of
//     NoOtherDesignObjectsDeclared.
//   - Which object the port's low connection refers to. The port name
//     already fixes the connection; the object behind it is HLDB's
//     representation, which the LRM does not fix before elaboration. Nearest
//     assertion: the port name in TestModule.
//   - How HLDB splits the macro text into tokens and whether it keeps white
//     space, including a CR from a CR LF line ending, as tokens. The LRM
//     defines the macro text, not its storage, and never defines CR LF as
//     one newline (5.3), so the text is compared with white space removed.
//     Nearest assertion: the blank macro text check in
//     FourEmptyMacroDefinitions.
//   - The end line of each definition. The LRM gives a macro definition no
//     end line: Annex K lists vpiEndLine, but Clause 37 uses it only for
//     assertion locations (37.49), and macro definitions are not in the VPI
//     object model at all. Where HLDB ends the span of a definition with
//     blank macro text, on its own line or at the start of the next one, is
//     an HLDB model convention, not an LRM rule. Nearest assertion: the
//     start line check in FourEmptyMacroDefinitions.
//   - How a string literal's value is stored: with or without its double
//     quotes. Both denote the same string (5.9). Nearest assertion:
//     DisplayArgumentTexts, which compares the unquoted text.
//   - The sizes of the string constants. 5.9 gives a string literal 8 bits
//     per character only when it is used as an operand in an expression,
//     which a $display argument is not required to be. Nearest assertion:
//     DisplayArgumentsAreStringConstants.
//   - The seven one-line comments and the block comment (5.4). The LRM gives
//     comments no meaning; how HLDB groups, classifies (vpiComment and
//     vpiDocumentComment are marked non-standard) and attaches them is a tool
//     convention. Nearest assertions: the line checks of the definitions,
//     the module and the processes.
//   - Columns. The LRM numbers lines (22.13) but not columns. Nearest
//     assertions: the same line checks.
//   - The SourceFile time unit and precision. There is no `timescale or
//     timeunit, so the default applies, and the default is
//     implementation-specific (3.14.2.3). Nearest assertion:
//     SourceFileHasNoIncludesDirectivesOrMacroUses (zero directives).
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
#include <hldb/named_argument.h>
#include <hldb/net.h>
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
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "22.6--ifdef-nested.sv";

// LRM 5.3 white space: spaces, tabs, newlines (CR LF on a CRLF checkout) and formfeeds.
constexpr std::string_view kWhiteSpace = " \t\r\n\f";

// A processed initial procedure: its line and the text of its one $display argument.
struct ProcessedDisplay {
  uint32_t m_line;
  std::string_view m_text;
};

// The three blocks of text with an initial procedure that 22.6 processes.
const std::vector<ProcessedDisplay> kProcessedDisplays = {
    {23, "wow is defined"}, {25, "nest_one is defined"}, {27, "nest_two is defined"}};

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

// Number of objects in 'collection' named 'name'.
template <typename T>
size_t countByName(const std::vector<T *> *collection, std::string_view name) {
  size_t count = 0;
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (object->getName() == name) ++count;
    }
  }
  return count;
}

// Number of objects in 'collection' that start on 'line'.
template <typename T>
size_t countOnLine(const std::vector<T *> *collection, uint32_t line) {
  size_t count = 0;
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (object->getStartLine() == line) ++count;
    }
  }
  return count;
}

// The object in 'collection' that starts on 'line', or nullptr unless there is exactly one.
template <typename T>
const T *findOnLine(const std::vector<T *> *collection, uint32_t line) {
  if (countOnLine(collection, line) != 1) return nullptr;
  for (const T *object : *collection) {
    if (object->getStartLine() == line) return object;
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

// The literal of 'constant' as written: its vpiDecompile text (37.59), or,
// when that is empty, its value text after any "<format>:" prefix.
std::string_view literalOf(const hldb::Constant *constant) {
  const std::string_view decompile = constant->getDecompile();
  if (!decompile.empty()) return decompile;
  const std::string_view value = constant->getValue();
  const size_t colon = value.find(':');
  return (colon == std::string_view::npos) ? value : value.substr(colon + 1);
}

// Checks that 'nets' holds exactly the wire net out (23.2.2.3, 22.8).
void expectOutWire(const hldb::NetCollection *nets) {
  ASSERT_EQ(sizeOf(nets), 1u);
  const hldb::Net *const out = nets->front();
  EXPECT_EQ(out->getName(), "out");
  EXPECT_EQ(out->getNetType(), vpiWire) << "out has the default net type wire (22.8)";
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

class IfdefNestedTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.6--ifdef-nested.hlc"}); }
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

  // The module definition of test from the file under test, or nullptr unless there is exactly one.
  static const hldb::Module *getTestDefinition() {
    if ((m_design == nullptr) || (countFromSourceFile(m_design->getAllModules()) != 1)) return nullptr;
    return firstFromSourceFile(m_design->getAllModules());
  }

  // The $display call of the process of test that starts on 'line', or nullptr.
  static const hldb::TFCall *getDisplayCall(uint32_t line) {
    const hldb::Module *const test = getTestDefinition();
    if (test == nullptr) return nullptr;
    const hldb::Process *const process = findOnLine(test->getProcesses(), line);
    return (process == nullptr) ? nullptr : ::any_cast<hldb::TFCall>(process->getStmt());
  }

  // The only argument of the $display call on 'line' as a constant, or
  // nullptr unless the call has exactly one argument and it is a constant.
  static const hldb::Constant *getDisplayArgument(uint32_t line) {
    const hldb::TFCall *const call = getDisplayCall(line);
    if ((call == nullptr) || (sizeOf(call->getArguments()) != 1)) return nullptr;
    return call->getArguments()->front()->getHighConn<hldb::Constant>();
  }
};

// The module, its port declaration, the four definitions (22.5.1), the
// nested conditional directives (Syntax 22-5) and the three processed
// initial procedures (9.2.1, 21.2.1) are legal, and the ignored blocks of
// text "still follow the SystemVerilog lexical conventions" (22.6), so
// nothing is fatal, a syntax error or an error.
TEST_F(IfdefNestedTest, CompilesWithoutErrors) {
  ASSERT_NE(m_session, nullptr);
  const ErrorContainer *const errorContainer = m_session->getErrorContainer();
  ASSERT_NE(errorContainer, nullptr);

  const ErrorContainer::Stats stats = errorContainer->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0) << describeErrors(errorContainer, ErrorDefinition::FATAL);
  EXPECT_EQ(stats.nbSyntax, 0) << describeErrors(errorContainer, ErrorDefinition::SYNTAX);
  EXPECT_EQ(stats.nbError, 0) << describeErrors(errorContainer, ErrorDefinition::ERROR);
}

// Each of these diagnostics would mean HLC misread a definition, the
// conditional directives or a string literal in a block of text.
TEST_F(IfdefNestedTest, ReportsNoPreprocessorDiagnostics) {
  // Lines 18-21 match Syntax 22-2, and lines 22-41 with their nested
  // constructs match Syntax 22-5.
  EXPECT_EQ(findError(ErrorDefinition::PP_SYNTAX_ERROR), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_SYNTAX_ERROR), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_ELSEIF), nullptr);

  // `define "can be used both inside and outside design elements" (22.5.1),
  // and the conditional directives "may appear anywhere in the source
  // description" (22.6).
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_IN_DESIGN_ELEMENT), nullptr);

  // Each name is defined once, and none is a compiler directive name (22.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_MULTIPLY_DEFINED_MACRO), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_NAME_RESERVED), nullptr);

  // `ifdef tests whether a name is defined (22.6); the names after it are
  // not macro usages, and all four are defined anyway.
  EXPECT_EQ(findError(ErrorDefinition::PP_UNKOWN_MACRO), nullptr);

  // Every string literal, in the processed and in the ignored blocks, is
  // closed on its line and holds no escape sequence (5.9).
  EXPECT_EQ(findError(ErrorDefinition::PP_UNTERMINATED_STRING), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_UNESCAPED_CHARACTER_IN_STRING), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_UNRECOGNIZED_ESCAPED_SEQUENCE), nullptr);

  // The file is plain ASCII.
  EXPECT_EQ(findError(ErrorDefinition::PP_NON_ASCII_CONTENT), nullptr);
}

TEST_F(IfdefNestedTest, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

TEST_F(IfdefNestedTest, SourceFileHasNoIncludesDirectivesOrMacroUses) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  // There is no `include in the file (22.4).
  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);

  // `define, `ifdef, `else and `endif are the only directives in the file.
  // None of the directives HLDB records as generic directives (those with a
  // vpiDirectiveType code: `timescale, `default_nettype, `celldefine, ...)
  // occurs, and these four have no vpiDirectiveType code, so a generic
  // directive here would be one the source does not contain.
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);

  // A macro is substituted only where `text_macro_identifier appears
  // (22.5.1); no `wow, `nest_one, `second_nest or `nest_two appears. The
  // names after `ifdef are tested, not used (22.6).
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// Lines 18-21: `define wow, `define nest_one, `define second_nest and
// `define nest_two. No "(" follows a name, so none has formal arguments, and
// each macro text is blank, so each macro "is defined to be empty" (22.5.1).
TEST_F(IfdefNestedTest, FourEmptyMacroDefinitions) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);
  const hldb::PreprocMacroDefinitionCollection *const definitions = sourceFile->getPreprocMacroDefinitions();
  ASSERT_EQ(sizeOf(definitions), 4u);

  const std::vector<std::string_view> names = {"wow", "nest_one", "second_nest", "nest_two"};
  const std::vector<uint32_t> lines = {18, 19, 20, 21};
  for (size_t index = 0; index < names.size(); ++index) {
    EXPECT_EQ(countByName(definitions, names[index]), 1u) << names[index] << " is defined once";
    const hldb::PreprocMacroDefinition *const definition = findByName(definitions, names[index]);
    if (definition == nullptr) continue;

    // Each line is a `define (22.5.1), not an `undef (22.5.2) or an
    // `undefineall (22.5.3).
    EXPECT_EQ(definition->getType(), vpiPMDDefine) << names[index];

    const hldb::Identifier *const nameObj = definition->getNameObj();
    EXPECT_NE(nameObj, nullptr) << names[index];
    if (nameObj != nullptr) {
      EXPECT_EQ(nameObj->getName(), names[index]);
      EXPECT_EQ(nameObj->getStartLine(), lines[index]) << names[index];
    }

    EXPECT_EQ(sizeOf(definition->getArguments()), 0u) << names[index];
    EXPECT_EQ(strippedMacroText(definition), "") << names[index] << " has blank macro text";

    // The `define directive starts on its line. Its end line is not checked;
    // see the header.
    EXPECT_EQ(definition->getStartLine(), lines[index]) << names[index];
  }
}

// Lines 16-42: module test(out); output out; ... endmodule. The non-ANSI
// port list holds the one port out (23.2.2.1), at vpiPortIndex 0 (37.14
// detail 9), declared output. Of the eight initial procedures written, the
// three of lines 23, 25 and 27 are processed, so test holds exactly three
// processes and no other module item.
TEST_F(IfdefNestedTest, TestModule) {
  ASSERT_NE(m_design, nullptr);
  ASSERT_EQ(countFromSourceFile(m_design->getAllModules()), 1u);
  const hldb::Module *const test = getTestDefinition();
  ASSERT_NE(test, nullptr);

  EXPECT_EQ(test->getDefName(), "test");
  EXPECT_EQ(test->getStartLine(), 16u);
  EXPECT_EQ(test->getEndLine(), 42u);

  // "endmodule" carries no ": test" end label.
  EXPECT_EQ(test->getEndLabel(), "");

  const hldb::PortCollection *const ports = test->getPorts();
  ASSERT_EQ(sizeOf(ports), 1u);
  const hldb::Port *const out = ports->front();
  EXPECT_EQ(out->getName(), "out");
  EXPECT_EQ(out->getPortIndex(), 0);
  EXPECT_EQ(out->getDirection(), vpiOutput);

  EXPECT_EQ(sizeOf(test->getProcesses()), 3u) << "the processed blocks hold the initial procedures of lines 23, 25, 27";
  EXPECT_EQ(sizeOf(test->getVariables()), 0u);
  EXPECT_EQ(sizeOf(test->getContAssigns()), 0u);
  EXPECT_EQ(sizeOf(test->getPrimitives()), 0u);
  EXPECT_EQ(sizeOf(test->getModules()), 0u);
}

// 22.6: wow, nest_one and nest_two are defined, so the first blocks of text
// of `ifdef wow (line 22), `ifdef nest_one (line 24) and `ifdef nest_two
// (line 26) are processed, with the initial procedures of lines 23, 25 and
// 27 (9.2.1). Each statement is the $display call itself; $display is a
// built-in system task (21.2.1), so each call is a vpiSysTaskCall whose
// vpiUserDefn is false (37.42), with the one argument written on its line.
TEST_F(IfdefNestedTest, ProcessedBlocksAreCompiled) {
  const hldb::Module *const test = getTestDefinition();
  ASSERT_NE(test, nullptr);
  const hldb::ProcessCollection *const processes = test->getProcesses();
  ASSERT_EQ(sizeOf(processes), 3u);

  for (const ProcessedDisplay &expected : kProcessedDisplays) {
    const hldb::Process *const process = findOnLine(processes, expected.m_line);
    EXPECT_NE(process, nullptr) << "one initial procedure on line " << expected.m_line;
    if (process == nullptr) continue;
    EXPECT_EQ(process->getVpiType(), static_cast<uint32_t>(vpiInitial)) << "line " << expected.m_line;

    const hldb::Any *const stmt = process->getStmt();
    EXPECT_NE(stmt, nullptr) << "line " << expected.m_line;
    if (stmt == nullptr) continue;
    EXPECT_EQ(stmt->getVpiType(), static_cast<uint32_t>(vpiSysTaskCall)) << "line " << expected.m_line;

    const hldb::TFCall *const call = getDisplayCall(expected.m_line);
    EXPECT_NE(call, nullptr) << "line " << expected.m_line;
    if (call == nullptr) continue;
    EXPECT_EQ(call->getName(), "$display") << "line " << expected.m_line;
    EXPECT_EQ(call->getStartLine(), expected.m_line);
    EXPECT_EQ(sizeOf(call->getArguments()), 1u) << "line " << expected.m_line;

    const hldb::SysTaskCall *const taskCall = ::any_cast<hldb::SysTaskCall>(stmt);
    ASSERT_NE(taskCall, nullptr);
    EXPECT_FALSE(taskCall->getUserDefn()) << "line " << expected.m_line;
  }
}

// 22.6: "If the evaluation succeeds, the associated block_of_text ... is
// processed ..., and if there are subsequent `elsif or `else directives
// before the next `endif, these directives and their blocks of text are
// ignored." No initial procedure of an ignored block exists.
TEST_F(IfdefNestedTest, IgnoredBlocksAreNotCompiled) {
  const hldb::Module *const test = getTestDefinition();
  ASSERT_NE(test, nullptr);
  const hldb::ProcessCollection *const processes = test->getProcesses();

  EXPECT_EQ(countOnLine(processes, 29), 0u) << "`else of `ifdef nest_two, which succeeded";
  EXPECT_EQ(countOnLine(processes, 32), 0u) << "`else of `ifdef nest_one, which succeeded";
  EXPECT_EQ(countOnLine(processes, 35), 0u) << "`else of `ifdef wow, which succeeded";

  // second_nest is defined, but its whole construct lies in the ignored
  // `else block of `ifdef wow, so neither of its blocks is processed.
  EXPECT_EQ(countOnLine(processes, 37), 0u) << "`ifdef second_nest inside an ignored block";
  EXPECT_EQ(countOnLine(processes, 39), 0u) << "`else of `ifdef second_nest inside an ignored block";
}

// Each $display argument is a string literal (5.9), a vpiConstant with
// vpiConstType vpiStringConst in the VPI object model (37.42 detail 8
// example).
TEST_F(IfdefNestedTest, DisplayArgumentsAreStringConstants) {
  for (const ProcessedDisplay &expected : kProcessedDisplays) {
    const hldb::Constant *const argument = getDisplayArgument(expected.m_line);
    EXPECT_NE(argument, nullptr) << "the call on line " << expected.m_line << " has one constant argument";
    if (argument != nullptr) EXPECT_EQ(argument->getConstType(), vpiStringConst) << "line " << expected.m_line;
  }
}

// Lines 23, 25 and 27 pass "wow is defined", "nest_one is defined" and
// "nest_two is defined"; these texts appear in no ignored block, so they
// also tell the processed blocks apart from the ignored ones.
TEST_F(IfdefNestedTest, DisplayArgumentTexts) {
  for (const ProcessedDisplay &expected : kProcessedDisplays) {
    const hldb::Constant *const argument = getDisplayArgument(expected.m_line);
    EXPECT_NE(argument, nullptr) << "the call on line " << expected.m_line << " has one constant argument";
    if (argument != nullptr) EXPECT_EQ(unquote(literalOf(argument)), expected.m_text) << "line " << expected.m_line;
  }
}

// 23.2.2.3 and 22.8: the port declaration "output out;" makes out a wire
// net. Before elaboration the model may not have recorded the net yet; if it
// has, it must be exactly that one.
TEST_F(IfdefNestedTest, PortNetBeforeElaboration) {
  const hldb::Module *const test = getTestDefinition();
  ASSERT_NE(test, nullptr);
  const size_t netCount = sizeOf(test->getNets());
  ASSERT_TRUE((netCount == 0) || (netCount == 1)) << "net count: " << netCount;
  if (netCount != 0) expectOutWire(test->getNets());
}

// test is the only module and is never instantiated, so nothing else is
// declared in the file, and test is the one top-level module (23.3.1).
TEST_F(IfdefNestedTest, NoOtherDesignObjectsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllClasses()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);

  // A text macro is preprocessor text only (22.5.1): wow, nest_one,
  // second_nest and nest_two are not parameters or variables.
  EXPECT_EQ(countFromSourceFile(m_design->getParameters()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParamAssigns()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getVariables()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getTypedefs()), 0u);

  // A top-level module is implicitly instantiated once, under its own name
  // (23.3.1). The instance tree only exists after elaboration; the instance
  // holds the three processed initial procedures and the wire net out.
  if (m_design->getElaborated()) {
    const hldb::ModuleCollection *const topModules = m_design->getTopModules();
    ASSERT_EQ(countFromSourceFile(topModules), 1u);
    const hldb::Module *const topInstance = firstFromSourceFile(topModules);
    EXPECT_EQ(topInstance->getName(), "test");
    EXPECT_EQ(topInstance->getDefName(), "test");
    EXPECT_EQ(sizeOf(topInstance->getProcesses()), 3u);
    expectOutWire(topInstance->getNets());
  }
}
}  // namespace hlc

#ifndef DAVTESTS_BULK_BUILD
int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
