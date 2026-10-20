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

// Test for tests/Google/chapter-22/22.6--ifdef-chained-nested.sv
//
// Lines 1-7 are one-line "//" license comments, lines 8-9 are blank, lines
// 10-15 are one block comment carrying the test metadata, and the
// SystemVerilog text is:
//
//   16: module test;
//   17: `ifdef first_block
//   18:   `ifndef second_nest
//   19:     initial $display("first_block is defined");
//   20:   `else
//   21:     initial $display("first_block and second_nest defined");
//   22:   `endif
//   23: `elsif second_block
//   24:   initial $display("second_block defined, first_block is not");
//   25: `else
//   26:   `ifndef last_result
//   27:     initial $display("first_block, second_block,", " last_result not defined.");
//   28:   `elsif real_last
//   29:     initial $display("first_block, second_block not defined,", " last_result and real_last defined.");
//   30:   `else
//   31:     initial $display("Only last_result defined!");
//   32:   `endif
//   33: `endif
//   34: endmodule
//
// This is Example 3 of IEEE 1800-2023 22.6, "usage of chained nested
// conditional compilation directives", with the line 27 and 29 calls each
// written on one line instead of two.
//
// Lines 17-33 are one conditional_compilation_directive (Syntax 22-5):
// `ifdef first_block, `elsif second_block, `else, `endif. Lines 18-22 and
// 26-32 are inner constructs nested in its first and last blocks of text:
// `ifndef second_nest, `else, `endif and `ifndef last_result, `elsif
// real_last, `else, `endif. 22.6: "Nesting of conditional compilation
// constructs shall be permitted", and "Until the inner conditional
// compilation construct is terminated with `endif, all compiler directives
// apply only to the inner construct and shall not terminate the
// block_of_text in the outer construct." The directives "may appear anywhere
// in the source description", here inside a module.
//
// None of first_block, second_nest, second_block, last_result and real_last
// is defined: the file holds no `define, and the run is assumed to define no
// macro on the command line. So, by 22.6:
//   - `ifdef first_block (line 17) does not succeed, and its block of text,
//     lines 18-22 with the whole inner construct, is ignored. The `else of
//     line 20 belongs to that inner construct and does not end the block.
//   - `elsif second_block (line 23) does not succeed, and line 24 is ignored.
//   - "If there is an `else compiler directive and none of the previous
//     evaluations succeeded, the `else block_of_text ... is processed", so
//     lines 26-32 are processed.
//   - "The `ifndef text_macro_identifier directive is treated as `ifdef
//     (!text_macro_identifier)". last_result is not defined, so `ifndef
//     last_result (line 26) succeeds and line 27 is processed; "if there are
//     subsequent `elsif or `else directives before the next `endif, these
//     directives and their blocks of text are ignored", so lines 29 and 31
//     are ignored.
// The module therefore compiles as:
//
//   module test;
//     initial $display("first_block, second_block,", " last_result not defined.");
//   endmodule
//
// The names after `ifdef, `ifndef and `elsif are tested, not substituted:
// none of them is a macro usage.
//
// "module test;" has no port list at all (a module_ansi_header without a
// list_of_port_declarations), so test has no port. Its one module item is an
// initial procedure (9.2.1) whose statement is a call of the built-in system
// task $display (21.2.1), a vpiSysTaskCall that is not user-defined (37.42),
// with exactly two arguments, the string literals "first_block,
// second_block," and " last_result not defined." (5.9), each a vpiConstant
// with vpiConstType vpiStringConst (37.42 detail 8 example).
//
// test is never instantiated, so it is a top-level module, implicitly
// instantiated once under its own name (23.3.1).
//
// Every assertion below follows from the LRM; a red run means HLC deviates
// from it. Some checks read an LRM fact through a code: an empty generic
// directive list says the file holds none of the directives HLDB records
// that way (`timescale, `default_nettype, `celldefine and the others with a
// vpiDirectiveType code); and vpiInitial, vpiSysTaskCall and vpiStringConst
// are codes of the LRM's own VPI object model (Clause 37, Annex K).
//
// What is checked
//   - No fatal, syntax or error diagnostic, and none of the preprocessor
//     diagnostics a wrong reading of the nested conditional directives or of
//     the string literals in the ignored blocks would produce; in particular
//     no "unknown macro" for the tested, undefined names.
//   - Exactly one SourceFile for the .sv, with no includes, no generic
//     directives, no macro definitions and no macro instances.
//   - Exactly one module, test, on lines 16-34, with no port and exactly one
//     process, and no net, variable, continuous assignment, primitive or
//     module instance.
//   - That process is the initial procedure of line 27, the only processed
//     block of text: its statement is a $display system task call, not
//     user-defined, on line 27, with exactly two arguments. None of the
//     initial procedures of the ignored lines 19, 21, 24, 29 and 31 exists.
//   - Both arguments are string constants, with the texts "first_block,
//     second_block," and " last_result not defined.", the second with its
//     leading space.
//   - No other design object is created from this file.
//   - After elaboration only: one top-level instance named test, also with
//     exactly one process.
//
// What is NOT checked, and why
//   - The PreprocMacroCondition objects of the eleven conditional directives.
//     The headers expose no collection that holds them, so reading them would
//     guess a tool convention. The effect of each condition is checked where
//     it lives instead. Nearest assertions: OnlyLine27IsCompiled and
//     DisplayArgumentTexts.
//   - A run that defines any of the five tested names on the command line.
//     The fixture defines none, and the run is assumed not to; with one
//     defined, another block would be compiled. Nearest assertion:
//     OnlyLine27IsCompiled.
//   - What $display prints, and the newline after it. That output only
//     exists while simulation time advances, so it is permanently out of
//     scope. Nearest assertion: DisplayArgumentTexts, which fixes the
//     arguments it prints from.
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
//     convention. Nearest assertions: the line checks in TestModule and
//     OnlyLine27IsCompiled.
//   - Columns. The LRM numbers lines (22.13) but not columns. Nearest
//     assertions: the same line checks.
//   - The SourceFile time unit and precision. There is no `timescale or
//     timeunit, so the default applies, and the default is
//     implementation-specific (3.14.2.3). Nearest assertion:
//     SourceFileHasNoIncludesDirectivesOrMacros (zero directives).
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
#include <hldb/initial.h>
#include <hldb/interface.h>
#include <hldb/module.h>
#include <hldb/named_argument.h>
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
constexpr std::string_view kSourceFileName = "22.6--ifdef-chained-nested.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
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

// The literal of 'constant' as written: its vpiDecompile text (37.59), or,
// when that is empty, its value text after any "<format>:" prefix.
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

class IfdefChainedNestedTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.6--ifdef-chained-nested.hlc"}); }
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

  // The statement of test's only process, or nullptr unless test has exactly one process.
  static const hldb::Any *getInitialStatement() {
    const hldb::Module *const test = getTestDefinition();
    if ((test == nullptr) || (sizeOf(test->getProcesses()) != 1)) return nullptr;
    return test->getProcesses()->front()->getStmt();
  }

  // The $display call made by test's only process, or nullptr.
  static const hldb::TFCall *getDisplayCall() { return ::any_cast<hldb::TFCall>(getInitialStatement()); }

  // The 'index'-th argument of the $display call as a constant, or nullptr
  // unless the call has exactly two arguments and that one is a constant.
  static const hldb::Constant *getDisplayArgument(size_t index) {
    const hldb::TFCall *const call = getDisplayCall();
    if ((call == nullptr) || (sizeOf(call->getArguments()) != 2)) return nullptr;
    return call->getArguments()->at(index)->getHighConn<hldb::Constant>();
  }
};

// The module, the nested conditional directives (Syntax 22-5) and the one
// processed initial procedure (9.2.1, 21.2.1) are legal, and the ignored
// blocks of text "still follow the SystemVerilog lexical conventions" (22.6),
// so nothing is fatal, a syntax error or an error.
TEST_F(IfdefChainedNestedTest, CompilesWithoutErrors) {
  ASSERT_NE(m_session, nullptr);
  const ErrorContainer *const errorContainer = m_session->getErrorContainer();
  ASSERT_NE(errorContainer, nullptr);

  const ErrorContainer::Stats stats = errorContainer->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0) << describeErrors(errorContainer, ErrorDefinition::FATAL);
  EXPECT_EQ(stats.nbSyntax, 0) << describeErrors(errorContainer, ErrorDefinition::SYNTAX);
  EXPECT_EQ(stats.nbError, 0) << describeErrors(errorContainer, ErrorDefinition::ERROR);
}

// Each of these diagnostics would mean HLC misread the conditional directives
// or a string literal in a block of text.
TEST_F(IfdefChainedNestedTest, ReportsNoPreprocessorDiagnostics) {
  // Lines 17-33 and the two nested constructs match Syntax 22-5; each `elsif
  // follows an `ifdef or `ifndef of its own construct and precedes its `else,
  // and is spelled `elsif.
  EXPECT_EQ(findError(ErrorDefinition::PP_SYNTAX_ERROR), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_SYNTAX_ERROR), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_ELSEIF), nullptr);

  // `ifdef, `ifndef and `elsif test whether a name is defined (22.6); the
  // five names are not macro usages, so their being undefined is no
  // "unknown macro".
  EXPECT_EQ(findError(ErrorDefinition::PP_UNKOWN_MACRO), nullptr);

  // The conditional directives "may appear anywhere in the source
  // description" (22.6), here inside a module.
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_IN_DESIGN_ELEMENT), nullptr);

  // Every string literal, in the processed and in the ignored blocks, is
  // closed on its line and holds no escape sequence (5.9).
  EXPECT_EQ(findError(ErrorDefinition::PP_UNTERMINATED_STRING), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_UNESCAPED_CHARACTER_IN_STRING), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_UNRECOGNIZED_ESCAPED_SEQUENCE), nullptr);

  // The file is plain ASCII.
  EXPECT_EQ(findError(ErrorDefinition::PP_NON_ASCII_CONTENT), nullptr);
}

TEST_F(IfdefChainedNestedTest, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

TEST_F(IfdefChainedNestedTest, SourceFileHasNoIncludesDirectivesOrMacros) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  // There is no `include in the file (22.4).
  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);

  // `ifdef, `ifndef, `elsif, `else and `endif are the only directives in the
  // file. None of the directives HLDB records as generic directives (those
  // with a vpiDirectiveType code: `timescale, `default_nettype, `celldefine,
  // ...) occurs, and the conditional directives have no vpiDirectiveType
  // code, so a generic directive here would be one the source does not
  // contain.
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);

  // The file defines no macro, and the names after `ifdef, `ifndef and
  // `elsif are tested, not used (22.6), so there is no macro definition and
  // no macro instance.
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// Lines 16-34: module test; ... endmodule. "module test;" has no port list,
// so test has no port. Of the five initial procedures written, only the one
// of line 27 is processed, so test holds exactly one process and nothing
// else.
TEST_F(IfdefChainedNestedTest, TestModule) {
  ASSERT_NE(m_design, nullptr);
  ASSERT_EQ(countFromSourceFile(m_design->getAllModules()), 1u);
  const hldb::Module *const test = getTestDefinition();
  ASSERT_NE(test, nullptr);

  EXPECT_EQ(test->getDefName(), "test");
  EXPECT_EQ(test->getStartLine(), 16u);
  EXPECT_EQ(test->getEndLine(), 34u);

  // "endmodule" carries no ": test" end label.
  EXPECT_EQ(test->getEndLabel(), "");

  EXPECT_EQ(sizeOf(test->getPorts()), 0u) << "\"module test;\" has no port list";
  EXPECT_EQ(sizeOf(test->getProcesses()), 1u) << "only the block of text of `ifndef last_result is processed";
  EXPECT_EQ(sizeOf(test->getNets()), 0u);
  EXPECT_EQ(sizeOf(test->getVariables()), 0u);
  EXPECT_EQ(sizeOf(test->getContAssigns()), 0u);
  EXPECT_EQ(sizeOf(test->getPrimitives()), 0u);
  EXPECT_EQ(sizeOf(test->getModules()), 0u);
}

// 22.6, with none of the five tested names defined: line 17 fails and lines
// 18-22 are ignored (the `else of line 20 only ends a block of the nested
// construct, not the outer one); line 23 fails and line 24 is ignored; the
// `else of line 25 is processed; within it, `ifndef last_result succeeds,
// so line 27 is processed and lines 29 and 31 are ignored. The one process
// is therefore the initial procedure of line 27 (9.2.1), whose statement is
// the $display call itself. $display is a built-in system task (21.2.1), so
// the call is a vpiSysTaskCall whose vpiUserDefn is false (37.42); it has
// the two arguments written on line 27.
TEST_F(IfdefChainedNestedTest, OnlyLine27IsCompiled) {
  const hldb::Module *const test = getTestDefinition();
  ASSERT_NE(test, nullptr);
  const hldb::ProcessCollection *const processes = test->getProcesses();
  ASSERT_EQ(sizeOf(processes), 1u);
  const hldb::Process *const process = processes->front();
  EXPECT_EQ(process->getVpiType(), static_cast<uint32_t>(vpiInitial));
  EXPECT_EQ(process->getStartLine(), 27u) << "lines 19, 21, 24, 29 and 31 are in ignored blocks of text";

  const hldb::Any *const stmt = getInitialStatement();
  ASSERT_NE(stmt, nullptr);
  EXPECT_EQ(stmt->getVpiType(), static_cast<uint32_t>(vpiSysTaskCall));

  const hldb::TFCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "$display");
  EXPECT_EQ(call->getStartLine(), 27u);
  EXPECT_EQ(sizeOf(call->getArguments()), 2u);

  const hldb::SysTaskCall *const taskCall = ::any_cast<hldb::SysTaskCall>(stmt);
  ASSERT_NE(taskCall, nullptr);
  EXPECT_FALSE(taskCall->getUserDefn());
}

// Each $display argument is a string literal (5.9), a vpiConstant with
// vpiConstType vpiStringConst in the VPI object model (37.42 detail 8
// example).
TEST_F(IfdefChainedNestedTest, DisplayArgumentsAreStringConstants) {
  for (size_t index = 0; index < 2; ++index) {
    const hldb::Constant *const argument = getDisplayArgument(index);
    EXPECT_NE(argument, nullptr) << "argument " << (index + 1) << " is a constant";
    if (argument != nullptr) EXPECT_EQ(argument->getConstType(), vpiStringConst) << "argument " << (index + 1);
  }
}

// Line 27: $display("first_block, second_block,", " last_result not
// defined."); these two texts appear in no other block of text, so they also
// tell the processed block apart from the ignored ones. The second keeps its
// leading space: it is inside the string literal (5.9).
TEST_F(IfdefChainedNestedTest, DisplayArgumentTexts) {
  const hldb::Constant *const first = getDisplayArgument(0);
  ASSERT_NE(first, nullptr) << "argument 1 is a constant";
  EXPECT_EQ(unquote(literalOf(first)), "first_block, second_block,");

  const hldb::Constant *const second = getDisplayArgument(1);
  ASSERT_NE(second, nullptr) << "argument 2 is a constant";
  EXPECT_EQ(unquote(literalOf(second)), " last_result not defined.");
}

// test is the only module and is never instantiated, so nothing else is
// declared in the file, and test is the one top-level module (23.3.1).
TEST_F(IfdefChainedNestedTest, NoOtherDesignObjectsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllClasses()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);

  // The five tested names are not declarations.
  EXPECT_EQ(countFromSourceFile(m_design->getParameters()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParamAssigns()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getVariables()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getTypedefs()), 0u);

  // A top-level module is implicitly instantiated once, under its own name
  // (23.3.1). The instance tree only exists after elaboration; the instance
  // holds the one processed initial procedure and no net.
  if (m_design->getElaborated()) {
    const hldb::ModuleCollection *const topModules = m_design->getTopModules();
    ASSERT_EQ(countFromSourceFile(topModules), 1u);
    const hldb::Module *const topInstance = firstFromSourceFile(topModules);
    EXPECT_EQ(topInstance->getName(), "test");
    EXPECT_EQ(topInstance->getDefName(), "test");
    EXPECT_EQ(sizeOf(topInstance->getProcesses()), 1u);
    EXPECT_EQ(sizeOf(topInstance->getNets()), 0u);
  }
}
}  // namespace hlc

#ifndef DAVTESTS_BULK_BUILD
int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
