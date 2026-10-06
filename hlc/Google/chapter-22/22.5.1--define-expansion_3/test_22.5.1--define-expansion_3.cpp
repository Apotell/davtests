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

// Tests for 22.5.1--define-expansion_3.sv (tags: 22.5.1, type: preprocessing)
//
// SV source (lines 1-15 are the license and metadata comments):
//   16: `define D(x,y) initial $display("start", x , y, "end");
//   17: module top ();
//   18: `D(, "msg2 ")
//   19: endmodule
//
// This is the third usage of the IEEE 1800-2023 Sec 22.5.1 "Example macro
// without defaults", where the LRM states the result itself:
//   `D(, "msg2 ")
//     // expands to 'initial $display("start", , "msg2 ", "end");'
//
// IEEE 1800-2023 rules this fixture exercises:
//   - Sec 22.5.1: line 16 defines D with two formal arguments, x and y in
//     that order, neither with a default, and macro text
//     "initial $display("start", x , y, "end");".
//   - Line 18 uses D with two actual arguments. The first is empty: "An
//     actual argument may be empty or white space only, in which case the
//     formal argument is substituted by the argument default if one is
//     specified or by nothing if no default is specified." x has no default,
//     so it is substituted by nothing. The second is the string literal
//     "msg2 ", whose trailing space is inside the quotes and so is part of
//     the literal. Two actual arguments are given for two formal arguments,
//     so neither "fewer" nor "more" actual arguments applies and the usage is
//     legal.
//   - The expansion leaves two adjacent commas right after "start". Sec
//     21.2.1: "An empty argument is characterized by a comma at the beginning
//     or end of the argument list or by two adjacent commas." Sec 37.42
//     detail 8: "An empty (omitted) argument (see 21.2.1) shall be
//     represented as an expression with a vpiType of vpiOperation and a
//     vpiOpType of vpiNullOp." So $display has four arguments: "start", an
//     empty argument, "msg2 ", and "end".
//   - Annex K (normative vpi_user.h): "#define vpiStringConst 6 /* string
//     literal */". The three literal arguments are string constants.
//     $display is a built-in system task (Sec 21.2), so vpiUserDefn
//     (Sec 37.42) is false.
//   - The expansion is an initial procedure (Sec 9.2.1, initial_construct ::=
//     initial statement_or_null) whose statement is the $display call itself.
//   - Location of the expanded text. Sec 22.12: "The compiler shall maintain
//     the current line number and file name of the file being compiled."
//     Sec 22.13: "`__LINE__ expands to the current input line number". Annex
//     K: "#define vpiLineNo 6 /* line number where the object is used */".
//     The expanded initial procedure and $display are produced where D is
//     used, so both are on line 18, not on the definition line 16.
//   - Sec 37.14 detail 10: "module top ();" declares exactly one null port
//     (no name, index 0, vpiPort, size 0, no low or high connection).
//
// Checked:
//   - zero error-class diagnostics (fatal, syntax, error): the empty first
//     actual argument is legal
//   - exactly one macro definition, D (line 16), with formal arguments x and
//     y in that order and macro text "initial $display("start", x , y,
//     "end");" (compared with white space removed)
//   - exactly one macro usage: D on line 18, column 1, with two actual
//     arguments, an empty one and "msg2 " (with quotes and its inner trailing
//     space; both compared after trimming surrounding white space), bound to
//     the line-16 definition
//   - exactly one module definition, defName "top", lines 17-19, static
//     default lifetime (Sec 6.21, Sec 37.3.7), one null port
//   - 'top' holds exactly one process and nothing else (no nets, variables,
//     parameters, param assigns, continuous assignments, tasks/functions,
//     instantiations)
//   - that process is an initial procedure on line 18 whose statement is a
//     $display system task call on line 18, not user-defined
//   - the $display has four arguments: the string constant "start", a
//     vpiNullOp operation for the empty argument, and the string constants
//     "msg2 " (trailing space kept) and "end"
//   - after elaboration only: exactly one top-level instance "top"
//     (Sec 23.3.1), vpiTopModule (Sec 37.5), defined at line 17
//     (Sec 37.10), static lifetime, whose single port is a null port with no
//     low or high connection; before elaboration there are zero top-level
//     instances (Sec 3.12)
//
// What is NOT checked, and why:
//   - What $display prints, including the single space Sec 21.2.1 says an
//     empty argument produces. That exists only while simulation time
//     advances and is permanently out of scope for a static compiler; its
//     static half, the vpiNullOp argument, is asserted by
//     DisplayReceivesExpandedArguments.
//   - The operands of the vpiNullOp operation: the LRM does not define any.
//   - Columns of the expanded initial procedure and $display. Expanded text
//     has no column of its own in the source; the LRM fixes only the line.
//   - The storage format of a string constant's value (prefix, quotes). The
//     LRM defines the literal's characters, not HLC's storage format, so the
//     literal arguments are compared using the format HLC uses for "start".
//   - vpiSize of the string constants: the VPI clause does not define it for
//     string constants.
//   - PreprocMacroDefinition::getType() and PreprocMacroInstance::getBody(),
//     getItems() and getObjects(): HLDB fields with no LRM counterpart.
//   - Whether `define is recorded as a directive entry. The LRM has no notion
//     of a recorded directive list.
//   - Warning-, note- and info-level diagnostics. The LRM defines only
//     errors; it neither requires nor forbids warnings on legal source.
//   - Time unit and precision: no `timescale in the source, so tool-specific
//     (Sec 22.7).
//   - Directive defaults (vpiDefNetType, vpiUnconnDrive, vpiCellInstance,
//     vpiDefDelayMode): no directive that sets them appears here; the 22.3
//     `resetall fixtures assert the default values.
//   - Comment nodes: grouping is a tool convention.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/identifier.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/operation.h>
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

static constexpr std::string_view kMainFileName = "22.5.1--define-expansion_3.sv";

class DefineExpansion3Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.5.1--define-expansion_3.hlc"}); }
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

  // The text with leading and trailing white space removed; white space
  // inside it (for example inside a string literal) is kept.
  static std::string_view trimmed(std::string_view text) {
    size_t first = 0u;
    while ((first < text.size()) && isWhiteSpace(text[first])) ++first;
    size_t last = text.size();
    while ((last > first) && isWhiteSpace(text[last - 1u])) --last;
    return text.substr(first, last - first);
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

  // The single initial procedure of 'top'.
  static const hldb::Initial *getInitial() {
    const hldb::Module *const top = getTop();
    if ((top == nullptr) || (countOf(top->getProcesses()) != 1u)) return nullptr;
    return any_cast<hldb::Initial>(top->getProcesses()->at(0));
  }

  // The statement of the initial procedure, as a system task call.
  static const hldb::SysTaskCall *getDisplayCall() {
    const hldb::Initial *const initial = getInitial();
    return (initial == nullptr) ? nullptr : initial->getStmt<hldb::SysTaskCall>();
  }
};

// --- diagnostics ----

TEST_F(DefineExpansion3Test, CompilerReportsZeroErrors) {
  // Sec 22.5.1: an actual argument may be empty; two actual arguments are
  // given for two formal arguments, so the usage is legal.
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

// --- macro definition ----

TEST_F(DefineExpansion3Test, MacroDDefinition) {
  // Line 16: `define D(x,y) initial $display("start", x , y, "end");
  ASSERT_EQ(allMacroDefinitions().size(), 1u) << "D is the only macro defined";
  const hldb::PreprocMacroDefinition *const md = findMacroDefinition("D");
  ASSERT_NE(md, nullptr) << "macro 'D' is not defined";
  EXPECT_TRUE(endsWith(md->getFile(), kMainFileName)) << "file: " << md->getFile();
  EXPECT_EQ(md->getStartLine(), 16u);
  ASSERT_EQ(countOf(md->getArguments()), 2u) << "two formal arguments, x and y (Sec 22.5.1)";
  ASSERT_NE(md->getArguments()->at(0), nullptr);
  ASSERT_NE(md->getArguments()->at(1), nullptr);
  EXPECT_EQ(md->getArguments()->at(0)->getName(), "x");
  EXPECT_EQ(md->getArguments()->at(1)->getName(), "y");
  EXPECT_EQ(macroTextWithoutWhiteSpace(md), "initial$display(\"start\",x,y,\"end\");")
      << "macro text is: initial $display(\"start\", x , y, \"end\");";
}

// --- macro usage ----

TEST_F(DefineExpansion3Test, MacroDIsUsedOnceWithAnEmptyFirstArgument) {
  // Line 18: `D(, "msg2 ")
  const std::vector<const hldb::PreprocMacroInstance *> instances = allMacroInstances();
  ASSERT_EQ(instances.size(), 1u);
  const hldb::PreprocMacroInstance *const mi = instances.front();
  ASSERT_NE(mi, nullptr);
  EXPECT_EQ(mi->getName(), "D");
  EXPECT_TRUE(endsWith(mi->getFile(), kMainFileName)) << "file: " << mi->getFile();
  EXPECT_EQ(mi->getStartLine(), 18u);
  EXPECT_EQ(mi->getStartColumn(), 1u);
  // Sec 22.5.1: two actual arguments; the first is empty, the second is the
  // literal "msg2 " (its inner trailing space is part of the literal).
  ASSERT_EQ(countOf(mi->getArguments()), 2u) << "the comma separates two actual arguments; the first is empty";
  ASSERT_NE(mi->getArguments()->at(0), nullptr);
  ASSERT_NE(mi->getArguments()->at(1), nullptr);
  EXPECT_EQ(trimmed(mi->getArguments()->at(0)->getName()), "") << "the first actual argument is empty";
  EXPECT_EQ(trimmed(mi->getArguments()->at(1)->getName()), "\"msg2 \"");
  // Name binding: the usage resolves to the definition on line 16.
  const hldb::PreprocMacroDefinition *const definition = findMacroDefinition("D");
  ASSERT_NE(definition, nullptr) << "macro 'D' is not defined";
  EXPECT_EQ(mi->getPreprocMacroDefinition(), definition);
}

// --- module definition ----

TEST_F(DefineExpansion3Test, DesignHasExactlyOneModuleTop) {
  ASSERT_EQ(countOf(m_design->getAllModules()), 1u) << "the source declares exactly one design element";
  const hldb::Module *const top = m_design->getAllModules()->at(0);
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getDefName(), "top");
  EXPECT_EQ(top, getTop());
}

TEST_F(DefineExpansion3Test, ModuleTopSpansLines17To19) {
  // Sec 22.12: the expansion on line 18 does not shift 'endmodule' off
  // line 19.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(endsWith(top->getFile(), kMainFileName)) << "file: " << top->getFile();
  EXPECT_EQ(top->getStartLine(), 17u);
  EXPECT_EQ(top->getStartColumn(), 1u);
  EXPECT_EQ(top->getEndLine(), 19u);
}

TEST_F(DefineExpansion3Test, ModuleTopHasStaticDefaultLifetime) {
  // "module top" carries no lifetime qualifier; Sec 6.21: "The default
  // lifetime is static."
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_FALSE(top->getAutomatic());
}

TEST_F(DefineExpansion3Test, ModuleTopHasOneNullPort) {
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

TEST_F(DefineExpansion3Test, ModuleTopHoldsOnlyOneProcess) {
  // The expansion of `D adds one initial procedure; nothing is declared or
  // instantiated.
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

// --- the expanded module item ----

TEST_F(DefineExpansion3Test, ExpansionIsAnInitialWithOneDisplay) {
  // initial $display(...); -- the statement is the call itself, no
  // begin-end block. Both are produced where D is used, on line 18.
  const hldb::Initial *const initial = getInitial();
  ASSERT_NE(initial, nullptr) << "the only process is not an initial procedure";
  EXPECT_EQ(initial->getVpiType(), static_cast<uint32_t>(vpiInitial));
  EXPECT_EQ(initial->getStartLine(), 18u) << "expanded text is located where the macro is used (Sec 22.12, 22.13)";

  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr) << "the initial procedure's statement is not a system task call";
  EXPECT_EQ(call->getName(), "$display");
  EXPECT_FALSE(call->getUserDefn()) << "$display is a built-in system task (Sec 21.2)";
  EXPECT_EQ(call->getStartLine(), 18u) << "expanded text is located where the macro is used (Sec 22.12, 22.13)";
}

TEST_F(DefineExpansion3Test, DisplayReceivesExpandedArguments) {
  // The LRM's own expansion: $display("start", , "msg2 ", "end").
  const hldb::SysTaskCall *const call = getDisplayCall();
  ASSERT_NE(call, nullptr);
  ASSERT_EQ(countOf(call->getArguments()), 4u) << "\"start\", an empty argument, \"msg2 \", \"end\"";

  // Argument 0, "start". Its stored value also shows how HLC wraps a
  // string literal's characters, which the other literals are compared with.
  const hldb::Constant *const start = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(start, nullptr) << "argument 0 is not a constant";
  EXPECT_EQ(start->getConstType(), vpiStringConst) << "Annex K: #define vpiStringConst 6 /* string literal */";
  const std::string_view startValue = start->getValue();
  const size_t at = startValue.find("start");
  ASSERT_NE(at, std::string_view::npos) << "argument 0 value: " << startValue;
  const std::string prefix(startValue.substr(0u, at));
  const std::string suffix(startValue.substr(at + std::string_view("start").size()));

  // Argument 1, the empty argument left by substituting x with nothing.
  const hldb::Operation *const empty = any_cast<hldb::Operation>(call->getArguments()->at(1));
  ASSERT_NE(empty, nullptr) << "argument 1 is not an operation (Sec 37.42 detail 8)";
  EXPECT_EQ(empty->getOpType(), vpiNullOp) << "Sec 37.42 detail 8: an empty argument is a vpiNullOp operation";

  // Argument 2, "msg2 ": the trailing space is part of the literal.
  const hldb::Constant *const msg2 = any_cast<hldb::Constant>(call->getArguments()->at(2));
  ASSERT_NE(msg2, nullptr) << "argument 2 is not a constant";
  EXPECT_EQ(msg2->getConstType(), vpiStringConst);
  EXPECT_EQ(msg2->getValue(), prefix + "msg2 " + suffix) << "the literal \"msg2 \" keeps its trailing space";

  // Argument 3, "end".
  const hldb::Constant *const end = any_cast<hldb::Constant>(call->getArguments()->at(3));
  ASSERT_NE(end, nullptr) << "argument 3 is not a constant";
  EXPECT_EQ(end->getConstType(), vpiStringConst);
  EXPECT_EQ(end->getValue(), prefix + "end" + suffix);
}

// --- elaboration ----

TEST_F(DefineExpansion3Test, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    // Sec 23.3.1: 'top' appears in no instantiation, so it is the top-level
    // module, implicitly instantiated once under its own name.
    ASSERT_EQ(countOf(m_design->getTopModules()), 1u);
    const hldb::Module *const inst = m_design->getTopModules()->at(0);
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->getName(), "top");
    EXPECT_EQ(inst->getDefName(), "top");
    EXPECT_TRUE(inst->getTopModule());
    // Sec 37.10 definition location of the instance (line 17).
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
