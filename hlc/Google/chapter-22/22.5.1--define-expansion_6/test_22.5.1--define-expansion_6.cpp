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

// Tests for 22.5.1--define-expansion_6.sv (tags: 22.5.1, type: preprocessing)
//
// SV source (lines 1-16 are the license and metadata comments):
//   17: `define D(x,y) initial $display("start", x , y, "end");
//   18: module top ();
//   19: `D("msg1")
//   20: endmodule
//
// The fixture's metadata says ":should_fail_because: If fewer actual
// arguments are specified than the number of formal arguments and all the
// remaining formal arguments have defaults, then the defaults are substituted
// for the additional formal arguments. It shall be an error if any of the
// remaining formal arguments does not have a default specified.", quoting
// IEEE 1800-2023 Sec 22.5.1. The same clause's "Example macro without
// defaults" lists this exact usage:
//   `D("msg1")
//     // illegal, only one argument
//
// IEEE 1800-2023 rules this fixture exercises:
//   - Sec 22.5.1: line 17 defines D with two formal arguments, x and y in
//     that order, neither with a default, and macro text
//     "initial $display("start", x , y, "end");". The definition itself is
//     legal.
//   - Line 19 uses D with one actual argument, "msg1". There is no comma, so
//     there is no second (empty) actual argument; y is a remaining formal
//     argument without a default, so the usage "shall be an error". A
//     compile-time error may be reported at any time prior to simulation
//     (Sec 3.12).
//
// HLC's error catalog has an entry whose name is this rule,
// PP_MACRO_NO_DEFAULT_VALUE (the opposite case, more actual than formal
// arguments, is PP_TOO_MANY_ARGS_MACRO). The LRM requires only an error, so
// the LRM check identifies the diagnostic by severity and line; a separate
// test checks that HLC files it under its own dedicated entry.
//
// The LRM gives illegal source no semantics, so everything beyond that error
// is an HLC error-recovery expectation, not an LRM requirement.
//
// Checked -- required by IEEE 1800-2023 Sec 22.5.1:
//   - at least one error-class diagnostic (FATAL, SYNTAX or ERROR) on line
//     19, and an error-class total of at least 1
//   - the legal definition on line 17 is intact: exactly one macro
//     definition, D, with formal arguments x and y in that order and macro
//     text "initial $display("start", x , y, "end");" (compared with white
//     space removed)
//
// Checked -- HLC's catalog entry for the rule (NOT an LRM requirement):
//   - exactly one PP_MACRO_NO_DEFAULT_VALUE, on line 19, and that entry has
//     an error-class severity
//
// Checked -- HLC error-recovery requirements (NOT defined by the LRM,
// which gives illegal source no semantics): after reporting the illegal
// usage, HLC compiles the rest of the file normally. So:
//   - no cascade: every error-class diagnostic is on line 19
//   - the usage is still recorded: D on line 19, column 1, with exactly one
//     actual argument "msg1" (with quotes), bound to the line-17 definition
//   - 'top' is still one intact module:
//     - exactly one module definition, defName "top", lines 18-20
//     - static default lifetime (Sec 6.21, Sec 37.3.7)
//     - one null port: no name, index 0, vpiPort, size 0, no low/high conn
//       (Sec 37.14 details 1, 8, 9, 10, 11; detail 10 names "module M();"
//       as a null port)
//     - no declarations or instantiations: no nets, variables, parameters,
//       param assigns, continuous assignments, tasks/functions,
//       instantiations (the macro text declares none of these, whatever HLC
//       does with the illegal usage)
//     - after elaboration only: exactly one top-level instance "top"
//       (Sec 23.3.1), vpiTopModule (Sec 37.5), defined at line 18
//       (Sec 37.10), static lifetime, whose single port is a null port with
//       no low or high connection; before elaboration there are zero
//       top-level instances (Sec 3.12)
//
// What is NOT checked, and why:
//   - What, if anything, the illegal usage expands to, and so whether 'top'
//     holds an initial procedure. The LRM gives illegal source no semantics,
//     and no single recovery is more correct than another.
//   - The column of the diagnostic and its message text. The LRM does not
//     say where a diagnostic must point, and the wording is HLC's own.
//   - The compiler process's exit status (what ":should_fail_because" checks
//     in other harnesses) is not part of the HLDB model; the in-process
//     equivalent, an error-class diagnostic on line 19, is asserted by
//     FewerActualArgumentsIsAnErrorOnLine19.
//   - PreprocMacroDefinition::getType() and PreprocMacroInstance::getBody(),
//     getItems() and getObjects(): HLDB fields with no LRM counterpart.
//   - Whether `define is recorded as a directive entry. The LRM has no notion
//     of a recorded directive list.
//   - Warning-, note- and info-level diagnostics. The LRM defines only
//     errors; it neither requires nor forbids warnings.
//   - Time unit and precision: no `timescale in the source, so tool-specific
//     (Sec 22.7).
//   - Directive defaults (vpiDefNetType, vpiUnconnDrive, vpiCellInstance,
//     vpiDefDelayMode): no directive that sets them appears here; the 22.3
//     `resetall fixtures assert the default values.
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

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/identifier.h>
#include <hldb/module.h>
#include <hldb/port.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/preproc_macro_instance.h>
#include <hldb/source_file.h>
#include <hldb/vpi_user.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace hlc {

static constexpr std::string_view kMainFileName = "22.5.1--define-expansion_6.sv";
static constexpr uint32_t kUsageLine = 19u;

class DefineExpansion6Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.5.1--define-expansion_6.hlc"}); }
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

  // vpi_get(vpiSize, object) through HLDB's generic VPI property query
  // (HLDB has no Ports::getSize() accessor); -1 if HLDB returns no integer.
  static int64_t vpiSizeOf(const hldb::Any *object) {
    const hldb::Any::vpi_property_value_t value = object->getVpiPropertyValue(vpiSize);
    return std::holds_alternative<int64_t>(value) ? std::get<int64_t>(value) : -1;
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

  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }
};

// ===== Required by IEEE 1800-2023 Sec 22.5.1 =====

TEST_F(DefineExpansion6Test, FewerActualArgumentsIsAnErrorOnLine19) {
  // Sec 22.5.1: `D("msg1") gives one actual argument for two formal
  // arguments, and y has no default: "It shall be an error".
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GE(stats.nbFatal + stats.nbSyntax + stats.nbError, 1) << "illegal source must draw an error";
  size_t onUsageLine = 0u;
  for (const Error *const error : errorClassDiagnostics()) {
    if (lineOf(*error) == kUsageLine) ++onUsageLine;
  }
  EXPECT_GE(onUsageLine, 1u) << "no error-class diagnostic for `D(\"msg1\") on line 19; error-class diagnostics:"
                             << describeErrorClassDiagnostics();
}

TEST_F(DefineExpansion6Test, MacroDDefinitionIsIntact) {
  // Line 17: `define D(x,y) initial $display("start", x , y, "end");
  // The definition is legal; only its usage on line 19 is not.
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

TEST_F(DefineExpansion6Test, ReportedAsMacroNoDefaultValue) {
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

TEST_F(DefineExpansion6Test, ErrorsStayOnLine19) {
  // HLC recovery requirement, not an LRM rule: reporting the illegal usage
  // must not cascade into errors on other lines.
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  size_t elsewhere = 0u;
  for (const Error *const error : errorClassDiagnostics()) {
    if (lineOf(*error) != kUsageLine) ++elsewhere;
  }
  EXPECT_EQ(elsewhere, 0u) << "error-class diagnostics:" << describeErrorClassDiagnostics()
                           << " -- diagnostics off line 19 are a recovery cascade from the illegal usage";
}

TEST_F(DefineExpansion6Test, IllegalUsageIsStillRecorded) {
  // Line 19: `D("msg1") -- one actual argument, no comma.
  const std::vector<const hldb::PreprocMacroInstance *> instances = allMacroInstances();
  ASSERT_EQ(instances.size(), 1u);
  const hldb::PreprocMacroInstance *const mi = instances.front();
  ASSERT_NE(mi, nullptr);
  EXPECT_EQ(mi->getName(), "D");
  EXPECT_TRUE(endsWith(mi->getFile(), kMainFileName)) << "file: " << mi->getFile();
  EXPECT_EQ(mi->getStartLine(), kUsageLine);
  EXPECT_EQ(mi->getStartColumn(), 1u);
  ASSERT_EQ(countOf(mi->getArguments()), 1u) << "only one actual argument is written";
  ASSERT_NE(mi->getArguments()->at(0), nullptr);
  EXPECT_EQ(withoutWhiteSpace(mi->getArguments()->at(0)->getName()), "\"msg1\"");
  const hldb::PreprocMacroDefinition *const definition = findMacroDefinition("D");
  ASSERT_NE(definition, nullptr) << "macro 'D' is not defined";
  EXPECT_EQ(mi->getPreprocMacroDefinition(), definition);
}

TEST_F(DefineExpansion6Test, DesignHasExactlyOneModuleTop) {
  ASSERT_EQ(countOf(m_design->getAllModules()), 1u) << "the source declares exactly one design element";
  const hldb::Module *const top = m_design->getAllModules()->at(0);
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getDefName(), "top");
  EXPECT_EQ(top, getTop());
}

TEST_F(DefineExpansion6Test, ModuleTopSpansLines18To20) {
  // Sec 22.12: whatever the illegal usage on line 19 becomes, 'module top'
  // stays at line 18 and 'endmodule' at line 20.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(endsWith(top->getFile(), kMainFileName)) << "file: " << top->getFile();
  EXPECT_EQ(top->getStartLine(), 18u);
  EXPECT_EQ(top->getStartColumn(), 1u);
  EXPECT_EQ(top->getEndLine(), 20u);
}

TEST_F(DefineExpansion6Test, ModuleTopHasStaticDefaultLifetime) {
  // "module top" carries no lifetime qualifier; Sec 6.21: "The default
  // lifetime is static."
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_FALSE(top->getAutomatic());
}

TEST_F(DefineExpansion6Test, ModuleTopHasOneNullPort) {
  // Sec 37.14 detail 10 names "module M();" as declaring a null port.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_EQ(countOf(top->getPorts()), 1u) << "\"()\" declares one null port (Sec 37.14 detail 10)";
  const hldb::Port *const port = top->getPorts()->at(0);
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), "") << "Sec 37.14 detail 8: a null port has no name";
  EXPECT_FALSE(port->getExplicitName());
  EXPECT_EQ(port->getPortIndex(), 0) << "Sec 37.14 detail 9: the first port has index 0";
  EXPECT_EQ(port->getPortType(), vpiPort) << "Sec 37.14 detail 1";
  EXPECT_EQ(port->getLowConn(), nullptr) << "Sec 37.14 detail 10: a null port has no low connection";
  EXPECT_EQ(port->getHighConn(), nullptr) << "Sec 37.14 detail 10: 'top' is never instantiated";
  EXPECT_EQ(vpiSizeOf(port), 0) << "Sec 37.14 detail 11: vpiSize of a null port is 0";
}

TEST_F(DefineExpansion6Test, ModuleTopHasNoDeclarations) {
  // The macro text declares nothing, so whatever HLC does with the illegal
  // usage, 'top' gains no declaration or instantiation.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(countOf(top->getNets()), 0u);
  EXPECT_EQ(countOf(top->getVariables()), 0u);
  EXPECT_EQ(countOf(top->getParameters()), 0u);
  EXPECT_EQ(countOf(top->getParamAssigns()), 0u);
  EXPECT_EQ(countOf(top->getContAssigns()), 0u);
  EXPECT_EQ(countOf(top->getTaskFuncs()), 0u);
  EXPECT_EQ(countOf(top->getRefInstances()), 0u);
  EXPECT_EQ(countOf(top->getModules()), 0u);
}

TEST_F(DefineExpansion6Test, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    // Sec 23.3.1: 'top' appears in no instantiation, so it is the top-level
    // module, implicitly instantiated once under its own name.
    ASSERT_EQ(countOf(m_design->getTopModules()), 1u);
    const hldb::Module *const inst = m_design->getTopModules()->at(0);
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->getName(), "top");
    EXPECT_EQ(inst->getDefName(), "top");
    EXPECT_TRUE(inst->getTopModule());
    // Sec 37.10 definition location of the instance (line 18).
    EXPECT_EQ(inst->getDefLineNo(), 18);
    EXPECT_TRUE(endsWith(inst->getDefFile(), kMainFileName)) << "defFile: " << inst->getDefFile();
    EXPECT_FALSE(inst->getAutomatic()) << "Sec 6.21 / 37.3.7: no lifetime qualifier, so the default lifetime is static";
    // Sec 37.14: the top-level instance's single port is a null port with no
    // connection reaching it.
    ASSERT_EQ(countOf(inst->getPorts()), 1u);
    const hldb::Port *const port = inst->getPorts()->at(0);
    ASSERT_NE(port, nullptr);
    EXPECT_EQ(port->getName(), "") << "Sec 37.14 detail 8";
    EXPECT_EQ(port->getLowConn(), nullptr) << "Sec 37.14 detail 10";
    EXPECT_EQ(port->getHighConn(), nullptr) << "Sec 37.14 detail 10";
    EXPECT_EQ(vpiSizeOf(port), 0) << "Sec 37.14 detail 11";
  } else {
    // Sec 3.12: the instance tree is built by elaboration; before it there
    // are no top-level instances.
    EXPECT_EQ(countOf(m_design->getTopModules()), 0u);
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
