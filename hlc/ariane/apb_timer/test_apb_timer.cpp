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

// Source under test: tests/ariane/fpga/src/apb_timer/apb_timer.sv, compiled
// together with timer.sv (see the Compile() arguments; timer.sv itself is tested in
// hlc/ariane/timer).
//
// clang-format off
// ----- begin apb_timer.sv -----
// (non-ASCII smart quotes in the original license comment converted to ASCII)
// // Copyright 2015 ETH Zurich and University of Bologna.
// // Copyright and related rights are licensed under the Solderpad Hardware
// // License, Version 0.51 (the "License"); you may not use this file except in
// // compliance with the License.  You may obtain a copy of the License at
// // http://solderpad.org/licenses/SHL-0.51. Unless required by applicable law
// // or agreed to in writing, software, hardware and materials distributed under
// // this License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// // CONDITIONS OF ANY KIND, either express or implied. See the License for the
// // specific language governing permissions and limitations under the License.
//
// `define REGS_MAX_ADR             2'd2
//
// module apb_timer
// #(
//     parameter APB_ADDR_WIDTH = 12,  //APB slaves are 4KB by default
//     parameter TIMER_CNT = 2 // how many timers should be instantiated
// )
// (
//     input  logic                      HCLK,
//     input  logic                      HRESETn,
//     input  logic [APB_ADDR_WIDTH-1:0] PADDR,
//     input  logic               [31:0] PWDATA,
//     input  logic                      PWRITE,
//     input  logic                      PSEL,
//     input  logic                      PENABLE,
//     output logic               [31:0] PRDATA,
//     output logic                      PREADY,
//     output logic                      PSLVERR,
//
//     output logic [(TIMER_CNT * 2) - 1:0] irq_o // overflow and cmp interrupt
// );
//
//     logic [TIMER_CNT-1:0] psel_int, pready, pslverr;
//     logic [$clog2(TIMER_CNT) - 1:0] slave_address_int;
//     logic [TIMER_CNT-1:0] [31:0] prdata;
//
//     assign slave_address_int = PADDR[$clog2(TIMER_CNT)+ `REGS_MAX_ADR + 1:`REGS_MAX_ADR + 2];
//
//     always_comb
//     begin
//         psel_int = '0;
//         psel_int[slave_address_int] = PSEL;
//     end
//
//     // output mux
//     always_comb
//     begin
//
//         if (psel_int != '0)
//         begin
//             PRDATA = prdata[slave_address_int];
//             PREADY = pready[slave_address_int];
//             PSLVERR = pslverr[slave_address_int];
//         end
//         else
//         begin
//             PRDATA = '0;
//             PREADY = 1'b1;
//             PSLVERR = 1'b0;
//         end
//     end
//
//
//     genvar k;
//
//     generate
//     for(k = 0; k < TIMER_CNT; k++)
//     begin : TIMER_GEN
//       timer #(
//           .APB_ADDR_WIDTH ( APB_ADDR_WIDTH )
//       ) timer_i (
//           .HCLK       ( HCLK          ),
//           .HRESETn    ( HRESETn       ),
//
//           .PADDR      ( PADDR        ),
//           .PWDATA     ( PWDATA       ),
//           .PWRITE     ( PWRITE       ),
//           .PSEL       ( psel_int[k]  ),
//           .PENABLE    ( PENABLE      ),
//           .PRDATA     ( prdata[k]    ),
//           .PREADY     ( pready[k]    ),
//           .PSLVERR    ( pslverr[k]   ),
//
//           .irq_o      ( irq_o[2*k+1 : 2*k] )
//       );
//     end
// endgenerate
// endmodule
// ----- end apb_timer.sv -----
// clang-format on
//
// IEEE 1800-2023 semantics exercised:
//   apb_timer instantiates timer, so apb_timer is the only top. Its two
//   untyped parameters (Sec 6.20.2) keep their defaults APB_ADDR_WIDTH = 12
//   and TIMER_CNT = 2. Port and variable widths depend on them, including
//   [(TIMER_CNT * 2) - 1:0] = [3:0] and [$clog2(TIMER_CNT) - 1:0] = [0:0]
//   ($clog2 is a constant system function, Sec 20.8.1). 'input logic' ports
//   are nets, 'output logic' ports and body 'logic' are variables
//   (Sec 23.2.2.3, 6.5). The macro `REGS_MAX_ADR (2'd2, Sec 22.5.1) appears in
//   a part-select bound: PADDR[$clog2(2) + 2 + 1 : 2 + 2] = PADDR[4:4]
//   (Sec 11.5.1). Two always_comb procedures (Sec 9.2.2.2) do a default plus
//   a variable-index bit-select write, and an if/else output mux (Sec 12.4).
//   'genvar k;' followed by 'generate for (...) begin : TIMER_GEN' is a loop
//   generate construct inside a generate region (Sec 27.3, 27.4). It is
//   elaborated once per value of k (0 and 1), each block instance is named
//   TIMER_GEN[k] and holds its own timer_i instance; inside the block k acts
//   as a localparam (Sec 27.4). The timer instance uses a named parameter
//   value assignment (Sec 23.10.2) and named port connections
//   (Sec 23.3.2.2) whose actuals depend on k: psel_int[k], prdata[k],
//   irq_o[2*k+1 : 2*k].
//
// What we have to check:
//   - both modules defined; apb_timer is the only top; no missing-module
//     error for timer; the macro name is not an identifier
//   - parameters: overridable, defaults 12 and 2
//   - 11 ports: order, direction, widths (irq_o [3:0])
//   - inputs are nets, outputs are variables
//   - body variables and widths, incl. the $clog2-sized slave_address_int
//   - assign slave_address_int = PADDR[4:4]
//   - always_comb #1: psel_int = '0; psel_int[slave_address_int] = PSEL
//   - always_comb #2: if (psel_int != '0) begin 3 muxed reads end else
//     begin 3 defaults end
//   - definition: generate region containing the GenFor over k
//   - elaboration: 2 TIMER_GEN scopes, each with exactly one timer_i of
//     module timer, APB_ADDR_WIDTH overridden to 12, k-dependent actuals
//     (psel_int[k], prdata[k], irq_o[2k+1:2k]) and k covering {0, 1}
//   - elaborated timer_i port widths follow the overridden parameter
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
//   - genvar k as a declared object outside the loop -- a genvar is not a
//     run-time object and has no VPI variable representation required by
//     IEEE; its effect is checked through the elaborated scopes
//   - timer.sv internals -- covered by hlc/ariane/timer/test_timer.cpp
//   - mux / select behaviour -- runtime-only, needs simulation

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
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/gen_for.h>
#include <hldb/gen_region.h>
#include <hldb/gen_scope.h>
#include <hldb/gen_scope_array.h>
#include <hldb/if_else.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/named_argument.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/part_select.h>
#include <hldb/port.h>
#include <hldb/process_stmt.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/scope.h>
#include <hldb/select.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cstdint>
#include <deque>
#include <set>
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
// Explicit name -> value bindings (used for the genvar of a generate block).
using Bindings = std::vector<std::pair<std::string_view, int64_t>>;

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

// Evaluates a constant integer expression made of literals, names bound in
// 'bindings', parameter references (looked up in 'scopes', innermost first),
// + - * and $clog2. Works whether or not the compiler already folded it.
bool evalInt(const hldb::Any *expr, const ScopeChain &scopes, int64_t &value, const Bindings &bindings = {}) {
  if (expr == nullptr) return false;
  if (const hldb::Constant *const constant = any_cast<hldb::Constant>(expr)) {
    return parseConstant(constant, value);
  }
  if (const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr)) {
    for (const std::pair<std::string_view, int64_t> &binding : bindings) {
      if (binding.first == ref->getName()) {
        value = binding.second;
        return true;
      }
    }
    for (const hldb::Scope *const scope : scopes) {
      if (const hldb::ParamAssign *const pa =
              hldb::findByName<hldb::ParamAssign, hldb::ParamAssign>(ref->getName(), scope->getParamAssigns())) {
        return evalInt(pa->getRhs(), scopes, value, bindings);
      }
    }
    return false;
  }
  if (const hldb::Operation *const op = any_cast<hldb::Operation>(expr)) {
    const hldb::AnyCollection *const operands = op->getOperands();
    if ((operands == nullptr) || (operands->size() != 2)) return false;
    int64_t lhs = 0;
    int64_t rhs = 0;
    if (!evalInt(operands->at(0), scopes, lhs, bindings) || !evalInt(operands->at(1), scopes, rhs, bindings)) {
      return false;
    }
    switch (op->getOpType()) {
      case vpiAddOp: value = lhs + rhs; return true;
      case vpiSubOp: value = lhs - rhs; return true;
      case vpiMultOp: value = lhs * rhs; return true;
      default: return false;
    }
  }
  if (const hldb::SysFuncCall *const call = any_cast<hldb::SysFuncCall>(expr)) {
    const hldb::NamedArgumentCollection *const args = call->getArguments();
    if ((selectBaseName(call->getName()) != "$clog2") || (args == nullptr) || (args->size() != 1)) return false;
    const hldb::NamedArgument *const arg = args->front();
    const hldb::Any *const actual = (arg->getHighConn() != nullptr) ? arg->getHighConn() : arg->getLowConn();
    int64_t n = 0;
    if (!evalInt(actual, scopes, n, bindings)) return false;
    int64_t result = 0;
    while ((int64_t{1} << result) < n) ++result;
    value = result;
    return true;
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

// Statements of a begin-end block, or nullptr if 'stmt' is not a block.
const hldb::AnyCollection *blockStmts(const hldb::Any *stmt) {
  const hldb::Begin *const block = any_cast<hldb::Begin>(stmt);
  return (block == nullptr) ? nullptr : block->getStmts();
}

// Returns 'stmt' as a blocking Assignment, or nullptr.
const hldb::Assignment *asBlockingAssign(const hldb::Any *stmt) {
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(stmt);
  if ((assign == nullptr) || !assign->getBlocking()) return nullptr;
  return assign;
}

// Checks that 'expr' is base[index] where index is a simple identifier.
void expectVarSelect(const hldb::Any *expr, std::string_view base, std::string_view index) {
  const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(expr);
  ASSERT_NE(sel, nullptr) << "expected " << base << "[" << index << "]";
  EXPECT_EQ(baseName(sel), base);
  ASSERT_NE(sel->getIndex(), nullptr);
  EXPECT_EQ(selectBaseName(sel->getIndex()->getName()), index);
}

// Finds the first loop generate construct, looking inside generate regions;
// 'inRegion' tells whether it was found inside a generate region.
const hldb::GenFor *findGenFor(const hldb::AnyCollection *stmts, bool &inRegion) {
  if (stmts == nullptr) return nullptr;
  for (const hldb::Any *const stmt : *stmts) {
    if (const hldb::GenFor *const genFor = any_cast<hldb::GenFor>(stmt)) return genFor;
    if (const hldb::GenRegion *const region = any_cast<hldb::GenRegion>(stmt)) {
      bool nested = false;
      if (const hldb::GenFor *const genFor = findGenFor(region->getStmts(), nested)) {
        inRegion = true;
        return genFor;
      }
    }
  }
  return nullptr;
}

struct PortSpec {
  std::string_view m_name;
  int32_t m_direction;
  std::vector<Bounds> m_bounds;
};
}  // namespace

class ApbTimerTest : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-wd", "../../../tests/ariane/fpga/src/apb_timer", "-d", "ast", "-d", "db", "-nobuiltin",
                       "apb_timer.sv", "timer.sv"});
  }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Unelaborated module definition (structure of statements/expressions).
  static const hldb::Module *def() { return hldb::findByDefName<hldb::Module>("apb_timer", m_design->getAllModules()); }
  // Elaborated top instance (nets/variables/parameter values).
  static const hldb::Module *top() {
    return (hldb::findByName<hldb::Module>("apb_timer", m_design->getTopModules()) != nullptr
                ? hldb::findByName<hldb::Module>("apb_timer", m_design->getTopModules())
                : hldb::findByDefName<hldb::Module>("apb_timer", m_design->getAllModules()));
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

  // All elaborated generate block instances of TIMER_GEN under the top.
  static std::vector<const hldb::GenScope *> timerGenScopes() {
    std::vector<const hldb::GenScope *> scopes;
    const hldb::Module *const mod = top();
    if ((mod == nullptr) || (mod->getGenScopeArrays() == nullptr)) return scopes;
    for (const hldb::GenScopeArray *const array : *mod->getGenScopeArrays()) {
      if ((array->getName().substr(0, 9) != "TIMER_GEN") || (array->getGenScopes() == nullptr)) continue;
      for (const hldb::GenScope *const scope : *array->getGenScopes()) scopes.emplace_back(scope);
    }
    return scopes;
  }
};

TEST_F(ApbTimerTest, HierarchyAndMacroName) {
  ASSERT_NE(def(), nullptr);
  EXPECT_NE(hldb::findByDefName<hldb::Module>("timer", m_design->getAllModules()), nullptr);
  const hldb::Module *const mod = top();
  ASSERT_NE(mod, nullptr) << "apb_timer is instantiated by nobody, so it is the top";
  EXPECT_EQ(mod->getDefName(), "apb_timer");
  EXPECT_EQ(hldb::findByName<hldb::Module>("timer", m_design->getTopModules()), nullptr)
      << "timer is instantiated, so it is not a top";
  EXPECT_EQ(findError(ErrorDefinition::ELAB_NO_MODULE_DEFINITION, "timer"), nullptr);
  // Sec 22.5.1: the macro name is substituted, never an identifier.
  EXPECT_EQ(hldb::findByName<hldb::Parameter>("REGS_MAX_ADR", mod->getParameters()), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("REGS_MAX_ADR", mod->getVariables()), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("REGS_MAX_ADR", mod->getNets()), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "REGS_MAX_ADR"), nullptr);
}

TEST_F(ApbTimerTest, UntypedParametersKeepDefaults) {
  const hldb::Module *const mod = top();
  ASSERT_NE(mod, nullptr);
  for (const std::pair<std::string_view, int64_t> &item :
       std::vector<std::pair<std::string_view, int64_t>>{{"APB_ADDR_WIDTH", 12}, {"TIMER_CNT", 2}}) {
    SCOPED_TRACE(std::string(item.first));
    const hldb::Parameter *const param = hldb::findByName<hldb::Parameter>(item.first, mod->getParameters());
    ASSERT_NE(param, nullptr);
    EXPECT_FALSE(param->getLocalParam()) << "declared in the #( ) parameter port list (Sec 6.20.1)";
    const hldb::ParamAssign *const pa =
        hldb::findByName<hldb::ParamAssign, hldb::ParamAssign>(item.first, mod->getParamAssigns());
    ASSERT_NE(pa, nullptr);
    EXPECT_FALSE(pa->getOverridden());
    int64_t value = 0;
    ASSERT_TRUE(evalInt(pa->getRhs(), {mod}, value));
    EXPECT_EQ(value, item.second);
  }
}

TEST_F(ApbTimerTest, PortListOrderDirectionAndWidth) {
  const hldb::Module *const mod = top();
  ASSERT_NE(mod, nullptr);
  const std::vector<PortSpec> expected = {
      {"HCLK", vpiInput, {}},          {"HRESETn", vpiInput, {}},        {"PADDR", vpiInput, {{11, 0}}},
      {"PWDATA", vpiInput, {{31, 0}}}, {"PWRITE", vpiInput, {}},         {"PSEL", vpiInput, {}},
      {"PENABLE", vpiInput, {}},       {"PRDATA", vpiOutput, {{31, 0}}}, {"PREADY", vpiOutput, {}},
      {"PSLVERR", vpiOutput, {}},      {"irq_o", vpiOutput, {{3, 0}}},  // [(TIMER_CNT * 2) - 1:0]
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

TEST_F(ApbTimerTest, InputPortsAreNetsOutputPortsAreVariables) {
  const hldb::Module *const mod = top();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getPorts(), nullptr);
  for (const hldb::Port *const port : *mod->getPorts()) {
    const std::string_view name = port->getName();
    SCOPED_TRACE(std::string(name));
    const hldb::Net *const net = hldb::findByName<hldb::Net>(name, mod->getNets());
    const hldb::Variable *const var = hldb::findByName<hldb::Variable>(name, mod->getVariables());
    if (port->getDirection() == vpiInput) {
      ASSERT_NE(net, nullptr) << "Sec 23.2.2.3: 'input logic' -> net";
      EXPECT_EQ(net->getNetType(), vpiWire);
      EXPECT_EQ(var, nullptr);
    } else {
      EXPECT_NE(var, nullptr) << "Sec 23.2.2.3: 'output logic' -> variable";
      EXPECT_EQ(net, nullptr);
    }
  }
}

TEST_F(ApbTimerTest, BodyVariablesAndParameterDrivenWidths) {
  const hldb::Module *const mod = top();
  ASSERT_NE(mod, nullptr);
  const std::vector<std::pair<std::string_view, std::vector<Bounds>>> expected = {
      {"psel_int", {{1, 0}}},        {"pready", {{1, 0}}},
      {"pslverr", {{1, 0}}},         {"slave_address_int", {{0, 0}}},  // [$clog2(2) - 1:0]
      {"prdata", {{1, 0}, {31, 0}}},
  };
  for (const std::pair<std::string_view, std::vector<Bounds>> &item : expected) {
    SCOPED_TRACE(std::string(item.first));
    const hldb::Variable *const var = hldb::findByName<hldb::Variable>(item.first, mod->getVariables());
    ASSERT_NE(var, nullptr) << "module-body 'logic' is a variable (Sec 6.5)";
    EXPECT_EQ(hldb::findByName<hldb::Net>(item.first, mod->getNets()), nullptr);
    std::vector<Bounds> bounds;
    ASSERT_TRUE(logicBounds(var->getTypespec(), {mod}, bounds));
    EXPECT_EQ(bounds, item.second);
  }
}

TEST_F(ApbTimerTest, SlaveAddressPartSelectUsesMacroAndClog2) {
  const hldb::Module *const mod = def();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getContAssigns(), nullptr);
  ASSERT_EQ(mod->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = mod->getContAssigns()->front();
  EXPECT_FALSE(ca->getNetDeclAssign());
  ASSERT_NE(ca->getLhs(), nullptr);
  EXPECT_EQ(selectBaseName(ca->getLhs()->getName()), "slave_address_int");
  const hldb::PartSelect *const ps = any_cast<hldb::PartSelect>(ca->getRhs());
  ASSERT_NE(ps, nullptr);
  EXPECT_EQ(baseName(ps), "PADDR");
  ASSERT_NE(ps->getRange(), nullptr);
  int64_t left = 0;
  int64_t right = 0;
  ASSERT_TRUE(evalInt(ps->getRange()->getLeftExpr(), {mod}, left));
  ASSERT_TRUE(evalInt(ps->getRange()->getRightExpr(), {mod}, right));
  EXPECT_EQ(left, 4);   // $clog2(2) + 2'd2 + 1
  EXPECT_EQ(right, 4);  // 2'd2 + 2
}

TEST_F(ApbTimerTest, SelectDecodeAlwaysComb) {
  const hldb::Always *const proc = alwaysComb(0);
  ASSERT_NE(proc, nullptr);
  const hldb::AnyCollection *const stmts = blockStmts(proc->getStmt());
  ASSERT_NE(stmts, nullptr);
  ASSERT_EQ(stmts->size(), 2u);
  const hldb::Assignment *const clear = asBlockingAssign(stmts->at(0));
  ASSERT_NE(clear, nullptr);
  ASSERT_NE(clear->getLhs(), nullptr);
  EXPECT_EQ(selectBaseName(clear->getLhs()->getName()), "psel_int");
  const hldb::Constant *const zero = any_cast<hldb::Constant>(clear->getRhs());
  ASSERT_NE(zero, nullptr);
  EXPECT_EQ(zero->getDecompile(), "'0");
  // psel_int[slave_address_int] = PSEL;  (non-constant bit-select index)
  const hldb::Assignment *const set = asBlockingAssign(stmts->at(1));
  ASSERT_NE(set, nullptr);
  expectVarSelect(set->getLhs(), "psel_int", "slave_address_int");
  ASSERT_NE(set->getRhs(), nullptr);
  EXPECT_EQ(selectBaseName(set->getRhs()->getName()), "PSEL");
}

TEST_F(ApbTimerTest, OutputMuxAlwaysComb) {
  const hldb::Always *const proc = alwaysComb(1);
  ASSERT_NE(proc, nullptr);
  const hldb::AnyCollection *const stmts = blockStmts(proc->getStmt());
  ASSERT_NE(stmts, nullptr);
  ASSERT_EQ(stmts->size(), 1u);
  const hldb::IfElse *const mux = any_cast<hldb::IfElse>(stmts->front());
  ASSERT_NE(mux, nullptr);

  // psel_int != '0
  const hldb::Operation *const cond = any_cast<hldb::Operation>(mux->getCondition());
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiNeqOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  EXPECT_EQ(selectBaseName(cond->getOperands()->at(0)->getName()), "psel_int");
  const hldb::Constant *const zero = any_cast<hldb::Constant>(cond->getOperands()->at(1));
  ASSERT_NE(zero, nullptr);
  EXPECT_EQ(zero->getDecompile(), "'0");

  // then: PRDATA = prdata[slave_address_int]; PREADY = ...; PSLVERR = ...;
  const hldb::AnyCollection *const thenStmts = blockStmts(mux->getStmt());
  ASSERT_NE(thenStmts, nullptr);
  ASSERT_EQ(thenStmts->size(), 3u);
  const std::vector<std::pair<std::string_view, std::string_view>> reads = {
      {"PRDATA", "prdata"}, {"PREADY", "pready"}, {"PSLVERR", "pslverr"}};
  for (size_t i = 0; i < reads.size(); ++i) {
    SCOPED_TRACE(std::string(reads[i].first));
    const hldb::Assignment *const assign = asBlockingAssign(thenStmts->at(i));
    ASSERT_NE(assign, nullptr);
    ASSERT_NE(assign->getLhs(), nullptr);
    EXPECT_EQ(selectBaseName(assign->getLhs()->getName()), reads[i].first);
    expectVarSelect(assign->getRhs(), reads[i].second, "slave_address_int");
  }

  // else: PRDATA = '0; PREADY = 1'b1; PSLVERR = 1'b0;
  const hldb::AnyCollection *const elseStmts = blockStmts(mux->getElseStmt());
  ASSERT_NE(elseStmts, nullptr);
  ASSERT_EQ(elseStmts->size(), 3u);
  const hldb::Assignment *const rdata = asBlockingAssign(elseStmts->at(0));
  ASSERT_NE(rdata, nullptr);
  EXPECT_EQ(selectBaseName(rdata->getLhs()->getName()), "PRDATA");
  const hldb::Constant *const fill = any_cast<hldb::Constant>(rdata->getRhs());
  ASSERT_NE(fill, nullptr);
  EXPECT_EQ(fill->getDecompile(), "'0");
  const std::vector<std::pair<std::string_view, int64_t>> flags = {{"PREADY", 1}, {"PSLVERR", 0}};
  for (size_t i = 0; i < flags.size(); ++i) {
    SCOPED_TRACE(std::string(flags[i].first));
    const hldb::Assignment *const assign = asBlockingAssign(elseStmts->at(i + 1));
    ASSERT_NE(assign, nullptr);
    EXPECT_EQ(selectBaseName(assign->getLhs()->getName()), flags[i].first);
    const hldb::Constant *const bit = any_cast<hldb::Constant>(assign->getRhs());
    ASSERT_NE(bit, nullptr);
    EXPECT_EQ(bit->getSize(), 1);
    int64_t value = -1;
    ASSERT_TRUE(parseConstant(bit, value));
    EXPECT_EQ(value, flags[i].second);
  }
}

TEST_F(ApbTimerTest, LoopGenerateInsideGenerateRegion) {
  const hldb::Module *const mod = def();
  ASSERT_NE(mod, nullptr);
  bool inRegion = false;
  const hldb::GenFor *const genFor = findGenFor(mod->getGenStmts(), inRegion);
  ASSERT_NE(genFor, nullptr) << "for (k ...) begin : TIMER_GEN is a loop generate construct (Sec 27.4)";
  EXPECT_TRUE(inRegion) << "it is enclosed in generate ... endgenerate (Sec 27.3)";

  ASSERT_NE(genFor->getForInitStmts(), nullptr);
  ASSERT_EQ(genFor->getForInitStmts()->size(), 1u);
  const hldb::Assignment *const init = any_cast<hldb::Assignment>(genFor->getForInitStmts()->front());
  ASSERT_NE(init, nullptr);
  ASSERT_NE(init->getLhs(), nullptr);
  EXPECT_EQ(selectBaseName(init->getLhs()->getName()), "k");
  int64_t initValue = -1;
  ASSERT_TRUE(evalInt(init->getRhs(), {mod}, initValue));
  EXPECT_EQ(initValue, 0);

  const hldb::Operation *const cond = any_cast<hldb::Operation>(genFor->getCondition());
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiLtOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  EXPECT_EQ(selectBaseName(cond->getOperands()->at(0)->getName()), "k");
  EXPECT_EQ(selectBaseName(cond->getOperands()->at(1)->getName()), "TIMER_CNT");

  ASSERT_NE(genFor->getForIncStmts(), nullptr);
  ASSERT_EQ(genFor->getForIncStmts()->size(), 1u);
  const hldb::Operation *const inc = any_cast<hldb::Operation>(genFor->getForIncStmts()->front());
  ASSERT_NE(inc, nullptr);
  EXPECT_EQ(inc->getOpType(), vpiPostIncOp);
}

TEST_F(ApbTimerTest, ElaboratedTimerGenScopesHoldOneTimerEach) {
  GTEST_SKIP() << "HLC 786 builds no elaborated hierarchy: getTopModules() is empty and child instances / generate "
                  "scopes are not created. "
                  "Per IEEE 1800-2023 Sec 23.10 and 27 elaboration builds the instance tree and expands generate "
                  "blocks. Fix pending.";
  const hldb::Module *const mod = top();
  ASSERT_NE(mod, nullptr);
  const std::vector<const hldb::GenScope *> scopes = timerGenScopes();
  ASSERT_EQ(scopes.size(), 2u) << "TIMER_CNT = 2 -> TIMER_GEN[0], TIMER_GEN[1] (Sec 27.4)";

  std::set<int64_t> seenK;
  for (const hldb::GenScope *const scope : scopes) {
    ASSERT_NE(scope->getModules(), nullptr);
    ASSERT_EQ(scope->getModules()->size(), 1u);
    const hldb::Module *const timer = scope->getModules()->front();
    EXPECT_EQ(selectBaseName(timer->getName()), "timer_i");
    EXPECT_EQ(timer->getDefName(), "timer");
    const ScopeChain chain = {timer, scope, mod};

    // #(.APB_ADDR_WIDTH(APB_ADDR_WIDTH)) -> 12 inside the instance.
    const hldb::ParamAssign *const pa =
        hldb::findByName<hldb::ParamAssign, hldb::ParamAssign>("APB_ADDR_WIDTH", timer->getParamAssigns());
    ASSERT_NE(pa, nullptr);
    EXPECT_TRUE(pa->getOverridden()) << "value comes from the instance parameter assignment";
    int64_t width = 0;
    ASSERT_TRUE(evalInt(pa->getRhs(), chain, width));
    EXPECT_EQ(width, 12);

    // The genvar value of this block instance: k is a localparam inside the
    // block (Sec 27.4); fall back to the scope index if it was folded away.
    Bindings bindings;
    int64_t index = -1;
    if (evalInt(scope->getIndex(), chain, index)) bindings.emplace_back("k", index);

    // .PSEL(psel_int[k])
    const hldb::Port *const psel = hldb::findByName<hldb::Port>("PSEL", timer->getPorts());
    ASSERT_NE(psel, nullptr);
    const hldb::BitSelect *const pselActual = any_cast<hldb::BitSelect>(psel->getHighConn());
    ASSERT_NE(pselActual, nullptr);
    EXPECT_EQ(baseName(pselActual), "psel_int");
    int64_t k = -1;
    ASSERT_TRUE(evalInt(pselActual->getIndex(), chain, k, bindings));
    seenK.emplace(k);

    // .PRDATA(prdata[k])
    const hldb::Port *const prdata = hldb::findByName<hldb::Port>("PRDATA", timer->getPorts());
    ASSERT_NE(prdata, nullptr);
    const hldb::BitSelect *const prdataActual = any_cast<hldb::BitSelect>(prdata->getHighConn());
    ASSERT_NE(prdataActual, nullptr);
    EXPECT_EQ(baseName(prdataActual), "prdata");
    int64_t prdataIndex = -1;
    ASSERT_TRUE(evalInt(prdataActual->getIndex(), chain, prdataIndex, bindings));
    EXPECT_EQ(prdataIndex, k);

    // .irq_o(irq_o[2*k+1 : 2*k])
    const hldb::Port *const irq = hldb::findByName<hldb::Port>("irq_o", timer->getPorts());
    ASSERT_NE(irq, nullptr);
    const hldb::PartSelect *const irqActual = any_cast<hldb::PartSelect>(irq->getHighConn());
    ASSERT_NE(irqActual, nullptr);
    EXPECT_EQ(baseName(irqActual), "irq_o");
    ASSERT_NE(irqActual->getRange(), nullptr);
    int64_t left = 0;
    int64_t right = 0;
    ASSERT_TRUE(evalInt(irqActual->getRange()->getLeftExpr(), chain, left, bindings));
    ASSERT_TRUE(evalInt(irqActual->getRange()->getRightExpr(), chain, right, bindings));
    EXPECT_EQ(left, 2 * k + 1);
    EXPECT_EQ(right, 2 * k);

    // Port widths inside the instance follow the overridden parameter.
    const hldb::Port *const paddr = hldb::findByName<hldb::Port>("PADDR", timer->getPorts());
    ASSERT_NE(paddr, nullptr);
    std::vector<Bounds> bounds;
    ASSERT_TRUE(logicBounds(paddr->getTypespec(), chain, bounds));
    const std::vector<Bounds> expectedPaddr = {{11, 0}};
    EXPECT_EQ(bounds, expectedPaddr);
    ASSERT_TRUE(logicBounds(irq->getTypespec(), chain, bounds));
    const std::vector<Bounds> expectedIrq = {{1, 0}};
    EXPECT_EQ(bounds, expectedIrq);
  }
  const std::set<int64_t> expectedK = {0, 1};
  EXPECT_EQ(seenK, expectedK) << "one block instance per genvar value";
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
