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

// Test for tests/Google/chapter-22/22.6--ifdef-behavioral.sv
//
// Lines 1-7 are one-line "//" license comments, lines 8-9 are blank, lines
// 10-15 are one block comment carrying the test metadata, and the
// SystemVerilog text is:
//
//   16: module and_op (a, b, c);
//   17:   output a;
//   18:   input b, c;
//   19:   `ifdef behavioral
//   20:     wire a = b & c;
//   21:   `else
//   22:     and a1 (a,b,c);
//   23:   `endif
//   24: endmodule
//
// This is Example 1 of IEEE 1800-2023 22.6, which says of it: "If the
// identifier behavioral is defined, a continuous net assignment will be
// compiled in; otherwise, an and gate will be instantiated."
//
// Lines 19-23 are a conditional_compilation_directive (Syntax 22-5), which
// "may appear anywhere in the source description", here inside a module.
// behavioral is defined nowhere: the file holds no `define, and the run is
// assumed to define no macro on the command line. 22.6: "If the `ifdef is
// followed by a text_macro_identifier without parentheses, it succeeds if
// that identifier is currently defined as a text macro name"; it is not, so
// "the associated block_of_text ... is ignored", and the `else block is
// processed. So the module compiles as:
//
//   module and_op (a, b, c);
//     output a;
//     input b, c;
//     and a1 (a,b,c);
//   endmodule
//
// The name behavioral after `ifdef is tested, not substituted: it is no macro
// usage.
//
// and_op has a non-ANSI port list (23.2.2.1) of three ports, a, b and c, in
// that order (vpiPortIndex 0, 1, 2, 37.14 detail 9), declared output a and
// input b and c. "An implicit data type declaration implies a net unless the
// var keyword is used" (23.2.2.3), and with no `default_nettype the default
// net type is wire (22.8), so a, b and c are wire nets.
//
// Line 22 is a gate instantiation (28.3) of the n-input gate and (28.4): "The
// first terminal in the terminal list shall connect to the output of the
// gate and all other terminals connect to its inputs", so a is the output
// and b and c are the inputs. In the VPI object model the gate is a vpiGate
// primitive with vpiPrimType vpiAndPrim, its terminals carry vpiDirection and
// a vpiTermIndex starting at zero (37.35 detail 3), and "vpiSize shall return
// the number of inputs" (37.35 detail 1), 2 here. It has no delay.
//
// and_op is never instantiated, so it is a top-level module, implicitly
// instantiated once under its own name (23.3.1).
//
// Every assertion below follows from the LRM; a red run means HLC deviates
// from it. Some checks read an LRM fact through a code: an empty generic
// directive list says the file holds none of the directives HLDB records
// that way (`timescale, `default_nettype, `celldefine and the others with a
// vpiDirectiveType code); and vpiOutput, vpiInput, vpiWire, vpiGate,
// vpiAndPrim and vpiSize are codes of the LRM's own VPI object model
// (Clause 37, Annex K).
//
// What is checked
//   - No fatal, syntax or error diagnostic, and none of the preprocessor
//     diagnostics a wrong reading of the conditional directives would
//     produce; in particular no "unknown macro" for the tested, undefined
//     name behavioral.
//   - Exactly one SourceFile for the .sv, with no includes, no generic
//     directives, no macro definitions and no macro instances.
//   - Exactly one module, and_op, on lines 16-24, with the ports a (output),
//     b and c (input) at indexes 0, 1 and 2.
//   - The `else block is compiled: exactly one primitive, the and gate a1,
//     with no delay and the terminals a (output, index 0), b and c (inputs,
//     indexes 1 and 2); the gate reports vpiSize 2.
//   - The `ifdef block is ignored: no continuous assignment, no process, no
//     variable and no module instance, and a carries no net declaration
//     assignment.
//   - Before elaboration, and_op holds either no nets yet or exactly the wire
//     nets a, b and c; after elaboration, the top-level instance and_op holds
//     exactly those three.
//   - No other design object is created from this file.
//
// What is NOT checked, and why
//   - The PreprocMacroCondition objects of `ifdef, `else and `endif. The
//     headers expose no collection that holds them, so reading them would
//     guess a tool convention. The effect of the condition is checked where
//     it lives instead. Nearest assertions: ElseBlockIsCompiled and
//     IfdefBlockIsIgnored.
//   - A run that defines behavioral on the command line. The fixture does not
//     define it, and the run is assumed not to; with it defined, the other
//     block would be compiled. Nearest assertion: ElseBlockIsCompiled.
//   - When HLDB records the nets declared by the port declarations. 23.2.2.3
//     says they are nets, not at which stage a tool records them, so before
//     elaboration "none yet" is accepted besides the three wires. Nearest
//     assertions: PortNetsBeforeElaboration and the elaborated part of
//     NoOtherDesignObjectsDeclared.
//   - Which net a port's low connection refers to, and which nets the gate
//     terminals bind to. Name binding only exists after elaboration, and the
//     terminal names already fix the connection. Nearest assertions: the
//     terminal names in ElseBlockIsCompiled.
//   - How HLDB represents a terminal expression (for example as a RefObj).
//     The LRM fixes the name, not the HLDB class, so the name is read through
//     Any::getName(). Nearest assertion: the terminal names in
//     ElseBlockIsCompiled.
//   - The gate definition name (vpiDefName). The LRM lists the property but
//     does not say what it holds for a built-in gate. Nearest assertion:
//     ElseBlockIsCompiled (vpiPrimType vpiAndPrim).
//   - The values the gate drives. They only exist while simulation time
//     advances, so they are permanently out of scope. Nearest assertion:
//     ElseBlockIsCompiled (the gate kind and its terminals).
//   - The seven one-line comments and the block comment (5.4). The LRM gives
//     comments no meaning; how HLDB groups, classifies (vpiComment and
//     vpiDocumentComment are marked non-standard) and attaches them is a tool
//     convention. Nearest assertions: the line checks in AndOpModule and
//     ElseBlockIsCompiled.
//   - Columns. The LRM numbers lines (22.13) but not columns. Nearest
//     assertions: the same line checks.
//   - The SourceFile time unit and precision. There is no `timescale or
//     timeunit, so the default applies, and the default is
//     implementation-specific (3.14.2.3). Nearest assertion:
//     SourceFileHasNoIncludesDirectivesOrMacros (zero directives).
//   - The warning count. The LRM neither requires nor forbids warnings here.
//     Nearest assertion: CompilesWithoutErrors.
//   - Only the top-level instance and its nets are gated on
//     Design::getElaborated(): the instance tree only exists after
//     elaboration. Everything else is established by parsing and is asserted
//     unconditionally.

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
#include <hldb/expr.h>
#include <hldb/interface.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/port.h>
#include <hldb/preproc_macro_definition.h>
#include <hldb/preproc_macro_instance.h>
#include <hldb/prim_term.h>
#include <hldb/primitive.h>
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
constexpr std::string_view kSourceFileName = "22.6--ifdef-behavioral.sv";

// The ports of and_op in port-list order, which are also its nets.
const std::vector<std::string_view> kPortNames = {"a", "b", "c"};

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

// The object named 'name' in 'collection', or nullptr.
template <typename T>
const T *findByName(const std::vector<T *> *collection, std::string_view name) {
  if (collection != nullptr) {
    for (const T *object : *collection) {
      if (object->getName() == name) return object;
    }
  }
  return nullptr;
}

// The name 'term' connects to, or empty when it has no expression.
std::string_view terminalName(const hldb::PrimTerm *term) {
  const hldb::Expr *const expr = term->getExpr();
  return (expr == nullptr) ? std::string_view() : expr->getName();
}

// Checks that 'nets' holds exactly the wire nets a, b and c (23.2.2.3, 22.8),
// none with a net declaration assignment.
void expectPortWires(const hldb::NetCollection *nets) {
  ASSERT_EQ(sizeOf(nets), kPortNames.size());
  for (std::string_view name : kPortNames) {
    const hldb::Net *const net = findByName(nets, name);
    EXPECT_NE(net, nullptr) << "no net " << name;
    if (net == nullptr) continue;
    EXPECT_EQ(net->getNetType(), vpiWire) << name << " has the default net type wire (22.8)";
    EXPECT_FALSE(net->getNetDeclAssign()) << name << ": the `ifdef block with wire a = b & c; is ignored";
  }
}

// The line a diagnostic points at, or 0 when it carries no location.
uint32_t lineOf(const Error &error) { return error.getLocations().empty() ? 0 : error.getLocations().front().m_line; }

// Every diagnostic of 'severity' as " [<code> line <n>: <text>]", for failure
// messages, so a red run shows what was reported and where. <text> is HLC's
// message template, so its placeholders are not filled in.
std::string describeErrors(const ErrorContainer *errorContainer, ErrorDefinition::ErrorSeverity severity) {
  const ErrorDefinition::ErrorMap &errorInfoMap = ErrorDefinition::getErrorInfoMap();
  std::string description;
  for (const Error &error : errorContainer->getErrors()) {
    const ErrorDefinition::ErrorMap::const_iterator errorInfo = errorInfoMap.find(error.getType());
    if ((errorInfo == errorInfoMap.end()) || (errorInfo->second.m_severity != severity)) continue;
    description += " [" + std::to_string(static_cast<int32_t>(error.getType()));
    description += " line " + std::to_string(lineOf(error));
    description += ": " + std::string(errorInfo->second.m_errorText) + "]";
  }
  return description;
}
}  // namespace

class IfdefBehavioralTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "22.6--ifdef-behavioral.hlc"}); }
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

  // The module definition of and_op from the file under test, or nullptr unless there is exactly one.
  static const hldb::Module *getAndOpDefinition() {
    if ((m_design == nullptr) || (countFromSourceFile(m_design->getAllModules()) != 1)) return nullptr;
    return firstFromSourceFile(m_design->getAllModules());
  }
};

// The module, its port declarations, the conditional directives and the
// selected gate instantiation are all legal (23.2.2.1, 22.6, 28.3, 28.4), so
// nothing is fatal, a syntax error or an error.
TEST_F(IfdefBehavioralTest, CompilesWithoutErrors) {
  ASSERT_NE(m_session, nullptr);
  const ErrorContainer *const errorContainer = m_session->getErrorContainer();
  ASSERT_NE(errorContainer, nullptr);

  const ErrorContainer::Stats stats = errorContainer->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0) << describeErrors(errorContainer, ErrorDefinition::FATAL);
  EXPECT_EQ(stats.nbSyntax, 0) << describeErrors(errorContainer, ErrorDefinition::SYNTAX);
  EXPECT_EQ(stats.nbError, 0) << describeErrors(errorContainer, ErrorDefinition::ERROR);
}

// Each of these diagnostics would mean HLC misread the conditional directives.
TEST_F(IfdefBehavioralTest, ReportsNoPreprocessorDiagnostics) {
  // Lines 19-23 match Syntax 22-5.
  EXPECT_EQ(findError(ErrorDefinition::PP_SYNTAX_ERROR), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_MACRO_SYNTAX_ERROR), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_ELSEIF), nullptr);

  // `ifdef tests whether behavioral is defined (22.6); the name is not a
  // macro usage, so its being undefined is no "unknown macro".
  EXPECT_EQ(findError(ErrorDefinition::PP_UNKOWN_MACRO), nullptr);

  // The conditional directives "may appear anywhere in the source
  // description" (22.6), here inside a module.
  EXPECT_EQ(findError(ErrorDefinition::PP_ILLEGAL_DIRECTIVE_IN_DESIGN_ELEMENT), nullptr);

  // The file is plain ASCII.
  EXPECT_EQ(findError(ErrorDefinition::PP_NON_ASCII_CONTENT), nullptr);
}

TEST_F(IfdefBehavioralTest, SourceFileRecordedOnce) {
  ASSERT_NE(m_design, nullptr);
  EXPECT_EQ(findSourceFiles().size(), 1u);
}

TEST_F(IfdefBehavioralTest, SourceFileHasNoIncludesDirectivesOrMacros) {
  const hldb::SourceFile *const sourceFile = getSourceFile();
  ASSERT_NE(sourceFile, nullptr);

  // There is no `include in the file (22.4).
  EXPECT_EQ(sizeOf(sourceFile->getIncludes()), 0u);

  // `ifdef, `else and `endif are the only directives in the file. None of the
  // directives HLDB records as generic directives (those with a
  // vpiDirectiveType code: `timescale, `default_nettype, `celldefine, ...)
  // occurs, and the conditional directives have no vpiDirectiveType code, so a
  // generic directive here would be one the source does not contain.
  EXPECT_EQ(sizeOf(sourceFile->getDirectives()), 0u);

  // The file defines no macro, and behavioral after `ifdef is tested, not
  // used (22.6), so there is no macro definition and no macro instance.
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroDefinitions()), 0u);
  EXPECT_EQ(sizeOf(sourceFile->getPreprocMacroInstances()), 0u);
}

// Lines 16-24: module and_op (a, b, c); output a; input b, c; ... endmodule.
// The non-ANSI port list holds a, b and c in that order (23.2.2.1), at
// vpiPortIndex 0, 1 and 2 (37.14 detail 9), declared output, input, input.
TEST_F(IfdefBehavioralTest, AndOpModule) {
  ASSERT_NE(m_design, nullptr);
  ASSERT_EQ(countFromSourceFile(m_design->getAllModules()), 1u);
  const hldb::Module *const andOp = getAndOpDefinition();
  ASSERT_NE(andOp, nullptr);

  EXPECT_EQ(andOp->getDefName(), "and_op");
  EXPECT_EQ(andOp->getStartLine(), 16u);
  EXPECT_EQ(andOp->getEndLine(), 24u);

  // "endmodule" carries no ": and_op" end label.
  EXPECT_EQ(andOp->getEndLabel(), "");

  const hldb::PortCollection *const ports = andOp->getPorts();
  ASSERT_EQ(sizeOf(ports), 3u);
  const std::vector<int32_t> directions = {vpiOutput, vpiInput, vpiInput};
  for (size_t index = 0; index < kPortNames.size(); ++index) {
    const hldb::Port *const port = ports->at(index);
    EXPECT_EQ(port->getName(), kPortNames[index]) << "port " << index;
    EXPECT_EQ(port->getPortIndex(), static_cast<int32_t>(index)) << kPortNames[index];
    EXPECT_EQ(port->getDirection(), directions[index]) << kPortNames[index];
  }
}

// 22.6: behavioral is not defined, so the `ifdef block is ignored and the
// `else block, and a1 (a,b,c);, is processed: an instance of the n-input
// gate and (28.4), a vpiGate primitive with vpiPrimType vpiAndPrim, named a1,
// on line 22, with no delay, the output a at terminal index 0 and the inputs
// b and c at indexes 1 and 2 (37.35 detail 3).
TEST_F(IfdefBehavioralTest, ElseBlockIsCompiled) {
  const hldb::Module *const andOp = getAndOpDefinition();
  ASSERT_NE(andOp, nullptr);
  ASSERT_EQ(sizeOf(andOp->getPrimitives()), 1u) << "the `else block holds the one gate a1";
  const hldb::Primitive *const gate = andOp->getPrimitives()->front();

  EXPECT_EQ(gate->getName(), "a1");
  EXPECT_EQ(gate->getVpiType(), static_cast<uint32_t>(vpiGate)) << "a1 is a gate, not a UDP or a switch";
  EXPECT_EQ(gate->getPrimType(), vpiAndPrim);
  EXPECT_EQ(gate->getStartLine(), 22u);
  EXPECT_EQ(sizeOf(gate->getDelays()), 0u) << "and a1 (a,b,c); has no delay";

  const hldb::PrimTermCollection *const terms = gate->getPrimTerms();
  ASSERT_EQ(sizeOf(terms), 3u);
  for (size_t index = 0; index < kPortNames.size(); ++index) {
    const hldb::PrimTerm *const term = terms->at(index);
    EXPECT_EQ(term->getTermIndex(), static_cast<int32_t>(index)) << "terminal " << index;
    EXPECT_EQ(term->getDirection(), (index == 0) ? vpiOutput : vpiInput) << "terminal " << index;
    EXPECT_EQ(terminalName(term), kPortNames[index]) << "terminal " << index;
  }
}

// 37.35 detail 1: "vpiSize shall return the number of inputs." a1 has the two
// inputs b and c.
TEST_F(IfdefBehavioralTest, GateReportsTwoInputs) {
  const hldb::Module *const andOp = getAndOpDefinition();
  ASSERT_NE(andOp, nullptr);
  const hldb::Primitive *const gate = findByName(andOp->getPrimitives(), "a1");
  ASSERT_NE(gate, nullptr);
  const hldb::Any::vpi_property_value_t size = gate->getVpiPropertyValue(vpiSize);
  ASSERT_TRUE(std::holds_alternative<int64_t>(size));
  EXPECT_EQ(std::get<int64_t>(size), 2);
}

// 22.6: the `ifdef block, wire a = b & c;, "is ignored". So and_op holds no
// continuous assignment, and nothing else besides the gate: no process,
// variable or module instance.
TEST_F(IfdefBehavioralTest, IfdefBlockIsIgnored) {
  const hldb::Module *const andOp = getAndOpDefinition();
  ASSERT_NE(andOp, nullptr);
  EXPECT_EQ(sizeOf(andOp->getContAssigns()), 0u) << "the continuous net assignment is in the ignored block";
  EXPECT_EQ(sizeOf(andOp->getProcesses()), 0u);
  EXPECT_EQ(sizeOf(andOp->getVariables()), 0u);
  EXPECT_EQ(sizeOf(andOp->getModules()), 0u);
}

// 23.2.2.3 and 22.8: the port declarations make a, b and c wire nets, and the
// ignored `ifdef block gives a no net declaration assignment. Before
// elaboration the model may not have recorded the nets yet; if it has, they
// must be exactly those three.
TEST_F(IfdefBehavioralTest, PortNetsBeforeElaboration) {
  const hldb::Module *const andOp = getAndOpDefinition();
  ASSERT_NE(andOp, nullptr);
  const size_t netCount = sizeOf(andOp->getNets());
  ASSERT_TRUE((netCount == 0) || (netCount == kPortNames.size())) << "net count: " << netCount;
  if (netCount != 0) expectPortWires(andOp->getNets());
}

// and_op is the only module and is never instantiated, so nothing else is
// declared in the file, and and_op is the one top-level module (23.3.1).
TEST_F(IfdefBehavioralTest, NoOtherDesignObjectsDeclared) {
  ASSERT_NE(m_design, nullptr);

  EXPECT_EQ(countFromSourceFile(m_design->getAllInterfaces()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPrograms()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllPackages()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllClasses()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getAllUdps()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getConfigs()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getCheckerDecls()), 0u);

  EXPECT_EQ(countFromSourceFile(m_design->getParameters()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getParamAssigns()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getVariables()), 0u);
  EXPECT_EQ(countFromSourceFile(m_design->getTypedefs()), 0u);

  // A top-level module is implicitly instantiated once, under its own name
  // (23.3.1). The instance tree only exists after elaboration; the instance
  // holds the three wire nets of the ports and the one gate.
  if (m_design->getElaborated()) {
    const hldb::ModuleCollection *const topModules = m_design->getTopModules();
    ASSERT_EQ(countFromSourceFile(topModules), 1u);
    const hldb::Module *const topInstance = firstFromSourceFile(topModules);
    EXPECT_EQ(topInstance->getName(), "and_op");
    EXPECT_EQ(topInstance->getDefName(), "and_op");
    EXPECT_EQ(sizeOf(topInstance->getPrimitives()), 1u);
    expectPortWires(topInstance->getNets());
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
