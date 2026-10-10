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

// Test for tests/Google/chapter-22/22.5.1--define-expansion_21.sv
//
// Lines 1-7 are one-line "//" license comments, lines 8-9 are blank, lines
// 10-16 are one block comment carrying the test metadata (its
// ":should_fail_because:" says macro text shall not be split across string
// literals), and the SystemVerilog text is:
//
//   17: `define first_half "start of string
//   18: module top ();
//   19: initial $display(`first_half end of string");
//   20: endmodule
//
// IEEE 1800-2023 22.5.1: "The text specified for macro text shall not be split
// across the following lexical tokens: ... String literals ...", followed by
// "The following is illegal syntax because it is split across a string:"
// and this exact pair of lines:
//
//   `define first_half "start of string
//   $display(`first_half end of string");
//
// The macro text of line 17 ends at the newline (22.5.1), so it holds an
// opening double quote with no closing one; and a quoted string cannot
// contain a newline (5.9: a quoted_string_item is any ASCII character except
// a backslash, a newline or a double quote). Line 19 tries to complete that
// string with the source text end of string", which again opens a quote that
// the newline leaves unclosed. The illegal construct is the pair of lines 17 and 19;
// everything else in the file is legal: the comments, the module header on
// line 18 and endmodule on line 20.
//
// "module top ();" has a list_of_ports holding one null port: 37.14 detail 10
// names "module M();" as the null port case, with vpiLowConn NULL; detail 11
// gives a null port vpiSize 0, and detail 9 gives the first port vpiPortIndex 0.
//
// top is never instantiated, so it is a top-level module, implicitly
// instantiated once under its own name (23.3.1).
//
// Every assertion below follows from the LRM, except the parts labelled as
// HLC error recovery; a red run means HLC deviates from the LRM. An empty
// generic directive list reads an LRM fact through an HLDB code: the file
// holds none of the directives HLDB records that way (`timescale,
// `default_nettype, `celldefine and the others with a vpiDirectiveType code).
//
// What is checked
//   - At least one error-class diagnostic (fatal, syntax or error) points at
//     line 17 or line 19.
//   - HLC diagnoses an unterminated string, PP_UNTERMINATED_STRING, on line
//     17 or line 19, with an error-class severity: the string opened on
//     either line is not closed before the newline (5.9).
//   - Every error-class diagnostic points at line 17 or line 19. For lines
//     1-16 and 18 this is an LRM fact: legal text draws no error. For a
//     diagnostic on line 20 or later, or without a location, it is an HLC
//     error-recovery requirement, not an LRM rule: such a diagnostic would be
//     a cascade from the illegal lines.
//   - None of the preprocessor diagnostics that a misreading of the
//     definition or of the usage would produce.
//   - Exactly one SourceFile for the .sv, with no includes and no generic
//     directives.
//   - Exactly one module, top, on lines 18-20, with one null port. Lines 18
//     and 20 are legal; that the module is still recorded whole around the
//     illegal line 19 is HLC error recovery.
//   - No other design object is created from this file.
//   - After elaboration only: one top-level instance named top.
//
// What is NOT checked, and why
//   - The PreprocMacroDefinition of first_half and the PreprocMacroInstance of
//     its usage: whether they are recorded, and with which tokens, arguments,
//     body, items and objects. The macro text is illegal and the LRM gives the
//     pair no meaning, so any model of them is consistent with the LRM.
//     Nearest assertions: ReportsAnErrorOnTheIllegalLines and
//     DiagnosesAnUnterminatedString.
//   - Whether HLC also reports the usage on line 19 as an unknown macro. If
//     HLC rejects the illegal definition, the macro is undefined at line 19;
//     a diagnostic on line 19 is one more diagnostic on an illegal line, which
//     the LRM does not limit. Nearest assertion: ErrorsOnlyOnTheIllegalLines.
//   - The contents of module top: its processes and the $display call. They
//     could only come from the illegal lines, which the LRM does not define.
//     Nearest assertions: TopModule and TopHasOneNullPort.
//   - How many diagnostics lines 17 and 19 draw, and of which severity,
//     beyond the required error, and which of the two lines carries it. The
//     LRM marks the pair illegal but does not say where a tool reports it or
//     how many diagnostics it draws. Nearest assertions:
//     ReportsAnErrorOnTheIllegalLines and ErrorsOnlyOnTheIllegalLines.
//   - The diagnostic's symbol text and column. The LRM requires the error but
//     not its message format. Nearest assertion: DiagnosesAnUnterminatedString.
//   - What $display would print. It only exists while simulation time
//     advances, so it is permanently out of scope; here the call is not even
//     legal. Nearest assertion: ReportsAnErrorOnTheIllegalLines.
//   - The seven one-line comments and the block comment (5.4). The LRM gives
//     comments no meaning; how HLDB groups, classifies (vpiComment and
//     vpiDocumentComment are marked non-standard) and attaches them is a tool
//     convention. Nearest assertions: the line checks in TopModule.
//   - Columns. The LRM numbers lines (22.13) but not columns. Nearest
//     assertions: the line checks in TopModule.
//   - The SourceFile time unit and precision. There is no `timescale or
//     timeunit, so the default applies, and the default is
//     implementation-specific (3.14.2.3). Nearest assertion:
//     SourceFileHasNoIncludesOrDirectives (zero directives).
//   - The warning count. The LRM neither requires nor forbids warnings here.
//     Nearest assertion: ErrorsOnlyOnTheIllegalLines.
//   - Only the top-level instance check is gated on Design::getElaborated():
//     the instance tree only exists after elaboration. Elaboration-category
//     diagnostics are not excluded from ErrorsOnlyOnTheIllegalLines, because
//     top exists and so a design with a top-level module (23.3.1) gives an
//     elaborating run nothing legitimate to report off lines 17 and 19.

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
#include <hldb/interface.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/port.h>
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
constexpr std::string_view kSourceFileName = "22.5.1--define-expansion_21.sv";

// The two lines of the illegal construct: the definition and the usage.
constexpr uint32_t kDefinitionLine = 17;
constexpr uint32_t kUsageLine = 19;

bool endsWith(std::string_view text, std::string_view suffix) {
  return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
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

// True for the severities the LRM's "illegal" can map to.
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

// True when 'line' is one of the two lines of the illegal construct.
bool isIllegalLine(uint32_t line) { return (line == kDefinitionLine) || (line == kUsageLine); }

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

class DefineExpansion21Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.5.1--define-expansion_21.hlc"}); }
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

// 22.5.1: macro text "shall not be split across ... String literals", and the
// LRM calls exactly lines 17 and 19 "illegal syntax because it is split
// across a string". It requires an error for the pair; it does not say which
// kind or on which of the two lines, so any error-class diagnostic on line
// 17 or 19 satisfies it.
TEST_F(DefineExpansion21Test, ReportsAnErrorOnTheIllegalLines) {
  ASSERT_NE(m_session, nullptr);
  const ErrorContainer *const errorContainer = m_session->getErrorContainer();
  ASSERT_NE(errorContainer, nullptr);

  size_t errorsOnIllegalLines = 0;
  for (const Error &error : errorContainer->getErrors()) {
    if (isErrorClass(error) && isIllegalLine(lineOf(error))) ++errorsOnIllegalLines;
  }
  EXPECT_GE(errorsOnIllegalLines, 1u) << "no error on line " << kDefinitionLine << " or " << kUsageLine << ";"
                                      << describeAllErrors(errorContainer);
}

// 5.9: a quoted string cannot contain a newline (quoted_string_item excludes
// it), so the string opened on line 17, and the one opened on line 19, each
// end unterminated at their newline. HLC's code for exactly that lexical
// condition is PP_UNTERMINATED_STRING.
TEST_F(DefineExpansion21Test, DiagnosesAnUnterminatedString) {
  ASSERT_NE(m_session, nullptr);
  const ErrorContainer *const errorContainer = m_session->getErrorContainer();
  ASSERT_NE(errorContainer, nullptr);

  size_t unterminatedOnIllegalLines = 0;
  for (const Error &error : errorContainer->getErrors()) {
    if ((error.getType() == ErrorDefinition::PP_UNTERMINATED_STRING) && isIllegalLine(lineOf(error))) {
      ++unterminatedOnIllegalLines;
    }
  }
  EXPECT_GE(unterminatedOnIllegalLines, 1u) << "no PP_UNTERMINATED_STRING on line " << kDefinitionLine << " or "
                                            << kUsageLine << ";" << describeAllErrors(errorContainer);

  // The LRM calls this illegal; it does not say which kind of error, so any
  // error-class severity is accepted.
  const ErrorDefinition::ErrorMap &errorInfoMap = ErrorDefinition::getErrorInfoMap();
  const ErrorDefinition::ErrorMap::const_iterator errorInfo =
      errorInfoMap.find(ErrorDefinition::PP_UNTERMINATED_STRING);
  ASSERT_NE(errorInfo, errorInfoMap.end());
  EXPECT_TRUE(isErrorClass(errorInfo->second.m_severity)) << "severity " << errorInfo->second.m_severity;
}

// Lines 17 and 19 are the illegal pair (22.5.1); the LRM requires an error for
// it but does not limit how many diagnostics, or of which severity, they
// draw, so any number of them on those two lines is accepted. Elsewhere this
// check mixes two kinds of fact:
//   - LRM: lines 1-16 are comments and blank lines and line 18 is a
//     well-formed module header, so an error pointing at them would mean HLC
//     rejected legal text.
//   - HLC error recovery, not an LRM rule: the LRM gives the illegal pair no
//     meaning and does not define what a tool does after reporting it, so an
//     error on line 20 or later, or without a location, is a cascade, not a
//     conformance failure.
TEST_F(DefineExpansion21Test, ErrorsOnlyOnTheIllegalLines) {
  ASSERT_NE(m_session, nullptr);
  const ErrorContainer *const errorContainer = m_session->getErrorContainer();
  ASSERT_NE(errorContainer, nullptr);

  std::string offLines;
  for (const Error &error : errorContainer->getErrors()) {
    if (isErrorClass(error) && !isIllegalLine(lineOf(error))) offLines += describeError(error);
  }
  EXPECT_EQ(offLines, "") << "error-class diagnostics off lines " << kDefinitionLine << " and " << kUsageLine;
}

// Each of these diagnostics would mean HLC misread the definition or the usage.
TEST_F(DefineExpansion21Test, ReportsNoMisreadingDiagnostics) {
  // first_half has no formal arguments: no "(" follows its name on line 17,
  // and its usage takes no parentheses. So no argument can be one too many,
  // lack a default, or be unused or undefined.
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_HAS_SPACE_BEFORE_ARGS), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_PARENTHESIS_NEEDED), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_TOO_MANY_ARGS_MACRO), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_NO_DEFAULT_VALUE), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_UNUSED_ARGUMENT), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_UNDEFINED_ARGUMENT), nullptr);

  // first_half is defined once.
  EXPECT_EQ(findError(ErrorDefinition::PP_MULTIPLY_DEFINED_MACRO), nullptr);

  // first_half is not a compiler directive name (22.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_NAME_RESERVED), nullptr);

  // The macro text does not refer to any macro.
  EXPECT_EQ(findError(ErrorDefinition::PP_RECURSIVE_MACRO_DEFINITION), nullptr);

  // `define is outside module top, and a macro usage may appear anywhere in
  // the compilation unit after its definition (22.5.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_IN_DESIGN_ELEMENT), nullptr);

  // The file is plain ASCII.
  EXPECT_EQ(findError(ErrorDefinition::PP_NON_ASCII_CONTENT), nullptr);
}

TEST_F(DefineExpansion21Test, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

TEST_F(DefineExpansion21Test, SourceFileHasNoIncludesOrDirectives) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  // There is no `include in the file (22.4).
  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);

  // `define is the only directive in the file (`first_half is a macro usage,
  // not a directive). None of the directives HLDB records as generic
  // directives (those with a vpiDirectiveType code: `timescale,
  // `default_nettype, `celldefine, ...) occurs, and `define has no
  // vpiDirectiveType code, so a generic directive here would be one the
  // source does not contain.
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);
}

// Lines 18-20: module top (); ... endmodule. Only what does not depend on the
// illegal line 19 is checked. Lines 18 and 20 are legal; that the module is
// still recorded whole around line 19 is HLC error recovery.
TEST_F(DefineExpansion21Test, TopModule) {
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
TEST_F(DefineExpansion21Test, TopHasOneNullPort) {
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
TEST_F(DefineExpansion21Test, NoOtherDesignObjectsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllClasses()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);

  // The macro first_half is preprocessor text, not a declaration.
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
