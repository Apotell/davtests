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

// Test for tests/Google/chapter-22/22.11--pragma-basic.sv
//
// THE SOURCE
//
// Lines 1-7 are one-line comments with the license, lines 8-9 are blank and
// lines 10-15 are one block comment carrying the test metadata. The
// SystemVerilog text is one compiler directive and no design element:
//
//   16: `pragma pragma_name "pragma_value"
//
// The repository stores the file with LF line endings; a Windows checkout
// (core.autocrlf) turns them into CR LF. 5.3 lists "newlines" as white space
// but does not name the carriage return, so this test treats CR LF as one
// line ending and asserts nothing about the CR itself.
//
// WHAT THE LRM SAYS ABOUT IT
//
// Line 16 is the only compiler directive in the file. "All compiler
//   directives are preceded by the (`) character" (22.2), and `pragma is one
//   of them (22.1). Lines 1-15 hold none: they are comments (5.4) and blank
//   lines, and "Directives are not recognized within comments or string
//   literals" (22.2).
//
// Syntax 22-8 (22.11) gives the form of the directive:
//
//     pragma ::= `pragma pragma_name [ pragma_expression { , pragma_expression } ]
//     pragma_name ::= simple_identifier
//     pragma_expression ::= pragma_keyword | pragma_keyword = pragma_value | pragma_value
//     pragma_value ::= ( pragma_expression { , pragma_expression } ) | number | string | identifier
//     pragma_keyword ::= simple_identifier
//
//   - The pragma_name is pragma_name, a simple_identifier (A.9.3). It starts
//     with a letter and has no spaces (A.9.3, footnote 54), it is not a
//     keyword (Table B.1), and its 11 characters are well within the
//     identifier length limit, which "shall be at least 1024 characters"
//     (5.6).
//   - "pragma_value" is the only pragma_expression: no comma follows it. It is
//     a string literal, so it is not a pragma_keyword (a simple_identifier),
//     and with no "=" it is no pragma_keyword = pragma_value pair either. It
//     is a pragma_value of the string kind.
//
// "The pragma specification is identified by the pragma_name" (22.11), and
//   pragma_name is none of the standard pragmas reset, resetall and protect
//   (22.11.1). "The effect of pragmas other than those specified in this
//   standard is implementation-specific", and "Unless otherwise specified,
//   pragma directives for pragma_names that are not recognized by an
//   implementation shall have no effect on interpretation of the
//   SystemVerilog source text" (22.11). The line is legal, so it draws no
//   error, and since the file has no design element, nothing in the design
//   comes from it.
//
// "Unless specified below as supporting multiple lines, compiler directives
//   shall be all on one line" (22.2), and 22.11 specifies no such support.
//   "The line number shall be reset to 1 at the beginning of each file" and
//   "incremented as each line is read" (22.12), so the directive, its name
//   and its value are all on line 16.
//
// "pragma_value" is a quoted string (5.9). Its 12 characters, pragma_value,
//   are letters and an underscore, each a quoted_string_item; there is no
//   escape sequence and no newline ("A quoted string shall be contained in a
//   single line", 5.9). The double quotes enclose the string and are not part
//   of its value. "String literals are packed arrays (of a width that is a
//   multiple of 8 bits)", with "one 8-bit ASCII value representing one
//   character" (5.9), so the literal is 12 x 8 = 96 bits wide.
//
// The LRM's object model represents a string literal as a vpiConstant
//   ("numerical constant or string literal") whose vpiConstType is
//   vpiStringConst ("string literal", Annex K). Its value is read as a string
//   of its characters (vpiStringVal, 38.15), and its vpiDecompile "shall
//   return a string with a functionally equivalent expression to the original
//   expression within the source code" (37.59), which for a string literal is
//   the quoted text.
//
// `pragma is a compiler directive (22.1), not a usage of a text macro
//   (22.5.1), and the file defines no macro.
//
// The file has no module, and "A design shall contain at least one top-level
//   module" (23.3.1). An elaborating run may report that, so the error checks
//   below leave HLC's elaboration-category diagnostics out.
//
// HOW HLDB RECORDS IT
//
// HLDB models a `pragma as a PragmaDirective: its pragma_name, as an
// Identifier, and the list of its pragma_expressions. A GenericDirective holds
// only a directive type code and a single value, so it could not keep the
// pragma_name and the expression list together. The SourceFile node is HLDB's
// record of the text of one file (its includes, macro definitions, macro uses
// and directives) and owns what it records, and a PragmaDirective owns its
// name and its expressions. So line 16 is expected as the one directive of the
// SourceFile: a PragmaDirective named pragma_name with one expression, a
// Constant for the string literal. These are model facts, and every value
// they hold comes from the text of line 16 as the LRM reads it.
//
// Every expected value comes from the source text and the LRM, so a red test
// means HLC deviates from the LRM or from its own model.
// PP_ILLEGAL_DIRECTIVE_VALUE is HLC's code for a directive argument it does
// not accept, PP_ILLEGAL_DIRECTIVE_IN_DESIGN_ELEMENT its code for a directive
// placed inside a design element, and PP_ILLEGAL_PROTECT_PRAGMA its code for a
// protect pragma it rejects.
//
// WHAT IS CHECKED (one fact per test, in source order)
//
//   CompilesWithoutErrors                     no fatal, syntax or error
//                                             diagnostic (outside elaboration)
//   Line16PragmaIsAccepted                    no such diagnostic on line 16
//   ReportsNoDirectiveMisreadingDiagnostics   no diagnostic of any severity
//                                             that misreads line 16 as a
//                                             directive
//   ReportsNoProtectPragmaDiagnostics         none that takes pragma_name for
//                                             protect
//   ReportsNoPragmaNameDiagnostics            none that rejects the identifier
//                                             pragma_name
//   ReportsNoStringLiteralDiagnostics         none that misreads the string
//                                             literal or the ASCII text
//   SourceFileHasNoIncludesOrMacros           one SourceFile, no includes,
//                                             no macro definitions or usages
//   SourceFileRecordsOneDirective             the SourceFile records exactly
//                                             one directive
//   Line16DirectiveIsAPragmaDirective         that directive is a
//                                             PragmaDirective
//   PragmaDirectiveIsOnLine16                 it comes from this file, lies on
//                                             line 16 only and is owned by the
//                                             SourceFile
//   PragmaNameIsPragmaName                    its name is pragma_name, an
//                                             Identifier on line 16 owned by
//                                             the directive
//   PragmaHasOnePragmaExpression              it has exactly one
//                                             pragma_expression
//   PragmaValueIsAStringLiteral               that expression is a Constant of
//                                             const type vpiStringConst
//   PragmaValueIsPragmaValue                  value pragma_value, decompiled
//                                             "pragma_value", 96 bits wide
//   PragmaValueIsOnLine16                     it comes from this file, lies on
//                                             line 16 only and is owned by the
//                                             directive
//   NoDesignObjectsDeclared                   nothing in the design comes
//                                             from this file
//
// No check is gated on Design::getElaborated(): nothing in the file
// elaborates or reduces. There is no design element, and the only value is a
// string literal inside a directive, with no operator or parameter to
// evaluate. The 23.3.1 error an elaborating run may report is left out, not
// asserted.
//
// WHAT IS NOT CHECKED, AND WHY
//
//   - Any effect of the pragma. 22.11 leaves the effect of a pragma that is
//     not standard implementation-specific and gives a pragma_name the
//     implementation does not recognize none, and the file has no design
//     element it could act on. Whether HLC recognizes pragma_name is not
//     recorded in the model. Nearest assertions: CompilesWithoutErrors and
//     NoDesignObjectsDeclared.
//   - Run-time behavior. A pragma "alters interpretation of the SystemVerilog
//     source" (22.11), which happens at compile time, and the file has no
//     procedural code, so nothing in it exists only while simulation time
//     advances. This is permanently out of scope. Nearest assertions:
//     PragmaNameIsPragmaName and PragmaValueIsPragmaValue.
//   - The data type of the value. 5.9 treats a string literal as an unsigned
//     integer constant when it is used as an operand in an expression or an
//     assignment, and a pragma_value is neither, so the typespec of the
//     Constant is not asserted. Nearest assertions: PragmaValueIsAStringLiteral
//     and PragmaValueIsPragmaValue (vpiStringConst, 96 bits).
//   - The directive list of the Design. It has no LRM counterpart, and the
//     directive is text of this file, which HLDB records on the SourceFile.
//     Nearest assertions: SourceFileRecordsOneDirective and
//     PragmaDirectiveIsOnLine16.
//   - Elaboration-category diagnostics. The 23.3.1 error for a design without
//     a top-level module is legitimate here. Nearest assertion:
//     CompilesWithoutErrors (every other error-class diagnostic).
//   - Warnings and informational diagnostics, for example that pragma_name is
//     not recognized. Having "no effect on interpretation" (22.11) neither
//     requires nor forbids one. Nearest assertions: CompilesWithoutErrors and
//     the four ReportsNo... tests.
//   - The comments on lines 1-15 (5.4) and columns. The LRM gives comments no
//     meaning, how HLDB keeps them is a tool convention, and the LRM numbers
//     lines (22.12, 22.13) but not columns. Nearest assertions: the line 16
//     checks, which hold only if lines 1-15 were read as whole lines.

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
#include <hldb/identifier.h>
#include <hldb/interface.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/pragma_directive.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/preproc_macro_instance.h>
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
#include <vector>

namespace hlc {
namespace {
constexpr std::string_view kSourceFileName = "22.11--pragma-basic.sv";

// The line of the `pragma directive, the only SystemVerilog text in the file.
constexpr uint32_t kPragmaLine = 16;

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

// The line a diagnostic points at, or 0 when it carries no location.
uint32_t lineOf(const Error &error) { return error.getLocations().empty() ? 0 : error.getLocations().front().m_line; }

// True when HLC's error table gives 'error' a fatal, syntax or error
// severity outside the elaboration category. Elaboration diagnostics are left
// out because a design without a top-level module may draw one (23.3.1).
bool isSourceError(const Error &error) {
  const ErrorDefinition::ErrorMap &errorInfoMap = ErrorDefinition::getErrorInfoMap();
  const ErrorDefinition::ErrorMap::const_iterator errorInfo = errorInfoMap.find(error.getType());
  if (errorInfo == errorInfoMap.end()) return false;
  const ErrorDefinition::ErrorSeverity severity = errorInfo->second.m_severity;
  const bool isErrorClass = (severity == ErrorDefinition::FATAL) || (severity == ErrorDefinition::SYNTAX) ||
                            (severity == ErrorDefinition::ERROR);
  return isErrorClass && (errorInfo->second.m_category != ErrorDefinition::ELAB);
}

// One diagnostic as " [<code> line <n>: <message template>]", so that a red
// run shows what was reported and where.
std::string describeError(const Error &error) {
  const ErrorDefinition::ErrorMap &errorInfoMap = ErrorDefinition::getErrorInfoMap();
  const ErrorDefinition::ErrorMap::const_iterator errorInfo = errorInfoMap.find(error.getType());
  std::string description = " [" + std::to_string(static_cast<int32_t>(error.getType()));
  description += " line " + std::to_string(lineOf(error));
  if (errorInfo != errorInfoMap.end()) description += ": " + std::string(errorInfo->second.m_errorText);
  return description + "]";
}

// Every directive 'sourceFile' records as " [vpiType <type> line <n>]", so
// that a red run shows what HLDB recorded instead.
std::string describeDirectives(const hldb::SourceFile *sourceFile) {
  std::string description;
  if (const hldb::DirectiveCollection *const directives = sourceFile->getDirectives()) {
    for (const hldb::Directive *directive : *directives) {
      description += " [vpiType " + std::to_string(directive->getVpiType());
      description += " line " + std::to_string(directive->getStartLine()) + "]";
    }
  }
  return description;
}
}  // namespace

class PragmaBasicTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.11--pragma-basic.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Every source error (see isSourceError), described; empty when there is
  // none. A run without an error container is reported, not passed.
  static std::string sourceErrors() {
    if ((m_session == nullptr) || (m_session->getErrorContainer() == nullptr)) return "no error container";
    std::string description;
    for (const Error &error : m_session->getErrorContainer()->getErrors()) {
      if (isSourceError(error)) description += describeError(error);
    }
    return description;
  }

  // The same, for the source errors that point at 'line' only.
  static std::string sourceErrorsOnLine(uint32_t line) {
    if ((m_session == nullptr) || (m_session->getErrorContainer() == nullptr)) return "no error container";
    std::string description;
    for (const Error &error : m_session->getErrorContainer()->getErrors()) {
      if (isSourceError(error) && (lineOf(error) == line)) description += describeError(error);
    }
    return description;
  }

  // The SourceFile nodes of the file under test.
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

  // The SourceFile node of the file under test, or nullptr unless there is
  // exactly one.
  static const hldb::SourceFile *getSourceFile() {
    const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
    return (sourceFiles.size() == 1) ? sourceFiles.front() : nullptr;
  }

  // The first PragmaDirective that the SourceFile records, or nullptr.
  static const hldb::PragmaDirective *findPragmaDirective() {
    const hldb::SourceFile *const sourceFile = getSourceFile();
    if ((sourceFile == nullptr) || (sourceFile->getDirectives() == nullptr)) return nullptr;
    for (const hldb::Directive *directive : *sourceFile->getDirectives()) {
      if (const hldb::PragmaDirective *const pragma = any_cast<hldb::PragmaDirective>(directive)) return pragma;
    }
    return nullptr;
  }

  // The first pragma_expression of that PragmaDirective, or nullptr.
  static const hldb::Any *findPragmaExpression() {
    const hldb::PragmaDirective *const pragma = findPragmaDirective();
    if ((pragma == nullptr) || (sizeOf(pragma->getExprs()) == 0)) return nullptr;
    return pragma->getExprs()->front();
  }
};

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// Every line of the file is legal, so nothing is fatal, a syntax error or an
// error, apart from the 23.3.1 elaboration error left out above.
TEST_F(PragmaBasicTest, CompilesWithoutErrors) { EXPECT_EQ(sourceErrors(), ""); }

// Line 16: `pragma pragma_name "pragma_value". It matches Syntax 22-8, and a
// pragma_name the implementation does not recognize has no effect (22.11).
TEST_F(PragmaBasicTest, Line16PragmaIsAccepted) { EXPECT_EQ(sourceErrorsOnLine(kPragmaLine), ""); }

// Each of these diagnostics, at any severity and on any line, would mean HLC
// misread line 16 as a directive.
TEST_F(PragmaBasicTest, ReportsNoDirectiveMisreadingDiagnostics) {
  // Line 16 matches Syntax 22-8, for the preprocessor and for the parser.
  EXPECT_EQ(findError(ErrorDefinition::PP_SYNTAX_ERROR), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PA_SYNTAX_ERROR), nullptr);

  // A string is one of the pragma_value forms of Syntax 22-8.
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_VALUE), nullptr);

  // The file has no design element, and 22.11 does not restrict where a
  // `pragma may appear.
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_IN_DESIGN_ELEMENT), nullptr);

  // `pragma is a compiler directive (22.1), not a usage of a text macro.
  EXPECT_EQ(findError(ErrorDefinition::PP_UNKOWN_MACRO), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PA_UNKOWN_MACRO), nullptr);
}

// pragma_name is not protect, so line 16 is no protect pragma and starts no
// protected envelope (22.11.1, Clause 34). Either diagnostic, at any severity,
// would mean HLC took it for one.
TEST_F(PragmaBasicTest, ReportsNoProtectPragmaDiagnostics) {
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_PROTECT_PRAGMA), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_DECRYPTION_FAILED), nullptr);
}

// pragma_name is a legal simple_identifier (A.9.3).
TEST_F(PragmaBasicTest, ReportsNoPragmaNameDiagnostics) {
  // It is not a keyword (Table B.1).
  EXPECT_EQ(findError(ErrorDefinition::PA_RESERVED_KEYWORD), nullptr);

  // Its 11 characters are within any limit an implementation may set, which
  // "shall be at least 1024 characters" (5.6).
  EXPECT_EQ(findError(ErrorDefinition::PA_MAX_LENGTH_IDENTIFIER), nullptr);
}

// "pragma_value" is a well-formed quoted string (5.9) in a plain ASCII file.
TEST_F(PragmaBasicTest, ReportsNoStringLiteralDiagnostics) {
  // Its closing double quote is on line 16.
  EXPECT_EQ(findError(ErrorDefinition::PP_UNTERMINATED_STRING), nullptr);

  // Letters and an underscore are quoted_string_items; none needs an escape.
  EXPECT_EQ(findError(ErrorDefinition::PP_UNESCAPED_CHARACTER_IN_STRING), nullptr);

  // It has no backslash, so it has no escape sequence (5.9.1).
  EXPECT_EQ(findError(ErrorDefinition::PP_UNRECOGNIZED_ESCAPED_SEQUENCE), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_ESCAPE_SEQUENCE_OUT_OF_RANGE), nullptr);

  // Every character of the file is ASCII.
  EXPECT_EQ(findError(ErrorDefinition::PP_NON_ASCII_CONTENT), nullptr);
}

// ---------------------------------------------------------------------------
// The source file
// ---------------------------------------------------------------------------

// The file is read once. It has no `include (22.4) and no `define (22.5.1),
// and `pragma is a compiler directive, not a macro usage (22.1).
TEST_F(PragmaBasicTest, SourceFileHasNoIncludesOrMacros) {
  const std::vector<const hldb::SourceFile *> sourceFiles = findSourceFiles();
  ASSERT_EQ(sourceFiles.size(), 1u);
  const hldb::SourceFile *const sourceFile = sourceFiles.front();

  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// ---------------------------------------------------------------------------
// Line 16: the `pragma directive
// ---------------------------------------------------------------------------

// Line 16 is the only directive of the file: lines 1-15 are comments and
// blank lines, and no directive is recognized within a comment (22.2).
TEST_F(PragmaBasicTest, SourceFileRecordsOneDirective) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 1u) << describeDirectives(sourceFile);
}

// That directive is a `pragma (22.11), which HLDB records as a
// PragmaDirective.
TEST_F(PragmaBasicTest, Line16DirectiveIsAPragmaDirective) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);
  ASSERT_GE(sizeOf(sourceFile->getDirectives()), 1u);
  const hldb::Directive *const directive = sourceFile->getDirectives()->front();
  ASSERT_NE(directive, nullptr);
  EXPECT_EQ(directive->getAnyType(), hldb::AnyType::PragmaDirective) << describeDirectives(sourceFile);
}

// A compiler directive is all on one line (22.2), here line 16 (22.12), and
// the SourceFile that records the directive owns it.
TEST_F(PragmaBasicTest, PragmaDirectiveIsOnLine16) {
  const hldb::PragmaDirective *const pragma = findPragmaDirective();
  ASSERT_NE(pragma, nullptr);

  EXPECT_TRUE(endsWith(pragma->getFile(), kSourceFileName)) << pragma->getFile();
  EXPECT_EQ(pragma->getStartLine(), kPragmaLine);
  EXPECT_EQ(pragma->getEndLine(), kPragmaLine);
  EXPECT_EQ(pragma->getParent(), static_cast<const hldb::Any *>(getSourceFile()));
}

// "The pragma specification is identified by the pragma_name, which follows
// the `pragma directive" (22.11): the simple_identifier pragma_name, one
// token with no spaces (A.9.3, footnote 54), on line 16.
TEST_F(PragmaBasicTest, PragmaNameIsPragmaName) {
  const hldb::PragmaDirective *const pragma = findPragmaDirective();
  ASSERT_NE(pragma, nullptr);
  EXPECT_EQ(pragma->getName(), "pragma_name");

  const hldb::Identifier *const name = pragma->getNameObj();
  ASSERT_NE(name, nullptr);
  EXPECT_EQ(name->getName(), "pragma_name");
  EXPECT_TRUE(endsWith(name->getFile(), kSourceFileName)) << name->getFile();
  EXPECT_EQ(name->getStartLine(), kPragmaLine);
  EXPECT_EQ(name->getEndLine(), kPragmaLine);
  EXPECT_EQ(name->getParent(), static_cast<const hldb::Any *>(pragma));
}

// No comma follows "pragma_value", so the directive has exactly one
// pragma_expression (Syntax 22-8).
TEST_F(PragmaBasicTest, PragmaHasOnePragmaExpression) {
  const hldb::PragmaDirective *const pragma = findPragmaDirective();
  ASSERT_NE(pragma, nullptr);
  EXPECT_EQ(sizeOf(pragma->getExprs()), 1u);
}

// That pragma_expression is a pragma_value of the string kind (Syntax 22-8),
// and a string literal is a vpiConstant whose vpiConstType is vpiStringConst
// (Annex K).
TEST_F(PragmaBasicTest, PragmaValueIsAStringLiteral) {
  const hldb::Any *const expression = findPragmaExpression();
  ASSERT_NE(expression, nullptr);
  ASSERT_EQ(expression->getAnyType(), hldb::AnyType::Constant) << "vpiType " << expression->getVpiType();
  const hldb::Constant *const value = any_cast<hldb::Constant>(expression);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getConstType(), vpiStringConst);
}

// The value of the string literal is its 12 characters without the enclosing
// double quotes (5.9, read as vpiStringVal, 38.15), its decompiled form is the
// quoted text (37.59), and at one 8-bit ASCII value per character it is 96
// bits wide (5.9).
TEST_F(PragmaBasicTest, PragmaValueIsPragmaValue) {
  const hldb::Constant *const value = any_cast<hldb::Constant>(findPragmaExpression());
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getValue(), "pragma_value");
  EXPECT_EQ(value->getDecompile(), "\"pragma_value\"");
  EXPECT_EQ(value->getSize(), 96);
}

// A quoted string is contained in a single line (5.9), here line 16, and the
// directive whose pragma_expression it is owns it.
TEST_F(PragmaBasicTest, PragmaValueIsOnLine16) {
  const hldb::PragmaDirective *const pragma = findPragmaDirective();
  ASSERT_NE(pragma, nullptr);
  const hldb::Any *const value = findPragmaExpression();
  ASSERT_NE(value, nullptr);

  EXPECT_TRUE(endsWith(value->getFile(), kSourceFileName)) << value->getFile();
  EXPECT_EQ(value->getStartLine(), kPragmaLine);
  EXPECT_EQ(value->getEndLine(), kPragmaLine);
  EXPECT_EQ(value->getParent(), static_cast<const hldb::Any *>(pragma));
}

// ---------------------------------------------------------------------------
// Nothing is declared
// ---------------------------------------------------------------------------

// The file has no design element, and a pragma whose pragma_name the
// implementation does not recognize has no effect on interpretation (22.11).
// So nothing in the design comes from this file.
TEST_F(PragmaBasicTest, NoDesignObjectsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllModules()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getTopModules()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getTopPackages()), 0u);
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
