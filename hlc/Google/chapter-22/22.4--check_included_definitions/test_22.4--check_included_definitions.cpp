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

// Tests for 22.4--check_included_definitions.sv (tags: 22.4,
// type: preprocessing parsing)
//
// SV source, 22.4--check_included_definitions.sv (lines 1-15 are comments):
//   16: `include "include_directory/defs.sv"
//   17: module top ();
//   18: initial begin
//   19:         $display(":assert:(`TWO_PLUS_TWO == 5)");
//   20: <TAB>$display(":assert:('%s' == '%s')", `define_var, "define_var");
//   21: end
//   22:
//   23: endmodule
//
// Included file, include_directory/defs.sv (lines 1-16 are comments/blank):
//   17: `define define_var "define_var"
//   18: `define TWO_PLUS_TWO 5
//
// IEEE 1800-2023 rules this fixture exercises:
//   - Sec 22.4: "The result is as though the contents of the included source
//     file appear in place of the `include compiler directive." So both
//     macros are defined before 'module top' and are visible inside it.
//     A quoted relative filename is searched in the compiler's current
//     working directory and optionally user-specified locations.
//   - Sec 22.5.1: text_macro_definition ::= `define text_macro_name
//     macro_text. Neither name has a parenthesized formal-argument list, so
//     both macros take zero arguments. The macro text of define_var is the
//     string literal "define_var"; that of TWO_PLUS_TWO is the number 5.
//   - Sec 22.5.1: "Macro substitution and argument substitution shall not
//     occur within string literals." The `TWO_PLUS_TWO on line 19 is inside a
//     string literal, so it is not a macro usage and the argument stays
//     ":assert:(`TWO_PLUS_TWO == 5)" character for character.
//   - The `define_var on line 20 is outside any string literal, so it is the
//     only macro usage in the design. It expands to the string literal
//     "define_var", making the 2nd and 3rd arguments of that $display the
//     same string literal.
//   - Sec 22.12: "The compiler shall maintain the current line number and
//     file name of the file being compiled." The 18 lines of defs.sv must not
//     shift the main file: 'module top' stays at line 17 of the main file,
//     and the macro definitions are reported in defs.sv at lines 17 and 18.
//   - Annex K (normative vpi_user.h): "#define vpiConstant 7 /* numerical
//     constant or string literal */" and "#define vpiStringConst 6 /* string
//     literal */". A string literal argument is a constant whose vpiConstType
//     is vpiStringConst. $display is a built-in system task (Sec 21.2), so
//     vpiUserDefn (Sec 37.42) is false.
//   - Sec 37.14 detail 10: "module top ();" declares one null port (see
//     test_22.3--resetall_basic.cpp for the reasoning).
//
// Checked:
//   - zero error-class diagnostics (fatal, syntax, error): the include
//     resolves and both macros are defined before use
//   - the main source file includes exactly one file, defs.sv
//   - exactly two macro definitions, define_var (defs.sv line 17) and
//     TWO_PLUS_TWO (defs.sv line 18), each with zero formal arguments and
//     macro text "define_var" (with quotes) and 5 respectively
//   - exactly one macro usage: define_var, in the main file at line 20, with
//     no actual arguments, bound to the define_var definition from defs.sv
//   - exactly one module definition, defName "top", main file lines 17-23,
//     static default lifetime (Sec 6.21), one null port
//   - 'top' holds exactly one process and nothing else (no nets, variables,
//     parameters, param assigns, continuous assignments, tasks/functions,
//     instantiations)
//   - that process is an initial procedure (line 18) whose statement is an
//     unnamed begin-end block (lines 18-21) holding exactly two $display
//     system task calls, on lines 19 and 20
//   - 1st $display: one argument, a string constant whose text contains
//     ":assert:(`TWO_PLUS_TWO == 5)" verbatim
//   - 2nd $display: three arguments, all string constants; the 1st contains
//     ":assert:('%s' == '%s')", the 2nd (the expansion of `define_var) has
//     exactly the value of the 3rd (the literal "define_var")
//   - after elaboration only: exactly one top-level instance "top"
//     (Sec 23.3.1), vpiTopModule (Sec 37.5), defined at main-file line 17
//     (Sec 37.10); before elaboration there are zero top-level instances
//     (Sec 3.12)
//
// What is NOT checked, and why:
//   - What the two $display calls print, how '%s' is formatted, and the
//     ":assert:" comparison the sv-tests harness performs on the printed text.
//     All of that exists only while simulation time advances and is
//     permanently out of scope for a static compiler. Its static half, the
//     argument constants handed to $display, is asserted by
//     FirstDisplayKeepsMacroTextInsideString and
//     SecondDisplayReceivesExpandedMacroString.
//   - The directory the include was resolved from. Sec 22.4 leaves the search
//     locations to the working directory and user-specified paths, which are
//     command-line facts, not source facts; only the included file's name is
//     asserted (MainFileIncludesDefsSv).
//   - Columns on line 20. The line starts with a tab, and the LRM defines no
//     tab width, so only line numbers are asserted there.
//   - The exact spelling of a string constant's stored value (prefix, quotes).
//     The LRM defines the literal's characters, not HLC's storage format, so
//     values are checked by the literal's text being contained in them and by
//     equality between the two "define_var" literals.
//   - vpiSize of the string constants. The VPI clause does not define it for
//     string constants.
//   - PreprocMacroDefinition::getType() and PreprocMacroInstance::getBody(),
//     getItems() and getObjects(). These are HLDB fields with no counterpart
//     in the LRM, so there is no standard value to assert.
//   - Whether the `include or `define directives are recorded as directive
//     entries. The LRM has no notion of a recorded directive list.
//   - Warning-, note- and info-level diagnostics. The LRM defines only
//     errors; it neither requires nor forbids warnings on legal source.
//   - Comment nodes in either file: grouping is a tool convention.
//   - Time unit and precision: no `timescale anywhere, so tool-specific
//     (Sec 22.7).

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
#include <vector>

namespace hlc {

static constexpr std::string_view kMainFileName = "22.4--check_included_definitions.sv";
static constexpr std::string_view kIncludedFileName = "defs.sv";

class CheckIncludedDefinitionsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.4--check_included_definitions.hlc"}); }
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

  static const hldb::SourceFile *findSourceFile(std::string_view suffix) {
    for (const hldb::SourceFile *const sf : allSourceFiles()) {
      if (endsWith(sf->getName(), suffix)) return sf;
    }
    return nullptr;
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

  // The index-th statement of the initial block, as a system task call.
  static const hldb::SysTaskCall *getDisplayCall(size_t index) {
    const hldb::Begin *const block = getInitialBlock();
    if ((block == nullptr) || (countOf(block->getStmts()) <= index)) return nullptr;
    return any_cast<hldb::SysTaskCall>(block->getStmts()->at(index));
  }

  // The index-th argument of a call, as a constant.
  static const hldb::Constant *getConstantArgument(const hldb::SysTaskCall *call, size_t index) {
    if ((call == nullptr) || (countOf(call->getArguments()) <= index)) return nullptr;
    return any_cast<hldb::Constant>(call->getArguments()->at(index));
  }
};

// --- diagnostics ----

TEST_F(CheckIncludedDefinitionsTest, CompilerReportsZeroErrors) {
  // The include resolves, and both macros are defined (in defs.sv) before
  // their use, so the source is legal.
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

// --- `include ----

TEST_F(CheckIncludedDefinitionsTest, MainFileIncludesDefsSv) {
  // Sec 22.4: line 16 includes exactly one file, include_directory/defs.sv.
  const hldb::SourceFile *const mainFile = findSourceFile(kMainFileName);
  ASSERT_NE(mainFile, nullptr) << "source file '" << kMainFileName << "' not found";
  ASSERT_EQ(countOf(mainFile->getIncludes()), 1u);
  const hldb::SourceFile *const included = mainFile->getIncludes()->at(0);
  ASSERT_NE(included, nullptr);
  EXPECT_TRUE(endsWith(included->getName(), kIncludedFileName)) << "included: " << included->getName();
}

// --- macro definitions from defs.sv ----

TEST_F(CheckIncludedDefinitionsTest, DesignHasExactlyTwoMacroDefinitions) {
  // defs.sv defines define_var and TWO_PLUS_TWO; the main file defines none.
  EXPECT_EQ(allMacroDefinitions().size(), 2u);
}

TEST_F(CheckIncludedDefinitionsTest, DefineVarDefinition) {
  // defs.sv line 17: `define define_var "define_var"
  const hldb::PreprocMacroDefinition *const md = findMacroDefinition("define_var");
  ASSERT_NE(md, nullptr) << "macro 'define_var' is not defined";
  EXPECT_TRUE(endsWith(md->getFile(), kIncludedFileName)) << "file: " << md->getFile();
  EXPECT_EQ(md->getStartLine(), 17u);
  EXPECT_EQ(countOf(md->getArguments()), 0u) << "no formal-argument list (Sec 22.5.1)";
  EXPECT_EQ(macroTextWithoutWhiteSpace(md), "\"define_var\"") << "macro text is the string literal \"define_var\"";
}

TEST_F(CheckIncludedDefinitionsTest, TwoPlusTwoDefinition) {
  // defs.sv line 18: `define TWO_PLUS_TWO 5
  const hldb::PreprocMacroDefinition *const md = findMacroDefinition("TWO_PLUS_TWO");
  ASSERT_NE(md, nullptr) << "macro 'TWO_PLUS_TWO' is not defined";
  EXPECT_TRUE(endsWith(md->getFile(), kIncludedFileName)) << "file: " << md->getFile();
  EXPECT_EQ(md->getStartLine(), 18u);
  EXPECT_EQ(countOf(md->getArguments()), 0u) << "no formal-argument list (Sec 22.5.1)";
  EXPECT_EQ(macroTextWithoutWhiteSpace(md), "5");
}

// --- macro usages ----

TEST_F(CheckIncludedDefinitionsTest, DefineVarIsTheOnlyMacroUsage) {
  // Sec 22.5.1: the `TWO_PLUS_TWO on line 19 is inside a string literal and
  // is not substituted, so `define_var on line 20 is the only macro usage.
  const std::vector<const hldb::PreprocMacroInstance *> instances = allMacroInstances();
  ASSERT_EQ(instances.size(), 1u) << "`TWO_PLUS_TWO inside a string literal is not a macro usage";
  const hldb::PreprocMacroInstance *const mi = instances.front();
  ASSERT_NE(mi, nullptr);
  EXPECT_EQ(mi->getName(), "define_var");
  EXPECT_TRUE(endsWith(mi->getFile(), kMainFileName)) << "file: " << mi->getFile();
  EXPECT_EQ(mi->getStartLine(), 20u);
  EXPECT_EQ(countOf(mi->getArguments()), 0u) << "`define_var is used without actual arguments";
  // Name binding: the usage resolves to the definition made in defs.sv.
  const hldb::PreprocMacroDefinition *const definition = findMacroDefinition("define_var");
  ASSERT_NE(definition, nullptr) << "macro 'define_var' is not defined";
  EXPECT_EQ(mi->getPreprocMacroDefinition(), definition);
}

// --- module definition ----

TEST_F(CheckIncludedDefinitionsTest, DesignHasExactlyOneModuleTop) {
  ASSERT_EQ(countOf(m_design->getAllModules()), 1u) << "only 'top' is declared; defs.sv declares no design element";
  const hldb::Module *const top = m_design->getAllModules()->at(0);
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getDefName(), "top");
  EXPECT_EQ(top, getTop());
}

TEST_F(CheckIncludedDefinitionsTest, ModuleTopLocationIsInMainFile) {
  // Sec 22.12: the 18 lines of defs.sv do not shift the main file's line
  // numbers; 'module top' is at main-file line 17 and 'endmodule' at 23.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(endsWith(top->getFile(), kMainFileName)) << "file: " << top->getFile();
  EXPECT_EQ(top->getStartLine(), 17u);
  EXPECT_EQ(top->getStartColumn(), 1u);
  EXPECT_EQ(top->getEndLine(), 23u);
}

TEST_F(CheckIncludedDefinitionsTest, ModuleTopHasStaticDefaultLifetime) {
  // "module top" carries no lifetime qualifier; Sec 6.21: "The default
  // lifetime is static."
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_FALSE(top->getAutomatic());
}

TEST_F(CheckIncludedDefinitionsTest, ModuleTopHasOneNullPort) {
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

TEST_F(CheckIncludedDefinitionsTest, ModuleTopHoldsOnlyOneProcess) {
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

TEST_F(CheckIncludedDefinitionsTest, InitialBlockHoldsTwoDisplayCalls) {
  // Lines 18-21: initial begin $display(...); $display(...); end
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_EQ(countOf(top->getProcesses()), 1u);
  const hldb::Initial *const initial = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(initial, nullptr) << "the only process is not an initial procedure";
  EXPECT_EQ(initial->getVpiType(), static_cast<uint32_t>(vpiInitial));
  EXPECT_EQ(initial->getStartLine(), 18u);

  const hldb::Begin *const block = getInitialBlock();
  ASSERT_NE(block, nullptr) << "the initial procedure's statement is not a begin-end block";
  EXPECT_EQ(block->getVpiType(), static_cast<uint32_t>(vpiBegin)) << "the block has no label, so it is unnamed";
  EXPECT_EQ(block->getStartLine(), 18u);
  EXPECT_EQ(block->getEndLine(), 21u);
  ASSERT_EQ(countOf(block->getStmts()), 2u);

  const uint32_t expectedLines[2] = {19u, 20u};
  for (size_t i = 0; i < 2u; ++i) {
    const hldb::SysTaskCall *const call = getDisplayCall(i);
    ASSERT_NE(call, nullptr) << "statement " << i << " is not a system task call";
    EXPECT_EQ(call->getName(), "$display") << "statement " << i;
    EXPECT_FALSE(call->getUserDefn()) << "$display is a built-in system task (Sec 21.2)";
    EXPECT_EQ(call->getStartLine(), expectedLines[i]) << "statement " << i;
  }
}

TEST_F(CheckIncludedDefinitionsTest, FirstDisplayKeepsMacroTextInsideString) {
  // Line 19. Sec 22.5.1: no macro substitution within a string literal, so
  // the argument keeps "`TWO_PLUS_TWO" and is not ":assert:(5 == 5)".
  const hldb::SysTaskCall *const call = getDisplayCall(0);
  ASSERT_NE(call, nullptr);
  ASSERT_EQ(countOf(call->getArguments()), 1u);
  const hldb::Constant *const arg = getConstantArgument(call, 0);
  ASSERT_NE(arg, nullptr) << "the argument is not a constant";
  EXPECT_EQ(arg->getConstType(), vpiStringConst) << "Annex K: #define vpiStringConst 6 /* string literal */";
  EXPECT_TRUE(contains(arg->getValue(), ":assert:(`TWO_PLUS_TWO == 5)")) << "value: " << arg->getValue();
}

TEST_F(CheckIncludedDefinitionsTest, SecondDisplayReceivesExpandedMacroString) {
  // Line 20: $display(":assert:('%s' == '%s')", `define_var, "define_var");
  // `define_var expands to the string literal "define_var" (Sec 22.5.1), so
  // the 2nd and 3rd arguments are the same string literal.
  const hldb::SysTaskCall *const call = getDisplayCall(1);
  ASSERT_NE(call, nullptr);
  ASSERT_EQ(countOf(call->getArguments()), 3u);
  const hldb::Constant *const format = getConstantArgument(call, 0);
  const hldb::Constant *const expanded = getConstantArgument(call, 1);
  const hldb::Constant *const literal = getConstantArgument(call, 2);
  ASSERT_NE(format, nullptr) << "argument 0 is not a constant";
  ASSERT_NE(expanded, nullptr) << "argument 1 (`define_var) did not expand to a constant";
  ASSERT_NE(literal, nullptr) << "argument 2 is not a constant";
  EXPECT_EQ(format->getConstType(), vpiStringConst);
  EXPECT_EQ(expanded->getConstType(), vpiStringConst);
  EXPECT_EQ(literal->getConstType(), vpiStringConst);
  EXPECT_TRUE(contains(format->getValue(), ":assert:('%s' == '%s')")) << "value: " << format->getValue();
  EXPECT_TRUE(contains(literal->getValue(), "define_var")) << "value: " << literal->getValue();
  EXPECT_EQ(expanded->getValue(), literal->getValue()) << "`define_var must expand to the literal \"define_var\"";
}

// --- elaboration ----

TEST_F(CheckIncludedDefinitionsTest, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    // Sec 23.3.1: 'top' appears in no instantiation, so it is the top-level
    // module, implicitly instantiated once under its own name.
    ASSERT_EQ(countOf(m_design->getTopModules()), 1u);
    const hldb::Module *const inst = m_design->getTopModules()->at(0);
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->getName(), "top");
    EXPECT_EQ(inst->getDefName(), "top");
    EXPECT_TRUE(inst->getTopModule());
    // Sec 37.10 definition location of the instance (main file, line 17).
    EXPECT_EQ(inst->getDefLineNo(), 17);
    EXPECT_TRUE(endsWith(inst->getDefFile(), kMainFileName)) << "defFile: " << inst->getDefFile();
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
