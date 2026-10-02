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

// Tests for 22.4--include_with_comment.sv (tags: 22.4,
// type: preprocessing parsing)
//
// SV source, 22.4--include_with_comment.sv (lines 1-15 are comments):
//   16: `include "dummy_include.sv" // comments after `include are perfectly legal
//   17: module top ();
//   18: endmodule
//
// Included file, dummy_include.sv: 15 lines, all of them comments. It holds
// no code.
//
// IEEE 1800-2023 rules this fixture exercises:
//   - Sec 22.4: "Only white space or a comment may appear on the same line as
//     the `include compiler directive." Line 16 follows the directive with a
//     one-line comment, which the rule allows, so the line is legal.
//   - Sec 22.2: "Directives are not recognized within comments or string
//     literals." The comment itself contains the text "`include", but that is
//     not a directive. Line 16 therefore holds exactly one `include, and the
//     same-line rule is not engaged by the commented text. Nor is the
//     commented "`include" a macro usage (Sec 22.5.1: directive names are not
//     macro names, and comments are not scanned for macro usages).
//   - Sec 22.4: "The result is as though the contents of the included source
//     file appear in place of the `include compiler directive." The contents
//     are only comments, so the include adds no design element, no macro and
//     no further include.
//   - Sec 22.12: "The compiler shall maintain the current line number and
//     file name of the file being compiled." The 15 lines of dummy_include.sv
//     must not shift the main file: 'module top' stays at main-file line 17
//     (it would be reported at 31 otherwise) and 'endmodule' at line 18.
//   - Sec 37.14 detail 10: "vpiLowConn shall return NULL if the module or
//     interface or program port is a null port (e.g., "module M();")". So
//     "module top ();" declares exactly one null port: no name (detail 8),
//     port index 0 (detail 9), vpiPortType vpiPort (detail 1), vpiSize 0
//     (detail 11), no low connection, and no high connection because 'top'
//     is never instantiated (detail 10).
//
// Checked:
//   - zero error-class diagnostics (fatal, syntax, error): a trailing comment
//     is allowed after `include, and the "`include" inside it is not a
//     directive
//   - the main source file includes exactly one file, dummy_include.sv (the
//     commented "`include" adds no second include), and that file includes
//     nothing further
//   - the design holds zero macro definitions and zero macro usages
//   - exactly one module definition, defName "top", main-file lines 17-18,
//     static default lifetime (Sec 6.21: "The default lifetime is static.";
//     Sec 37.3.7 vpiAutomatic == false), one null port, empty body
//   - after elaboration only: exactly one top-level instance "top"
//     (Sec 23.3.1: "A top-level module is implicitly instantiated once, and
//     its instance name is the same as the module name."), vpiTopModule
//     (Sec 37.5), defined at main-file line 17 (Sec 37.10), static lifetime,
//     whose single port is a null port (no name, size 0) with no low or high
//     connection; before elaboration there are zero top-level instances
//     (Sec 3.12)
//
// What is NOT checked, and why:
//   - Comment nodes, including the trailing comment on line 16: how
//     comments are recorded and grouped is a tool convention the LRM does
//     not determine. The comment's effect, that it is not a directive and
//     not a macro usage, is asserted instead (MainFileIncludesDummyIncludeOnce,
//     DesignHasNoMacros).
//   - The directory the include was resolved from. Sec 22.4 searches the
//     current working directory and user-specified locations, which are
//     command-line facts, not source facts; only the included file's name is
//     asserted.
//   - Whether the `include is recorded as a directive entry. The LRM has no
//     notion of a recorded directive list.
//   - Warning-, note- and info-level diagnostics. The LRM defines only
//     errors; it neither requires nor forbids warnings on legal source.
//   - Time unit and precision: no `timescale in the source, so the defaults
//     are tool-specific (Sec 22.7).
//   - Directive defaults (vpiDefNetType, vpiUnconnDrive, vpiCellInstance,
//     vpiDefDelayMode). The LRM does define them with no directive present
//     (e.g. Sec 22.8: implicit nets are of type wire), but no directive that
//     sets them appears here, so they are not this fixture's subject; the
//     22.3 `resetall fixtures assert the same default values.
//   - Runtime behavior: the fixture has none. 'top' is empty, so nothing
//     happens while simulation time advances.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/port.h>
#include <hldb/source_file.h>
#include <hldb/vpi_user.h>

#include <algorithm>
#include <string_view>
#include <vector>

namespace hlc {

static constexpr std::string_view kMainFileName = "22.4--include_with_comment.sv";
static constexpr std::string_view kIncludedFileName = "dummy_include.sv";

class IncludeWithCommentTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.4--include_with_comment.hlc"}); }
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

  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- diagnostics ----

TEST_F(IncludeWithCommentTest, CompilerReportsZeroErrors) {
  // Sec 22.4 allows a comment on the `include line, and Sec 22.2 keeps the
  // "`include" inside that comment from being a directive.
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

// --- `include ----

TEST_F(IncludeWithCommentTest, MainFileIncludesDummyIncludeOnce) {
  // Sec 22.4: line 16 includes exactly one file, "dummy_include.sv". The
  // "`include" inside the trailing comment is not a directive (Sec 22.2), so
  // it adds no second include.
  const hldb::SourceFile *const mainFile = findSourceFile(kMainFileName);
  ASSERT_NE(mainFile, nullptr) << "source file '" << kMainFileName << "' not found";
  ASSERT_EQ(countOf(mainFile->getIncludes()), 1u) << "the commented \"`include\" is not a directive (Sec 22.2)";
  const hldb::SourceFile *const included = mainFile->getIncludes()->at(0);
  ASSERT_NE(included, nullptr);
  EXPECT_TRUE(endsWith(included->getName(), kIncludedFileName)) << "included: " << included->getName();
}

TEST_F(IncludeWithCommentTest, DummyIncludeIncludesNothing) {
  // dummy_include.sv holds only comments, so it has no `include of its own.
  const hldb::SourceFile *const included = findSourceFile(kIncludedFileName);
  ASSERT_NE(included, nullptr) << "source file '" << kIncludedFileName << "' not found";
  EXPECT_EQ(countOf(included->getIncludes()), 0u);
}

// --- macros ----

TEST_F(IncludeWithCommentTest, DesignHasNoMacros) {
  // Neither file contains a `define. The `include directive is not a macro
  // usage (Sec 22.5.1), and the "`include" inside the comment is neither a
  // directive nor a macro usage (Sec 22.2).
  size_t definitions = 0u;
  size_t usages = 0u;
  for (const hldb::SourceFile *const sf : allSourceFiles()) {
    definitions += countOf(sf->getPreprocMacroDefinitions());
    usages += countOf(sf->getPreprocMacroInstances());
  }
  EXPECT_EQ(definitions, 0u);
  EXPECT_EQ(usages, 0u);
}

// --- module definition ----

TEST_F(IncludeWithCommentTest, DesignHasExactlyOneModuleTop) {
  ASSERT_EQ(countOf(m_design->getAllModules()), 1u)
      << "only 'top' is declared; dummy_include.sv declares no design element";
  const hldb::Module *const top = m_design->getAllModules()->at(0);
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getDefName(), "top");
  EXPECT_EQ(top, getTop());
}

TEST_F(IncludeWithCommentTest, ModuleTopLocationIsInMainFile) {
  // Sec 22.12: the 15 lines of dummy_include.sv do not shift the main
  // file's line numbers; 'module top' is at main-file line 17 and
  // 'endmodule' at 18.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(endsWith(top->getFile(), kMainFileName)) << "file: " << top->getFile();
  EXPECT_EQ(top->getStartLine(), 17u);
  EXPECT_EQ(top->getStartColumn(), 1u);
  EXPECT_EQ(top->getEndLine(), 18u);
}

TEST_F(IncludeWithCommentTest, ModuleTopHasStaticDefaultLifetime) {
  // "module top" carries no lifetime qualifier; Sec 6.21: "The default
  // lifetime is static."
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_FALSE(top->getAutomatic());
}

TEST_F(IncludeWithCommentTest, ModuleTopHasOneNullPort) {
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

TEST_F(IncludeWithCommentTest, ModuleTopBodyIsEmpty) {
  // Nothing appears between "module top ();" and "endmodule", and the
  // included text adds nothing to it.
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

TEST_F(IncludeWithCommentTest, TopIsTheOnlyTopLevelInstance) {
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
