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

// Tests for 22.5.1--define-expansion_7.sv (tags: 22.5.1, type: preprocessing)
//
// SV source (lines 1-16 are the license and metadata comments):
//   17: `define D(x,y) initial $display("start", x , y, "end");
//   18: `D()
// There is no design element in the file: the usage on line 18 is at
// compilation-unit scope.
//
// The fixture's metadata says ":should_fail_because: To use a macro defined
// with arguments, the name of the text macro shall be followed by a list of
// actual arguments in parentheses, separated by commas.", quoting IEEE
// 1800-2023 Sec 22.5.1. `D() does have parentheses; what makes it illegal is
// stated by the same clause's "Example macro without defaults", which lists
// this exact usage:
//   `D()
//     // illegal, only one empty argument
// i.e. the parentheses hold one actual argument, which is empty.
//
// IEEE 1800-2023 rules this fixture exercises:
//   - Sec 22.5.1: line 17 defines D with two formal arguments, x and y in
//     that order, neither with a default, and macro text
//     "initial $display("start", x , y, "end");". The definition itself is
//     legal.
//   - Line 18 gives one actual argument (empty) for two formal arguments. "If
//     fewer actual arguments are specified than the number of formal
//     arguments and all the remaining formal arguments have defaults, then
//     the defaults are substituted for the additional formal arguments. It
//     shall be an error if any of the remaining formal arguments does not
//     have a default specified." y has no default, so the usage is an error
//     (a compile-time error, Sec 3.12).
//
// HLC's error catalog has an entry whose name is this rule,
// PP_MACRO_NO_DEFAULT_VALUE. The LRM requires only an error, so the LRM check
// identifies the diagnostic by severity and line; a separate test checks that
// HLC files it under its own dedicated entry.
//
// The LRM gives illegal source no semantics, so everything beyond that error
// is an HLC error-recovery expectation, not an LRM requirement. Whatever HLC
// does with the illegal usage, it cannot produce a module: the macro text is
// an initial procedure, and an initial procedure is not a compilation-unit
// item (Annex A.1.2: description ::= module_declaration | udp_declaration |
// interface_declaration | program_declaration | package_declaration |
// { attribute_instance } package_item).
//
// Checked -- required by IEEE 1800-2023 Sec 22.5.1:
//   - at least one error-class diagnostic (FATAL, SYNTAX or ERROR) on line
//     18, and an error-class total of at least 1
//   - the legal definition on line 17 is intact: exactly one macro
//     definition, D, with formal arguments x and y in that order and macro
//     text "initial $display("start", x , y, "end");" (compared with white
//     space removed)
//
// Checked -- HLC's catalog entry for the rule (NOT an LRM requirement):
//   - exactly one PP_MACRO_NO_DEFAULT_VALUE, on line 18, and that entry has
//     an error-class severity
//
// Checked -- HLC error-recovery requirements (NOT defined by the LRM,
// which gives illegal source no semantics):
//   - no cascade: every error-class diagnostic from source processing (any
//     category other than elaboration) is on line 18
//   - the usage is still recorded: D on line 18, column 1, with exactly one
//     actual argument, which is empty, bound to the line-17 definition
//   - the design holds no module definitions and no top-level instances
//
// What is NOT checked, and why:
//   - Elaboration-category diagnostics. Sec 23.3.1: "A design shall contain
//     at least one top-level module." This file contains none, so an
//     elaborating run may legitimately report that on its own; such
//     diagnostics are left out of the cascade check (SourceErrorsStayOnLine18).
//     Whether compiling a file with no design element must draw that error
//     is not this preprocessing fixture's subject.
//   - What, if anything, the illegal usage expands to. The LRM gives illegal
//     source no semantics, and no single recovery is more correct than
//     another.
//   - The exact number of error-class diagnostics: a recovery that expands
//     the usage anyway would also meet an initial procedure at
//     compilation-unit scope, which is illegal in its own right (Annex A.1.2).
//   - The column of the diagnostic and its message text. The LRM does not
//     say where a diagnostic must point, and the wording is HLC's own.
//   - The compiler process's exit status (what ":should_fail_because" checks
//     in other harnesses) is not part of the HLDB model; the in-process
//     equivalent, an error-class diagnostic on line 18, is asserted by
//     FewerActualArgumentsIsAnErrorOnLine18.
//   - PreprocMacroDefinition::getType() and PreprocMacroInstance::getBody(),
//     getItems() and getObjects(): HLDB fields with no LRM counterpart.
//   - Whether `define is recorded as a directive entry. The LRM has no notion
//     of a recorded directive list.
//   - Warning-, note- and info-level diagnostics. The LRM defines only
//     errors; it neither requires nor forbids warnings.
//   - Runtime behavior: what $display would print exists only while
//     simulation time advances and is permanently out of scope.
//   - Comment nodes: grouping is a tool convention.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/ErrorReporting/Location.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/design.h>
#include <hldb/identifier.h>
#include <hldb/module.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/preproc_macro_instance.h>
#include <hldb/source_file.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {

static constexpr std::string_view kMainFileName = "22.5.1--define-expansion_7.sv";
static constexpr uint32_t kUsageLine = 18u;

class DefineExpansion7Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.5.1--define-expansion_7.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static bool endsWith(std::string_view text, std::string_view suffix) {
    return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
  }

  // A collection HLC never allocated holds zero elements.
  template <typename T>
  static size_t countOf(const std::vector<T *> *collection) {
    return (collection == nullptr) ? 0u : collection->size();
  }

  static bool isWhiteSpace(char c) { return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n'); }

  static std::string withoutWhiteSpace(std::string_view text) {
    std::string result;
    for (const char c : text) {
      if (!isWhiteSpace(c)) result += c;
    }
    return result;
  }

  // True if HLC's catalog files this diagnostic type under an error-class
  // severity (FATAL, SYNTAX or ERROR).
  static bool isErrorClass(ErrorDefinition::ErrorType type) {
    const ErrorDefinition::ErrorMap &infoMap = ErrorDefinition::getErrorInfoMap();
    const ErrorDefinition::ErrorMap::const_iterator it = infoMap.find(type);
    if (it == infoMap.end()) return false;
    const ErrorDefinition::ErrorSeverity severity = it->second.m_severity;
    return (severity == ErrorDefinition::FATAL) || (severity == ErrorDefinition::SYNTAX) ||
           (severity == ErrorDefinition::ERROR);
  }

  // True if HLC's catalog files this diagnostic type under the elaboration
  // category.
  static bool isElaborationDiagnostic(ErrorDefinition::ErrorType type) {
    const ErrorDefinition::ErrorMap &infoMap = ErrorDefinition::getErrorInfoMap();
    const ErrorDefinition::ErrorMap::const_iterator it = infoMap.find(type);
    return (it != infoMap.end()) && (it->second.m_category == ErrorDefinition::ELAB);
  }

  // The line of a diagnostic's primary location, or 0 if it has none.
  static uint32_t lineOf(const Error &error) {
    return error.getLocations().empty() ? 0u : error.getLocations().front().m_line;
  }

  // Every error-class diagnostic, in report order.
  static std::vector<const Error *> errorClassDiagnostics() {
    std::vector<const Error *> result;
    if (m_session->getErrorContainer() == nullptr) return result;
    for (const Error &error : m_session->getErrorContainer()->getErrors()) {
      if (isErrorClass(error.getType())) result.emplace_back(&error);
    }
    return result;
  }

  // Every reported diagnostic of the given type, in report order.
  static std::vector<const Error *> errorsOfType(ErrorDefinition::ErrorType type) {
    std::vector<const Error *> result;
    if (m_session->getErrorContainer() == nullptr) return result;
    for (const Error &error : m_session->getErrorContainer()->getErrors()) {
      if (error.getType() == type) result.emplace_back(&error);
    }
    return result;
  }

  // "type@line:column" for every error-class diagnostic, for failure
  // messages.
  static std::string describeErrorClassDiagnostics() {
    std::string text;
    for (const Error *const error : errorClassDiagnostics()) {
      text += " " + std::to_string(static_cast<int32_t>(error->getType()));
      if (!error->getLocations().empty()) {
        const Location &location = error->getLocations().front();
        text += "@" + std::to_string(location.m_line) + ":" + std::to_string(location.m_column);
      }
    }
    return text.empty() ? std::string(" (none)") : text;
  }

  // Every source file of the design plus every file reachable through
  // includes, each listed once.
  static std::vector<const hldb::SourceFile *> allSourceFiles() {
    std::vector<const hldb::SourceFile *> result;
    if (m_design->getSourceFiles() != nullptr) {
      for (const hldb::SourceFile *const sf : *m_design->getSourceFiles()) {
        if (sf != nullptr) result.emplace_back(sf);
      }
    }
    for (size_t i = 0; i < result.size(); ++i) {
      if (result[i]->getIncludes() == nullptr) continue;
      for (const hldb::SourceFile *const inc : *result[i]->getIncludes()) {
        if ((inc != nullptr) && (std::find(result.begin(), result.end(), inc) == result.end())) {
          result.emplace_back(inc);
        }
      }
    }
    return result;
  }

  static std::vector<const hldb::PreprocMacroDefinition *> allMacroDefinitions() {
    std::vector<const hldb::PreprocMacroDefinition *> result;
    for (const hldb::SourceFile *const sf : allSourceFiles()) {
      if (sf->getPreprocMacroDefinitions() == nullptr) continue;
      for (const hldb::PreprocMacroDefinition *const md : *sf->getPreprocMacroDefinitions()) {
        result.emplace_back(md);
      }
    }
    return result;
  }

  static std::vector<const hldb::PreprocMacroInstance *> allMacroInstances() {
    std::vector<const hldb::PreprocMacroInstance *> result;
    for (const hldb::SourceFile *const sf : allSourceFiles()) {
      if (sf->getPreprocMacroInstances() == nullptr) continue;
      for (const hldb::PreprocMacroInstance *const mi : *sf->getPreprocMacroInstances()) {
        result.emplace_back(mi);
      }
    }
    return result;
  }

  static const hldb::PreprocMacroDefinition *findMacroDefinition(std::string_view name) {
    for (const hldb::PreprocMacroDefinition *const md : allMacroDefinitions()) {
      if ((md != nullptr) && (md->getName() == name)) return md;
    }
    return nullptr;
  }

  // The macro text of a definition with all white space removed, so the
  // result does not depend on how HLC splits the text into tokens.
  static std::string macroTextWithoutWhiteSpace(const hldb::PreprocMacroDefinition *md) {
    std::string text;
    if (md->getTokens() == nullptr) return text;
    for (const hldb::Identifier *const token : *md->getTokens()) {
      if (token != nullptr) text += withoutWhiteSpace(token->getName());
    }
    return text;
  }
};

// ===== Required by IEEE 1800-2023 Sec 22.5.1 =====

TEST_F(DefineExpansion7Test, FewerActualArgumentsIsAnErrorOnLine18) {
  // Sec 22.5.1: `D() gives one (empty) actual argument for two formal
  // arguments, and y has no default: "It shall be an error".
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GE(stats.nbFatal + stats.nbSyntax + stats.nbError, 1) << "illegal source must draw an error";
  size_t onUsageLine = 0u;
  for (const Error *const error : errorClassDiagnostics()) {
    if (lineOf(*error) == kUsageLine) ++onUsageLine;
  }
  EXPECT_GE(onUsageLine, 1u) << "no error-class diagnostic for `D() on line 18; error-class diagnostics:"
                             << describeErrorClassDiagnostics();
}

TEST_F(DefineExpansion7Test, MacroDDefinitionIsIntact) {
  // Line 17: `define D(x,y) initial $display("start", x , y, "end");
  // The definition is legal; only its usage on line 18 is not.
  ASSERT_EQ(allMacroDefinitions().size(), 1u) << "D is the only macro defined";
  const hldb::PreprocMacroDefinition *const md = findMacroDefinition("D");
  ASSERT_NE(md, nullptr) << "macro 'D' is not defined";
  EXPECT_TRUE(endsWith(md->getFile(), kMainFileName)) << "file: " << md->getFile();
  EXPECT_EQ(md->getStartLine(), 17u);
  ASSERT_EQ(countOf(md->getArguments()), 2u) << "two formal arguments, x and y (Sec 22.5.1)";
  ASSERT_NE(md->getArguments()->at(0), nullptr);
  ASSERT_NE(md->getArguments()->at(1), nullptr);
  EXPECT_EQ(md->getArguments()->at(0)->getName(), "x");
  EXPECT_EQ(md->getArguments()->at(1)->getName(), "y");
  EXPECT_EQ(macroTextWithoutWhiteSpace(md), "initial$display(\"start\",x,y,\"end\");")
      << "macro text is: initial $display(\"start\", x , y, \"end\");";
}

// ===== HLC's catalog entry for the rule (not an LRM requirement) =====

TEST_F(DefineExpansion7Test, ReportedAsMacroNoDefaultValue) {
  // HLC catalog check, not an LRM rule: HLC has a dedicated entry for a
  // remaining formal argument without a default, and should use it here.
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const std::vector<const Error *> errors = errorsOfType(ErrorDefinition::PP_MACRO_NO_DEFAULT_VALUE);
  ASSERT_EQ(errors.size(), 1u) << "error-class diagnostics:" << describeErrorClassDiagnostics();
  EXPECT_EQ(lineOf(*errors.front()), kUsageLine);
  EXPECT_TRUE(isErrorClass(ErrorDefinition::PP_MACRO_NO_DEFAULT_VALUE))
      << "\"It shall be an error\" -- the entry must be FATAL, SYNTAX or ERROR";
}

// ===== HLC error-recovery requirements (not defined by the LRM) =====

TEST_F(DefineExpansion7Test, SourceErrorsStayOnLine18) {
  // HLC recovery requirement, not an LRM rule: reporting the illegal usage
  // must not cascade into errors on other lines. Elaboration-category
  // diagnostics are left out: Sec 23.3.1 lets an elaborating run report the
  // missing top-level module on its own.
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  size_t elsewhere = 0u;
  for (const Error *const error : errorClassDiagnostics()) {
    if (isElaborationDiagnostic(error->getType())) continue;
    if (lineOf(*error) != kUsageLine) ++elsewhere;
  }
  EXPECT_EQ(elsewhere, 0u) << "error-class diagnostics:" << describeErrorClassDiagnostics()
                           << " -- source diagnostics off line 18 are a recovery cascade from the illegal usage";
}

TEST_F(DefineExpansion7Test, IllegalUsageIsStillRecorded) {
  // Line 18: `D() -- the parentheses hold one actual argument, which is
  // empty ("only one empty argument", Sec 22.5.1 example).
  const std::vector<const hldb::PreprocMacroInstance *> instances = allMacroInstances();
  ASSERT_EQ(instances.size(), 1u);
  const hldb::PreprocMacroInstance *const mi = instances.front();
  ASSERT_NE(mi, nullptr);
  EXPECT_EQ(mi->getName(), "D");
  EXPECT_TRUE(endsWith(mi->getFile(), kMainFileName)) << "file: " << mi->getFile();
  EXPECT_EQ(mi->getStartLine(), kUsageLine);
  EXPECT_EQ(mi->getStartColumn(), 1u);
  ASSERT_EQ(countOf(mi->getArguments()), 1u) << "\"()\" holds one empty actual argument";
  ASSERT_NE(mi->getArguments()->at(0), nullptr);
  EXPECT_EQ(withoutWhiteSpace(mi->getArguments()->at(0)->getName()), "") << "the one actual argument is empty";
  const hldb::PreprocMacroDefinition *const definition = findMacroDefinition("D");
  ASSERT_NE(definition, nullptr) << "macro 'D' is not defined";
  EXPECT_EQ(mi->getPreprocMacroDefinition(), definition);
}

TEST_F(DefineExpansion7Test, DesignHasNoModules) {
  // The file declares no design element, and the macro text (an initial
  // procedure) cannot become one at compilation-unit scope (Annex A.1.2).
  EXPECT_EQ(countOf(m_design->getAllModules()), 0u);
  EXPECT_EQ(countOf(m_design->getTopModules()), 0u) << "with no module there is no top-level instance";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
