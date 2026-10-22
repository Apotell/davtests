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

// Source under test: tests/ariane/fpga/src/apb_node/src/apb_node.sv
// (compiled alone, see the Compile() arguments).
//
// clang-format off
// ----- begin apb_node.sv -----
// (non-ASCII smart quotes in the original license comment converted to ASCII)
// // Copyright 2016 ETH Zurich and University of Bologna.
// // Copyright and related rights are licensed under the Solderpad Hardware
// // License, Version 0.51 (the "License"); you may not use this file except in
// // compliance with the License.  You may obtain a copy of the License at
// // http://solderpad.org/licenses/SHL-0.51. Unless required by applicable law
// // or agreed to in writing, software, hardware and materials distributed under
// // this License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// // CONDITIONS OF ANY KIND, either express or implied. See the License for the
// // specific language governing permissions and limitations under the License.
//
// // Engineer:       Davide Rossi - davide.rossi@unibo.it
// //                 Florian Zaruba - zarubaf@iis.ee.ethz.ch
// // Design Name:    APB Bus
// // Module Name:    apb_node
// // Project Name:   PULP/Kerbin
// // Language:       SystemVerilog
// //
// // Description:    This component implements a configurable APB node
//
// module apb_node #(
//         parameter int unsigned NB_MASTER      = 8,
//         parameter int unsigned APB_DATA_WIDTH = 32,
//         parameter int unsigned APB_ADDR_WIDTH = 32
// )(
//         // SLAVE PORT
//         input logic 					 penable_i,
//         input logic 					 pwrite_i,
//         input logic [APB_ADDR_WIDTH-1:0] 		 paddr_i,
//         input logic                                      psel_i,
//         input logic [APB_DATA_WIDTH-1:0] 		 pwdata_i,
//         output logic [APB_DATA_WIDTH-1:0] 		 prdata_o,
//         output logic 					 pready_o,
//         output logic 					 pslverr_o,
//
//         // MASTER PORTS
//         output logic [NB_MASTER-1:0] 			 penable_o,
//         output logic [NB_MASTER-1:0] 			 pwrite_o,
//         output logic [NB_MASTER-1:0][APB_ADDR_WIDTH-1:0] paddr_o,
//         output logic [NB_MASTER-1:0] 			 psel_o,
//         output logic [NB_MASTER-1:0][APB_DATA_WIDTH-1:0] pwdata_o,
//         input logic [NB_MASTER-1:0][APB_DATA_WIDTH-1:0]  prdata_i,
//         input logic [NB_MASTER-1:0] 			 pready_i,
//         input logic [NB_MASTER-1:0] 			 pslverr_i,
//
//         // CONFIGURATION PORT
//         input logic [NB_MASTER-1:0][APB_ADDR_WIDTH-1:0]  START_ADDR_i,
//         input logic [NB_MASTER-1:0][APB_ADDR_WIDTH-1:0]  END_ADDR_i
//     );
//
//     always_comb begin : match_address
//         psel_o = '0;
//
//         // generate the select signal based on the supplied address
//         for (int unsigned i = 0; i < NB_MASTER; i++)
//             psel_o[i]  =  psel_i & (paddr_i >= START_ADDR_i[i]) && (paddr_i <= END_ADDR_i[i]);
//     end
//
//     always_comb begin
//         // default assignment - keep silent by default
//         penable_o = '0;
//         pwrite_o  = '0;
//         paddr_o   = '0;
//         pwdata_o  = '0;
//         prdata_o  = '0;
//         pready_o  = 1'b0;
//         pslverr_o = 1'b0;
//         // select the right master
//         for (int unsigned i = 0; i < NB_MASTER; i++) begin
//             // master j was selected because the address matched int the generate statement above
//             if (psel_o[i]) begin
//                 // master out - slave in
//                 penable_o[i] = penable_i;
//                 pwrite_o[i]  = pwrite_i;
//                 paddr_o[i]   = paddr_i;
//                 pwdata_o[i]  = pwdata_i;
//                 // master in - slave out
//                 prdata_o     = prdata_i[i];
//                 pready_o     = pready_i[i];
//                 pslverr_o    = pslverr_i[i];
//             end
//         end
//     end
// endmodule
// ----- end apb_node.sv -----
// clang-format on
//
// IEEE 1800-2023 semantics exercised:
//   apb_node is the only module, so it is the elaborated top and its
//   parameters keep their defaults. The '#( )' header declares three
//   overridable value parameters of type 'int unsigned' (Sec 6.20.1, 6.20.2;
//   'unsigned' removes the default signedness of int, Sec 6.11). The ANSI
//   ports use parameter-dependent packed widths, including two-dimensional
//   packed arrays [NB_MASTER-1:0][W-1:0] (Sec 7.4.1). 'input logic' ports
//   are nets and 'output logic' ports are variables (Sec 23.2.2.3). Two
//   always_comb procedures (Sec 9.2.2.2): the first is a named block
//   'begin : match_address' (Sec 9.3.4) holding a for-loop whose loop
//   variable is declared in the init as 'int unsigned i' (Sec 12.7.1); its
//   body mixes '&' and '&&', and per the precedence table (Sec 11.3.2) '&'
//   binds tighter, giving (psel_i & (a >= b)) && (c <= d). The second block
//   gives every output a default value with blocking assignments
//   (Sec 10.4.1) and then, in a for-loop, overrides the outputs of the
//   selected master with a one-armed if (Sec 12.4) using element selects of
//   packed arrays (Sec 7.4.5, 11.5.1).
//
// What we have to check:
//   - parameters: names, not localparam, int unsigned type, default values
//   - 18 ports: order, direction, evaluated packed widths (1-D and 2-D)
//   - inputs are nets, outputs are variables
//   - always_comb #1: named block 'match_address', psel_o = '0, for-loop
//     (int unsigned i = 0; i < NB_MASTER; i++), body assignment with the
//     correct operator precedence tree
//   - always_comb #2: unnamed block, the 7 default assignments in order and
//     their values, for-loop with begin/if(psel_o[i]) and the 7 per-master
//     assignments (4 element-select lhs, 3 element-select rhs)
//   - all procedural assignments are blocking
//
// Checked:
//   - all items listed above
//
// Not checked:
//   - HLC 786 builds no elaborated hierarchy (Design::getTopModules() is empty).
//     For a module nobody instantiates, its getAllModules() entry is the module
//     with default parameters, i.e. the IEEE top-level instance, so the top-level
//     checks fall back to that entry. Child instances and generate scopes have no
//     such entry; those tests are skipped with the reason in the skip message.
//   - combinational behaviour (address decode result, last-match-wins of the
//     loop) -- runtime-only, needs simulation
//   - the implicit sensitivity list of always_comb -- it is inferred by the
//     simulator and is not part of the VPI object model

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/for_stmt.h>
#include <hldb/if_stmt.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/process_stmt.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/scope.h>
#include <hldb/select.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <deque>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hlc {
// HLC 786 stores Constant::getValue() as the bare digits of the literal in its
// own base (e.g. "0068746469772d6f" for 64'h00687464_69772d6f); the base is only
// in getConstType(). This returns the "<KIND>:<digits>" form the value parsers
// below expect. The text lives in a static pool so the view stays valid.
[[maybe_unused]] static std::string_view constantValueText(const hldb::Constant *constant) {
  static std::deque<std::string> s_pool;
  std::string_view raw = constant->getValue();
  if ((raw.size() >= 2) && (raw.front() == '"') && (raw.back() == '"')) raw = raw.substr(1, raw.size() - 2);
  if (raw.find(':') != std::string_view::npos) return s_pool.emplace_back(raw);
  std::string kind;
  switch (constant->getConstType()) {
    case vpiBinaryConst: kind = "BIN:"; break;
    case vpiOctConst: kind = "OCT:"; break;
    case vpiHexConst: kind = "HEX:"; break;
    case vpiStringConst: kind = "STRING:"; break;
    case vpiUIntConst: kind = "UINT:"; break;
    case vpiIntConst: kind = "INT:"; break;
    default: kind = "DEC:"; break;
  }
  return s_pool.emplace_back(kind + std::string(raw));
}
// Name of a reference without any trailing select: HLC 786 names a bit/part
// select "sig[2]"; the tests compare the selected signal's name "sig".
[[maybe_unused]] static std::string_view selectBaseName(std::string_view name) {
  const size_t bracket = name.find('[');
  return (bracket == std::string_view::npos) ? name : name.substr(0, bracket);
}
namespace {
using ScopeChain = std::vector<const hldb::Scope *>;
using Bounds = std::pair<int64_t, int64_t>;

// Parses the value string of an hldb::Constant ("UINT:12", "HEX:ff",
// "BIN:01", ...) into an integer. Returns false for x/z digits or kinds that
// are not integers.
bool parseConstant(const hldb::Constant *constant, int64_t &value) {
  const std::string_view text = constantValueText(constant);
  const size_t colon = text.find(':');
  if (colon == std::string_view::npos) return false;
  const std::string_view kind = text.substr(0, colon);
  std::string digits;
  for (const char ch : text.substr(colon + 1)) {
    if (ch != '_') digits += ch;
  }
  int32_t base = 10;
  if (kind == "HEX") {
    base = 16;
  } else if (kind == "BIN") {
    base = 2;
  } else if (kind == "OCT") {
    base = 8;
  } else if ((kind != "UINT") && (kind != "INT") && (kind != "DEC")) {
    return false;
  }
  if (digits.empty()) return false;
  try {
    size_t used = 0;
    value = static_cast<int64_t>(std::stoull(digits, &used, base));
    if (used != digits.size()) return false;
  } catch (...) {
    return false;
  }
  return true;
}

// Evaluates a constant integer expression made of literals, parameter
// references (looked up in 'scopes', innermost first) and + - *.
// Works whether or not the compiler already folded the expression.
bool evalInt(const hldb::Any *expr, const ScopeChain &scopes, int64_t &value) {
  if (expr == nullptr) return false;
  if (const hldb::Constant *const constant = any_cast<hldb::Constant>(expr)) {
    return parseConstant(constant, value);
  }
  if (const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr)) {
    for (const hldb::Scope *const scope : scopes) {
      if (const hldb::ParamAssign *const pa =
              hldb::findByName<hldb::ParamAssign, hldb::ParamAssign>(ref->getName(), scope->getParamAssigns())) {
        return evalInt(pa->getRhs(), scopes, value);
      }
    }
    return false;
  }
  if (const hldb::Operation *const op = any_cast<hldb::Operation>(expr)) {
    const hldb::AnyCollection *const operands = op->getOperands();
    if ((operands == nullptr) || (operands->size() != 2)) return false;
    int64_t lhs = 0;
    int64_t rhs = 0;
    if (!evalInt(operands->at(0), scopes, lhs) || !evalInt(operands->at(1), scopes, rhs)) return false;
    switch (op->getOpType()) {
      case vpiAddOp: value = lhs + rhs; return true;
      case vpiSubOp: value = lhs - rhs; return true;
      case vpiMultOp: value = lhs * rhs; return true;
      default: return false;
    }
  }
  return false;
}

// Collects the evaluated [left:right] bounds of each packed dimension of a
// logic-typed declaration, outermost first (empty for a scalar). Returns
// false if the type is not a logic typespec or a bound cannot be evaluated.
bool logicBounds(const hldb::RefTypespec *rt, const ScopeChain &scopes, std::vector<Bounds> &bounds) {
  bounds.clear();
  if (rt == nullptr) return false;
  const hldb::LogicTypespec *const lt = rt->getActual<hldb::LogicTypespec>();
  if (lt == nullptr) return false;
  if (const hldb::RangeCollection *const ranges = lt->getRanges()) {
    for (const hldb::Range *const range : *ranges) {
      int64_t left = 0;
      int64_t right = 0;
      if (!evalInt(range->getLeftExpr(), scopes, left) || !evalInt(range->getRightExpr(), scopes, right)) {
        return false;
      }
      bounds.emplace_back(left, right);
    }
  }
  return true;
}

// Name of the object a (possibly nested) select ultimately indexes.
std::string_view baseName(const hldb::Any *expr) {
  if (expr == nullptr) return {};
  if (const hldb::Select *const sel = any_cast<hldb::Select>(expr)) {
    if (sel->getPrefix() != nullptr) return baseName(sel->getPrefix());
    return sel->getName();
  }
  return expr->getName();
}

// Returns 'expr' as an operation of the given type and operand count, or
// nullptr if it is not exactly that.
const hldb::Operation *asOp(const hldb::Any *expr, int32_t opType, size_t operandCount) {
  const hldb::Operation *const op = any_cast<hldb::Operation>(expr);
  if ((op == nullptr) || (op->getOpType() != opType) || (op->getOperands() == nullptr) ||
      (op->getOperands()->size() != operandCount)) {
    return nullptr;
  }
  return op;
}

// Checks that 'expr' is the element select base[index] with a simple
// identifier index.
void expectBitSelect(const hldb::Any *expr, std::string_view base, std::string_view index) {
  const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(expr);
  ASSERT_NE(sel, nullptr) << "expected " << base << "[" << index << "]";
  EXPECT_EQ(baseName(sel), base);
  ASSERT_NE(sel->getIndex(), nullptr);
  EXPECT_EQ(selectBaseName(sel->getIndex()->getName()), index);
}

// Checks 'for (int unsigned i = 0; i < NB_MASTER; i++)'.
void expectMasterLoopHeader(const hldb::ForStmt *loop, const ScopeChain &scopes) {
  // Loop variable: declared in the for-init, type int unsigned (Sec 12.7.1).
  const hldb::Variable *loopVar = hldb::findByName<hldb::Variable>("i", loop->getVariables());
  ASSERT_NE(loop->getForInitStmts(), nullptr);
  ASSERT_EQ(loop->getForInitStmts()->size(), 1u);
  const hldb::Assignment *const init = any_cast<hldb::Assignment>(loop->getForInitStmts()->front());
  ASSERT_NE(init, nullptr);
  ASSERT_NE(init->getLhs(), nullptr);
  EXPECT_EQ(selectBaseName(init->getLhs()->getName()), "i");
  if (loopVar == nullptr) loopVar = any_cast<hldb::Variable>(init->getLhs());
  ASSERT_NE(loopVar, nullptr) << "loop variable 'i' must be declared by the for-init";
  ASSERT_NE(loopVar->getTypespec(), nullptr);
  const hldb::IntTypespec *const ts = loopVar->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(ts, nullptr);
  EXPECT_FALSE(ts->getSigned()) << "int unsigned";
  int64_t initValue = -1;
  ASSERT_TRUE(evalInt(init->getRhs(), scopes, initValue));
  EXPECT_EQ(initValue, 0);

  const hldb::Operation *const cond = asOp(loop->getCondition(), vpiLtOp, 2);
  ASSERT_NE(cond, nullptr) << "i < NB_MASTER";
  EXPECT_EQ(selectBaseName(cond->getOperands()->at(0)->getName()), "i");
  int64_t limit = 0;
  ASSERT_TRUE(evalInt(cond->getOperands()->at(1), scopes, limit));
  EXPECT_EQ(limit, 8);

  ASSERT_NE(loop->getForIncStmts(), nullptr);
  ASSERT_EQ(loop->getForIncStmts()->size(), 1u);
  const hldb::Operation *const inc = asOp(loop->getForIncStmts()->front(), vpiPostIncOp, 1);
  ASSERT_NE(inc, nullptr) << "i++";
  EXPECT_EQ(selectBaseName(inc->getOperands()->front()->getName()), "i");
}

struct PortSpec {
  std::string_view m_name;
  int32_t m_direction;
  std::vector<Bounds> m_bounds;
};
}  // namespace

class ApbNodeTest : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-wd", "../../../tests/ariane/fpga/src/apb_node/src", "-d", "ast", "-d", "db", "-nobuiltin",
                       "apb_node.sv"});
  }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Unelaborated module definition (structure of statements/expressions).
  static const hldb::Module *def() { return hldb::findByDefName<hldb::Module>("apb_node", m_design->getAllModules()); }
  // Elaborated top instance (nets/variables/parameter values).
  static const hldb::Module *top() {
    return (hldb::findByName<hldb::Module>("apb_node", m_design->getTopModules()) != nullptr
                ? hldb::findByName<hldb::Module>("apb_node", m_design->getTopModules())
                : hldb::findByDefName<hldb::Module>("apb_node", m_design->getAllModules()));
  }

  // The n-th (0-based) always_comb procedure of the definition.
  static const hldb::Always *alwaysComb(size_t n) {
    const hldb::Module *const mod = def();
    if ((mod == nullptr) || (mod->getProcesses() == nullptr)) return nullptr;
    size_t seen = 0;
    for (const hldb::Process *const proc : *mod->getProcesses()) {
      const hldb::Always *const always = any_cast<hldb::Always>(proc);
      if ((always == nullptr) || (always->getAlwaysType() != vpiAlwaysComb)) continue;
      if (seen == n) return always;
      ++seen;
    }
    return nullptr;
  }
};

TEST_F(ApbNodeTest, ParametersAreIntUnsignedWithDefaults) {
  const hldb::Module *const mod = top();
  ASSERT_NE(mod, nullptr) << "apb_node is instantiated by nobody, so it must be a top instance";
  const std::vector<std::pair<std::string_view, int64_t>> expected = {
      {"NB_MASTER", 8}, {"APB_DATA_WIDTH", 32}, {"APB_ADDR_WIDTH", 32}};
  for (const std::pair<std::string_view, int64_t> &item : expected) {
    SCOPED_TRACE(std::string(item.first));
    const hldb::Parameter *const param = hldb::findByName<hldb::Parameter>(item.first, mod->getParameters());
    ASSERT_NE(param, nullptr);
    EXPECT_FALSE(param->getLocalParam()) << "declared in the #( ) parameter port list (Sec 6.20.1)";
    ASSERT_NE(param->getTypespec(), nullptr);
    const hldb::IntTypespec *const ts = param->getTypespec()->getActual<hldb::IntTypespec>();
    ASSERT_NE(ts, nullptr);
    EXPECT_FALSE(ts->getSigned()) << "int unsigned";
    const hldb::ParamAssign *const pa =
        hldb::findByName<hldb::ParamAssign, hldb::ParamAssign>(item.first, mod->getParamAssigns());
    ASSERT_NE(pa, nullptr);
    EXPECT_FALSE(pa->getOverridden());
    int64_t value = 0;
    ASSERT_TRUE(evalInt(pa->getRhs(), {mod}, value));
    EXPECT_EQ(value, item.second);
  }
}

TEST_F(ApbNodeTest, PortListOrderDirectionAndWidth) {
  const hldb::Module *const mod = top();
  ASSERT_NE(mod, nullptr);
  const std::vector<PortSpec> expected = {
      {"penable_i", vpiInput, {}},
      {"pwrite_i", vpiInput, {}},
      {"paddr_i", vpiInput, {{31, 0}}},
      {"psel_i", vpiInput, {}},
      {"pwdata_i", vpiInput, {{31, 0}}},
      {"prdata_o", vpiOutput, {{31, 0}}},
      {"pready_o", vpiOutput, {}},
      {"pslverr_o", vpiOutput, {}},
      {"penable_o", vpiOutput, {{7, 0}}},
      {"pwrite_o", vpiOutput, {{7, 0}}},
      {"paddr_o", vpiOutput, {{7, 0}, {31, 0}}},
      {"psel_o", vpiOutput, {{7, 0}}},
      {"pwdata_o", vpiOutput, {{7, 0}, {31, 0}}},
      {"prdata_i", vpiInput, {{7, 0}, {31, 0}}},
      {"pready_i", vpiInput, {{7, 0}}},
      {"pslverr_i", vpiInput, {{7, 0}}},
      {"START_ADDR_i", vpiInput, {{7, 0}, {31, 0}}},
      {"END_ADDR_i", vpiInput, {{7, 0}, {31, 0}}},
  };
  ASSERT_NE(mod->getPorts(), nullptr);
  ASSERT_EQ(mod->getPorts()->size(), expected.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    const hldb::Port *const port = mod->getPorts()->at(i);
    SCOPED_TRACE(std::string(expected[i].m_name));
    EXPECT_EQ(selectBaseName(port->getName()), expected[i].m_name);
    EXPECT_EQ(port->getDirection(), expected[i].m_direction);
    std::vector<Bounds> bounds;
    EXPECT_TRUE(logicBounds(port->getTypespec(), {mod}, bounds));
    EXPECT_EQ(bounds, expected[i].m_bounds);
  }
}

TEST_F(ApbNodeTest, InputPortsAreNetsOutputPortsAreVariables) {
  const hldb::Module *const mod = top();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getPorts(), nullptr);
  for (const hldb::Port *const port : *mod->getPorts()) {
    const std::string_view name = port->getName();
    SCOPED_TRACE(std::string(name));
    const hldb::Net *const net = hldb::findByName<hldb::Net>(name, mod->getNets());
    const hldb::Variable *const var = hldb::findByName<hldb::Variable>(name, mod->getVariables());
    if (port->getDirection() == vpiInput) {
      // Sec 23.2.2.3: input with a data type but no net type -> net.
      ASSERT_NE(net, nullptr);
      EXPECT_EQ(net->getNetType(), vpiWire);
      EXPECT_EQ(var, nullptr);
    } else {
      // Sec 23.2.2.3: 'output logic' -> variable.
      EXPECT_NE(var, nullptr);
      EXPECT_EQ(net, nullptr);
    }
  }
}

TEST_F(ApbNodeTest, MatchAddressBlockComputesSelects) {
  const hldb::Always *const proc = alwaysComb(0);
  ASSERT_NE(proc, nullptr);
  const hldb::Begin *const block = any_cast<hldb::Begin>(proc->getStmt());
  ASSERT_NE(block, nullptr) << "always_comb body is the begin-end block directly (no event control)";
  EXPECT_EQ(selectBaseName(block->getName()), "match_address");
  ASSERT_NE(block->getStmts(), nullptr);
  ASSERT_EQ(block->getStmts()->size(), 2u);

  // psel_o = '0;
  const hldb::Assignment *const clear = any_cast<hldb::Assignment>(block->getStmts()->at(0));
  ASSERT_NE(clear, nullptr);
  EXPECT_TRUE(clear->getBlocking());
  ASSERT_NE(clear->getLhs(), nullptr);
  EXPECT_EQ(selectBaseName(clear->getLhs()->getName()), "psel_o");
  const hldb::Constant *const zero = any_cast<hldb::Constant>(clear->getRhs());
  ASSERT_NE(zero, nullptr);
  EXPECT_EQ(zero->getDecompile(), "'0");

  const hldb::ForStmt *const loop = any_cast<hldb::ForStmt>(block->getStmts()->at(1));
  ASSERT_NE(loop, nullptr);
  expectMasterLoopHeader(loop, {def()});

  // psel_o[i] = psel_i & (paddr_i >= START_ADDR_i[i]) && (paddr_i <= END_ADDR_i[i]);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(loop->getStmt());
  ASSERT_NE(assign, nullptr) << "single-statement loop body without begin-end";
  EXPECT_TRUE(assign->getBlocking());
  expectBitSelect(assign->getLhs(), "psel_o", "i");

  // Sec 11.3.2: '&' has higher precedence than '&&'.
  const hldb::Operation *const logAnd = asOp(assign->getRhs(), vpiLogAndOp, 2);
  ASSERT_NE(logAnd, nullptr) << "top operator must be &&";
  const hldb::Operation *const bitAnd = asOp(logAnd->getOperands()->at(0), vpiBitAndOp, 2);
  ASSERT_NE(bitAnd, nullptr) << "left operand of && must be psel_i & (...)";
  EXPECT_EQ(selectBaseName(bitAnd->getOperands()->at(0)->getName()), "psel_i");
  const hldb::Operation *const ge = asOp(bitAnd->getOperands()->at(1), vpiGeOp, 2);
  ASSERT_NE(ge, nullptr);
  EXPECT_EQ(selectBaseName(ge->getOperands()->at(0)->getName()), "paddr_i");
  expectBitSelect(ge->getOperands()->at(1), "START_ADDR_i", "i");
  const hldb::Operation *const le = asOp(logAnd->getOperands()->at(1), vpiLeOp, 2);
  ASSERT_NE(le, nullptr);
  EXPECT_EQ(selectBaseName(le->getOperands()->at(0)->getName()), "paddr_i");
  expectBitSelect(le->getOperands()->at(1), "END_ADDR_i", "i");
}

TEST_F(ApbNodeTest, MuxBlockDefaultsThenSelectedMaster) {
  const hldb::Always *const proc = alwaysComb(1);
  ASSERT_NE(proc, nullptr);
  const hldb::Begin *const block = any_cast<hldb::Begin>(proc->getStmt());
  ASSERT_NE(block, nullptr);
  EXPECT_TRUE(block->getName().empty()) << "unnamed begin-end";
  ASSERT_NE(block->getStmts(), nullptr);
  ASSERT_EQ(block->getStmts()->size(), 8u) << "7 default assignments + for-loop";

  // Default assignments: 5 x '0, then 2 x 1'b0, all blocking.
  const std::vector<std::string_view> defaults = {"penable_o", "pwrite_o", "paddr_o",  "pwdata_o",
                                                  "prdata_o",  "pready_o", "pslverr_o"};
  for (size_t i = 0; i < defaults.size(); ++i) {
    SCOPED_TRACE(std::string(defaults[i]));
    const hldb::Assignment *const assign = any_cast<hldb::Assignment>(block->getStmts()->at(i));
    ASSERT_NE(assign, nullptr);
    EXPECT_TRUE(assign->getBlocking());
    ASSERT_NE(assign->getLhs(), nullptr);
    EXPECT_EQ(assign->getLhs()->getAnyType(), hldb::AnyType::RefObj);
    EXPECT_EQ(selectBaseName(assign->getLhs()->getName()), defaults[i]);
    const hldb::Constant *const value = any_cast<hldb::Constant>(assign->getRhs());
    ASSERT_NE(value, nullptr);
    if (i < 5) {
      EXPECT_EQ(value->getDecompile(), "'0");
    } else {
      EXPECT_EQ(value->getConstType(), vpiBinaryConst);
      EXPECT_EQ(value->getSize(), 1);
      int64_t bit = -1;
      ASSERT_TRUE(parseConstant(value, bit));
      EXPECT_EQ(bit, 0);
    }
  }

  const hldb::ForStmt *const loop = any_cast<hldb::ForStmt>(block->getStmts()->at(7));
  ASSERT_NE(loop, nullptr);
  expectMasterLoopHeader(loop, {def()});

  const hldb::Begin *const loopBody = any_cast<hldb::Begin>(loop->getStmt());
  ASSERT_NE(loopBody, nullptr);
  ASSERT_NE(loopBody->getStmts(), nullptr);
  ASSERT_EQ(loopBody->getStmts()->size(), 1u);
  const hldb::Any *const ifAny = loopBody->getStmts()->front();
  ASSERT_EQ(ifAny->getAnyType(), hldb::AnyType::IfStmt) << "if without else";
  const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(ifAny);
  expectBitSelect(ifStmt->getCondition(), "psel_o", "i");

  const hldb::Begin *const thenBlock = any_cast<hldb::Begin>(ifStmt->getStmt());
  ASSERT_NE(thenBlock, nullptr);
  ASSERT_NE(thenBlock->getStmts(), nullptr);
  ASSERT_EQ(thenBlock->getStmts()->size(), 7u);
  const hldb::AnyCollection &stmts = *thenBlock->getStmts();
  for (const hldb::Any *const stmt : stmts) {
    const hldb::Assignment *const assign = any_cast<hldb::Assignment>(stmt);
    ASSERT_NE(assign, nullptr);
    EXPECT_TRUE(assign->getBlocking());
  }
  // master out - slave in: <out>[i] = <in>;
  const std::vector<std::pair<std::string_view, std::string_view>> masterOut = {
      {"penable_o", "penable_i"}, {"pwrite_o", "pwrite_i"}, {"paddr_o", "paddr_i"}, {"pwdata_o", "pwdata_i"}};
  for (size_t i = 0; i < masterOut.size(); ++i) {
    SCOPED_TRACE(std::string(masterOut[i].first));
    const hldb::Assignment *const assign = any_cast<hldb::Assignment>(stmts[i]);
    expectBitSelect(assign->getLhs(), masterOut[i].first, "i");
    ASSERT_NE(assign->getRhs(), nullptr);
    EXPECT_EQ(assign->getRhs()->getAnyType(), hldb::AnyType::RefObj);
    EXPECT_EQ(selectBaseName(assign->getRhs()->getName()), masterOut[i].second);
  }
  // master in - slave out: <out> = <in>[i];
  const std::vector<std::pair<std::string_view, std::string_view>> masterIn = {
      {"prdata_o", "prdata_i"}, {"pready_o", "pready_i"}, {"pslverr_o", "pslverr_i"}};
  for (size_t i = 0; i < masterIn.size(); ++i) {
    SCOPED_TRACE(std::string(masterIn[i].first));
    const hldb::Assignment *const assign = any_cast<hldb::Assignment>(stmts[masterOut.size() + i]);
    ASSERT_NE(assign->getLhs(), nullptr);
    EXPECT_EQ(assign->getLhs()->getAnyType(), hldb::AnyType::RefObj);
    EXPECT_EQ(selectBaseName(assign->getLhs()->getName()), masterIn[i].first);
    expectBitSelect(assign->getRhs(), masterIn[i].second, "i");
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
