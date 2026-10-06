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

// Tests for 22.4--include_via_define.sv (tags: 22.4,
// type: preprocessing parsing)
//
// SV source (lines 1-15 are the license and metadata comments):
//   16: `define DO_INCLUDE(FN) `include FN
//   18: // Check that multiple define references don't throw a multiple
//       // `include-on-line error
//   19: `DO_INCLUDE("dummy_include.sv") `DO_INCLUDE("dummy_include.sv")
//   21: // Check that ifdefs
//   22: `ifdef NEVER
//   23:  `DO_INCLUDE("SHOULD_NOT_BE_INCLUDED")
//   24: `endif
//   26: module top ();
//   27: endmodule
// (lines 17, 20 and 25 are blank). On line 19 the two macro usages start at
// columns 1 and 33.
//
// Included file, dummy_include.sv: 15 lines, all of them comments. It holds
// no code.
//
// IEEE 1800-2023 rules this fixture exercises:
//   - Sec 22.5.1: text_macro_definition ::= `define text_macro_name
//     macro_text, text_macro_name ::= text_macro_identifier
//     [ ( list_of_formal_arguments ) ]. Line 16 defines DO_INCLUDE with one
//     formal argument, FN, and macro text "`include FN". "Any compiler
//     directives appearing within the macro text shall be ignored until the
//     macro is used." Sec 22.2: such a directive "shall be processed ...
//     where the text macro is used."
//   - Line 19 holds two usages of DO_INCLUDE, each with the actual argument
//     "dummy_include.sv"; each expands to `include "dummy_include.sv", so
//     dummy_include.sv is included (Sec 22.4: "as though the contents of the
//     included source file appear in place of the `include").
//   - Sec 22.4: "Only white space or a comment may appear on the same line as
//     the `include compiler directive." Line 19 does not break this rule.
//     The `include directive appears in the macro text on line 16, where
//     nothing follows "`include FN". Line 19 holds macro usages, and Sec 22.2
//     states "(a macro usage is not considered a directive)". This test
//     follows that reading, which is also what the fixture expects ("Check
//     that multiple define references don't throw a multiple
//     `include-on-line error"). The other possible reading applies the rule
//     to the text after expansion, which would make line 19 illegal. The LRM
//     never says the rule applies after expansion, so this test does not
//     adopt it.
//   - Sec 22.6: NEVER is never defined in the source, so `ifdef NEVER fails
//     and "the associated block_of_text ... is ignored". The usage on line 23
//     is therefore not a macro usage, nothing is included from it, and the
//     nonexistent file SHOULD_NOT_BE_INCLUDED is never looked up.
//   - Sec 22.12: "The compiler shall maintain the current line number and
//     file name of the file being compiled." Two inclusions of the 15-line
//     dummy_include.sv do not shift the main file: 'module top' stays at
//     line 26 and 'endmodule' at line 27.
//   - Sec 37.14 detail 10: "module top ();" declares exactly one null port
//     (no name, index 0, vpiPort, size 0, no low or high connection).
//
// Checked:
//   - zero error-class diagnostics (fatal, syntax, error): line 19 is legal
//     under the reading above, both includes resolve, and the ignored block
//     triggers no lookup of SHOULD_NOT_BE_INCLUDED
//   - exactly one macro definition, DO_INCLUDE (main file, line 16), with
//     one formal argument "FN" and macro text "`include FN"
//   - exactly two macro usages, both DO_INCLUDE on line 19 (columns 1 and
//     33), each with one actual argument "dummy_include.sv" (with quotes),
//     each bound to the DO_INCLUDE definition; none on line 23
//   - dummy_include.sv is recorded as included through the macro, and no file
//     named SHOULD_NOT_BE_INCLUDED is recorded as included
//   - exactly one module definition, defName "top", main-file lines 26-27,
//     static default lifetime (Sec 6.21, Sec 37.3.7), one null port, empty
//     body
//   - after elaboration only: exactly one top-level instance "top"
//     (Sec 23.3.1), vpiTopModule (Sec 37.5), defined at main-file line 26
//     (Sec 37.10), static lifetime, whose single port is a null port with no
//     low or high connection; before elaboration there are zero top-level
//     instances (Sec 3.12)
//
// What is NOT checked, and why:
//   - How many include records HLC keeps for the two expansions, and where.
//     Sec 22.4 makes each expansion include the file (so twice), but whether
//     HLDB records an include produced by a macro expansion on the main file
//     or under the macro usage, and once or twice, is a model convention.
//     Only that dummy_include.sv is recorded as included at least once is
//     asserted (DummyIncludeIsIncludedThroughTheMacro).
//   - The expanded text of each usage (PreprocMacroInstance::getBody()) and
//     PreprocMacroDefinition::getType(). These HLDB fields have no documented
//     meaning that maps to an LRM term.
//   - Whether NEVER is defined on the command line. That is not a source
//     fact; the source never defines it, and this test assumes the run
//     defines no extra macros.
//   - Comment nodes: grouping is a tool convention.
//   - Whether the directives are recorded as directive entries. The LRM has
//     no notion of a recorded directive list.
//   - Warning-, note- and info-level diagnostics. The LRM defines only
//     errors; it neither requires nor forbids warnings on legal source.
//   - Time unit and precision: no `timescale in the source, so tool-specific
//     (Sec 22.7).
//   - Directive defaults (vpiDefNetType, vpiUnconnDrive, vpiCellInstance,
//     vpiDefDelayMode): no directive that sets them appears here; the 22.3
//     `resetall fixtures assert the default values.
//   - Runtime behavior: the fixture has none. 'top' is empty.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
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
#include <vector>

namespace hlc {

static constexpr std::string_view kMainFileName = "22.4--include_via_define.sv";
static constexpr std::string_view kIncludedFileName = "dummy_include.sv";
static constexpr std::string_view kNeverIncludedFileName = "SHOULD_NOT_BE_INCLUDED";

class IncludeViaDefineTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.4--include_via_define.hlc"}); }
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

  static std::string withoutWhiteSpace(std::string_view text) {
    std::string result;
    for (const char c : text) {
      if ((c != ' ') && (c != '\t') && (c != '\r') && (c != '\n')) result += c;
    }
    return result;
  }

  // Every source file of the design plus every file reachable through
  // include lists, each listed once.
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

  // Every file recorded as included below 'sf', whether on an include list or
  // as an item of a macro usage whose expansion performed the include.
  static void collectIncludedFiles(const hldb::SourceFile *sf, std::vector<const hldb::SourceFile *> &out) {
    if (sf->getIncludes() != nullptr) {
      for (const hldb::SourceFile *const inc : *sf->getIncludes()) addIncludedFile(inc, out);
    }
    if (sf->getPreprocMacroInstances() != nullptr) {
      for (const hldb::PreprocMacroInstance *const mi : *sf->getPreprocMacroInstances()) collectFromMacroItems(mi, out);
    }
  }

  static void collectFromMacroItems(const hldb::PreprocMacroInstance *mi, std::vector<const hldb::SourceFile *> &out) {
    if ((mi == nullptr) || (mi->getItems() == nullptr)) return;
    for (const hldb::Any *const item : *mi->getItems()) {
      if (const hldb::SourceFile *const inc = any_cast<hldb::SourceFile>(item)) {
        addIncludedFile(inc, out);
      } else if (const hldb::PreprocMacroInstance *const nested = any_cast<hldb::PreprocMacroInstance>(item)) {
        collectFromMacroItems(nested, out);
      }
    }
  }

  static void addIncludedFile(const hldb::SourceFile *inc, std::vector<const hldb::SourceFile *> &out) {
    if ((inc == nullptr) || (std::find(out.begin(), out.end(), inc) != out.end())) return;
    out.emplace_back(inc);
    collectIncludedFiles(inc, out);
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

// --- diagnostics ----

TEST_F(IncludeViaDefineTest, CompilerReportsZeroErrors) {
  // Line 19 holds macro usages, not `include directives (Sec 22.2), both
  // includes resolve, and the ignored `ifdef block looks nothing up.
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

// --- macro definition ----

TEST_F(IncludeViaDefineTest, DoIncludeIsTheOnlyMacroDefinition) {
  // Line 16: `define DO_INCLUDE(FN) `include FN
  // dummy_include.sv defines nothing, and `ifdef NEVER defines nothing.
  ASSERT_EQ(allMacroDefinitions().size(), 1u);
  const hldb::PreprocMacroDefinition *const md = findMacroDefinition("DO_INCLUDE");
  ASSERT_NE(md, nullptr) << "macro 'DO_INCLUDE' is not defined";
  EXPECT_TRUE(endsWith(md->getFile(), kMainFileName)) << "file: " << md->getFile();
  EXPECT_EQ(md->getStartLine(), 16u);
  ASSERT_EQ(countOf(md->getArguments()), 1u) << "one formal argument, FN (Sec 22.5.1)";
  ASSERT_NE(md->getArguments()->at(0), nullptr);
  EXPECT_EQ(md->getArguments()->at(0)->getName(), "FN");
  EXPECT_EQ(macroTextWithoutWhiteSpace(md), "`includeFN") << "macro text is \"`include FN\"";
}

// --- macro usages ----

TEST_F(IncludeViaDefineTest, TwoUsagesOfDoIncludeOnLine19) {
  // Line 19 holds two usages. The usage on line 23 sits in the ignored
  // `ifdef NEVER block (Sec 22.6), so it is not a macro usage.
  const std::vector<const hldb::PreprocMacroInstance *> instances = allMacroInstances();
  ASSERT_EQ(instances.size(), 2u) << "the usage on line 23 is in an ignored block (Sec 22.6)";
  const hldb::PreprocMacroDefinition *const definition = findMacroDefinition("DO_INCLUDE");
  ASSERT_NE(definition, nullptr) << "macro 'DO_INCLUDE' is not defined";
  std::vector<uint16_t> columns;
  for (const hldb::PreprocMacroInstance *const mi : instances) {
    ASSERT_NE(mi, nullptr);
    EXPECT_EQ(mi->getName(), "DO_INCLUDE");
    EXPECT_TRUE(endsWith(mi->getFile(), kMainFileName)) << "file: " << mi->getFile();
    EXPECT_EQ(mi->getStartLine(), 19u);
    columns.emplace_back(mi->getStartColumn());
    // Sec 22.5.1: one actual argument, the string literal "dummy_include.sv".
    ASSERT_EQ(countOf(mi->getArguments()), 1u);
    ASSERT_NE(mi->getArguments()->at(0), nullptr);
    EXPECT_EQ(withoutWhiteSpace(mi->getArguments()->at(0)->getName()), "\"dummy_include.sv\"");
    // Name binding: each usage resolves to the definition on line 16.
    EXPECT_EQ(mi->getPreprocMacroDefinition(), definition);
  }
  std::sort(columns.begin(), columns.end());
  ASSERT_EQ(columns.size(), 2u);
  EXPECT_EQ(columns[0], 1u) << "the first usage starts at column 1";
  EXPECT_EQ(columns[1], 33u) << "the second usage starts at column 33";
}

// --- includes performed by the expansions ----

TEST_F(IncludeViaDefineTest, DummyIncludeIsIncludedThroughTheMacro) {
  // Each usage on line 19 expands to `include "dummy_include.sv" (Sec 22.2,
  // Sec 22.4), so the file is included.
  const hldb::SourceFile *const mainFile = findSourceFile(kMainFileName);
  ASSERT_NE(mainFile, nullptr) << "source file '" << kMainFileName << "' not found";
  std::vector<const hldb::SourceFile *> included;
  collectIncludedFiles(mainFile, included);
  size_t dummyRecords = 0u;
  for (const hldb::SourceFile *const sf : included) {
    if (endsWith(sf->getName(), kIncludedFileName)) ++dummyRecords;
  }
  EXPECT_GE(dummyRecords, 1u) << "dummy_include.sv is not recorded as included";
}

TEST_F(IncludeViaDefineTest, IgnoredBlockIncludesNothing) {
  // Sec 22.6: the `ifdef NEVER block is ignored, so SHOULD_NOT_BE_INCLUDED
  // is never included.
  const hldb::SourceFile *const mainFile = findSourceFile(kMainFileName);
  ASSERT_NE(mainFile, nullptr) << "source file '" << kMainFileName << "' not found";
  std::vector<const hldb::SourceFile *> included;
  collectIncludedFiles(mainFile, included);
  for (const hldb::SourceFile *const sf : included) {
    EXPECT_FALSE(endsWith(sf->getName(), kNeverIncludedFileName)) << "included: " << sf->getName();
  }
  for (const hldb::PreprocMacroInstance *const mi : allMacroInstances()) {
    EXPECT_NE(mi->getStartLine(), 23u) << "line 23 is inside the ignored `ifdef NEVER block";
  }
}

// --- module definition ----

TEST_F(IncludeViaDefineTest, DesignHasExactlyOneModuleTop) {
  ASSERT_EQ(countOf(m_design->getAllModules()), 1u)
      << "only 'top' is declared; dummy_include.sv declares no design element";
  const hldb::Module *const top = m_design->getAllModules()->at(0);
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getDefName(), "top");
  EXPECT_EQ(top, getTop());
}

TEST_F(IncludeViaDefineTest, ModuleTopLocationIsInMainFile) {
  // Sec 22.12: two inclusions of dummy_include.sv do not shift the main
  // file's line numbers; 'module top' is at line 26 and 'endmodule' at 27.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(endsWith(top->getFile(), kMainFileName)) << "file: " << top->getFile();
  EXPECT_EQ(top->getStartLine(), 26u);
  EXPECT_EQ(top->getStartColumn(), 1u);
  EXPECT_EQ(top->getEndLine(), 27u);
}

TEST_F(IncludeViaDefineTest, ModuleTopHasStaticDefaultLifetime) {
  // "module top" carries no lifetime qualifier; Sec 6.21: "The default
  // lifetime is static."
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_FALSE(top->getAutomatic());
}

TEST_F(IncludeViaDefineTest, ModuleTopHasOneNullPort) {
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

TEST_F(IncludeViaDefineTest, ModuleTopBodyIsEmpty) {
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

// --- elaboration ----

TEST_F(IncludeViaDefineTest, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    // Sec 23.3.1: 'top' appears in no instantiation, so it is the top-level
    // module, implicitly instantiated once under its own name.
    ASSERT_EQ(countOf(m_design->getTopModules()), 1u);
    const hldb::Module *const inst = m_design->getTopModules()->at(0);
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->getName(), "top");
    EXPECT_EQ(inst->getDefName(), "top");
    EXPECT_TRUE(inst->getTopModule());
    // Sec 37.10 definition location of the instance (main file, line 26).
    EXPECT_EQ(inst->getDefLineNo(), 26);
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
