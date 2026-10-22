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

// Source under test: tests/ariane/fpga/src/fan_ctrl.sv (copied verbatim
// below; it is the only file in the Compile() arguments).
//
// // Copyright 2018 ETH Zurich and University of Bologna.
// // Copyright and related rights are licensed under the Solderpad Hardware
// // License, Version 0.51 (the "License"); you may not use this file except in
// // compliance with the License.  You may obtain a copy of the License at
// // http://solderpad.org/licenses/SHL-0.51. Unless required by applicable law
// // or agreed to in writing, software, hardware and materials distributed under
// // this License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// // CONDITIONS OF ANY KIND, either express or implied. See the License for the
// // specific language governing permissions and limitations under the License.
//
// // Description: PWM Fan Control for Genesys II board
// // Author: Florian Zaruba, zarubaf@iis.ee.ethz.ch
//
// module fan_ctrl (
//     input  logic       clk_i,
//     input  logic       rst_ni,
//     input  logic [3:0] pwm_setting_i,
//     output logic       fan_pwm_o
// );
//     logic [3:0]  ms_clock_d, ms_clock_q;
//     logic [19:0] cycle_counter_d, cycle_counter_q;
//
//     // clock divider
//     always_comb begin
//         cycle_counter_d = cycle_counter_q;
//         ms_clock_d = ms_clock_q;
//
//         // divide clock by 499999
//         if (cycle_counter_q == 499_999) begin
//             cycle_counter_d = 0;
//             ms_clock_d = ms_clock_q + 1;
//         end else begin
//             cycle_counter_d = cycle_counter_q + 1;
//         end
//
//         if (ms_clock_q == 15) begin
//             ms_clock_d = 0;
//         end
//     end
//
//     // duty cycle
//     always_comb begin
//         if (ms_clock_q < pwm_setting_i) begin
//             fan_pwm_o = 1'b1;
//         end else begin
//             fan_pwm_o = 1'b0;
//         end
//     end
//
//     always_ff @(posedge clk_i or negedge rst_ni) begin
//         if (~rst_ni) begin
//             ms_clock_q      <= '0;
//             cycle_counter_q <= '0;
//         end else begin
//             ms_clock_q      <= ms_clock_d;
//             cycle_counter_q <= cycle_counter_d;
//         end
//     end
// endmodule

// IEEE 1800-2023 semantics exercised:
//   fan_ctrl has four ANSI ports (Sec 23.2.2.2). The three 'input logic'
//   ports have no net-type keyword and therefore default to nets of the
//   default net type wire (Sec 23.2.2.3); 'output logic fan_pwm_o' is
//   declared with an explicit data type, so it is a variable, which is also
//   what a procedural assignment from always_comb requires (Sec 6.5, 10.4).
//   'logic [3:0] ms_clock_d, ms_clock_q;' is a list declaration: one data
//   type, two variables (Sec 6.8); logic declared in a module body without a
//   net-type keyword is a variable, unsigned by default (Sec 6.11).
//   The literal 499_999 uses underscores, which are ignored in a number
//   (Sec 5.7.1), so its value is 499999.
//   Two always_comb procedures (Sec 9.2.2.2) use blocking assignments with
//   if/else (Sec 12.4); one always_ff procedure (Sec 9.2.2.4) has the event
//   list '@(posedge clk_i or negedge rst_ni)' (Sec 9.4.2, 'or' is an event
//   OR) and uses nonblocking assignments (Sec 10.4.2); the asynchronous reset
//   branch assigns the unbased unsized literal '0 (Sec 5.7.1: all bits 0).
// What we have to check:
//   - port names, order, directions, widths; inputs are nets, fan_pwm_o is a
//     variable
//   - the four list-declared variables, their widths, unsigned, not nets
//   - three processes: always_comb, always_comb, always_ff, in source order
//   - the clock divider always_comb: statements, operators, blocking
//     assignments, the 499_999 literal value 499999, the compare with 15
//   - the duty-cycle always_comb: '<' compare, 1'b1 / 1'b0 assignments
//   - the always_ff: posedge/negedge event OR, ~rst_ni condition,
//     nonblocking assignments, '0 resets
// Checked:
//   - PortList, ListDeclaredVariables, ProcessKinds,
//     ClockDividerAlwaysComb, UnderscoreLiteralValue, DutyCycleAlwaysComb,
//     AlwaysFFWithAsyncReset
// Not checked:
//   - HLC 786 builds no elaborated hierarchy (Design::getTopModules() is empty).
//     For a module nobody instantiates, its getAllModules() entry is the module
//     with default parameters, i.e. the IEEE top-level instance, so the top-level
//     checks fall back to that entry. Child instances and generate scopes have no
//     such entry; those tests are skipped with the reason in the skip message.
//   - the PWM output waveform / counter wrap-around -- runtime-only, needs
//     simulation
//   - the implicit sensitivity list of always_comb (Sec 9.2.2.2.1) -- not
//     represented as an object in the IEEE VPI model

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/if_else.h>
#include <hldb/if_stmt.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/sys_func_call.h>
#include <hldb/typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <optional>
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
// Parses a digit string in the given base. Returns nullopt for anything that is
// not a plain integer (for example digits containing x or z).
[[maybe_unused]] std::optional<int64_t> parseDigits(const std::string &digits, int base) {
  if (digits.empty()) return std::nullopt;
  const bool negative = (digits[0] == '-');
  const std::string body = negative ? digits.substr(1) : digits;
  if (body.empty()) return std::nullopt;
  char *end = nullptr;
  const unsigned long long value = std::strtoull(body.c_str(), &end, base);
  if ((end == nullptr) || (*end != '\0')) return std::nullopt;
  const int64_t result = static_cast<int64_t>(value);
  return negative ? -result : result;
}

// Parses a SystemVerilog integer literal as IEEE 1800-2023 Sec 5.7.1 defines
// it: underscores are ignored, an optional size, an optional s, a base letter
// and the digits. '0 (unbased unsized, Sec 5.7.1) yields 0.
[[maybe_unused]] std::optional<int64_t> parseLiteral(std::string_view text) {
  std::string clean;
  for (const char c : text) {
    if ((c != '_') && (std::isspace(static_cast<unsigned char>(c)) == 0)) clean += c;
  }
  if (clean.empty()) return std::nullopt;
  const size_t quote = clean.find('\'');
  if (quote == std::string::npos) return parseDigits(clean, 10);
  size_t pos = quote + 1;
  if ((pos < clean.size()) && ((clean[pos] == 's') || (clean[pos] == 'S'))) ++pos;
  if (pos >= clean.size()) return std::nullopt;
  const char baseChar = static_cast<char>(std::tolower(static_cast<unsigned char>(clean[pos])));
  int base = 0;
  if (baseChar == 'h') {
    base = 16;
  } else if (baseChar == 'd') {
    base = 10;
  } else if (baseChar == 'o') {
    base = 8;
  } else if (baseChar == 'b') {
    base = 2;
  } else {
    return (clean.substr(pos) == "0") ? std::optional<int64_t>(0) : std::nullopt;
  }
  return parseDigits(clean.substr(pos + 1), base);
}

// Integer value of a Constant. Accepts both the "KIND:digits" form of
// getValue() (e.g. "UINT:5", "HEX:ff") and falls back to the literal text of
// getDecompile().
[[maybe_unused]] std::optional<int64_t> constantValue(const hldb::Constant *constant) {
  if (constant == nullptr) return std::nullopt;
  const std::string_view value = constantValueText(constant);
  const size_t colon = value.find(':');
  if (colon != std::string_view::npos) {
    const std::string_view kind = value.substr(0, colon);
    std::string digits;
    for (const char c : value.substr(colon + 1)) {
      if (c != '_') digits += c;
    }
    int base = 0;
    if ((kind == "INT") || (kind == "UINT") || (kind == "DEC")) {
      base = 10;
    } else if (kind == "HEX") {
      base = 16;
    } else if (kind == "BIN") {
      base = 2;
    } else if (kind == "OCT") {
      base = 8;
    }
    if (base != 0) {
      if (const std::optional<int64_t> parsed = parseDigits(digits, base)) return parsed;
    }
  }
  return parseLiteral(constant->getDecompile());
}

// Evaluates a constant integer expression made of literals, parameter
// references (resolved through the ParamAssigns of 'scope'), the operators
// + - * / and unary -, and the constant system function $clog2 (Sec 20.8.1).
// This keeps the assertions independent of whether the model stores a folded
// constant or the original expression tree.
[[maybe_unused]] std::optional<int64_t> evalConst(const hldb::Any *any, const hldb::Scope *scope, int32_t depth = 0) {
  if ((any == nullptr) || (depth > 16)) return std::nullopt;
  switch (any->getAnyType()) {
    case hldb::AnyType::Constant: return constantValue(any_cast<hldb::Constant>(any));
    case hldb::AnyType::Operation: {
      const hldb::Operation *const op = any_cast<hldb::Operation>(any);
      if (op->getOperands() == nullptr) return std::nullopt;
      std::vector<int64_t> values;
      for (const hldb::Any *const operand : *op->getOperands()) {
        const std::optional<int64_t> v = evalConst(operand, scope, depth + 1);
        if (!v) return std::nullopt;
        values.emplace_back(*v);
      }
      if (values.size() == 1) {
        if (op->getOpType() == vpiMinusOp) return -values[0];
        if (op->getOpType() == vpiPlusOp) return values[0];
        return std::nullopt;
      }
      if (values.size() != 2) return std::nullopt;
      switch (op->getOpType()) {
        case vpiAddOp: return values[0] + values[1];
        case vpiSubOp: return values[0] - values[1];
        case vpiMultOp: return values[0] * values[1];
        case vpiDivOp:
          if (values[1] == 0) return std::nullopt;
          return values[0] / values[1];
        default: return std::nullopt;
      }
    }
    case hldb::AnyType::SysFuncCall: {
      const hldb::SysFuncCall *const call = any_cast<hldb::SysFuncCall>(any);
      if ((selectBaseName(call->getName()) != "$clog2") || (call->getArguments() == nullptr) ||
          call->getArguments()->empty()) {
        return std::nullopt;
      }
      const hldb::NamedArgument *const arg = call->getArguments()->front();
      const hldb::Any *const argExpr = (arg->getHighConn() != nullptr) ? arg->getHighConn() : arg->getLowConn();
      const std::optional<int64_t> v = evalConst(argExpr, scope, depth + 1);
      if (!v) return std::nullopt;
      int64_t result = 0;
      while ((int64_t{1} << result) < *v) ++result;
      return result;
    }
    case hldb::AnyType::RefObj: {
      const hldb::RefObj *const ref = any_cast<hldb::RefObj>(any);
      if ((ref->getActual() != nullptr) && (ref->getActual()->getAnyType() == hldb::AnyType::Constant)) {
        return evalConst(ref->getActual(), scope, depth + 1);
      }
      if (scope == nullptr) return std::nullopt;
      const hldb::ParamAssign *const pa =
          hldb::findByName<hldb::ParamAssign, hldb::ParamAssign>(ref->getName(), scope->getParamAssigns());
      return (pa == nullptr) ? std::nullopt : evalConst(pa->getRhs(), scope, depth + 1);
    }
    case hldb::AnyType::Parameter: {
      if (scope == nullptr) return std::nullopt;
      const hldb::ParamAssign *const pa =
          hldb::findByName<hldb::ParamAssign, hldb::ParamAssign>(any->getName(), scope->getParamAssigns());
      return (pa == nullptr) ? std::nullopt : evalConst(pa->getRhs(), scope, depth + 1);
    }
    default: return std::nullopt;
  }
}

// Value of parameter 'name' in 'scope' (from its ParamAssign), or nullopt.
[[maybe_unused]] std::optional<int64_t> paramValue(const hldb::Scope *scope, std::string_view name) {
  if (scope == nullptr) return std::nullopt;
  const hldb::ParamAssign *const pa =
      hldb::findByName<hldb::ParamAssign, hldb::ParamAssign>(name, scope->getParamAssigns());
  return (pa == nullptr) ? std::nullopt : evalConst(pa->getRhs(), scope);
}

// Packed dimensions [left, right] of a declaration type, outermost first.
// Handles both a logic typespec carrying several ranges and a packed array
// typespec wrapping an element typespec.
[[maybe_unused]] std::vector<std::pair<int64_t, int64_t>> packedDims(const hldb::RefTypespec *refTs,
                                                                     const hldb::Scope *scope) {
  std::vector<std::pair<int64_t, int64_t>> dims;
  const hldb::Typespec *ts = (refTs == nullptr) ? nullptr : refTs->getActual();
  while (ts != nullptr) {
    if (const hldb::LogicTypespec *const lts = any_cast<hldb::LogicTypespec>(ts)) {
      if (lts->getRanges() != nullptr) {
        for (const hldb::Range *const r : *lts->getRanges()) {
          dims.emplace_back(evalConst(r->getLeftExpr(), scope).value_or(-999),
                            evalConst(r->getRightExpr(), scope).value_or(-999));
        }
      }
      break;
    }
    if (const hldb::ArrayTypespec *const ats = any_cast<hldb::ArrayTypespec>(ts)) {
      if (const hldb::Range *const r = ats->getRange()) {
        dims.emplace_back(evalConst(r->getLeftExpr(), scope).value_or(-999),
                          evalConst(r->getRightExpr(), scope).value_or(-999));
      }
      ts = (ats->getElemTypespec() == nullptr) ? nullptr : ats->getElemTypespec()->getActual();
      continue;
    }
    break;
  }
  return dims;
}

// Total bit width implied by the packed dimensions (1 for a scalar).
[[maybe_unused]] int64_t packedWidth(const hldb::RefTypespec *refTs, const hldb::Scope *scope) {
  int64_t width = 1;
  for (const std::pair<int64_t, int64_t> &d : packedDims(refTs, scope)) {
    width *= ((d.first > d.second) ? (d.first - d.second) : (d.second - d.first)) + 1;
  }
  return width;
}
}  // namespace

namespace {
const hldb::Any *resolve(const hldb::Any *any) {
  if (const hldb::RefObj *const ref = any_cast<hldb::RefObj>(any)) {
    return ref->getActual();
  }
  return any;
}

// i-th statement of a begin-end block, or nullptr.
const hldb::Any *stmtAt(const hldb::Any *block, size_t index) {
  const hldb::Begin *const begin = any_cast<hldb::Begin>(block);
  if ((begin == nullptr) || (begin->getStmts() == nullptr) || (index >= begin->getStmts()->size())) return nullptr;
  return begin->getStmts()->at(index);
}

// Number of statements in a begin-end block (0 when 'block' is not one).
size_t stmtCount(const hldb::Any *block) {
  const hldb::Begin *const begin = any_cast<hldb::Begin>(block);
  return ((begin == nullptr) || (begin->getStmts() == nullptr)) ? 0 : begin->getStmts()->size();
}

// i-th operand of an operation, or nullptr.
const hldb::Any *operandAt(const hldb::Any *expr, size_t index) {
  const hldb::Operation *const op = any_cast<hldb::Operation>(expr);
  if ((op == nullptr) || (op->getOperands() == nullptr) || (index >= op->getOperands()->size())) return nullptr;
  return op->getOperands()->at(index);
}
}  // namespace

class FanCtrlTest : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-wd", "../../../tests/ariane", "-d", "ast", "-d", "db", "-nobuiltin", "fpga/src/fan_ctrl.sv"});
  }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *definition() {
    return hldb::findByDefName<hldb::Module>("fan_ctrl", m_design->getAllModules());
  }
  static const hldb::Module *top() {
    return (hldb::findByDefName<hldb::Module>("fan_ctrl", m_design->getTopModules()) != nullptr
                ? hldb::findByDefName<hldb::Module>("fan_ctrl", m_design->getTopModules())
                : hldb::findByDefName<hldb::Module>("fan_ctrl", m_design->getAllModules()));
  }
  // The n-th process of the elaborated top, as an always procedure.
  static const hldb::Always *alwaysAt(size_t index) {
    const hldb::Module *const inst = top();
    if ((inst == nullptr) || (inst->getProcesses() == nullptr) || (index >= inst->getProcesses()->size())) {
      return nullptr;
    }
    return any_cast<hldb::Always>(inst->getProcesses()->at(index));
  }

  // Checks 'stmt' is an assignment 'lhs = rhs' / 'lhs <= rhs' and returns it.
  static const hldb::Assignment *expectAssign(const hldb::Any *stmt, bool blocking, std::string_view lhs) {
    EXPECT_NE(stmt, nullptr) << lhs;
    if (stmt == nullptr) return nullptr;
    EXPECT_EQ(stmt->getAnyType(), hldb::AnyType::Assignment) << lhs;
    const hldb::Assignment *const assign = any_cast<hldb::Assignment>(stmt);
    if (assign == nullptr) return nullptr;
    EXPECT_EQ(assign->getBlocking(), blocking) << lhs;
    EXPECT_NE(assign->getLhs(), nullptr) << lhs;
    if (assign->getLhs() != nullptr) {
      EXPECT_EQ(selectBaseName(assign->getLhs()->getName()), lhs);
    }
    EXPECT_NE(assign->getRhs(), nullptr) << lhs;
    return assign;
  }
};

// Sec 23.2.2.2 / 23.2.2.3: four ANSI ports; inputs are wire nets, the
// output declared 'output logic' is a variable.
TEST_F(FanCtrlTest, PortList) {
  const hldb::Module *const def = definition();
  ASSERT_NE(def, nullptr);
  ASSERT_NE(def->getPorts(), nullptr);
  struct Row {
    const char *m_name;
    int32_t m_direction;
    int64_t m_width;
  };
  const std::vector<Row> expected = {
      {"clk_i", vpiInput, 1}, {"rst_ni", vpiInput, 1}, {"pwm_setting_i", vpiInput, 4}, {"fan_pwm_o", vpiOutput, 1}};
  ASSERT_EQ(def->getPorts()->size(), expected.size());
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(selectBaseName(def->getPorts()->at(i)->getName()), expected[i].m_name) << "port #" << i;
    EXPECT_EQ(def->getPorts()->at(i)->getDirection(), expected[i].m_direction) << expected[i].m_name;
  }

  const hldb::Module *const inst = top();
  ASSERT_NE(inst, nullptr);
  for (const Row &row : expected) {
    const hldb::Port *const port = hldb::findByName<hldb::Port>(row.m_name, inst->getPorts());
    ASSERT_NE(port, nullptr) << row.m_name;
    ASSERT_NE(port->getTypespec(), nullptr) << row.m_name;
    ASSERT_NE(port->getTypespec()->getActual(), nullptr) << row.m_name;
    EXPECT_EQ(port->getTypespec()->getActual()->getAnyType(), hldb::AnyType::LogicTypespec) << row.m_name;
    EXPECT_EQ(packedWidth(port->getTypespec(), inst), row.m_width) << row.m_name;
    ASSERT_NE(port->getLowConn(), nullptr) << row.m_name;
    const hldb::Any *const internal = resolve(port->getLowConn());
    ASSERT_NE(internal, nullptr) << row.m_name;
    if (row.m_direction == vpiInput) {
      EXPECT_EQ(internal->getAnyType(), hldb::AnyType::Net) << row.m_name;
      const hldb::Net *const net = hldb::findByName<hldb::Net>(row.m_name, inst->getNets());
      ASSERT_NE(net, nullptr) << row.m_name;
      EXPECT_EQ(net->getNetType(), vpiWire) << row.m_name;
    } else {
      EXPECT_EQ(internal->getAnyType(), hldb::AnyType::Variable) << row.m_name;
      EXPECT_NE(hldb::findByName<hldb::Variable>(row.m_name, inst->getVariables()), nullptr) << row.m_name;
      EXPECT_EQ(hldb::findByName<hldb::Net>(row.m_name, inst->getNets()), nullptr) << row.m_name;
    }
  }
}

// Sec 6.8 / 6.11: each list declaration declares two logic variables of the
// shared packed type; logic is unsigned unless declared signed.
TEST_F(FanCtrlTest, ListDeclaredVariables) {
  const hldb::Module *const inst = top();
  ASSERT_NE(inst, nullptr);
  const std::vector<std::pair<std::string_view, int64_t>> expected = {
      {"ms_clock_d", 4}, {"ms_clock_q", 4}, {"cycle_counter_d", 20}, {"cycle_counter_q", 20}};
  for (const std::pair<std::string_view, int64_t> &e : expected) {
    const hldb::Variable *const var = hldb::findByName<hldb::Variable>(e.first, inst->getVariables());
    ASSERT_NE(var, nullptr) << e.first;
    EXPECT_EQ(hldb::findByName<hldb::Net>(e.first, inst->getNets()), nullptr) << e.first;
    ASSERT_NE(var->getTypespec(), nullptr) << e.first;
    ASSERT_NE(var->getTypespec()->getActual(), nullptr) << e.first;
    ASSERT_EQ(var->getTypespec()->getActual()->getAnyType(), hldb::AnyType::LogicTypespec) << e.first;
    EXPECT_FALSE(var->getTypespec()->getActual<hldb::LogicTypespec>()->getSigned()) << e.first;
    const std::vector<std::pair<int64_t, int64_t>> dims = packedDims(var->getTypespec(), inst);
    ASSERT_EQ(dims.size(), 1U) << e.first;
    EXPECT_EQ(dims[0].first, e.second - 1) << e.first;
    EXPECT_EQ(dims[0].second, 0) << e.first;
  }
}

// Sec 9.2.2.2 / 9.2.2.4: two always_comb procedures then one always_ff, in
// source order.
TEST_F(FanCtrlTest, ProcessKinds) {
  const hldb::Module *const inst = top();
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getProcesses(), nullptr);
  ASSERT_EQ(inst->getProcesses()->size(), 3U);
  const std::vector<int32_t> expected = {vpiAlwaysComb, vpiAlwaysComb, vpiAlwaysFF};
  for (size_t i = 0; i < expected.size(); ++i) {
    const hldb::Always *const always = alwaysAt(i);
    ASSERT_NE(always, nullptr) << "process #" << i;
    EXPECT_EQ(always->getAlwaysType(), expected[i]) << "process #" << i;
  }
}

// Clock divider: two default assignments, then
//   if (cycle_counter_q == 499_999) {cycle_counter_d = 0; ms_clock_d = ms_clock_q + 1;}
//   else cycle_counter_d = cycle_counter_q + 1;
// then if (ms_clock_q == 15) ms_clock_d = 0; -- all blocking (Sec 10.4.1).
TEST_F(FanCtrlTest, ClockDividerAlwaysComb) {
  const hldb::Always *const always = alwaysAt(0);
  ASSERT_NE(always, nullptr);
  const hldb::Any *const body = always->getStmt();
  ASSERT_NE(body, nullptr);
  ASSERT_EQ(body->getAnyType(), hldb::AnyType::Begin);
  ASSERT_EQ(stmtCount(body), 4U);
  const hldb::Module *const inst = top();

  const hldb::Assignment *const a0 = expectAssign(stmtAt(body, 0), true, "cycle_counter_d");
  ASSERT_NE(a0, nullptr);
  EXPECT_EQ(selectBaseName(a0->getRhs()->getName()), "cycle_counter_q");
  const hldb::Assignment *const a1 = expectAssign(stmtAt(body, 1), true, "ms_clock_d");
  ASSERT_NE(a1, nullptr);
  EXPECT_EQ(selectBaseName(a1->getRhs()->getName()), "ms_clock_q");

  // if (cycle_counter_q == 499_999) ... else ...
  const hldb::Any *const s2 = stmtAt(body, 2);
  ASSERT_NE(s2, nullptr);
  ASSERT_EQ(s2->getAnyType(), hldb::AnyType::IfElse);
  const hldb::IfElse *const ifElse = any_cast<hldb::IfElse>(s2);
  ASSERT_NE(ifElse->getCondition(), nullptr);
  ASSERT_EQ(ifElse->getCondition()->getAnyType(), hldb::AnyType::Operation);
  EXPECT_EQ(ifElse->getCondition<hldb::Operation>()->getOpType(), vpiEqOp);
  ASSERT_NE(operandAt(ifElse->getCondition(), 0), nullptr);
  EXPECT_EQ(selectBaseName(operandAt(ifElse->getCondition(), 0)->getName()), "cycle_counter_q");
  EXPECT_EQ(evalConst(operandAt(ifElse->getCondition(), 1), inst), std::optional<int64_t>(499999));

  const hldb::Any *const thenBlock = ifElse->getStmt();
  ASSERT_NE(thenBlock, nullptr);
  ASSERT_EQ(stmtCount(thenBlock), 2U);
  const hldb::Assignment *const t0 = expectAssign(stmtAt(thenBlock, 0), true, "cycle_counter_d");
  ASSERT_NE(t0, nullptr);
  EXPECT_EQ(evalConst(t0->getRhs(), inst), std::optional<int64_t>(0));
  const hldb::Assignment *const t1 = expectAssign(stmtAt(thenBlock, 1), true, "ms_clock_d");
  ASSERT_NE(t1, nullptr);
  ASSERT_EQ(t1->getRhs()->getAnyType(), hldb::AnyType::Operation);
  EXPECT_EQ(t1->getRhs<hldb::Operation>()->getOpType(), vpiAddOp);
  ASSERT_NE(operandAt(t1->getRhs(), 0), nullptr);
  EXPECT_EQ(selectBaseName(operandAt(t1->getRhs(), 0)->getName()), "ms_clock_q");
  EXPECT_EQ(evalConst(operandAt(t1->getRhs(), 1), inst), std::optional<int64_t>(1));

  const hldb::Any *const elseBlock = ifElse->getElseStmt();
  ASSERT_NE(elseBlock, nullptr);
  ASSERT_EQ(stmtCount(elseBlock), 1U);
  const hldb::Assignment *const e0 = expectAssign(stmtAt(elseBlock, 0), true, "cycle_counter_d");
  ASSERT_NE(e0, nullptr);
  ASSERT_EQ(e0->getRhs()->getAnyType(), hldb::AnyType::Operation);
  EXPECT_EQ(e0->getRhs<hldb::Operation>()->getOpType(), vpiAddOp);
  ASSERT_NE(operandAt(e0->getRhs(), 0), nullptr);
  EXPECT_EQ(selectBaseName(operandAt(e0->getRhs(), 0)->getName()), "cycle_counter_q");
  EXPECT_EQ(evalConst(operandAt(e0->getRhs(), 1), inst), std::optional<int64_t>(1));

  // if (ms_clock_q == 15) ms_clock_d = 0;  -- no else branch
  const hldb::Any *const s3 = stmtAt(body, 3);
  ASSERT_NE(s3, nullptr);
  ASSERT_EQ(s3->getAnyType(), hldb::AnyType::IfStmt);
  const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(s3);
  ASSERT_NE(ifStmt->getCondition(), nullptr);
  ASSERT_EQ(ifStmt->getCondition()->getAnyType(), hldb::AnyType::Operation);
  EXPECT_EQ(ifStmt->getCondition<hldb::Operation>()->getOpType(), vpiEqOp);
  ASSERT_NE(operandAt(ifStmt->getCondition(), 0), nullptr);
  EXPECT_EQ(selectBaseName(operandAt(ifStmt->getCondition(), 0)->getName()), "ms_clock_q");
  EXPECT_EQ(evalConst(operandAt(ifStmt->getCondition(), 1), inst), std::optional<int64_t>(15));
  ASSERT_EQ(stmtCount(ifStmt->getStmt()), 1U);
  const hldb::Assignment *const r0 = expectAssign(stmtAt(ifStmt->getStmt(), 0), true, "ms_clock_d");
  ASSERT_NE(r0, nullptr);
  EXPECT_EQ(evalConst(r0->getRhs(), inst), std::optional<int64_t>(0));
}

// Sec 5.7.1: underscores in a number are ignored, so 499_999 is the
// unsized decimal constant 499999.
TEST_F(FanCtrlTest, UnderscoreLiteralValue) {
  const hldb::Always *const always = alwaysAt(0);
  ASSERT_NE(always, nullptr);
  const hldb::IfElse *const ifElse = any_cast<hldb::IfElse>(stmtAt(always->getStmt(), 2));
  ASSERT_NE(ifElse, nullptr);
  const hldb::Any *const literal = operandAt(ifElse->getCondition(), 1);
  ASSERT_NE(literal, nullptr);
  ASSERT_EQ(literal->getAnyType(), hldb::AnyType::Constant);
  EXPECT_EQ(constantValue(any_cast<hldb::Constant>(literal)), std::optional<int64_t>(499999));
}

// Duty cycle: if (ms_clock_q < pwm_setting_i) fan_pwm_o = 1'b1; else
// fan_pwm_o = 1'b0; -- fan_pwm_o is assigned procedurally, so it must be a
// variable (checked in PortList).
TEST_F(FanCtrlTest, DutyCycleAlwaysComb) {
  const hldb::Always *const always = alwaysAt(1);
  ASSERT_NE(always, nullptr);
  const hldb::Module *const inst = top();
  const hldb::Any *const body = always->getStmt();
  ASSERT_NE(body, nullptr);
  ASSERT_EQ(body->getAnyType(), hldb::AnyType::Begin);
  ASSERT_EQ(stmtCount(body), 1U);
  const hldb::Any *const s0 = stmtAt(body, 0);
  ASSERT_NE(s0, nullptr);
  ASSERT_EQ(s0->getAnyType(), hldb::AnyType::IfElse);
  const hldb::IfElse *const ifElse = any_cast<hldb::IfElse>(s0);
  ASSERT_NE(ifElse->getCondition(), nullptr);
  ASSERT_EQ(ifElse->getCondition()->getAnyType(), hldb::AnyType::Operation);
  EXPECT_EQ(ifElse->getCondition<hldb::Operation>()->getOpType(), vpiLtOp);
  ASSERT_NE(operandAt(ifElse->getCondition(), 0), nullptr);
  ASSERT_NE(operandAt(ifElse->getCondition(), 1), nullptr);
  EXPECT_EQ(selectBaseName(operandAt(ifElse->getCondition(), 0)->getName()), "ms_clock_q");
  EXPECT_EQ(selectBaseName(operandAt(ifElse->getCondition(), 1)->getName()), "pwm_setting_i");

  ASSERT_EQ(stmtCount(ifElse->getStmt()), 1U);
  const hldb::Assignment *const t0 = expectAssign(stmtAt(ifElse->getStmt(), 0), true, "fan_pwm_o");
  ASSERT_NE(t0, nullptr);
  ASSERT_EQ(t0->getRhs()->getAnyType(), hldb::AnyType::Constant);
  EXPECT_EQ(evalConst(t0->getRhs(), inst), std::optional<int64_t>(1));
  EXPECT_EQ(any_cast<hldb::Constant>(t0->getRhs())->getSize(), 1);

  ASSERT_EQ(stmtCount(ifElse->getElseStmt()), 1U);
  const hldb::Assignment *const e0 = expectAssign(stmtAt(ifElse->getElseStmt(), 0), true, "fan_pwm_o");
  ASSERT_NE(e0, nullptr);
  ASSERT_EQ(e0->getRhs()->getAnyType(), hldb::AnyType::Constant);
  EXPECT_EQ(evalConst(e0->getRhs(), inst), std::optional<int64_t>(0));
  EXPECT_EQ(any_cast<hldb::Constant>(e0->getRhs())->getSize(), 1);
}

// Sec 9.2.2.4 / 9.4.2 / 10.4.2: always_ff @(posedge clk_i or negedge rst_ni)
// with an active-low asynchronous reset (if (~rst_ni)) assigning '0, else
// registering the _d values; all nonblocking.
TEST_F(FanCtrlTest, AlwaysFFWithAsyncReset) {
  const hldb::Always *const always = alwaysAt(2);
  ASSERT_NE(always, nullptr);
  const hldb::Module *const inst = top();
  ASSERT_NE(always->getStmt(), nullptr);
  ASSERT_EQ(always->getStmt()->getAnyType(), hldb::AnyType::EventControl);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  EXPECT_FALSE(ec->getIsImplicitControl());

  ASSERT_NE(ec->getCondition(), nullptr);
  ASSERT_EQ(ec->getCondition()->getAnyType(), hldb::AnyType::Operation);
  const hldb::Operation *const orOp = ec->getCondition<hldb::Operation>();
  EXPECT_EQ(orOp->getOpType(), vpiEventOrOp);
  ASSERT_NE(orOp->getOperands(), nullptr);
  ASSERT_EQ(orOp->getOperands()->size(), 2U);
  const std::vector<std::pair<int32_t, std::string_view>> edges = {{vpiPosedgeOp, "clk_i"}, {vpiNegedgeOp, "rst_ni"}};
  for (size_t i = 0; i < edges.size(); ++i) {
    const hldb::Any *const edge = orOp->getOperands()->at(i);
    ASSERT_EQ(edge->getAnyType(), hldb::AnyType::Operation) << "edge #" << i;
    EXPECT_EQ(any_cast<hldb::Operation>(edge)->getOpType(), edges[i].first) << "edge #" << i;
    ASSERT_NE(operandAt(edge, 0), nullptr) << "edge #" << i;
    EXPECT_EQ(selectBaseName(operandAt(edge, 0)->getName()), edges[i].second) << "edge #" << i;
  }

  const hldb::Any *const body = ec->getStmt();
  ASSERT_NE(body, nullptr);
  ASSERT_EQ(body->getAnyType(), hldb::AnyType::Begin);
  ASSERT_EQ(stmtCount(body), 1U);
  const hldb::Any *const s0 = stmtAt(body, 0);
  ASSERT_NE(s0, nullptr);
  ASSERT_EQ(s0->getAnyType(), hldb::AnyType::IfElse);
  const hldb::IfElse *const ifElse = any_cast<hldb::IfElse>(s0);
  ASSERT_NE(ifElse->getCondition(), nullptr);
  ASSERT_EQ(ifElse->getCondition()->getAnyType(), hldb::AnyType::Operation);
  EXPECT_EQ(ifElse->getCondition<hldb::Operation>()->getOpType(), vpiBitNegOp);
  ASSERT_NE(operandAt(ifElse->getCondition(), 0), nullptr);
  EXPECT_EQ(selectBaseName(operandAt(ifElse->getCondition(), 0)->getName()), "rst_ni");

  // Reset branch: both registers get the unbased unsized literal '0.
  ASSERT_EQ(stmtCount(ifElse->getStmt()), 2U);
  const std::vector<std::string_view> regs = {"ms_clock_q", "cycle_counter_q"};
  for (size_t i = 0; i < regs.size(); ++i) {
    const hldb::Assignment *const a = expectAssign(stmtAt(ifElse->getStmt(), i), false, regs[i]);
    ASSERT_NE(a, nullptr) << regs[i];
    ASSERT_EQ(a->getRhs()->getAnyType(), hldb::AnyType::Constant) << regs[i];
    EXPECT_EQ(evalConst(a->getRhs(), inst), std::optional<int64_t>(0)) << regs[i];
  }

  // Else branch: q <= d.
  ASSERT_EQ(stmtCount(ifElse->getElseStmt()), 2U);
  const std::vector<std::pair<std::string_view, std::string_view>> regd = {{"ms_clock_q", "ms_clock_d"},
                                                                           {"cycle_counter_q", "cycle_counter_d"}};
  for (size_t i = 0; i < regd.size(); ++i) {
    const hldb::Assignment *const a = expectAssign(stmtAt(ifElse->getElseStmt(), i), false, regd[i].first);
    ASSERT_NE(a, nullptr) << regd[i].first;
    EXPECT_EQ(selectBaseName(a->getRhs()->getName()), regd[i].second) << regd[i].first;
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
