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

// Source under test: tests/ariane/fpga/src/apb_timer/timer.sv (compiled
// alone, see the Compile() arguments).
//
// clang-format off
// ----- begin timer.sv -----
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
// // define three registers per timer - timer, cmp and prescaler registers
// `define REGS_MAX_IDX             'd2
// `define REG_TIMER                 2'b00
// `define REG_TIMER_CTRL            2'b01
// `define REG_CMP                   2'b10
// `define PRESCALER_STARTBIT        'd3
// `define PRESCALER_STOPBIT         'd5
// `define ENABLE_BIT                'd0
//
// module timer
// #(
//     parameter APB_ADDR_WIDTH = 12  //APB slaves are 4KB by default
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
//     output logic                [1:0] irq_o // overflow and cmp interrupt
// );
//
//     // APB register interface
//     logic [`REGS_MAX_IDX-1:0]       register_adr;
//     assign register_adr = PADDR[`REGS_MAX_IDX + 2:2];
//     // APB logic: we are always ready to capture the data into our regs
//     // not supporting transfare failure
//     assign PREADY  = 1'b1;
//     assign PSLVERR = 1'b0;
//     // registers
//     logic [0:`REGS_MAX_IDX] [31:0]  regs_q, regs_n;
//     logic [31:0] cycle_counter_n, cycle_counter_q;
//
//     logic [2:0] prescaler_int;
//
//     //irq logic
//     always_comb
//     begin
//         irq_o = 2'b0;
//
//         // overlow irq
//         if (regs_q[`REG_TIMER] == 32'hffff_ffff)
//             irq_o[0] = 1'b1;
//
//         // compare match irq if compare reg ist set
//         if (regs_q[`REG_CMP] != 'b0 && regs_q[`REG_TIMER] == regs_q[`REG_CMP])
//             irq_o[1] = 1'b1;
//
//     end
//
//     assign prescaler_int = regs_q[`REG_TIMER_CTRL][`PRESCALER_STOPBIT:`PRESCALER_STARTBIT];
//     // register write logic
//     always_comb
//     begin
//         regs_n = regs_q;
//         cycle_counter_n = cycle_counter_q + 1;
//
//         // reset timer after cmp or overflow
//         if (irq_o[0] == 1'b1 || irq_o[1] == 1'b1)
//             regs_n[`REG_TIMER] = 1'b0;
//         else if(regs_q[`REG_TIMER_CTRL][`ENABLE_BIT] && prescaler_int != 'b0 && prescaler_int == cycle_counter_q) // prescaler
//         begin
//             regs_n[`REG_TIMER] = regs_q[`REG_TIMER] + 1; //prescaler mode
//         end
//         else if (regs_q[`REG_TIMER_CTRL][`ENABLE_BIT] && regs_q[`REG_TIMER_CTRL][`PRESCALER_STOPBIT:`PRESCALER_STARTBIT] == 'b0) // normal count mode
//             regs_n[`REG_TIMER] = regs_q[`REG_TIMER] + 1;
//
//         // reset prescaler cycle counter
//         if (cycle_counter_q >= regs_q[`REG_TIMER_CTRL])
//             cycle_counter_n = 32'b0;
//
//         // written from APB bus - gets priority
//         if (PSEL && PENABLE && PWRITE)
//         begin
//
//             case (register_adr)
//                 `REG_TIMER:
//                     regs_n[`REG_TIMER] = PWDATA;
//
//                 `REG_TIMER_CTRL:
//                     regs_n[`REG_TIMER_CTRL] = PWDATA;
//
//                 `REG_CMP:
//                 begin
//                     regs_n[`REG_CMP] = PWDATA;
//                     regs_n[`REG_TIMER] = 32'b0; // reset timer if compare register is written
//                 end
//             endcase
//         end
//     end
//
//     // APB register read logic
//     always_comb
//     begin
//         PRDATA = 'b0;
//
//         if (PSEL && PENABLE && !PWRITE)
//         begin
//
//             case (register_adr)
//                 `REG_TIMER:
//                     PRDATA = regs_q[`REG_TIMER];
//
//                 `REG_TIMER_CTRL:
//                     PRDATA = regs_q[`REG_TIMER_CTRL];
//
//                 `REG_CMP:
//                     PRDATA = regs_q[`REG_CMP];
//             endcase
//
//         end
//     end
//     // synchronouse part
//     always_ff @(posedge HCLK, negedge HRESETn)
//     begin
//         if(~HRESETn)
//         begin
//             regs_q          <= '{default: 32'b0};
//             cycle_counter_q <= 32'b0;
//         end
//         else
//         begin
//             regs_q          <= regs_n;
//             cycle_counter_q <= cycle_counter_n;
//         end
//     end
//
//
// endmodule
// ----- end timer.sv -----
// clang-format on
//
// IEEE 1800-2023 semantics exercised:
//   Seven text macros are defined with `define (Sec 22.5.1) and used both as
//   constants (case items, select indices) and inside range bounds. The
//   preprocessor substitutes the macro text, so the design only ever sees
//   the literals ('d2, 2'b00, 2'b01, 2'b10, 'd3, 'd5, 'd0); the macro names
//   are never identifiers. timer is the only module, hence the elaborated
//   top. 'parameter APB_ADDR_WIDTH = 12' has no type (Sec 6.20.2) and keeps
//   its default 12. 'input logic' ports are nets, 'output logic' ports are
//   variables (Sec 23.2.2.3). register_adr is [`REGS_MAX_IDX-1:0] = [1:0];
//   regs_q/regs_n are 2-D packed arrays with an ASCENDING outer range
//   [0:`REGS_MAX_IDX] = [0:2] and [31:0] inner range (Sec 7.4.1). Four
//   continuous assignments (Sec 10.3) include a part-select PADDR[4:2] and a
//   part-select of an element select regs_q[1][5:3] (Sec 7.4.5, 11.5.1).
//   Three always_comb procedures (Sec 9.2.2.2) use if / if-else-if chains
//   (Sec 12.4, 12.4.1), '&&'/'||' with relational/equality operands
//   (Sec 11.3.2 precedence, && and || are left-associative) and case
//   statements on register_adr with macro case items (Sec 12.5). The
//   always_ff (Sec 9.2.2.4) has the event list 'posedge HCLK, negedge
//   HRESETn' where ',' is the same as 'or' (Sec 9.4.2.1), tests ~HRESETn
//   (asynchronous active-low reset) and resets regs_q with the array
//   assignment pattern '{default: 32'b0} (Sec 10.9.1) using nonblocking
//   assignments (Sec 10.4.2).
//
// What we have to check:
//   - timer is the top; no macro name shows up as a parameter, net or
//     variable, and none fails to bind
//   - APB_ADDR_WIDTH: overridable (not localparam), value 12
//   - 11 ports: order, direction, widths ([11:0], [31:0], [1:0], scalars)
//   - inputs are nets, outputs are variables
//   - body variables and their widths, incl. the ascending [0:2] range
//   - the 4 continuous assignments with macro-substituted select bounds
//   - always_comb #1 (irq): defaults, two ifs, compare with 32'hffff_ffff,
//     '&&' tree
//   - always_comb #2 (register write): defaults, if / else if / else if
//     chain, prescaler conditions, counter reset, write case with 3 items
//     (last item is a begin-end with 2 assignments), no default item
//   - always_comb #3 (read): default, if with !PWRITE, read case
//   - always_ff: posedge HCLK or negedge HRESETn, if (~HRESETn) / else,
//     nonblocking assignments, '{default: ...} assignment pattern
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
//   - internal representation of the 'default:' key inside '{default: 32'b0}
//     -- IEEE fixes the meaning (every element gets 32'b0) but not a VPI
//     object shape for the key; only the assignment-pattern operator is
//     asserted
//   - truncation of PADDR[4:2] (3 bits) into the 2-bit register_adr -- a
//     value/width effect at assignment time, runtime-only
//   - counter / interrupt behaviour -- needs simulation
//   - the type that an untyped parameter takes from its value (Sec 6.20.2)
//     -- not stored as a separate VPI property

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
#include <hldb/case_item.h>
#include <hldb/case_stmt.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/if_else.h>
#include <hldb/if_stmt.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
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

// Statements of a begin-end block, or nullptr if 'stmt' is not a block.
const hldb::AnyCollection *blockStmts(const hldb::Any *stmt) {
  const hldb::Begin *const block = any_cast<hldb::Begin>(stmt);
  return (block == nullptr) ? nullptr : block->getStmts();
}

// Checks that 'expr' is an integer literal with the given value.
void expectConst(const hldb::Any *expr, int64_t expected) {
  ASSERT_NE(any_cast<hldb::Constant>(expr), nullptr) << "expected literal " << expected;
  int64_t value = -1;
  ASSERT_TRUE(evalInt(expr, {}, value));
  EXPECT_EQ(value, expected);
}

// Checks that 'expr' is a simple identifier reference.
void expectRef(const hldb::Any *expr, std::string_view name) {
  ASSERT_NE(expr, nullptr) << "expected " << name;
  EXPECT_EQ(expr->getAnyType(), hldb::AnyType::RefObj);
  EXPECT_EQ(selectBaseName(expr->getName()), name);
}

// Checks that 'expr' is base[index] with a literal index (macro already
// substituted by the preprocessor).
void expectConstSelect(const hldb::Any *expr, std::string_view base, int64_t index) {
  const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(expr);
  ASSERT_NE(sel, nullptr) << "expected " << base << "[" << index << "]";
  EXPECT_EQ(baseName(sel), base);
  expectConst(sel->getIndex(), index);
}

// Checks that 'expr' is regs_q[REG_TIMER_CTRL][ENABLE_BIT] = regs_q[1][0].
void expectEnableBit(const hldb::Any *expr) {
  const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(expr);
  ASSERT_NE(sel, nullptr) << "expected regs_q[1][0]";
  expectConst(sel->getIndex(), 0);
  expectConstSelect(sel->getPrefix(), "regs_q", 1);
}

// Checks that 'expr' is regs_q[REG_TIMER_CTRL][5:3].
void expectPrescalerField(const hldb::Any *expr) {
  const hldb::PartSelect *const ps = any_cast<hldb::PartSelect>(expr);
  ASSERT_NE(ps, nullptr) << "expected regs_q[1][5:3]";
  ASSERT_NE(ps->getRange(), nullptr);
  expectConst(ps->getRange()->getLeftExpr(), 5);
  expectConst(ps->getRange()->getRightExpr(), 3);
  expectConstSelect(ps->getPrefix(), "regs_q", 1);
}

// Returns 'stmt' as an Assignment of the given kind (blocking or not).
const hldb::Assignment *asAssign(const hldb::Any *stmt, bool blocking) {
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(stmt);
  if ((assign == nullptr) || (assign->getBlocking() != blocking)) return nullptr;
  return assign;
}

// Checks 'regs_n[<index>] = <rhsName>;' (blocking).
void expectRegsNWrite(const hldb::Any *stmt, int64_t index, std::string_view rhsName) {
  const hldb::Assignment *const assign = asAssign(stmt, true);
  ASSERT_NE(assign, nullptr);
  expectConstSelect(assign->getLhs(), "regs_n", index);
  expectRef(assign->getRhs(), rhsName);
}

// Checks 'regs_n[REG_TIMER] = regs_q[REG_TIMER] + 1;' (blocking).
void expectTimerIncrement(const hldb::Any *stmt) {
  const hldb::Assignment *const assign = asAssign(stmt, true);
  ASSERT_NE(assign, nullptr);
  expectConstSelect(assign->getLhs(), "regs_n", 0);
  const hldb::Operation *const add = asOp(assign->getRhs(), vpiAddOp, 2);
  ASSERT_NE(add, nullptr);
  expectConstSelect(add->getOperands()->at(0), "regs_q", 0);
  expectConst(add->getOperands()->at(1), 1);
}

// Checks 'PSEL && PENABLE && <third>' parsed left-associatively.
const hldb::Any *apbAccessThirdTerm(const hldb::Any *expr) {
  const hldb::Operation *const outer = asOp(expr, vpiLogAndOp, 2);
  if (outer == nullptr) return nullptr;
  const hldb::Operation *const inner = asOp(outer->getOperands()->at(0), vpiLogAndOp, 2);
  if (inner == nullptr) return nullptr;
  if ((selectBaseName(inner->getOperands()->at(0)->getName()) != "PSEL") ||
      (selectBaseName(inner->getOperands()->at(1)->getName()) != "PENABLE")) {
    return nullptr;
  }
  return outer->getOperands()->at(1);
}

struct PortSpec {
  std::string_view m_name;
  int32_t m_direction;
  std::vector<Bounds> m_bounds;
};
}  // namespace

class TimerTest : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__,
            {"-wd", "../../../tests/ariane/fpga/src/apb_timer", "-d", "ast", "-d", "db", "-nobuiltin", "timer.sv"});
  }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Unelaborated module definition (structure of statements/expressions).
  static const hldb::Module *def() { return hldb::findByDefName<hldb::Module>("timer", m_design->getAllModules()); }
  // Elaborated top instance (nets/variables/parameter values).
  static const hldb::Module *top() {
    return (hldb::findByName<hldb::Module>("timer", m_design->getTopModules()) != nullptr
                ? hldb::findByName<hldb::Module>("timer", m_design->getTopModules())
                : hldb::findByDefName<hldb::Module>("timer", m_design->getAllModules()));
  }

  // The n-th (0-based) always procedure of the given kind in the definition.
  static const hldb::Always *nthAlways(int32_t alwaysType, size_t n) {
    const hldb::Module *const mod = def();
    if ((mod == nullptr) || (mod->getProcesses() == nullptr)) return nullptr;
    size_t seen = 0;
    for (const hldb::Process *const proc : *mod->getProcesses()) {
      const hldb::Always *const always = any_cast<hldb::Always>(proc);
      if ((always == nullptr) || (always->getAlwaysType() != alwaysType)) continue;
      if (seen == n) return always;
      ++seen;
    }
    return nullptr;
  }
};

TEST_F(TimerTest, TopAndMacroNamesAreNotIdentifiers) {
  ASSERT_NE(def(), nullptr);
  const hldb::Module *const mod = top();
  ASSERT_NE(mod, nullptr) << "timer is instantiated by nobody, so it is the top";
  EXPECT_EQ(mod->getDefName(), "timer");
  // Sec 22.5.1: macro names are replaced by their text before parsing.
  for (const std::string_view name : {"REGS_MAX_IDX", "REG_TIMER", "REG_TIMER_CTRL", "REG_CMP", "PRESCALER_STARTBIT",
                                      "PRESCALER_STOPBIT", "ENABLE_BIT"}) {
    SCOPED_TRACE(std::string(name));
    EXPECT_EQ(hldb::findByName<hldb::Parameter>(name, mod->getParameters()), nullptr);
    EXPECT_EQ(hldb::findByName<hldb::Variable>(name, mod->getVariables()), nullptr);
    EXPECT_EQ(hldb::findByName<hldb::Net>(name, mod->getNets()), nullptr);
    EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, name), nullptr);
  }
}

TEST_F(TimerTest, UntypedParameterKeepsDefault) {
  const hldb::Module *const mod = top();
  ASSERT_NE(mod, nullptr);
  const hldb::Parameter *const param = hldb::findByName<hldb::Parameter>("APB_ADDR_WIDTH", mod->getParameters());
  ASSERT_NE(param, nullptr);
  EXPECT_FALSE(param->getLocalParam()) << "declared in the #( ) parameter port list (Sec 6.20.1)";
  const hldb::ParamAssign *const pa =
      hldb::findByName<hldb::ParamAssign, hldb::ParamAssign>("APB_ADDR_WIDTH", mod->getParamAssigns());
  ASSERT_NE(pa, nullptr);
  EXPECT_FALSE(pa->getOverridden());
  int64_t value = 0;
  ASSERT_TRUE(evalInt(pa->getRhs(), {mod}, value));
  EXPECT_EQ(value, 12);
}

TEST_F(TimerTest, PortListOrderDirectionAndWidth) {
  const hldb::Module *const mod = top();
  ASSERT_NE(mod, nullptr);
  const std::vector<PortSpec> expected = {
      {"HCLK", vpiInput, {}},          {"HRESETn", vpiInput, {}},        {"PADDR", vpiInput, {{11, 0}}},
      {"PWDATA", vpiInput, {{31, 0}}}, {"PWRITE", vpiInput, {}},         {"PSEL", vpiInput, {}},
      {"PENABLE", vpiInput, {}},       {"PRDATA", vpiOutput, {{31, 0}}}, {"PREADY", vpiOutput, {}},
      {"PSLVERR", vpiOutput, {}},      {"irq_o", vpiOutput, {{1, 0}}},
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

TEST_F(TimerTest, InputPortsAreNetsOutputPortsAreVariables) {
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

TEST_F(TimerTest, BodyVariablesAndMacroDrivenWidths) {
  const hldb::Module *const mod = top();
  ASSERT_NE(mod, nullptr);
  const std::vector<std::pair<std::string_view, std::vector<Bounds>>> expected = {
      {"register_adr", {{1, 0}}},     // [`REGS_MAX_IDX-1:0] = ['d2-1:0]
      {"regs_q", {{0, 2}, {31, 0}}},  // [0:`REGS_MAX_IDX][31:0], ascending outer
      {"regs_n", {{0, 2}, {31, 0}}},  // same declaration
      {"cycle_counter_n", {{31, 0}}}, {"cycle_counter_q", {{31, 0}}}, {"prescaler_int", {{2, 0}}},
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

TEST_F(TimerTest, ContinuousAssignmentsWithMacroBounds) {
  const hldb::Module *const mod = def();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getContAssigns(), nullptr);
  ASSERT_EQ(mod->getContAssigns()->size(), 4u);
  const hldb::ContAssignCollection &assigns = *mod->getContAssigns();
  for (const hldb::ContAssign *const ca : assigns) EXPECT_FALSE(ca->getNetDeclAssign());

  // assign register_adr = PADDR[`REGS_MAX_IDX + 2:2];  ->  PADDR[4:2]
  expectRef(assigns[0]->getLhs(), "register_adr");
  const hldb::PartSelect *const addr = any_cast<hldb::PartSelect>(assigns[0]->getRhs());
  ASSERT_NE(addr, nullptr);
  EXPECT_EQ(baseName(addr), "PADDR");
  ASSERT_NE(addr->getRange(), nullptr);
  int64_t left = 0;
  int64_t right = 0;
  ASSERT_TRUE(evalInt(addr->getRange()->getLeftExpr(), {mod}, left));
  ASSERT_TRUE(evalInt(addr->getRange()->getRightExpr(), {mod}, right));
  EXPECT_EQ(left, 4);
  EXPECT_EQ(right, 2);

  // assign PREADY = 1'b1;  assign PSLVERR = 1'b0;
  expectRef(assigns[1]->getLhs(), "PREADY");
  expectConst(assigns[1]->getRhs(), 1);
  expectRef(assigns[2]->getLhs(), "PSLVERR");
  expectConst(assigns[2]->getRhs(), 0);

  // assign prescaler_int = regs_q[`REG_TIMER_CTRL][`PRESCALER_STOPBIT:`PRESCALER_STARTBIT];
  expectRef(assigns[3]->getLhs(), "prescaler_int");
  expectPrescalerField(assigns[3]->getRhs());
}

TEST_F(TimerTest, IrqAlwaysComb) {
  const hldb::Always *const proc = nthAlways(vpiAlwaysComb, 0);
  ASSERT_NE(proc, nullptr);
  const hldb::AnyCollection *const stmts = blockStmts(proc->getStmt());
  ASSERT_NE(stmts, nullptr);
  ASSERT_EQ(stmts->size(), 3u);

  // irq_o = 2'b0;
  const hldb::Assignment *const clear = asAssign(stmts->at(0), true);
  ASSERT_NE(clear, nullptr);
  expectRef(clear->getLhs(), "irq_o");
  expectConst(clear->getRhs(), 0);

  // if (regs_q[`REG_TIMER] == 32'hffff_ffff) irq_o[0] = 1'b1;
  ASSERT_EQ(stmts->at(1)->getAnyType(), hldb::AnyType::IfStmt);
  const hldb::IfStmt *const overflow = any_cast<hldb::IfStmt>(stmts->at(1));
  const hldb::Operation *const eqMax = asOp(overflow->getCondition(), vpiEqOp, 2);
  ASSERT_NE(eqMax, nullptr);
  expectConstSelect(eqMax->getOperands()->at(0), "regs_q", 0);
  expectConst(eqMax->getOperands()->at(1), INT64_C(0xffffffff));
  const hldb::Assignment *const setOvf = asAssign(overflow->getStmt(), true);
  ASSERT_NE(setOvf, nullptr);
  expectConstSelect(setOvf->getLhs(), "irq_o", 0);
  expectConst(setOvf->getRhs(), 1);

  // if (regs_q[`REG_CMP] != 'b0 && regs_q[`REG_TIMER] == regs_q[`REG_CMP]) irq_o[1] = 1'b1;
  ASSERT_EQ(stmts->at(2)->getAnyType(), hldb::AnyType::IfStmt);
  const hldb::IfStmt *const match = any_cast<hldb::IfStmt>(stmts->at(2));
  // Sec 11.3.2: == and != bind tighter than &&.
  const hldb::Operation *const both = asOp(match->getCondition(), vpiLogAndOp, 2);
  ASSERT_NE(both, nullptr);
  const hldb::Operation *const neq = asOp(both->getOperands()->at(0), vpiNeqOp, 2);
  ASSERT_NE(neq, nullptr);
  expectConstSelect(neq->getOperands()->at(0), "regs_q", 2);
  expectConst(neq->getOperands()->at(1), 0);
  const hldb::Operation *const eq = asOp(both->getOperands()->at(1), vpiEqOp, 2);
  ASSERT_NE(eq, nullptr);
  expectConstSelect(eq->getOperands()->at(0), "regs_q", 0);
  expectConstSelect(eq->getOperands()->at(1), "regs_q", 2);
  const hldb::Assignment *const setCmp = asAssign(match->getStmt(), true);
  ASSERT_NE(setCmp, nullptr);
  expectConstSelect(setCmp->getLhs(), "irq_o", 1);
  expectConst(setCmp->getRhs(), 1);
}

TEST_F(TimerTest, RegisterWriteAlwaysComb) {
  const hldb::Always *const proc = nthAlways(vpiAlwaysComb, 1);
  ASSERT_NE(proc, nullptr);
  const hldb::AnyCollection *const stmts = blockStmts(proc->getStmt());
  ASSERT_NE(stmts, nullptr);
  ASSERT_EQ(stmts->size(), 5u);

  // regs_n = regs_q;
  const hldb::Assignment *const copy = asAssign(stmts->at(0), true);
  ASSERT_NE(copy, nullptr);
  expectRef(copy->getLhs(), "regs_n");
  expectRef(copy->getRhs(), "regs_q");

  // cycle_counter_n = cycle_counter_q + 1;
  const hldb::Assignment *const count = asAssign(stmts->at(1), true);
  ASSERT_NE(count, nullptr);
  expectRef(count->getLhs(), "cycle_counter_n");
  const hldb::Operation *const add = asOp(count->getRhs(), vpiAddOp, 2);
  ASSERT_NE(add, nullptr);
  expectRef(add->getOperands()->at(0), "cycle_counter_q");
  expectConst(add->getOperands()->at(1), 1);

  // if (irq_o[0] == 1'b1 || irq_o[1] == 1'b1) regs_n[0] = 1'b0;
  const hldb::IfElse *const first = any_cast<hldb::IfElse>(stmts->at(2));
  ASSERT_NE(first, nullptr) << "if ... else if ... chain";
  const hldb::Operation *const either = asOp(first->getCondition(), vpiLogOrOp, 2);
  ASSERT_NE(either, nullptr);
  for (size_t bit = 0; bit < 2; ++bit) {
    const hldb::Operation *const eq = asOp(either->getOperands()->at(bit), vpiEqOp, 2);
    ASSERT_NE(eq, nullptr);
    expectConstSelect(eq->getOperands()->at(0), "irq_o", static_cast<int64_t>(bit));
    expectConst(eq->getOperands()->at(1), 1);
  }
  const hldb::Assignment *const resetTimer = asAssign(first->getStmt(), true);
  ASSERT_NE(resetTimer, nullptr);
  expectConstSelect(resetTimer->getLhs(), "regs_n", 0);
  expectConst(resetTimer->getRhs(), 0);

  // else if (regs_q[1][0] && prescaler_int != 'b0 && prescaler_int == cycle_counter_q) begin ... end
  const hldb::IfElse *const second = any_cast<hldb::IfElse>(first->getElseStmt());
  ASSERT_NE(second, nullptr);
  const hldb::Operation *const outer = asOp(second->getCondition(), vpiLogAndOp, 2);
  ASSERT_NE(outer, nullptr);
  const hldb::Operation *const inner = asOp(outer->getOperands()->at(0), vpiLogAndOp, 2);
  ASSERT_NE(inner, nullptr) << "&& is left-associative";
  expectEnableBit(inner->getOperands()->at(0));
  const hldb::Operation *const nonZero = asOp(inner->getOperands()->at(1), vpiNeqOp, 2);
  ASSERT_NE(nonZero, nullptr);
  expectRef(nonZero->getOperands()->at(0), "prescaler_int");
  expectConst(nonZero->getOperands()->at(1), 0);
  const hldb::Operation *const hit = asOp(outer->getOperands()->at(1), vpiEqOp, 2);
  ASSERT_NE(hit, nullptr);
  expectRef(hit->getOperands()->at(0), "prescaler_int");
  expectRef(hit->getOperands()->at(1), "cycle_counter_q");
  const hldb::AnyCollection *const prescaled = blockStmts(second->getStmt());
  ASSERT_NE(prescaled, nullptr);
  ASSERT_EQ(prescaled->size(), 1u);
  expectTimerIncrement(prescaled->front());

  // else if (regs_q[1][0] && regs_q[1][5:3] == 'b0) regs_n[0] = regs_q[0] + 1;
  ASSERT_NE(second->getElseStmt(), nullptr);
  ASSERT_EQ(second->getElseStmt()->getAnyType(), hldb::AnyType::IfStmt) << "last else-if has no else";
  const hldb::IfStmt *const third = any_cast<hldb::IfStmt>(second->getElseStmt());
  const hldb::Operation *const normal = asOp(third->getCondition(), vpiLogAndOp, 2);
  ASSERT_NE(normal, nullptr);
  expectEnableBit(normal->getOperands()->at(0));
  const hldb::Operation *const noPrescale = asOp(normal->getOperands()->at(1), vpiEqOp, 2);
  ASSERT_NE(noPrescale, nullptr);
  expectPrescalerField(noPrescale->getOperands()->at(0));
  expectConst(noPrescale->getOperands()->at(1), 0);
  expectTimerIncrement(third->getStmt());

  // if (cycle_counter_q >= regs_q[`REG_TIMER_CTRL]) cycle_counter_n = 32'b0;
  ASSERT_EQ(stmts->at(3)->getAnyType(), hldb::AnyType::IfStmt);
  const hldb::IfStmt *const wrap = any_cast<hldb::IfStmt>(stmts->at(3));
  const hldb::Operation *const ge = asOp(wrap->getCondition(), vpiGeOp, 2);
  ASSERT_NE(ge, nullptr);
  expectRef(ge->getOperands()->at(0), "cycle_counter_q");
  expectConstSelect(ge->getOperands()->at(1), "regs_q", 1);
  const hldb::Assignment *const clearCount = asAssign(wrap->getStmt(), true);
  ASSERT_NE(clearCount, nullptr);
  expectRef(clearCount->getLhs(), "cycle_counter_n");
  expectConst(clearCount->getRhs(), 0);

  // if (PSEL && PENABLE && PWRITE) begin case (register_adr) ... endcase end
  ASSERT_EQ(stmts->at(4)->getAnyType(), hldb::AnyType::IfStmt);
  const hldb::IfStmt *const write = any_cast<hldb::IfStmt>(stmts->at(4));
  expectRef(apbAccessThirdTerm(write->getCondition()), "PWRITE");
  const hldb::AnyCollection *const writeBody = blockStmts(write->getStmt());
  ASSERT_NE(writeBody, nullptr);
  ASSERT_EQ(writeBody->size(), 1u);
  const hldb::CaseStmt *const sel = any_cast<hldb::CaseStmt>(writeBody->front());
  ASSERT_NE(sel, nullptr);
  EXPECT_EQ(sel->getCaseType(), vpiCaseExact) << "plain 'case' (Sec 12.5)";
  expectRef(sel->getCondition(), "register_adr");
  ASSERT_NE(sel->getCaseItems(), nullptr);
  ASSERT_EQ(sel->getCaseItems()->size(), 3u) << "no default item";
  for (size_t i = 0; i < 3; ++i) {
    const hldb::CaseItem *const item = sel->getCaseItems()->at(i);
    ASSERT_NE(item->getExprs(), nullptr);
    ASSERT_EQ(item->getExprs()->size(), 1u);
    expectConst(item->getExprs()->front(), static_cast<int64_t>(i));  // 2'b00, 2'b01, 2'b10
  }
  expectRegsNWrite(sel->getCaseItems()->at(0)->getStmt(), 0, "PWDATA");
  expectRegsNWrite(sel->getCaseItems()->at(1)->getStmt(), 1, "PWDATA");
  const hldb::AnyCollection *const cmpBody = blockStmts(sel->getCaseItems()->at(2)->getStmt());
  ASSERT_NE(cmpBody, nullptr);
  ASSERT_EQ(cmpBody->size(), 2u);
  expectRegsNWrite(cmpBody->at(0), 2, "PWDATA");
  const hldb::Assignment *const clearTimer = asAssign(cmpBody->at(1), true);
  ASSERT_NE(clearTimer, nullptr);
  expectConstSelect(clearTimer->getLhs(), "regs_n", 0);
  expectConst(clearTimer->getRhs(), 0);
}

TEST_F(TimerTest, ReadAlwaysComb) {
  const hldb::Always *const proc = nthAlways(vpiAlwaysComb, 2);
  ASSERT_NE(proc, nullptr);
  const hldb::AnyCollection *const stmts = blockStmts(proc->getStmt());
  ASSERT_NE(stmts, nullptr);
  ASSERT_EQ(stmts->size(), 2u);

  // PRDATA = 'b0;
  const hldb::Assignment *const clear = asAssign(stmts->at(0), true);
  ASSERT_NE(clear, nullptr);
  expectRef(clear->getLhs(), "PRDATA");
  expectConst(clear->getRhs(), 0);

  // if (PSEL && PENABLE && !PWRITE)
  ASSERT_EQ(stmts->at(1)->getAnyType(), hldb::AnyType::IfStmt);
  const hldb::IfStmt *const read = any_cast<hldb::IfStmt>(stmts->at(1));
  const hldb::Operation *const notWrite = asOp(apbAccessThirdTerm(read->getCondition()), vpiNotOp, 1);
  ASSERT_NE(notWrite, nullptr) << "PSEL && PENABLE && !PWRITE";
  expectRef(notWrite->getOperands()->front(), "PWRITE");

  const hldb::AnyCollection *const readBody = blockStmts(read->getStmt());
  ASSERT_NE(readBody, nullptr);
  ASSERT_EQ(readBody->size(), 1u);
  const hldb::CaseStmt *const sel = any_cast<hldb::CaseStmt>(readBody->front());
  ASSERT_NE(sel, nullptr);
  EXPECT_EQ(sel->getCaseType(), vpiCaseExact);
  expectRef(sel->getCondition(), "register_adr");
  ASSERT_NE(sel->getCaseItems(), nullptr);
  ASSERT_EQ(sel->getCaseItems()->size(), 3u);
  for (size_t i = 0; i < 3; ++i) {
    SCOPED_TRACE(i);
    const hldb::CaseItem *const item = sel->getCaseItems()->at(i);
    ASSERT_NE(item->getExprs(), nullptr);
    ASSERT_EQ(item->getExprs()->size(), 1u);
    expectConst(item->getExprs()->front(), static_cast<int64_t>(i));
    const hldb::Assignment *const assign = asAssign(item->getStmt(), true);
    ASSERT_NE(assign, nullptr);
    expectRef(assign->getLhs(), "PRDATA");
    expectConstSelect(assign->getRhs(), "regs_q", static_cast<int64_t>(i));
  }
}

TEST_F(TimerTest, AlwaysFFWithAsyncActiveLowReset) {
  const hldb::Always *const proc = nthAlways(vpiAlwaysFF, 0);
  ASSERT_NE(proc, nullptr);
  const hldb::EventControl *const ec = any_cast<hldb::EventControl>(proc->getStmt());
  ASSERT_NE(ec, nullptr);
  EXPECT_FALSE(ec->getIsImplicitControl());

  // @(posedge HCLK, negedge HRESETn): ',' is the event 'or' (Sec 9.4.2.1).
  const hldb::Operation *const events = asOp(ec->getCondition(), vpiEventOrOp, 2);
  ASSERT_NE(events, nullptr);
  const hldb::Operation *const pos = asOp(events->getOperands()->at(0), vpiPosedgeOp, 1);
  ASSERT_NE(pos, nullptr);
  expectRef(pos->getOperands()->front(), "HCLK");
  const hldb::Operation *const neg = asOp(events->getOperands()->at(1), vpiNegedgeOp, 1);
  ASSERT_NE(neg, nullptr);
  expectRef(neg->getOperands()->front(), "HRESETn");

  const hldb::AnyCollection *const body = blockStmts(ec->getStmt());
  ASSERT_NE(body, nullptr);
  ASSERT_EQ(body->size(), 1u);
  const hldb::IfElse *const ifElse = any_cast<hldb::IfElse>(body->front());
  ASSERT_NE(ifElse, nullptr);
  const hldb::Operation *const notReset = asOp(ifElse->getCondition(), vpiBitNegOp, 1);
  ASSERT_NE(notReset, nullptr) << "~HRESETn";
  expectRef(notReset->getOperands()->front(), "HRESETn");

  // Reset branch: regs_q <= '{default: 32'b0}; cycle_counter_q <= 32'b0;
  const hldb::AnyCollection *const resetStmts = blockStmts(ifElse->getStmt());
  ASSERT_NE(resetStmts, nullptr);
  ASSERT_EQ(resetStmts->size(), 2u);
  const hldb::Assignment *const resetRegs = asAssign(resetStmts->at(0), false);
  ASSERT_NE(resetRegs, nullptr) << "nonblocking";
  expectRef(resetRegs->getLhs(), "regs_q");
  const hldb::Operation *const pattern = any_cast<hldb::Operation>(resetRegs->getRhs());
  ASSERT_NE(pattern, nullptr);
  EXPECT_EQ(pattern->getOpType(), vpiAssignmentPatternOp);
  const hldb::Assignment *const resetCount = asAssign(resetStmts->at(1), false);
  ASSERT_NE(resetCount, nullptr);
  expectRef(resetCount->getLhs(), "cycle_counter_q");
  expectConst(resetCount->getRhs(), 0);

  // Else branch: regs_q <= regs_n; cycle_counter_q <= cycle_counter_n;
  const hldb::AnyCollection *const runStmts = blockStmts(ifElse->getElseStmt());
  ASSERT_NE(runStmts, nullptr);
  ASSERT_EQ(runStmts->size(), 2u);
  const hldb::Assignment *const nextRegs = asAssign(runStmts->at(0), false);
  ASSERT_NE(nextRegs, nullptr);
  expectRef(nextRegs->getLhs(), "regs_q");
  expectRef(nextRegs->getRhs(), "regs_n");
  const hldb::Assignment *const nextCount = asAssign(runStmts->at(1), false);
  ASSERT_NE(nextCount, nullptr);
  expectRef(nextCount->getLhs(), "cycle_counter_q");
  expectRef(nextCount->getRhs(), "cycle_counter_n");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
