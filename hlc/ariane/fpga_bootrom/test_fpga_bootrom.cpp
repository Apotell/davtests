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

// Source under test: tests/ariane/fpga/src/bootrom/bootrom.sv (copied below;
// it is the only file in the Compile() arguments). This is the FPGA bootrom
// (884 64-bit words), a different file from the one tested in
// hlc/ariane/bootrom, although the module is also named 'bootrom'. The 884
// data words are elided except for the first and last five.
//
// /* Copyright 2018 ETH Zurich and University of Bologna.
//  * Copyright and related rights are licensed under the Solderpad Hardware
//  * License, Version 0.51 (the "License"); you may not use this file except in
//  * compliance with the License.  You may obtain a copy of the License at
//  * http://solderpad.org/licenses/SHL-0.51. Unless required by applicable law
//  * or agreed to in writing, software, hardware and materials distributed under
//  * this License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
//  * CONDITIONS OF ANY KIND, either express or implied. See the License for the
//  * specific language governing permissions and limitations under the License.
//  *
//  * File: $filename.v
//  *
//  * Description: Auto-generated bootrom
//  */
//
// // Auto-generated code
// module bootrom (
//    input  logic         clk_i,
//    input  logic         req_i,
//    input  logic [63:0]  addr_i,
//    output logic [63:0]  rdata_o
// );
//     localparam int RomSize = 884;
//
//     const logic [RomSize-1:0][63:0] mem = {
//         64'h00000000_00000000,
//         64'h0a0d2165_6e6f6420,
//         64'h00000000_00206567,
//         64'h616d6920_746f6f62,
//         64'h20676e69_79706f63,
// ... (874 lines elided) ...
//         64'h00100913_020004b7,
//         64'h27f000ef_01a11113,
//         64'h0210011b_03249663,
//         64'hf1402973_00000493,
//         64'h30491073_00800913
//     };
//
//     logic [$clog2(RomSize)-1:0] addr_q;
//
//     always_ff @(posedge clk_i) begin
//         if (req_i) begin
//             addr_q <= addr_i[$clog2(RomSize)-1+3:3];
//         end
//     end
//
//     // this prevents spurious Xes from propagating into
//     // the speculative fetch stage of the core
//     assign rdata_o = (addr_q < RomSize) ? mem[addr_q] : '0;
// endmodule

// IEEE 1800-2023 semantics exercised:
//   bootrom has four ANSI ports (Sec 23.2.2.2): 'input logic' ports are nets
//   of the default net type wire, 'output logic rdata_o' is a variable
//   (Sec 23.2.2.3). 'localparam int RomSize = 884' is a local parameter of
//   type int (Sec 6.20.4). 'const logic [RomSize-1:0][63:0] mem = {...}' is a
//   const variable (Sec 6.20.6) whose type is a 2-D packed array (Sec 7.4.1)
//   [883:0][63:0], initialised by a concatenation (Sec 11.4.12) of 884 sized
//   hex literals 64'h..., whose underscores are ignored (Sec 5.7.1).
//   addr_q is declared with the width $clog2(RomSize) = $clog2(884) = 10
//   (constant system function, Sec 20.8.1), i.e. [9:0]. The always_ff
//   (Sec 9.2.2.4) registers the part-select addr_i[$clog2(RomSize)-1+3:3] =
//   addr_i[12:3] (Sec 11.5.1) with a nonblocking assignment when req_i is
//   set. A continuous assignment (Sec 10.3) drives rdata_o with the
//   conditional operator (Sec 11.4.11): (addr_q < RomSize) ? mem[addr_q] : '0,
//   where '0 is an unbased unsized literal with all bits 0 (Sec 5.7.1).
// What we have to check:
//   - ports: names, order, directions, widths, net vs variable
//   - RomSize: localparam, type int, value 884
//   - mem: const variable, packed dims [883:0][63:0], initializer is a
//     concatenation of 884 64-bit hex constants with the right first,
//     second and last values
//   - addr_q: width from $clog2(RomSize) -> [9:0]
//   - always_ff: posedge clk_i, if (req_i), nonblocking addr_q <= addr_i[12:3]
//   - continuous assign of rdata_o with ?:, < RomSize, mem[addr_q], '0
// Checked:
//   - PortList, LocalparamRomSize, ConstPackedMemDeclaration,
//     MemInitializerConcatenation, AddrQWidthFromClog2,
//     AlwaysFFRegistersAddress, ReadDataContinuousAssign
// Not checked:
//   - HLC 786 builds no elaborated hierarchy (Design::getTopModules() is empty).
//     For a module nobody instantiates, its getAllModules() entry is the module
//     with default parameters, i.e. the IEEE top-level instance, so the top-level
//     checks fall back to that entry. Child instances and generate scopes have no
//     such entry; those tests are skipped with the reason in the skip message.
//   - the values of the 881 words not explicitly checked (only count, size,
//     base and the first/second/last values) -- repeated data, no new
//     IEEE semantics
//   - the read data actually returned for a given address -- runtime-only,
//     needs simulation

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
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/if_stmt.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/part_select.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/select.h>
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

const hldb::Any *stmtAt(const hldb::Any *block, size_t index) {
  const hldb::Begin *const begin = any_cast<hldb::Begin>(block);
  if ((begin == nullptr) || (begin->getStmts() == nullptr) || (index >= begin->getStmts()->size())) return nullptr;
  return begin->getStmts()->at(index);
}

size_t stmtCount(const hldb::Any *block) {
  const hldb::Begin *const begin = any_cast<hldb::Begin>(block);
  return ((begin == nullptr) || (begin->getStmts() == nullptr)) ? 0 : begin->getStmts()->size();
}

const hldb::Any *operandAt(const hldb::Any *expr, size_t index) {
  const hldb::Operation *const op = any_cast<hldb::Operation>(expr);
  if ((op == nullptr) || (op->getOperands() == nullptr) || (index >= op->getOperands()->size())) return nullptr;
  return op->getOperands()->at(index);
}

// Declaration initializer of a variable (stored either as its expression or
// as its value).
const hldb::Any *initializerOf(const hldb::Variable *var) {
  if (var == nullptr) return nullptr;
  return (var->getExpr() != nullptr) ? static_cast<const hldb::Any *>(var->getExpr())
                                     : static_cast<const hldb::Any *>(var->getValue());
}
}  // namespace

class FpgaBootromTest : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__,
            {"-wd", "../../../tests/ariane", "-d", "ast", "-d", "db", "-nobuiltin", "fpga/src/bootrom/bootrom.sv"});
  }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *definition() {
    return hldb::findByDefName<hldb::Module>("bootrom", m_design->getAllModules());
  }
  static const hldb::Module *top() {
    return (hldb::findByDefName<hldb::Module>("bootrom", m_design->getTopModules()) != nullptr
                ? hldb::findByDefName<hldb::Module>("bootrom", m_design->getTopModules())
                : hldb::findByDefName<hldb::Module>("bootrom", m_design->getAllModules()));
  }
};

// Sec 23.2.2.2 / 23.2.2.3: four ANSI ports; inputs are wire nets, the
// 'output logic' port is a variable.
TEST_F(FpgaBootromTest, PortList) {
  const hldb::Module *const def = definition();
  ASSERT_NE(def, nullptr);
  ASSERT_NE(def->getPorts(), nullptr);
  struct Row {
    const char *m_name;
    int32_t m_direction;
    int64_t m_width;
  };
  const std::vector<Row> expected = {
      {"clk_i", vpiInput, 1}, {"req_i", vpiInput, 1}, {"addr_i", vpiInput, 64}, {"rdata_o", vpiOutput, 64}};
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

// Sec 6.20.4: RomSize is a localparam of type int with value 884.
TEST_F(FpgaBootromTest, LocalparamRomSize) {
  const hldb::Module *const def = definition();
  ASSERT_NE(def, nullptr);
  const hldb::Parameter *const param = hldb::findByName<hldb::Parameter>("RomSize", def->getParameters());
  ASSERT_NE(param, nullptr);
  EXPECT_TRUE(param->getLocalParam());
  ASSERT_NE(param->getTypespec(), nullptr);
  ASSERT_NE(param->getTypespec()->getActual(), nullptr);
  EXPECT_EQ(param->getTypespec()->getActual()->getAnyType(), hldb::AnyType::IntTypespec);
  EXPECT_EQ(paramValue(def, "RomSize"), std::optional<int64_t>(884));

  const hldb::Module *const inst = top();
  ASSERT_NE(inst, nullptr);
  const hldb::ParamAssign *const pa =
      hldb::findByName<hldb::ParamAssign, hldb::ParamAssign>("RomSize", inst->getParamAssigns());
  ASSERT_NE(pa, nullptr);
  EXPECT_FALSE(pa->getOverridden());
  EXPECT_EQ(evalConst(pa->getRhs(), inst), std::optional<int64_t>(884));
}

// Sec 6.20.6 / 7.4.1: mem is a const variable of the 2-D packed type
// logic [RomSize-1:0][63:0] = [883:0][63:0].
TEST_F(FpgaBootromTest, ConstPackedMemDeclaration) {
  const hldb::Module *const inst = top();
  ASSERT_NE(inst, nullptr);
  const hldb::Variable *const mem = hldb::findByName<hldb::Variable>("mem", inst->getVariables());
  ASSERT_NE(mem, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("mem", inst->getNets()), nullptr);
  EXPECT_TRUE(mem->getConstantVariable());
  ASSERT_NE(mem->getTypespec(), nullptr);
  const std::vector<std::pair<int64_t, int64_t>> expected = {{883, 0}, {63, 0}};
  EXPECT_EQ(packedDims(mem->getTypespec(), inst), expected);

  // The definition, evaluated with its own RomSize, gives the same dims.
  const hldb::Module *const def = definition();
  ASSERT_NE(def, nullptr);
  const hldb::Variable *const defMem = hldb::findByName<hldb::Variable>("mem", def->getVariables());
  ASSERT_NE(defMem, nullptr);
  EXPECT_TRUE(defMem->getConstantVariable());
  EXPECT_EQ(packedDims(defMem->getTypespec(), def), expected);
}

// Sec 11.4.12 / 5.7.1: the initializer is a concatenation of 884 sized
// 64-bit hex literals; underscores in them are ignored.
TEST_F(FpgaBootromTest, MemInitializerConcatenation) {
  const hldb::Module *const inst = top();
  ASSERT_NE(inst, nullptr);
  const hldb::Variable *const mem = hldb::findByName<hldb::Variable>("mem", inst->getVariables());
  ASSERT_NE(mem, nullptr);
  const hldb::Any *const init = initializerOf(mem);
  ASSERT_NE(init, nullptr);
  ASSERT_EQ(init->getAnyType(), hldb::AnyType::Operation);
  const hldb::Operation *const concat = any_cast<hldb::Operation>(init);
  EXPECT_EQ(concat->getOpType(), vpiConcatOp);
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 884U);
  for (size_t i = 0; i < concat->getOperands()->size(); ++i) {
    const hldb::Any *const word = concat->getOperands()->at(i);
    ASSERT_EQ(word->getAnyType(), hldb::AnyType::Constant) << "word #" << i;
    const hldb::Constant *const c = any_cast<hldb::Constant>(word);
    EXPECT_EQ(c->getSize(), 64) << "word #" << i;
    EXPECT_EQ(c->getConstType(), vpiHexConst) << "word #" << i;
  }
  // 64'h00000000_00000000, 64'h0a0d2165_6e6f6420, ..., 64'h30491073_00800913
  EXPECT_EQ(constantValue(any_cast<hldb::Constant>(concat->getOperands()->at(0))), std::optional<int64_t>(0));
  EXPECT_EQ(constantValue(any_cast<hldb::Constant>(concat->getOperands()->at(1))),
            std::optional<int64_t>(INT64_C(0x0a0d21656e6f6420)));
  EXPECT_EQ(constantValue(any_cast<hldb::Constant>(concat->getOperands()->at(883))),
            std::optional<int64_t>(INT64_C(0x3049107300800913)));
}

// Sec 20.8.1: $clog2(884) = 10, so addr_q is logic [9:0]. The definition
// keeps the source expression $clog2(RomSize)-1.
TEST_F(FpgaBootromTest, AddrQWidthFromClog2) {
  const hldb::Module *const inst = top();
  ASSERT_NE(inst, nullptr);
  const hldb::Variable *const addrQ = hldb::findByName<hldb::Variable>("addr_q", inst->getVariables());
  ASSERT_NE(addrQ, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("addr_q", inst->getNets()), nullptr);
  const std::vector<std::pair<int64_t, int64_t>> expected = {{9, 0}};
  EXPECT_EQ(packedDims(addrQ->getTypespec(), inst), expected);

  const hldb::Module *const def = definition();
  ASSERT_NE(def, nullptr);
  const hldb::Variable *const defAddrQ = hldb::findByName<hldb::Variable>("addr_q", def->getVariables());
  ASSERT_NE(defAddrQ, nullptr);
  ASSERT_NE(defAddrQ->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lts = defAddrQ->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lts, nullptr);
  ASSERT_NE(lts->getRanges(), nullptr);
  ASSERT_EQ(lts->getRanges()->size(), 1U);
  const hldb::Any *const left = lts->getRanges()->front()->getLeftExpr();
  ASSERT_NE(left, nullptr);
  ASSERT_EQ(left->getAnyType(), hldb::AnyType::Operation);
  EXPECT_EQ(any_cast<hldb::Operation>(left)->getOpType(), vpiSubOp);
  const hldb::Any *const call = operandAt(left, 0);
  ASSERT_NE(call, nullptr);
  ASSERT_EQ(call->getAnyType(), hldb::AnyType::SysFuncCall);
  EXPECT_EQ(selectBaseName(any_cast<hldb::SysFuncCall>(call)->getName()), "$clog2");
  EXPECT_EQ(evalConst(call, def), std::optional<int64_t>(10));
  EXPECT_EQ(evalConst(left, def), std::optional<int64_t>(9));
}

// Sec 9.2.2.4 / 11.5.1: always_ff @(posedge clk_i) if (req_i)
// addr_q <= addr_i[$clog2(RomSize)-1+3:3], i.e. addr_i[12:3], nonblocking.
TEST_F(FpgaBootromTest, AlwaysFFRegistersAddress) {
  const hldb::Module *const inst = top();
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getProcesses(), nullptr);
  ASSERT_EQ(inst->getProcesses()->size(), 1U);
  const hldb::Always *const always = any_cast<hldb::Always>(inst->getProcesses()->front());
  ASSERT_NE(always, nullptr);
  EXPECT_EQ(always->getAlwaysType(), vpiAlwaysFF);

  ASSERT_NE(always->getStmt(), nullptr);
  ASSERT_EQ(always->getStmt()->getAnyType(), hldb::AnyType::EventControl);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec->getCondition(), nullptr);
  ASSERT_EQ(ec->getCondition()->getAnyType(), hldb::AnyType::Operation);
  EXPECT_EQ(ec->getCondition<hldb::Operation>()->getOpType(), vpiPosedgeOp);
  ASSERT_NE(operandAt(ec->getCondition(), 0), nullptr);
  EXPECT_EQ(selectBaseName(operandAt(ec->getCondition(), 0)->getName()), "clk_i");

  const hldb::Any *const body = ec->getStmt();
  ASSERT_EQ(stmtCount(body), 1U);
  const hldb::Any *const s0 = stmtAt(body, 0);
  ASSERT_NE(s0, nullptr);
  ASSERT_EQ(s0->getAnyType(), hldb::AnyType::IfStmt);
  const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(s0);
  ASSERT_NE(ifStmt->getCondition(), nullptr);
  EXPECT_EQ(selectBaseName(ifStmt->getCondition()->getName()), "req_i");

  ASSERT_EQ(stmtCount(ifStmt->getStmt()), 1U);
  const hldb::Any *const a0 = stmtAt(ifStmt->getStmt(), 0);
  ASSERT_NE(a0, nullptr);
  ASSERT_EQ(a0->getAnyType(), hldb::AnyType::Assignment);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(a0);
  EXPECT_FALSE(assign->getBlocking());
  ASSERT_NE(assign->getLhs(), nullptr);
  EXPECT_EQ(selectBaseName(assign->getLhs()->getName()), "addr_q");
  ASSERT_NE(assign->getRhs(), nullptr);
  ASSERT_EQ(assign->getRhs()->getAnyType(), hldb::AnyType::PartSelect);
  const hldb::PartSelect *const ps = assign->getRhs<hldb::PartSelect>();
  EXPECT_EQ(selectBaseName(ps->getName()), "addr_i");
  ASSERT_NE(ps->getRange(), nullptr);
  EXPECT_EQ(evalConst(ps->getRange()->getLeftExpr(), inst), std::optional<int64_t>(12));
  EXPECT_EQ(evalConst(ps->getRange()->getRightExpr(), inst), std::optional<int64_t>(3));
}

// Sec 10.3 / 11.4.11 / 5.7.1:
// assign rdata_o = (addr_q < RomSize) ? mem[addr_q] : '0;
TEST_F(FpgaBootromTest, ReadDataContinuousAssign) {
  const hldb::Module *const inst = top();
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getContAssigns(), nullptr);
  ASSERT_EQ(inst->getContAssigns()->size(), 1U);
  const hldb::ContAssign *const ca = inst->getContAssigns()->front();
  ASSERT_NE(ca->getLhs(), nullptr);
  EXPECT_EQ(selectBaseName(ca->getLhs()->getName()), "rdata_o");
  ASSERT_NE(ca->getRhs(), nullptr);
  ASSERT_EQ(ca->getRhs()->getAnyType(), hldb::AnyType::Operation);
  const hldb::Operation *const cond = ca->getRhs<hldb::Operation>();
  EXPECT_EQ(cond->getOpType(), vpiConditionOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 3U);

  const hldb::Any *const test = cond->getOperands()->at(0);
  ASSERT_EQ(test->getAnyType(), hldb::AnyType::Operation);
  EXPECT_EQ(any_cast<hldb::Operation>(test)->getOpType(), vpiLtOp);
  ASSERT_NE(operandAt(test, 0), nullptr);
  EXPECT_EQ(selectBaseName(operandAt(test, 0)->getName()), "addr_q");
  EXPECT_EQ(evalConst(operandAt(test, 1), inst), std::optional<int64_t>(884));

  const hldb::Any *const sel = cond->getOperands()->at(1);
  ASSERT_EQ(sel->getAnyType(), hldb::AnyType::BitSelect);
  const hldb::BitSelect *const bs = any_cast<hldb::BitSelect>(sel);
  EXPECT_EQ(selectBaseName(bs->getName()), "mem");
  ASSERT_NE(bs->getIndex(), nullptr);
  EXPECT_EQ(selectBaseName(bs->getIndex()->getName()), "addr_q");

  const hldb::Any *const zero = cond->getOperands()->at(2);
  ASSERT_EQ(zero->getAnyType(), hldb::AnyType::Constant);
  EXPECT_EQ(constantValue(any_cast<hldb::Constant>(zero)), std::optional<int64_t>(0));
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
