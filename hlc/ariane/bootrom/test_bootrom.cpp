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

// Source under test: tests/ariane/bootrom/bootrom.sv (compiled alone, see
// the Compile() arguments). The 187 64-bit ROM words are abbreviated below.
//
// clang-format off
// ----- begin bootrom.sv -----
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
//     localparam int RomSize = 187;
//
//     const logic [RomSize-1:0][63:0] mem = {
//         64'h00687464_69772d6f,
//         64'h692d6765_72007466,
//         64'h6968732d_67657200,
//         64'h73747075_72726574,
//         64'h6e690064_65657073,
//         // ... (177 lines elided) ...
//         64'h00000000_00000000,
//         64'h00000000_00000000,
//         64'h00008402_07458593,
//         64'h00000597_f1402573,
//         64'h01f41413_0010041b
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
// ----- end bootrom.sv -----
// clang-format on
//
// IEEE 1800-2023 semantics exercised:
//   bootrom is the only module, so it is the elaborated top. Its ANSI port
//   list declares 'input logic' ports, which are nets of the default net type
//   (Sec 23.2.2.3), and an 'output logic' port, which is a variable (Sec 6.5,
//   23.2.2.3). 'localparam int RomSize = 187' is a local (non-overridable)
//   parameter of the signed 32-bit type int (Sec 6.20.4, 6.11). 'mem' is a
//   const variable (Sec 6.20.6) of a two-dimensional packed logic array
//   [RomSize-1:0][63:0] (Sec 7.4.1), initialized by a concatenation of 187
//   sized hex literals (Sec 11.4.12, 5.7.1). addr_q is sized with the
//   constant system function $clog2 (Sec 20.8.1): $clog2(187) = 8, so
//   addr_q is [7:0]. The always_ff (Sec 9.2.2.4) has a single posedge event
//   (Sec 9.4.2) and a one-armed if (Sec 12.4) doing a nonblocking assignment
//   (Sec 10.4.2) of the part-select addr_i[10:3] (Sec 11.5.1). The
//   continuous assignment (Sec 10.3) drives rdata_o through a conditional
//   operator (Sec 11.4.11) whose false arm is the unbased unsized literal
//   '0 (Sec 5.7.1).
//
// What we have to check:
//   - module exists and is the elaborated top
//   - port order, directions and packed widths
//   - input ports are nets, output port and body 'logic' are variables
//   - RomSize: localparam, type int (signed), value 187
//   - mem: const variable, packed [186:0][63:0], initializer is a 187-operand
//     concatenation of 64-bit hex literals (first/last word values)
//   - addr_q width comes from $clog2(RomSize) -> [7:0]
//   - always_ff: posedge clk_i, if (req_i) without else, nonblocking assign
//     addr_q <= addr_i[10:3]
//   - assign rdata_o = (addr_q < RomSize) ? mem[addr_q] : '0
//   - no binding errors on the declared identifiers
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
//   - every one of the 187 ROM words -- only first, last and the count/size of
//     all operands are checked; checking each literal adds no new corner
//   - read data values / X-suppression behaviour of '0 -- runtime-only,
//     needs simulation
//   - '0 width extension to 64 bits in context -- a self-determined vs
//     context-determined sizing result, not represented in the VPI object
//     model as a separate value

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
#include <hldb/event_control.h>
#include <hldb/if_stmt.h>
#include <hldb/int_typespec.h>
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
// references (looked up in 'scopes', innermost first), + - * and $clog2.
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
  if (const hldb::SysFuncCall *const call = any_cast<hldb::SysFuncCall>(expr)) {
    const hldb::NamedArgumentCollection *const args = call->getArguments();
    if ((selectBaseName(call->getName()) != "$clog2") || (args == nullptr) || (args->size() != 1)) return false;
    const hldb::NamedArgument *const arg = args->front();
    const hldb::Any *const actual = (arg->getHighConn() != nullptr) ? arg->getHighConn() : arg->getLowConn();
    int64_t n = 0;
    if (!evalInt(actual, scopes, n)) return false;
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

struct PortSpec {
  std::string_view m_name;
  int32_t m_direction;
  std::vector<Bounds> m_bounds;
};
}  // namespace

class BootromTest : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-wd", "../../../tests/ariane/bootrom", "-d", "ast", "-d", "db", "-nobuiltin", "bootrom.sv"});
  }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Unelaborated module definition (structure of statements/expressions).
  static const hldb::Module *def() { return hldb::findByDefName<hldb::Module>("bootrom", m_design->getAllModules()); }
  // Elaborated top instance (nets/variables/parameter values).
  static const hldb::Module *top() {
    return (hldb::findByName<hldb::Module>("bootrom", m_design->getTopModules()) != nullptr
                ? hldb::findByName<hldb::Module>("bootrom", m_design->getTopModules())
                : hldb::findByDefName<hldb::Module>("bootrom", m_design->getAllModules()));
  }
};

TEST_F(BootromTest, ModuleIsDefinedAndIsElaboratedTop) {
  ASSERT_NE(def(), nullptr);
  const hldb::Module *const mod = top();
  ASSERT_NE(mod, nullptr) << "bootrom is instantiated by nobody, so it must be a top instance";
  EXPECT_EQ(mod->getDefName(), "bootrom");
  // Every identifier used in the body is declared; none may fail to bind.
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "RomSize"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "mem"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "addr_q"), nullptr);
}

TEST_F(BootromTest, PortListOrderDirectionAndWidth) {
  const hldb::Module *const mod = top();
  ASSERT_NE(mod, nullptr);
  const std::vector<PortSpec> expected = {
      {"clk_i", vpiInput, {}},
      {"req_i", vpiInput, {}},
      {"addr_i", vpiInput, {{63, 0}}},
      {"rdata_o", vpiOutput, {{63, 0}}},
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

TEST_F(BootromTest, InputPortsAreNetsOutputPortIsVariable) {
  const hldb::Module *const mod = top();
  ASSERT_NE(mod, nullptr);
  // Sec 23.2.2.3: 'input logic x' (no net type keyword) -> net of default
  // net type (wire).
  for (const std::string_view name : {"clk_i", "req_i", "addr_i"}) {
    SCOPED_TRACE(std::string(name));
    const hldb::Net *const net = hldb::findByName<hldb::Net>(name, mod->getNets());
    ASSERT_NE(net, nullptr);
    EXPECT_EQ(net->getNetType(), vpiWire);
    EXPECT_FALSE(net->getImplicitDecl());
    EXPECT_EQ(hldb::findByName<hldb::Variable>(name, mod->getVariables()), nullptr);
  }
  // Sec 23.2.2.3: 'output logic x' -> variable.
  EXPECT_NE(hldb::findByName<hldb::Variable>("rdata_o", mod->getVariables()), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("rdata_o", mod->getNets()), nullptr);
}

TEST_F(BootromTest, RomSizeIsLocalIntParameter) {
  const hldb::Module *const mod = top();
  ASSERT_NE(mod, nullptr);
  const hldb::Parameter *const param = hldb::findByName<hldb::Parameter>("RomSize", mod->getParameters());
  ASSERT_NE(param, nullptr);
  EXPECT_TRUE(param->getLocalParam());  // Sec 6.20.4
  ASSERT_NE(param->getTypespec(), nullptr);
  const hldb::IntTypespec *const ts = param->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(ts, nullptr) << "localparam int -> int typespec";
  EXPECT_TRUE(ts->getSigned());  // Sec 6.11: int is signed by default
  const hldb::ParamAssign *const pa =
      hldb::findByName<hldb::ParamAssign, hldb::ParamAssign>("RomSize", mod->getParamAssigns());
  ASSERT_NE(pa, nullptr);
  EXPECT_FALSE(pa->getOverridden());
  int64_t value = 0;
  ASSERT_TRUE(evalInt(pa->getRhs(), {mod}, value));
  EXPECT_EQ(value, 187);
}

TEST_F(BootromTest, MemIsConstPackedTwoDimVariable) {
  const hldb::Module *const mod = top();
  ASSERT_NE(mod, nullptr);
  const hldb::Variable *const mem = hldb::findByName<hldb::Variable>("mem", mod->getVariables());
  ASSERT_NE(mem, nullptr) << "'const logic' declaration is a variable (Sec 6.20.6)";
  EXPECT_EQ(hldb::findByName<hldb::Net>("mem", mod->getNets()), nullptr);
  EXPECT_TRUE(mem->getConstantVariable());
  std::vector<Bounds> bounds;
  ASSERT_TRUE(logicBounds(mem->getTypespec(), {mod}, bounds));
  const std::vector<Bounds> expected = {{186, 0}, {63, 0}};  // [RomSize-1:0][63:0]
  EXPECT_EQ(bounds, expected);
}

TEST_F(BootromTest, MemInitializerIsConcatenationOf187HexWords) {
  const hldb::Module *const mod = def();
  ASSERT_NE(mod, nullptr);
  const hldb::Variable *const mem = hldb::findByName<hldb::Variable>("mem", mod->getVariables());
  ASSERT_NE(mem, nullptr);
  const hldb::Expr *const init = (mem->getExpr() != nullptr) ? mem->getExpr() : mem->getValue();
  ASSERT_NE(init, nullptr) << "const variable must carry its initializer";
  const hldb::Operation *const concat = any_cast<hldb::Operation>(init);
  ASSERT_NE(concat, nullptr);
  EXPECT_EQ(concat->getOpType(), vpiConcatOp);
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 187u);
  for (const hldb::Any *const operand : *concat->getOperands()) {
    const hldb::Constant *const word = any_cast<hldb::Constant>(operand);
    ASSERT_NE(word, nullptr);
    EXPECT_EQ(word->getConstType(), vpiHexConst);
    EXPECT_EQ(word->getSize(), 64);
  }
  int64_t first = 0;
  int64_t last = 0;
  ASSERT_TRUE(parseConstant(any_cast<hldb::Constant>(concat->getOperands()->front()), first));
  ASSERT_TRUE(parseConstant(any_cast<hldb::Constant>(concat->getOperands()->back()), last));
  EXPECT_EQ(first, INT64_C(0x0068746469772d6f));  // 64'h00687464_69772d6f
  EXPECT_EQ(last, INT64_C(0x01f414130010041b));   // 64'h01f41413_0010041b
}

TEST_F(BootromTest, AddrQWidthComesFromClog2OfRomSize) {
  const hldb::Module *const mod = top();
  ASSERT_NE(mod, nullptr);
  const hldb::Variable *const addrQ = hldb::findByName<hldb::Variable>("addr_q", mod->getVariables());
  ASSERT_NE(addrQ, nullptr) << "module-body 'logic' is a variable (Sec 6.5)";
  EXPECT_EQ(hldb::findByName<hldb::Net>("addr_q", mod->getNets()), nullptr);
  EXPECT_FALSE(addrQ->getConstantVariable());
  std::vector<Bounds> bounds;
  ASSERT_TRUE(logicBounds(addrQ->getTypespec(), {mod}, bounds));
  const std::vector<Bounds> expected = {{7, 0}};  // $clog2(187) - 1 = 7
  EXPECT_EQ(bounds, expected);
}

TEST_F(BootromTest, AlwaysFFLatchesWordAddressOnRequest) {
  const hldb::Module *const mod = def();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getProcesses(), nullptr);
  ASSERT_EQ(mod->getProcesses()->size(), 1u);
  const hldb::Always *const proc = any_cast<hldb::Always>(mod->getProcesses()->front());
  ASSERT_NE(proc, nullptr);
  EXPECT_EQ(proc->getAlwaysType(), vpiAlwaysFF);

  const hldb::EventControl *const ec = any_cast<hldb::EventControl>(proc->getStmt());
  ASSERT_NE(ec, nullptr);
  EXPECT_FALSE(ec->getIsImplicitControl());
  const hldb::Operation *const edge = any_cast<hldb::Operation>(ec->getCondition());
  ASSERT_NE(edge, nullptr);
  EXPECT_EQ(edge->getOpType(), vpiPosedgeOp);
  ASSERT_NE(edge->getOperands(), nullptr);
  ASSERT_EQ(edge->getOperands()->size(), 1u);
  EXPECT_EQ(selectBaseName(edge->getOperands()->front()->getName()), "clk_i");

  const hldb::Begin *const body = any_cast<hldb::Begin>(ec->getStmt());
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 1u);
  const hldb::Any *const ifAny = body->getStmts()->front();
  ASSERT_EQ(ifAny->getAnyType(), hldb::AnyType::IfStmt) << "if without else";
  const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(ifAny);
  ASSERT_NE(ifStmt->getCondition(), nullptr);
  EXPECT_EQ(ifStmt->getCondition()->getAnyType(), hldb::AnyType::RefObj);
  EXPECT_EQ(selectBaseName(ifStmt->getCondition()->getName()), "req_i");

  const hldb::Begin *const thenBlock = any_cast<hldb::Begin>(ifStmt->getStmt());
  ASSERT_NE(thenBlock, nullptr);
  ASSERT_NE(thenBlock->getStmts(), nullptr);
  ASSERT_EQ(thenBlock->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(thenBlock->getStmts()->front());
  ASSERT_NE(assign, nullptr);
  EXPECT_FALSE(assign->getBlocking()) << "'<=' is nonblocking (Sec 10.4.2)";
  ASSERT_NE(assign->getLhs(), nullptr);
  EXPECT_EQ(selectBaseName(assign->getLhs()->getName()), "addr_q");

  const hldb::PartSelect *const ps = any_cast<hldb::PartSelect>(assign->getRhs());
  ASSERT_NE(ps, nullptr) << "addr_i[$clog2(RomSize)-1+3:3] is a part-select";
  EXPECT_EQ(baseName(ps), "addr_i");
  ASSERT_NE(ps->getRange(), nullptr);
  int64_t left = 0;
  int64_t right = 0;
  ASSERT_TRUE(evalInt(ps->getRange()->getLeftExpr(), {mod}, left));
  ASSERT_TRUE(evalInt(ps->getRange()->getRightExpr(), {mod}, right));
  EXPECT_EQ(left, 10);  // 8 - 1 + 3
  EXPECT_EQ(right, 3);
}

TEST_F(BootromTest, ReadDataAssignIsBoundsCheckedConditional) {
  const hldb::Module *const mod = def();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getContAssigns(), nullptr);
  ASSERT_EQ(mod->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = mod->getContAssigns()->front();
  EXPECT_FALSE(ca->getNetDeclAssign());
  ASSERT_NE(ca->getLhs(), nullptr);
  EXPECT_EQ(selectBaseName(ca->getLhs()->getName()), "rdata_o");

  const hldb::Operation *const cond = any_cast<hldb::Operation>(ca->getRhs());
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiConditionOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 3u);

  // (addr_q < RomSize)
  const hldb::Operation *const lt = any_cast<hldb::Operation>(cond->getOperands()->at(0));
  ASSERT_NE(lt, nullptr);
  EXPECT_EQ(lt->getOpType(), vpiLtOp);
  ASSERT_NE(lt->getOperands(), nullptr);
  ASSERT_EQ(lt->getOperands()->size(), 2u);
  EXPECT_EQ(selectBaseName(lt->getOperands()->at(0)->getName()), "addr_q");
  int64_t bound = 0;
  ASSERT_TRUE(evalInt(lt->getOperands()->at(1), {mod}, bound));
  EXPECT_EQ(bound, 187);

  // mem[addr_q]
  const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(cond->getOperands()->at(1));
  ASSERT_NE(sel, nullptr);
  EXPECT_EQ(baseName(sel), "mem");
  ASSERT_NE(sel->getIndex(), nullptr);
  EXPECT_EQ(selectBaseName(sel->getIndex()->getName()), "addr_q");

  // '0
  const hldb::Constant *const fill = any_cast<hldb::Constant>(cond->getOperands()->at(2));
  ASSERT_NE(fill, nullptr);
  EXPECT_EQ(fill->getDecompile(), "'0");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
