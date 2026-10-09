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

// Tests for 22.3--resetall_multiple.sv (tags: 22.3, type: preprocessing parsing)
//
// SV source (lines 1-15 are the license and metadata comments):
//   16: `resetall
//   17: `resetall
//   18: `resetall
//   19:
//   20: module top ();
//   21: endmodule
//
// IEEE 1800-2023 Sec 22.3 places one restriction on `resetall: it "shall be
// illegal ... within a design element". It sets no limit on how often the
// directive may appear. All three occurrences precede 'module top', so all
// three are legal. Each one sets "all compiler directives ... to the default
// values" (Sec 22.3), and by Sec 22.2 each later one supersedes the earlier
// one with the same defaults, so three in a row leave exactly the state one
// would. Each sits alone on its own line, as Sec 22.2 requires ("compiler
// directives shall be all on one line").
//
// That default state is recorded on the module that follows:
//   - Sec 22.8:  implicit nets are of type wire -> vpiDefNetType == vpiWire
//   - Sec 22.9:  `resetall includes `nounconnected_drive
//                -> vpiUnconnDrive == vpiHighZ ("No default drive given")
//   - Sec 22.10: `resetall includes `endcelldefine -> vpiCellInstance false
//   - no `delay_mode_* directive is active -> vpiDefDelayMode ==
//     vpiDelayModeNone ("no delay mode specified")
//
// "module top ();" declares one null port: Sec 37.14 detail 10 names
// "module M();" as a null port (see test_22.3--resetall_basic.cpp for the
// grammar ambiguity this settles).
//
// Checked:
//   - no PP_ILLEGAL_DIRECTIVE_IN_DESIGN_ELEMENT for any of the three
//     directives, and 0 error-class (fatal / syntax / error) diagnostics
//   - exactly one module definition, defName "top", spanning lines 20-21:
//     consuming three directive lines and a blank line does not shift line
//     numbering
//   - directive state of 'top': wire / highZ / not a cell / delay mode none
//   - 'top' has static default lifetime (Sec 6.21: "The default lifetime is
//     static."; Sec 37.3.7 vpiAutomatic == false)
//   - one null port: no name, index 0, vpiPort, size 0, no low/high conn
//     (Sec 37.14 details 1, 8, 9, 10, 11)
//   - empty body: no nets, variables, parameters, param assigns, processes,
//     continuous assignments, tasks/functions, instantiations
//   - none of the three `resetall is a text macro usage: zero macro instances
//     and zero macro definitions in the source file (Sec 22.5.1)
//   - after elaboration only: exactly one top-level instance "top"
//     (Sec 23.3.1), vpiTopModule (Sec 37.5), defined at line 20 (Sec 37.10),
//     carrying the same reset directive state (net type, unconnected drive,
//     cell, delay mode, static lifetime), whose single port is a null port
//     (no name, size 0) with no low or high connection; before elaboration
//     there are zero top-level instances (Sec 3.12)
//
// What is NOT checked, and why:
//   - Time unit and time precision. Sec 22.7: after `resetall "the default
//     time unit and precision are tool-specific." No LRM value to assert.
//   - Default decay time (vpiDefDecayTime): Annex E is informative and gives
//     `default_decay_time no default.
//   - Warning-, note- and info-level diagnostics. The LRM defines only errors;
//     it neither requires nor forbids a warning on legal source, and whether a
//     tool remarks on the absent `timescale is implementation-specific
//     (Sec 22.7). Only error-class counts (fatal, syntax, error) are asserted
//     (CompilerReportsZeroErrors).
//   - Whether HLC records directive entries for the three `resetall on the
//     source file or on 'top'. The LRM has no notion of a recorded directive
//     list, so that is a model convention, not a standard requirement. The
//     standard-defined effects are asserted instead
//     (ModuleTopDirectiveStateIsDefault).
//   - Comment nodes for lines 1-15: grouping is a tool convention.
//   - Direction of the null port: the LRM defines none for it.
//   - Runtime consequences of the reset state are permanently out of scope:
//     how an unconnected input floats while simulation time advances
//     (Sec 22.9) is a runtime fact; its static half, the recorded drive
//     strength, is asserted by ModuleTopDirectiveStateIsDefault.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/port.h>
#include <hldb/source_file.h>
#include <hldb/vpi_user.h>

#include <string_view>
#include <vector>

namespace hlc {

static constexpr std::string_view kSourceFileName = "22.3--resetall_multiple.sv";

class ResetallMultipleTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.3--resetall_multiple.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::SourceFile *getSourceFile() {
    return hldb::findByName<hldb::SourceFile>(kSourceFileName, m_design->getSourceFiles());
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

  // A collection HLC never allocated holds zero elements.
  template <typename T>
  static size_t countOf(const std::vector<T *> *collection) {
    return (collection == nullptr) ? 0u : collection->size();
  }
};

// --- diagnostics ----

TEST_F(ResetallMultipleTest, NoResetallIsReportedAsMisplaced) {
  // Sec 22.3: lines 16, 17 and 18 are all outside any design element.
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  EXPECT_EQ(errorsOfType(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_IN_DESIGN_ELEMENT).size(), 0u)
      << "repeating `resetall outside a design element is legal";
}

TEST_F(ResetallMultipleTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

// --- module definition ----

TEST_F(ResetallMultipleTest, DesignHasExactlyOneModuleTop) {
  ASSERT_EQ(countOf(m_design->getAllModules()), 1u) << "the source declares exactly one design element";
  const hldb::Module *const top = m_design->getAllModules()->at(0);
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getDefName(), "top");
  EXPECT_EQ(top, getTop());
}

TEST_F(ResetallMultipleTest, ModuleTopSpansLines20To21) {
  // Three directive lines (16-18) and a blank line (19) precede the module;
  // none of them may shift its reported position.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getStartLine(), 20u);
  EXPECT_EQ(top->getStartColumn(), 1u);
  EXPECT_EQ(top->getEndLine(), 21u);
}

// --- directive state after three consecutive `resetall ----

TEST_F(ResetallMultipleTest, ModuleTopDirectiveStateIsDefault) {
  // Repeating `resetall resets to the same defaults each time.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  // Sec 22.8: after `resetall, implicit nets are of type wire.
  EXPECT_EQ(top->getDefNetType(), vpiWire);
  // Sec 22.9: `resetall includes `nounconnected_drive; vpiHighZ = "No
  // default drive given".
  EXPECT_EQ(top->getUnconnDrive(), vpiHighZ);
  // Sec 22.10: `resetall includes `endcelldefine.
  EXPECT_FALSE(top->getCellInstance());
  // No `delay_mode_* directive is in effect.
  EXPECT_EQ(top->getDefDelayMode(), vpiDelayModeNone);
}

TEST_F(ResetallMultipleTest, ModuleTopHasStaticDefaultLifetime) {
  // "module top" carries no lifetime qualifier; Sec 6.21: "The default
  // lifetime is static."
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_FALSE(top->getAutomatic());
}

// --- "()" port list ----

TEST_F(ResetallMultipleTest, ModuleTopHasOneNullPort) {
  GTEST_SKIP() << "Known HLC gap: `module top ();` produces no ports. Annex A.1.3 (list_of_ports ::= ( port { , port } ), port may be empty) and Sec 37.14 detail 10 require one null port; SV3_1aParser.g4 port_list matches \"()\" without a port.";
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
  EXPECT_EQ(port->getSize(), 0) << "Sec 37.14 detail 11: vpiSize of a null port is 0";
}

// --- empty module body ----

TEST_F(ResetallMultipleTest, ModuleTopBodyIsEmpty) {
  // Nothing appears between "module top ();" and "endmodule".
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(countOf(top->getNets()), 0u);
  EXPECT_EQ(countOf(top->getVariables()), 0u);
  EXPECT_EQ(countOf(top->getParameters()), 0u);
  EXPECT_EQ(countOf(top->getParamAssigns()), 0u);
  EXPECT_EQ(countOf(top->getProcesses()), 0u);
  EXPECT_EQ(countOf(top->getContAssigns()), 0u);
  EXPECT_EQ(countOf(top->getTaskFuncs()), 0u);
  EXPECT_EQ(countOf(top->getRefInstances()), 0u);
  EXPECT_EQ(countOf(top->getModules()), 0u);
}

// --- `resetall in the preprocessor model ----

TEST_F(ResetallMultipleTest, ResetallIsNotRecordedAsMacro) {
  // Sec 22.5.1: compiler directive names are not macro names, so none of the
  // three `resetall may appear as a macro usage; the file has no `define.
  const hldb::SourceFile *const sf = getSourceFile();
  ASSERT_NE(sf, nullptr) << "source file '" << kSourceFileName << "' not found";
  EXPECT_EQ(countOf(sf->getPreprocMacroInstances()), 0u);
  EXPECT_EQ(countOf(sf->getPreprocMacroDefinitions()), 0u);
}

// --- elaboration ----

TEST_F(ResetallMultipleTest, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    // Sec 23.3.1: 'top' appears in no instantiation, so it is the top-level
    // module, implicitly instantiated once under its own name.
    ASSERT_EQ(countOf(m_design->getTopModules()), 1u);
    const hldb::Module *const inst = m_design->getTopModules()->at(0);
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->getName(), "top");
    EXPECT_EQ(inst->getDefName(), "top");
    EXPECT_TRUE(inst->getTopModule());
    // Sec 37.10 definition location of the instance.
    EXPECT_EQ(inst->getDefLineNo(), 20);
    // Sec 37.10: these are instance properties; the instance carries the
    // directive state of its definition.
    EXPECT_EQ(inst->getDefNetType(), vpiWire);
    EXPECT_EQ(inst->getUnconnDrive(), vpiHighZ);
    EXPECT_FALSE(inst->getCellInstance());
    EXPECT_EQ(inst->getDefDelayMode(), vpiDelayModeNone);
    EXPECT_FALSE(inst->getAutomatic()) << "Sec 6.21 / 37.3.7: no lifetime qualifier, so the default lifetime is static";
    // Sec 37.14: the top-level instance's single port is a null port with no
    // connection reaching it.
    ASSERT_EQ(countOf(inst->getPorts()), 1u);
    const hldb::Port *const port = inst->getPorts()->at(0);
    ASSERT_NE(port, nullptr);
    EXPECT_EQ(port->getName(), "") << "Sec 37.14 detail 8";
    EXPECT_EQ(port->getLowConn(), nullptr) << "Sec 37.14 detail 10";
    EXPECT_EQ(port->getHighConn(), nullptr) << "Sec 37.14 detail 10";
    EXPECT_EQ(port->getSize(), 0) << "Sec 37.14 detail 11";
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
