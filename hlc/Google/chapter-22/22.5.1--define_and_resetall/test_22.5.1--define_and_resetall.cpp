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

// Tests for 22.5.1--define_and_resetall.sv (tags: 22.5.1,
// type: preprocessing simulation)
//
// SV source (lines 1-15 are the license and metadata comments):
//   16: `define SOMESTRING "somestring"
//   17: `resetall
//   18:
//   19: module top ();
//   20: initial begin
//   21: <7 spaces><TAB>$display(":assert:('%s' == '%s')", `SOMESTRING, "somestring");
//   22: end
//   23: endmodule
//
// IEEE 1800-2023 rules this fixture exercises:
//   - Sec 22.5: "The text macro facility is not affected by the compiler
//     directive `resetall." Sec 22.3: "Not all compiler directives have a
//     default value (e.g., `define and `include). Directives that do not have
//     a default are not affected by `resetall." So SOMESTRING, defined on
//     line 16, is still defined after the `resetall on line 17, and its usage
//     on line 21 expands normally.
//   - Sec 22.5.1: text_macro_definition ::= `define text_macro_name
//     macro_text. SOMESTRING has no parenthesized formal-argument list, so it
//     takes zero arguments; its macro text is the string literal
//     "somestring". The usage on line 21 is outside any string literal, so
//     it is substituted, and the 2nd and 3rd arguments of $display become the
//     same string literal.
//   - Sec 22.3: `resetall on line 17 is outside any design element, so it is
//     legal, and it sets every directive that has a default back to that
//     default before 'module top' is compiled:
//       - Sec 22.8:  implicit nets are of type wire -> vpiDefNetType vpiWire
//       - Sec 22.9:  `resetall includes the effects of `nounconnected_drive
//                    -> vpiUnconnDrive vpiHighZ (Annex K: "No default drive
//                    given")
//       - Sec 22.10: `resetall includes the effects of `endcelldefine
//                    -> vpiCellInstance false
//       - no `delay_mode_* directive is in effect -> vpiDefDelayMode
//         vpiDelayModeNone (Annex K: "no delay mode specified")
//   - Annex K (normative vpi_user.h): "#define vpiConstant 7 /* numerical
//     constant or string literal */" and "#define vpiStringConst 6 /* string
//     literal */". A string literal argument is a constant whose vpiConstType
//     is vpiStringConst. $display is a built-in system task (Sec 21.2), so
//     vpiUserDefn (Sec 37.42) is false.
//   - Sec 22.12: "The compiler shall maintain the current line number and
//     file name of the file being compiled." The directives on lines 16-17 do
//     not shift the lines that follow.
//   - Sec 37.14 detail 10: "module top ();" declares exactly one null port
//     (no name, index 0, vpiPort, size 0, no low or high connection).
//
// Checked:
//   - zero error-class diagnostics (fatal, syntax, error): `resetall is
//     outside any design element, and SOMESTRING is still defined when used
//   - exactly one macro definition, SOMESTRING (line 16), with zero formal
//     arguments and macro text "somestring" (with quotes)
//   - exactly one macro usage: SOMESTRING on line 21, with no actual
//     arguments, bound to the line-16 definition (the `resetall in between
//     does not undefine it); `resetall itself is not a macro usage
//     (Sec 22.5.1: directive names are not macro names)
//   - exactly one module definition, defName "top", lines 19-23, static
//     default lifetime (Sec 6.21, Sec 37.3.7), one null port
//   - directive state of 'top' after `resetall: defNetType wire,
//     unconnDrive highZ, not a cell, delay mode none
//   - 'top' holds exactly one process and nothing else (no nets, variables,
//     parameters, param assigns, continuous assignments, tasks/functions,
//     instantiations)
//   - that process is an initial procedure (line 20) whose statement is an
//     unnamed begin-end block (lines 20-22) holding exactly one $display
//     system task call, on line 21
//   - the $display has three arguments, all string constants: the 1st
//     contains ":assert:('%s' == '%s')", the 2nd (the expansion of
//     `SOMESTRING) has exactly the value of the 3rd (the literal
//     "somestring")
//   - after elaboration only: exactly one top-level instance "top"
//     (Sec 23.3.1), vpiTopModule (Sec 37.5), defined at line 19
//     (Sec 37.10), carrying the same reset directive state (net type,
//     unconnected drive, cell, delay mode, static lifetime), whose single
//     port is a null port (no name, size 0) with no low or high connection;
//     before elaboration there are zero top-level instances (Sec 3.12)
//
// What is NOT checked, and why:
//   - What $display prints, how '%s' is formatted, and the ":assert:"
//     comparison the sv-tests harness performs on the printed text. The
//     fixture is tagged "simulation", but all of that exists only while
//     simulation time advances and is permanently out of scope for a static
//     compiler. Its static half, the argument constants handed to $display,
//     is asserted by DisplayReceivesExpandedMacroString.
//   - Columns on line 21. The line starts with spaces and a tab, and the LRM
//     defines no tab width, so only line numbers are asserted there.
//   - The exact spelling of a string constant's stored value (prefix,
//     quotes). The LRM defines the literal's characters, not HLC's storage
//     format, so values are checked by containment and by equality between
//     the two "somestring" literals.
//   - vpiSize of the string constants: the VPI clause does not define it for
//     string constants.
//   - PreprocMacroDefinition::getType() and PreprocMacroInstance::getBody(),
//     getItems() and getObjects(): HLDB fields with no LRM counterpart.
//   - Whether HLC records directive entries for `define or `resetall. The LRM
//     has no notion of a recorded directive list; the standard-defined
//     effects are asserted instead (ModuleTopDirectiveStateIsDefault and the
//     macro tests).
//   - Time unit and time precision. Sec 22.7: after `resetall "the default
//     time unit and precision are tool-specific." No LRM value to assert.
//   - Default decay time (vpiDefDecayTime): Annex E is informative and gives
//     `default_decay_time no default.
//   - Warning-, note- and info-level diagnostics. The LRM defines only
//     errors; it neither requires nor forbids warnings on legal source.
//   - Comment nodes: grouping is a tool convention.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/identifier.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/port.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/preproc_macro_instance.h>
#include <hldb/source_file.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace hlc {

static constexpr std::string_view kMainFileName = "22.5.1--define_and_resetall.sv";

class DefineAndResetallTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.5.1--define_and_resetall.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static bool endsWith(std::string_view text, std::string_view suffix) {
    return (text.size() >= suffix.size()) && (text.substr(text.size() - suffix.size()) == suffix);
  }

  static bool contains(std::string_view text, std::string_view part) {
    return text.find(part) != std::string_view::npos;
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
      if (token == nullptr) continue;
      for (const char c : token->getName()) {
        if ((c != ' ') && (c != '\t') && (c != '\r') && (c != '\n')) text += c;
      }
    }
    return text;
  }

  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  // The begin-end block of the single initial procedure in 'top'.
  static const hldb::Begin *getInitialBlock() {
    const hldb::Module *const top = getTop();
    if ((top == nullptr) || (countOf(top->getProcesses()) != 1u)) return nullptr;
    const hldb::Initial *const initial = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (initial == nullptr) return nullptr;
    return initial->getStmt<hldb::Begin>();
  }

  // The single statement of the initial block, as a system task call.
  static const hldb::SysTaskCall *getDisplayCall() {
    const hldb::Begin *const block = getInitialBlock();
    if ((block == nullptr) || (countOf(block->getStmts()) != 1u)) return nullptr;
    return any_cast<hldb::SysTaskCall>(block->getStmts()->at(0));
  }

  // The index-th argument of a call, as a constant.
  static const hldb::Constant *getConstantArgument(const hldb::SysTaskCall *call, size_t index) {
    if ((call == nullptr) || (countOf(call->getArguments()) <= index)) return nullptr;
    return any_cast<hldb::Constant>(call->getArguments()->at(index));
  }
};

// --- diagnostics ----

TEST_F(DefineAndResetallTest, CompilerReportsZeroErrors) {
  // Sec 22.3: `resetall on line 17 is outside any design element. Sec 22.5:
  // it leaves SOMESTRING defined, so its usage on line 21 is not undefined.
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

// --- `define survives `resetall ----

TEST_F(DefineAndResetallTest, SomeStringDefinition) {
  // Line 16: `define SOMESTRING "somestring"
  ASSERT_EQ(allMacroDefinitions().size(), 1u) << "SOMESTRING is the only macro defined";
  const hldb::PreprocMacroDefinition *const md = findMacroDefinition("SOMESTRING");
  ASSERT_NE(md, nullptr) << "macro 'SOMESTRING' is not defined";
  EXPECT_TRUE(endsWith(md->getFile(), kMainFileName)) << "file: " << md->getFile();
  EXPECT_EQ(md->getStartLine(), 16u);
  EXPECT_EQ(countOf(md->getArguments()), 0u) << "no formal-argument list (Sec 22.5.1)";
  EXPECT_EQ(macroTextWithoutWhiteSpace(md), "\"somestring\"") << "macro text is the string literal \"somestring\"";
}

TEST_F(DefineAndResetallTest, SomeStringIsUsedAfterResetall) {
  // Sec 22.5: `resetall does not affect text macros, so the usage on line 21
  // still resolves to the definition on line 16. `resetall is a directive,
  // not a macro usage, so SOMESTRING is the only usage.
  const std::vector<const hldb::PreprocMacroInstance *> instances = allMacroInstances();
  ASSERT_EQ(instances.size(), 1u) << "`SOMESTRING is the only macro usage; `resetall is a directive";
  const hldb::PreprocMacroInstance *const mi = instances.front();
  ASSERT_NE(mi, nullptr);
  EXPECT_EQ(mi->getName(), "SOMESTRING");
  EXPECT_TRUE(endsWith(mi->getFile(), kMainFileName)) << "file: " << mi->getFile();
  EXPECT_EQ(mi->getStartLine(), 21u);
  EXPECT_EQ(countOf(mi->getArguments()), 0u) << "`SOMESTRING is used without actual arguments";
  // Name binding across the `resetall on line 17.
  const hldb::PreprocMacroDefinition *const definition = findMacroDefinition("SOMESTRING");
  ASSERT_NE(definition, nullptr) << "macro 'SOMESTRING' is not defined";
  EXPECT_EQ(mi->getPreprocMacroDefinition(), definition);
}

// --- module definition ----

TEST_F(DefineAndResetallTest, DesignHasExactlyOneModuleTop) {
  ASSERT_EQ(countOf(m_design->getAllModules()), 1u) << "the source declares exactly one design element";
  const hldb::Module *const top = m_design->getAllModules()->at(0);
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getDefName(), "top");
  EXPECT_EQ(top, getTop());
}

TEST_F(DefineAndResetallTest, ModuleTopSpansLines19To23) {
  // Sec 22.12: the directives on lines 16-17 do not shift the lines after
  // them.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(endsWith(top->getFile(), kMainFileName)) << "file: " << top->getFile();
  EXPECT_EQ(top->getStartLine(), 19u);
  EXPECT_EQ(top->getStartColumn(), 1u);
  EXPECT_EQ(top->getEndLine(), 23u);
}

TEST_F(DefineAndResetallTest, ModuleTopDirectiveStateIsDefault) {
  // Sec 22.3: `resetall on line 17 sets every directive that has a default
  // back to it before 'module top' is compiled.
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

TEST_F(DefineAndResetallTest, ModuleTopHasStaticDefaultLifetime) {
  // "module top" carries no lifetime qualifier; Sec 6.21: "The default
  // lifetime is static."
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_FALSE(top->getAutomatic());
}

TEST_F(DefineAndResetallTest, ModuleTopHasOneNullPort) {
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

TEST_F(DefineAndResetallTest, ModuleTopHoldsOnlyOneProcess) {
  // The body is one initial procedure; nothing is declared or instantiated.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(countOf(top->getProcesses()), 1u);
  EXPECT_EQ(countOf(top->getNets()), 0u);
  EXPECT_EQ(countOf(top->getVariables()), 0u);
  EXPECT_EQ(countOf(top->getParameters()), 0u);
  EXPECT_EQ(countOf(top->getParamAssigns()), 0u);
  EXPECT_EQ(countOf(top->getContAssigns()), 0u);
  EXPECT_EQ(countOf(top->getTaskFuncs()), 0u);
  EXPECT_EQ(countOf(top->getRefInstances()), 0u);
  EXPECT_EQ(countOf(top->getModules()), 0u);
}

// --- initial procedure ----

TEST_F(DefineAndResetallTest, InitialBlockHoldsOneDisplayCall) {
  // Lines 20-22: initial begin $display(...); end
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_EQ(countOf(top->getProcesses()), 1u);
  const hldb::Initial *const initial = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(initial, nullptr) << "the only process is not an initial procedure";
  EXPECT_EQ(initial->getVpiType(), static_cast<uint32_t>(vpiInitial));
  EXPECT_EQ(initial->getStartLine(), 20u);

  const hldb::Begin *const block = getInitialBlock();
  ASSERT_NE(block, nullptr) << "the initial procedure's statement is not a begin-end block";
  EXPECT_EQ(block->getVpiType(), static_cast<uint32_t>(vpiBegin)) << "the block has no label, so it is unnamed";
  EXPECT_EQ(block->getStartLine(), 20u);
  EXPECT_EQ(block->getEndLine(), 22u);
  ASSERT_EQ(countOf(block->getStmts()), 1u);

  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr) << "the statement is not a system task call";
  EXPECT_EQ(call->getName(), "$display");
  EXPECT_FALSE(call->getUserDefn()) << "$display is a built-in system task (Sec 21.2)";
  EXPECT_EQ(call->getStartLine(), 21u);
}

TEST_F(DefineAndResetallTest, DisplayReceivesExpandedMacroString) {
  // Line 21: $display(":assert:('%s' == '%s')", `SOMESTRING, "somestring");
  // `SOMESTRING expands to the string literal "somestring" (Sec 22.5.1), so
  // the 2nd and 3rd arguments are the same string literal.
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr);
  ASSERT_EQ(countOf(call->getArguments()), 3u);
  const hldb::Constant *const format = getConstantArgument(call, 0);
  const hldb::Constant *const expanded = getConstantArgument(call, 1);
  const hldb::Constant *const literal = getConstantArgument(call, 2);
  ASSERT_NE(format, nullptr) << "argument 0 is not a constant";
  ASSERT_NE(expanded, nullptr) << "argument 1 (`SOMESTRING) did not expand to a constant";
  ASSERT_NE(literal, nullptr) << "argument 2 is not a constant";
  EXPECT_EQ(format->getConstType(), vpiStringConst) << "Annex K: #define vpiStringConst 6 /* string literal */";
  EXPECT_EQ(expanded->getConstType(), vpiStringConst);
  EXPECT_EQ(literal->getConstType(), vpiStringConst);
  EXPECT_TRUE(contains(format->getValue(), ":assert:('%s' == '%s')")) << "value: " << format->getValue();
  EXPECT_TRUE(contains(literal->getValue(), "somestring")) << "value: " << literal->getValue();
  EXPECT_EQ(expanded->getValue(), literal->getValue()) << "`SOMESTRING must expand to the literal \"somestring\"";
}

// --- elaboration ----

TEST_F(DefineAndResetallTest, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    // Sec 23.3.1: 'top' appears in no instantiation, so it is the top-level
    // module, implicitly instantiated once under its own name.
    ASSERT_EQ(countOf(m_design->getTopModules()), 1u);
    const hldb::Module *const inst = m_design->getTopModules()->at(0);
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->getName(), "top");
    EXPECT_EQ(inst->getDefName(), "top");
    EXPECT_TRUE(inst->getTopModule());
    // Sec 37.10 definition location of the instance (line 19).
    EXPECT_EQ(inst->getDefLineNo(), 19);
    EXPECT_TRUE(endsWith(inst->getDefFile(), kMainFileName)) << "defFile: " << inst->getDefFile();
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
