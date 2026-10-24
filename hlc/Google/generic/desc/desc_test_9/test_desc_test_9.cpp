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

// Validates the HLDB model built for tests/Google/generic/desc/desc_test_9.sv.
// Lines 1-7 are one-line "//" license comments, lines 8-9 are blank, lines
// 10-15 are the block comment carrying the suite's :name:/:description:/
// :type:/:tags: metadata, and the rest is:
//
//   16  `ifdef FPGA
//   17  `ifdef DEBUGGER
//   18  module mymod;
//   19  endmodule
//   20  `endif
//   21  `endif
//
// Lines 16-21 are one conditional_compilation_directive (IEEE 1800-2023
// 22.6, Syntax 22-5) with only an `ifdef group:
//
//   `ifdef ifdef_macro_identifier ifdef_group_of_lines `endif
//
// Its group of lines, lines 17-20, is itself a nested conditional
// compilation construct, `ifdef DEBUGGER ... `endif, whose own group of
// lines, lines 18-19, is the module declaration 'module mymod; endmodule'.
// 22.6: "Nesting of conditional compilation constructs shall be permitted",
// and "Until the inner conditional compilation construct is terminated with
// `endif, all compiler directives apply only to the inner construct and
// shall not terminate the block_of_text in the outer construct." So the
// `endif of line 20 closes the inner construct and the `endif of line 21
// closes the outer one.
//
// Neither FPGA nor DEBUGGER is defined: the file holds no `define and the
// .hlc passes no +define+. 22.6: "If the text macro identifier is not
// defined, the ifdef_group_of_lines is ignored". So `ifdef FPGA fails and
// lines 17-20 -- the whole inner construct, module mymod included -- are
// ignored. With DEBUGGER also undefined the inner construct would drop
// mymod anyway, so mymod is never declared on either count. The ignored
// lines "still follow the SystemVerilog lexical conventions" (22.6); they
// do, so they draw no diagnostic. FPGA and DEBUGGER are tested, not used
// (22.6), so they are neither macro definitions nor macro usages.
//
// After preprocessing no source text is left. An empty compilation unit is
// legal: source_text ::= [timeunits_declaration] { description } (A.1.2)
// allows zero descriptions. So the file compiles without errors and declares
// nothing -- no module (mymod included), interface, program, package, class,
// primitive, config, checker, subroutine, parameter, variable, typedef or
// let -- and, with no module, there is no top-level instance (23.3.1).
//
// The .hlc also puts UVM 1800.2-2017 on the include path ('-I'), but adds no
// UVM source file, and passes -nobuiltin, so nothing else is compiled. The
// absence of mymod is checked design-wide by name; every other count is
// restricted to objects whose location is this file.
//
// What is checked
//   - No fatal, syntax or error diagnostic, and none of the preprocessor
//     diagnostics a misreading of the nested conditional directives would
//     produce: a syntax error (e.g. the `endif of line 21 left unmatched
//     because the inner `endif was taken to close the outer construct), an
//     "unknown macro" for FPGA or DEBUGGER, or non-ASCII content.
//   - Exactly one SourceFile for the .sv, with no includes and no generic
//     directives (`timescale, `default_nettype, ...).
//   - No macro definition and no macro instance in the source file; in
//     particular none named FPGA or DEBUGGER.
//   - Module mymod of the ignored lines 18-19 exists nowhere in the design:
//     not as a module definition and not as a top-level instance.
//   - No design element and no compilation-unit declaration from this file.
//   - No top-level module instance from this file.
//
// What is NOT checked, and why
//   - The PreprocMacroCondition objects of the four conditional directives.
//     SourceFile and Design expose no collection that holds them, so reading
//     them would guess a tool convention. The effect of the construct (lines
//     17-20 are ignored) is checked instead. Nearest assertions:
//     ModuleMymodIsNotDeclared and NoDesignElementsDeclared.
//   - Whether `ifdef DEBUGGER is evaluated: it sits in an ignored group, and
//     nothing in the model records an evaluation.
//   - A run that defines FPGA and/or DEBUGGER on the command line. The .hlc
//     defines no macro; mymod would be compiled only with both defined.
//     Nearest assertion: ModuleMymodIsNotDeclared.
//   - The license and metadata comments (5.4): comments carry no meaning;
//     how HLDB stores them is a tool convention.
//   - The SourceFile time unit and precision: there is no `timescale and the
//     default is implementation-specific (3.14.2.3). The .hlc's -timescale
//     option is a tool option, not SystemVerilog text.
//   - The warning count. The LRM neither requires nor forbids a warning for
//     an empty compilation unit (a tool may warn that there is no top
//     module). Nearest assertion: CompilesWithoutErrors.
//   - Source line numbers of the directives: no node holding them is
//     exposed (see above).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/ErrorReporting/Location.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/checker_decl.h>
#include <hldb/class_defn.h>
#include <hldb/config_decl.h>
#include <hldb/design.h>
#include <hldb/directive.h>
#include <hldb/include_stmt.h>
#include <hldb/interface.h>
#include <hldb/let_decl.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/preproc_macro_instance.h>
#include <hldb/program.h>
#include <hldb/source_file.h>
#include <hldb/task_func.h>
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
constexpr std::string_view kSourceFileName = "desc_test_9.sv";

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
}

// Size of an HLDB collection; a collection that was never created is empty.
template <typename T>
size_t sizeOf(const std::vector<T *> *collection) {
  if (collection == nullptr) {
    return 0;
  }
  return collection->size();
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

// True when 'name' names module mymod, bare or library-qualified (HLDB may
// prefix a definition name with its library, e.g. "work@mymod"; how the
// library is spelled is a tool convention).
bool namesMymod(std::string_view name) { return (name == "mymod") || endsWith(name, "@mymod"); }

// Number of modules in 'collection' whose definition name or instance name
// is mymod, wherever they come from.
size_t countNamedMymod(const hldb::ModuleCollection *collection) {
  size_t count = 0;
  if (collection != nullptr) {
    for (const hldb::Module *module : *collection) {
      if (namesMymod(module->getDefName()) || namesMymod(module->getName())) ++count;
    }
  }
  return count;
}

// The line a diagnostic points at, or 0 when it carries no location.
uint32_t lineOf(const Error &error) {
  if (error.getLocations().empty()) {
    return 0;
  }
  return error.getLocations().front().m_line;
}

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

class DescTest9Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "desc_test_9.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // All SourceFile nodes recorded for the file under test.
  static std::vector<const hldb::SourceFile *> findSourceFiles() {
    std::vector<const hldb::SourceFile *> sourceFiles;
    if (m_design == nullptr) {
      return sourceFiles;
    }
    const hldb::SourceFileCollection *const collection = m_design->getSourceFiles();
    if (collection == nullptr) {
      return sourceFiles;
    }
    for (const hldb::SourceFile *sourceFile : *collection) {
      if (endsWith(sourceFile->getName(), kSourceFileName)) sourceFiles.emplace_back(sourceFile);
    }
    return sourceFiles;
  }

  // The SourceFile node for the file under test, or nullptr unless there is exactly one.
  static const hldb::SourceFile *getSourceFile() {
    const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
    if (sourceFiles.size() != 1) {
      return nullptr;
    }
    return sourceFiles.front();
  }
};

// Lines 16-21 match Syntax 22-5 with the inner construct of lines 17-20
// nested in the outer group, and the ignored lines 17-20 follow the lexical
// conventions (22.6); an empty compilation unit is legal source_text
// (A.1.2). So nothing is fatal, a syntax error or an error.
TEST_F(DescTest9Test, CompilesWithoutErrors) {
  ASSERT_NE(m_session, nullptr);
  const ErrorContainer *const errorContainer = m_session->getErrorContainer();
  ASSERT_NE(errorContainer, nullptr);

  const ErrorContainer::Stats stats = errorContainer->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0) << describeErrors(errorContainer, ErrorDefinition::FATAL);
  EXPECT_EQ(stats.nbSyntax, 0) << describeErrors(errorContainer, ErrorDefinition::SYNTAX);
  EXPECT_EQ(stats.nbError, 0) << describeErrors(errorContainer, ErrorDefinition::ERROR);
}

// Each of these diagnostics would mean HLC misread the nested conditional
// directives of lines 16-21.
TEST_F(DescTest9Test, ReportsNoPreprocessorDiagnostics) {
  // Lines 16-21 match Syntax 22-5. The `endif of line 20 terminates only the
  // inner construct (22.6), so the `endif of line 21 matches the outer
  // `ifdef FPGA and neither is left unmatched.
  EXPECT_EQ(findError(ErrorDefinition::PP_SYNTAX_ERROR), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_SYNTAX_ERROR), nullptr);

  // `ifdef tests whether FPGA / DEBUGGER is defined (22.6); neither is a
  // macro usage, so their being undefined is no "unknown macro".
  EXPECT_EQ(findError(ErrorDefinition::PP_UNKOWN_MACRO), nullptr);

  // The directives are outside any design element (22.6 allows them
  // anywhere in the source description anyway).
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_IN_DESIGN_ELEMENT), nullptr);

  // The file is plain ASCII.
  EXPECT_EQ(findError(ErrorDefinition::PP_NON_ASCII_CONTENT), nullptr);
}

TEST_F(DescTest9Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

TEST_F(DescTest9Test, SourceFileHasNoIncludesOrGenericDirectives) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  // There is no `include in the file (22.4).
  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getIncludeStmts()), 0u);

  // `ifdef and `endif are the only directives in the file. None of the
  // directives HLDB records as generic directives (those with a
  // vpiDirectiveType code: `timescale, `default_nettype, `celldefine, ...)
  // occurs, so a generic directive here would be one the source does not
  // contain.
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);
}

// The file holds no `define, and the names after `ifdef are tested, not
// used (22.6), so there is no macro definition and no macro instance.
TEST_F(DescTest9Test, SourceFileHasNoMacroDefinitionsOrInstances) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// Testing a name with `ifdef does not define it (22.6): neither FPGA (line
// 16) nor DEBUGGER (line 17) becomes a macro definition.
TEST_F(DescTest9Test, TestedNamesAreNotDefined) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  for (std::string_view name : {"FPGA", "DEBUGGER"}) {
    EXPECT_EQ(hldb::findByName<hldb::PreprocMacroDefinition>(name, sourceFile->getPreprocMacroDefinitions()), nullptr)
        << name << " is only tested, never defined";
  }
}

// 22.6: FPGA is not defined, so the outer ifdef_group_of_lines (lines
// 17-20) is ignored, and with it the inner construct and 'module mymod;
// endmodule' (lines 18-19). No module named mymod may exist anywhere in the
// design -- neither as a definition nor as a top-level instance (23.3.1).
TEST_F(DescTest9Test, ModuleMymodIsNotDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countNamedMymod(m_design->getAllModules()), 0u) << "lines 18-19 are in an ignored group of lines";
  EXPECT_EQ(countNamedMymod(m_design->getTopModules()), 0u) << "lines 18-19 are in an ignored group of lines";
}

// After preprocessing no source text is left (the outer group of lines is
// ignored), so the file declares no design element (3.2-3.12, A.1.2).
TEST_F(DescTest9Test, NoDesignElementsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllClasses()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);
}

// No compilation-unit item (3.12.1) survives preprocessing either: the
// tested names FPGA and DEBUGGER are not declarations.
TEST_F(DescTest9Test, NoCompilationUnitDeclarations) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getTaskFuncs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getLetDecls()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParameters()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParamAssigns()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getVariables()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getTypedefs()), 0u);
}

// With no module left in the file there is nothing to instantiate
// implicitly as a top-level module (23.3.1). This holds before elaboration
// (no instance tree yet) and after it (no module to elaborate), so it is not
// gated on getElaborated().
TEST_F(DescTest9Test, NoTopLevelInstances) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(countFromSourceFile(m_design->getTopModules()), 0u);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
