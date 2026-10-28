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

// Validates the HLDB model built for tests/PartSelect4/dut.sv:
//
//   module tlul_fifo_sync #(parameter int unsigned ReqDepth) ();
//   endmodule
//
//   module tlul_socket_1n #(parameter int unsigned N, parameter bit [N*4-1:0] DReqDepth) ();
//     for (genvar i = 0 ; i < N ; i++) begin : gen_dfifo
//       tlul_fifo_sync #(.ReqDepth(DReqDepth[i*4+:4])) fifo_d();
//     end
//   endmodule
//
//   module xbar_main;
//     tlul_socket_1n #(.DReqDepth (68'h200000000000000), .N(8)) u_s1n_25();
//   endmodule
//
//
//   module top();
//
//    parameter A = 68'h200000000000000;
//    parameter B = A[8*4+:4];
//
//   endmodule // top
//
// The point of the fixture is the indexed part-select '+:' (11.5.1) applied
// to a parameter (6.20.2: "Bit-selects and part-selects of parameters that
// are of integral types shall be allowed"), once with a base that is a
// genvar expression inside a loop generate construct (27.4) and used as a
// parameter override, and once with a constant base on a parameter wider than
// 64 bits. The regression this file exists to catch is HLC losing the
// indexed part-select (its prefix, its '+:' direction, its base or its width)
// or binding the names inside it to the wrong declaration.
//
// Values: in u_s1n_25, N = 8, so DReqDepth is bit [31:0] and the 68-bit
// override 68'h200000000000000 (= 2**57) is truncated to its low 32 bits,
// which are all 0 (6.11.2: "Automatic type conversions from a larger number
// of bits to a smaller number of bits involve truncations of the most
// significant bits (MSBs)."). The loop runs for i = 0 to 7, so gen_dfifo has
// 8 instances, and every fifo_d receives ReqDepth = 0. In top, A is the
// 68-bit value 2**57 and B = A[8*4+:4] = A[35:32] = 4'h0 (11.5.1: "The
// number of bits selected is equal to the width expression.").
//
// What is checked, and why:
//   Module tlul_fifo_sync
//     - '()' is a null port (37.14 detail 10, "module M();"): exactly 1
//       port, with no name, no low connection and port index 0
//     - exactly 1 parameter and 1 param_assignment, ReqDepth, a nonlocal
//       parameter port (6.20.4) of type int unsigned (6.11.3), with no
//       default value (6.20.1)
//   Module tlul_socket_1n
//     - '()' is one null port, as above
//     - exactly 2 parameters and 2 param_assignments, N and DReqDepth, in
//       that order, both nonlocal parameter ports (6.20.4) with no default
//       value (6.20.1)
//     - N is int unsigned; DReqDepth is bit [N*4-1:0], unsigned (6.11.3),
//       whose left bound is Operation vpiSubOp over (Operation vpiMultOp
//       over N and 4) and 1, with N bound to the module's own parameter N
//     - exactly 1 generate construct, a loop (27.4) over the genvar i:
//       initialized to 0, condition 'i < N' (vpiLtOp, N bound to the
//       parameter), iteration 'i++' (vpiPostIncOp); both i in the loop
//       generate scheme are bound to the genvar; the body is the named block
//       gen_dfifo
//     - gen_dfifo holds exactly 1 instance, fifo_d of tlul_fifo_sync,
//       whose only override is ReqDepth (by name, 23.10.2.2, bound to
//       tlul_fifo_sync's parameter) = DReqDepth[i*4+:4]
//     - that value is an indexed part-select (37.59) whose prefix is bound
//       to DReqDepth, whose type is vpiPosIndexed ('+:'), whose base is
//       Operation vpiMultOp over the name i and 4, and whose width is the
//       Constant 4 (11.5.1)
//     - that i, being inside the loop generate block, is bound to the
//       block's implicit localparam i (27.4)
//   Module xbar_main
//     - has no port (A.1.2: no list_of_port_declarations)
//     - holds exactly 1 instance, u_s1n_25 of tlul_socket_1n, with the 2
//       named overrides DReqDepth = 68'h200000000000000 and N = 8, in source
//       order, each bound to tlul_socket_1n's parameter (23.10.2.2)
//   Module top
//     - '()' is one null port, as above
//     - exactly 2 parameters and 2 param_assignments, A and B, in that
//       order, nonlocal (top has no parameter port list, so 6.20.1 does not
//       make 'parameter' a synonym for localparam)
//     - A's value is the literal 68'h200000000000000
//     - B's value is the indexed part-select A[8*4+:4] whose prefix is bound
//       to top's A, vpiPosIndexed, base Operation vpiMultOp over the
//       Constants 8 and 4, width the Constant 4 (11.5.1)
//   Literals
//     - 68'h200000000000000 is vpiHexConst, 68 bits and unsigned (5.7.1:
//       "The first token, a size constant, shall specify the size of the
//       integer literal constant in terms of its exact number of bits.", and
//       based numbers are "unsigned integers if the base format only is
//       used")
//     - every unsized literal (0, 1, 4, 8) is signed (5.7.1: "Simple decimal
//       numbers without the size and the base format shall be treated as
//       signed integers")
//   Elaboration
//     - the top-level instances are exactly xbar_main and top (23.3.1);
//       6.20.1: "if no default value is specified for a parameter of a
//       design element, then a tool shall not implicitly instantiate that
//       design element"
//     - top's A is logic [67:0] and B is logic [3:0] (6.20.2: "If the
//       expression is integral, the parameter is a logic vector of the same
//       size with range [size-1:0].") and B reduces to 0
//     - u_s1n_25's N is 8 and its DReqDepth reduces to 0
//     - u_s1n_25 holds 8 instances of gen_dfifo, each with a fifo_d whose
//       ReqDepth reduces to 0
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: the resolved types and reduced values are
// checked only when the design is elaborated, on Design::getTopModules();
// the declarations, bindings, overrides and unreduced expressions are
// asserted on the definitions in every run.
//
// KNOWN COMPILER BUG (reference to genvar inside generate block), not a
// defect in this test: HLC binds the name i in 'DReqDepth[i*4+:4]', which is
// inside the loop generate block gen_dfifo, to the genvar i, and gen_dfifo
// declares no parameter i, although 27.4 says "Within the generate block of a
// loop generate construct, there is an implicit localparam declaration." and
// "any reference to this name inside the loop generate block will be a
// reference to the localparam, not to the genvar." GenDfifoIIsImplicitLocalparam
// is expected to fail until HLC is fixed; it is intentionally not skipped or
// relaxed.
//
// Model convention: HLDB holds the object a part-select applies to as the
// Select's prefix (getPrefix()) rather than as its vpiParent (37.59 detail
// 10); the tests read the prefix and assert what it is bound to.
//
// Model convention: the genvar i of the loop generate construct is held as a
// Variable of the GenFor, and the overrides an instance writes are held on
// the ModuleTypespec its RefInstance names; the tests read them there.
//
// What is NOT checked, and why:
//   - vpiConstantSelect of either part-select. 37.59 detail 9 requires that
//     "vpiConstantSelect returns TRUE for its parent", and Clause 37 defines
//     that property for nets, variables and selects but not for a
//     parameter, so the standard does not decide it here.
//   - The size of the unsized literals 0, 1, 4 and 8. 5.7.1 only requires
//     that it "shall be at least 32", so it is implementation-dependent.
//   - The vpiConstType of the unsized literals. Annex K lists both
//     vpiDecConst ("decimal integer") and vpiIntConst ("integer constant
//     (1364-2001)"), and the standard does not say which one an unsized
//     decimal number has.
//   - The unelaborated type of A and B. 6.20.2 makes it the type of "the
//     final value assigned to the parameter, after any value overrides have
//     been applied", which exists only after elaboration, so it is asserted
//     only on the elaborated top.
//   - Which typespec kind HLC attaches to 68'h200000000000000. 5.7.1 gives a
//     literal a size and a signedness, not a data type keyword; both are
//     asserted, on getSize() and on the typespec's signed flag.
//   - The value of the implicit localparam i in each elaborated gen_dfifo
//     instance. Every fifo_d receives 0 whatever i is, because DReqDepth is
//     0, so the reduced ReqDepth does not tell the instances apart; the
//     count of 8 instances is asserted instead.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/bit_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_block.h>
#include <hldb/gen_for.h>
#include <hldb/gen_scope.h>
#include <hldb/gen_scope_array.h>
#include <hldb/indexed_part_select.h>
#include <hldb/indexed_range.h>
#include <hldb/int_typespec.h>
#include <hldb/integer_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/long_int_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {

class PartSelect4Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PartSelect4.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Parameter *getParam(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, scope->getParameters());
  }

  // The name the LHS of 'pa' writes, whatever node holds it.
  static std::string_view getLhsName(const hldb::ParamAssign *pa) {
    if (const hldb::RefObj *const ref = pa->getLhs<hldb::RefObj>()) return ref->getName();
    if (const hldb::Parameter *const p = pa->getLhs<hldb::Parameter>()) return p->getName();
    return {};
  }

  // The declaration the LHS of 'pa' is bound to.
  static const hldb::Any *getLhsActual(const hldb::ParamAssign *pa) {
    if (const hldb::RefObj *const ref = pa->getLhs<hldb::RefObj>()) return ref->getActual();
    return pa->getLhs();
  }

  // The ParamAssign in 'scope' whose LHS names 'name'.
  static const hldb::ParamAssign *getParamAssign(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr || scope->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *scope->getParamAssigns()) {
      if (getLhsName(pa) == name) return pa;
    }
    return nullptr;
  }

  // The overrides an unelaborated instance writes, in source order.
  static const std::vector<hldb::ParamAssign *> *getOverrides(const hldb::RefInstance *inst) {
    if (inst == nullptr || inst->getTypespec() == nullptr) return nullptr;
    const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
    if (mt == nullptr) return nullptr;
    return mt->getParamAssigns();
  }

  // The only generate construct of tlul_socket_1n.
  static const hldb::GenFor *getGenFor() {
    const hldb::Module *const m = getModule("tlul_socket_1n");
    if (m == nullptr || m->getGenStmts() == nullptr || m->getGenStmts()->size() != 1u) return nullptr;
    return any_cast<hldb::GenFor>(m->getGenStmts()->at(0));
  }

  static const hldb::GenBlock *getGenDfifo() {
    const hldb::GenFor *const gf = getGenFor();
    if (gf == nullptr) return nullptr;
    return gf->getStmt<hldb::GenBlock>();
  }

  // The genvar i of the loop generate construct.
  static const hldb::Variable *getGenvarI() {
    const hldb::GenFor *const gf = getGenFor();
    if (gf == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("i", gf->getVariables());
  }

  static const hldb::RefInstance *getFifoD() {
    const hldb::GenBlock *const gb = getGenDfifo();
    if (gb == nullptr || gb->getStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *gb->getStmts()) {
      const hldb::RefInstance *const inst = any_cast<hldb::RefInstance>(stmt);
      if ((inst != nullptr) && (inst->getName() == "fifo_d")) return inst;
    }
    return nullptr;
  }

  // The name i in the base of 'DReqDepth[i*4+:4]', inside gen_dfifo.
  static const hldb::RefObj *getGenDfifoI() {
    const std::vector<hldb::ParamAssign *> *const overrides = getOverrides(getFifoD());
    if (overrides == nullptr || overrides->size() != 1u) return nullptr;
    const hldb::IndexedPartSelect *const ps = overrides->at(0)->getRhs<hldb::IndexedPartSelect>();
    if (ps == nullptr || ps->getRange() == nullptr) return nullptr;
    const hldb::Operation *const base = ps->getRange()->getBaseExpr<hldb::Operation>();
    if (base == nullptr || base->getOperands() == nullptr || base->getOperands()->empty()) return nullptr;
    return any_cast<hldb::RefObj>(base->getOperands()->at(0));
  }

  static const hldb::RefInstance *getUS1n25() {
    const hldb::Module *const m = getModule("xbar_main");
    if (m == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>("u_s1n_25", m->getRefInstances());
  }

  // Integer value of a Constant, from its decompiled text or from a sized or
  // based literal. The radix HLC picks for a folded value is a tool
  // convention, so every integer form is accepted.
  static bool getIntValue(const hldb::Constant *c, uint64_t *value) {
    std::string text;
    for (char ch : c->getDecompile()) {
      if ((ch != '_') && (ch != ' ')) text += ch;
    }
    int base = 10;
    const std::string::size_type tick = text.find('\'');
    if (tick != std::string::npos) {
      std::string::size_type pos = tick + 1;
      if ((pos < text.size()) && ((text[pos] == 's') || (text[pos] == 'S'))) ++pos;
      if (pos >= text.size()) return false;
      switch (text[pos]) {
        case 'h':
        case 'H': base = 16; break;
        case 'd':
        case 'D': base = 10; break;
        case 'o':
        case 'O': base = 8; break;
        case 'b':
        case 'B': base = 2; break;
        default: return false;
      }
      text = text.substr(pos + 1);
    }
    if (text.empty()) return false;
    const char *const last = text.data() + text.size();
    const std::from_chars_result res = std::from_chars(text.data(), last, *value, base);
    return (res.ec == std::errc()) && (res.ptr == last);
  }

  // Returns 1 for signed, 0 for unsigned, -1 if the typespec of 'c' is
  // missing or of a kind that carries no signedness.
  static int32_t signedness(const hldb::Constant *c) {
    if ((c->getTypespec() == nullptr) || (c->getTypespec()->getActual() == nullptr)) return -1;
    const hldb::Typespec *const ts = c->getTypespec()->getActual();
    if (const hldb::LogicTypespec *const t = any_cast<hldb::LogicTypespec>(ts)) return t->getSigned() ? 1 : 0;
    if (const hldb::BitTypespec *const t = any_cast<hldb::BitTypespec>(ts)) return t->getSigned() ? 1 : 0;
    if (const hldb::IntTypespec *const t = any_cast<hldb::IntTypespec>(ts)) return t->getSigned() ? 1 : 0;
    if (const hldb::IntegerTypespec *const t = any_cast<hldb::IntegerTypespec>(ts)) return t->getSigned() ? 1 : 0;
    if (const hldb::LongIntTypespec *const t = any_cast<hldb::LongIntTypespec>(ts)) return t->getSigned() ? 1 : 0;
    return -1;
  }

  // Verifies 'expr' is the unsized decimal literal 'text', which 5.7.1 makes
  // signed.
  static void ExpectDecimalLiteral(const hldb::Any *expr, std::string_view text, std::string_view what) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(expr);
    ASSERT_NE(c, nullptr) << what << " is the literal " << text;
    EXPECT_EQ(c->getDecompile(), text) << what;
    EXPECT_EQ(signedness(c), 1) << "5.7.1: " << what << ", a simple decimal number, is signed";
  }

  // Verifies 'expr' is the 68-bit hex literal 68'h200000000000000.
  static void ExpectWideHexLiteral(const hldb::Any *expr, std::string_view what) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(expr);
    ASSERT_NE(c, nullptr) << what << " is the literal 68'h200000000000000";
    EXPECT_EQ(c->getDecompile(), "68'h200000000000000") << what;
    EXPECT_EQ(c->getConstType(), vpiHexConst) << "5.7.1: " << what << " is written with the base format 'h";
    EXPECT_EQ(c->getSize(), 68) << "5.7.1: " << what << " is sized 68 bits";
    EXPECT_EQ(signedness(c), 0) << "5.7.1: " << what << " has the base format only, so it is unsigned";
  }

  // Verifies 'module' declares exactly one null port.
  static void ExpectOneNullPort(std::string_view module) {
    const hldb::Module *const m = getModule(module);
    ASSERT_NE(m, nullptr) << "module '" << module << "' not found";
    ASSERT_NE(m->getPorts(), nullptr) << "37.14 detail 10: " << module << "'s '()' declares a null port";
    ASSERT_EQ(m->getPorts()->size(), 1u) << "37.14 detail 10: exactly one null port";
    const hldb::Port *const p = m->getPorts()->at(0);
    EXPECT_EQ(p->getName(), "") << "37.14 detail 8: a null port has no name, so NULL is returned";
    EXPECT_EQ(p->getLowConn(), nullptr) << "37.14 detail 10: vpiLowConn of a null port is NULL";
    EXPECT_EQ(p->getPortIndex(), 0) << "37.14 detail 9: the first port has a port index of zero";
  }

  // The elaborated module whose definition is 'defName' among the top-level
  // instances.
  static const hldb::Module *getElabTop(std::string_view defName) {
    return hldb::findByDefName<hldb::Module>(defName, m_design->getTopModules());
  }
};

// ---------------------------------------------------------------------------
// Ports
// ---------------------------------------------------------------------------

TEST_F(PartSelect4Test, EmptyPortListsDeclareOneNullPort) {
  for (std::string_view name : {"tlul_fifo_sync", "tlul_socket_1n", "top"}) {
    SCOPED_TRACE(name);
    ExpectOneNullPort(name);
  }
}

TEST_F(PartSelect4Test, XbarMainHasNoPort) {
  const hldb::Module *const m = getModule("xbar_main");
  ASSERT_NE(m, nullptr) << "module 'xbar_main' not found";
  EXPECT_TRUE(m->getPorts() == nullptr || m->getPorts()->empty())
      << "A.1.2: xbar_main's header writes no list_of_port_declarations";
}

// ---------------------------------------------------------------------------
// module tlul_fifo_sync
// ---------------------------------------------------------------------------

TEST_F(PartSelect4Test, ReqDepthIsIntUnsignedPortWithNoDefault) {
  const hldb::Module *const m = getModule("tlul_fifo_sync");
  ASSERT_NE(m, nullptr) << "module 'tlul_fifo_sync' not found";
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 1u);
  ASSERT_NE(m->getParamAssigns(), nullptr);
  EXPECT_EQ(m->getParamAssigns()->size(), 1u) << "A.2.4: the header writes 1 param_assignment";
  const hldb::Parameter *const p = getParam(m, "ReqDepth");
  ASSERT_NE(p, nullptr) << "parameter 'ReqDepth' not found";
  EXPECT_FALSE(p->getLocalParam()) << "6.20.4: a 'parameter' in the parameter port list is a nonlocal parameter";
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = p->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr) << "ReqDepth is declared 'int unsigned'";
  EXPECT_FALSE(it->getSigned()) << "6.11.3: 'int unsigned' is explicitly unsigned";
  const hldb::ParamAssign *const pa = getParamAssign(m, "ReqDepth");
  ASSERT_NE(pa, nullptr) << "A.2.4: 'ReqDepth' is a param_assignment";
  EXPECT_EQ(pa->getRhs(), nullptr) << "6.20.1: the default is omitted, so the parameter has no default value";
}

// ---------------------------------------------------------------------------
// module tlul_socket_1n
// ---------------------------------------------------------------------------

TEST_F(PartSelect4Test, SocketDeclaresNThenDReqDepthWithNoDefault) {
  const hldb::Module *const m = getModule("tlul_socket_1n");
  ASSERT_NE(m, nullptr) << "module 'tlul_socket_1n' not found";
  ASSERT_NE(m->getParameters(), nullptr);
  ASSERT_EQ(m->getParameters()->size(), 2u);
  ASSERT_NE(m->getParamAssigns(), nullptr);
  EXPECT_EQ(m->getParamAssigns()->size(), 2u) << "A.2.4: the header writes 2 param_assignments";
  const std::string_view names[] = {"N", "DReqDepth"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::Parameter *const p = any_cast<hldb::Parameter>(m->getParameters()->at(i));
    ASSERT_NE(p, nullptr) << "parameter " << i << " is a value parameter";
    EXPECT_EQ(p->getName(), names[i]) << "parameter " << i << " follows the order of declaration";
    EXPECT_FALSE(p->getLocalParam()) << "6.20.4: " << names[i] << " is a nonlocal parameter port";
    const hldb::ParamAssign *const pa = getParamAssign(m, names[i]);
    ASSERT_NE(pa, nullptr) << "A.2.4: '" << names[i] << "' is a param_assignment";
    EXPECT_EQ(pa->getRhs(), nullptr) << "6.20.1: " << names[i] << " has no default value";
  }
}

TEST_F(PartSelect4Test, NIsIntUnsigned) {
  const hldb::Parameter *const p = getParam(getModule("tlul_socket_1n"), "N");
  ASSERT_NE(p, nullptr) << "parameter 'N' not found";
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = p->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr) << "N is declared 'int unsigned'";
  EXPECT_FALSE(it->getSigned()) << "6.11.3: 'int unsigned' is explicitly unsigned";
}

TEST_F(PartSelect4Test, DReqDepthIsBitNTimes4Minus1To0) {
  const hldb::Module *const m = getModule("tlul_socket_1n");
  ASSERT_NE(m, nullptr);
  const hldb::Parameter *const p = getParam(m, "DReqDepth");
  ASSERT_NE(p, nullptr) << "parameter 'DReqDepth' not found";
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::BitTypespec *const bt = p->getTypespec()->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr) << "DReqDepth is declared 'bit [N*4-1:0]'";
  EXPECT_FALSE(bt->getSigned()) << "6.11.3: bit defaults to unsigned";
  ASSERT_NE(bt->getRanges(), nullptr);
  ASSERT_EQ(bt->getRanges()->size(), 1u) << "one packed dimension";
  const hldb::Range *const r = bt->getRanges()->at(0);
  const hldb::Operation *const sub = r->getLeftExpr<hldb::Operation>();
  ASSERT_NE(sub, nullptr) << "unreduced, the left bound is the expression N*4-1";
  EXPECT_EQ(sub->getOpType(), vpiSubOp);
  ASSERT_NE(sub->getOperands(), nullptr);
  ASSERT_EQ(sub->getOperands()->size(), 2u);
  const hldb::Operation *const mul = any_cast<hldb::Operation>(sub->getOperands()->at(0));
  ASSERT_NE(mul, nullptr) << "the first operand of '-' is N*4";
  EXPECT_EQ(mul->getOpType(), vpiMultOp);
  ASSERT_NE(mul->getOperands(), nullptr);
  ASSERT_EQ(mul->getOperands()->size(), 2u);
  const hldb::RefObj *const n = any_cast<hldb::RefObj>(mul->getOperands()->at(0));
  ASSERT_NE(n, nullptr) << "the first operand of '*' is the name N";
  EXPECT_EQ(n->getName(), "N");
  ASSERT_NE(getParam(m, "N"), nullptr);
  EXPECT_EQ(n->getActual(), getParam(m, "N")) << "6.20.1: a parameter can depend on earlier parameters";
  ExpectDecimalLiteral(mul->getOperands()->at(1), "4", "the second operand of '*'");
  ExpectDecimalLiteral(sub->getOperands()->at(1), "1", "the second operand of '-'");
  ExpectDecimalLiteral(r->getRightExpr(), "0", "the right bound");
}

TEST_F(PartSelect4Test, SocketHasOneLoopGenerateOverGenvarI) {
  const hldb::GenFor *const gf = getGenFor();
  ASSERT_NE(gf, nullptr) << "27.4: tlul_socket_1n holds exactly one loop generate construct";
  const hldb::Variable *const i = getGenvarI();
  ASSERT_NE(i, nullptr) << "27.4: the loop declares the genvar 'i'";
  ASSERT_NE(gf->getVariables(), nullptr);
  EXPECT_EQ(gf->getVariables()->size(), 1u);
}

TEST_F(PartSelect4Test, LoopInitializesIToZero) {
  const hldb::GenFor *const gf = getGenFor();
  ASSERT_NE(gf, nullptr);
  ASSERT_NE(gf->getForInitStmts(), nullptr);
  ASSERT_EQ(gf->getForInitStmts()->size(), 1u) << "27.4: 'genvar i = 0' is the genvar_initialization";
  const hldb::Assignment *const init = any_cast<hldb::Assignment>(gf->getForInitStmts()->at(0));
  ASSERT_NE(init, nullptr);
  ASSERT_NE(getGenvarI(), nullptr);
  EXPECT_EQ(init->getLhs(), getGenvarI()) << "the initialization assigns the genvar i";
  ExpectDecimalLiteral(init->getRhs(), "0", "the initial value of i");
}

TEST_F(PartSelect4Test, LoopConditionIsILessThanN) {
  const hldb::GenFor *const gf = getGenFor();
  ASSERT_NE(gf, nullptr);
  const hldb::Operation *const cond = gf->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "27.4: the loop has the condition 'i < N'";
  EXPECT_EQ(cond->getOpType(), vpiLtOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const i = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  ASSERT_NE(i, nullptr) << "the first operand of '<' is the name i";
  EXPECT_EQ(i->getName(), "i");
  ASSERT_NE(getGenvarI(), nullptr);
  EXPECT_EQ(i->getActual(), getGenvarI()) << "'i' is the loop's genvar";
  const hldb::RefObj *const n = any_cast<hldb::RefObj>(cond->getOperands()->at(1));
  ASSERT_NE(n, nullptr) << "the second operand of '<' is the name N";
  EXPECT_EQ(n->getName(), "N");
  ASSERT_NE(getParam(getModule("tlul_socket_1n"), "N"), nullptr);
  EXPECT_EQ(n->getActual(), getParam(getModule("tlul_socket_1n"), "N")) << "'N' is tlul_socket_1n's parameter";
}

TEST_F(PartSelect4Test, LoopIterationIsPostIncrementOfI) {
  const hldb::GenFor *const gf = getGenFor();
  ASSERT_NE(gf, nullptr);
  ASSERT_NE(gf->getForIncStmts(), nullptr);
  ASSERT_EQ(gf->getForIncStmts()->size(), 1u) << "27.4: 'i++' is the genvar_iteration";
  const hldb::Operation *const inc = any_cast<hldb::Operation>(gf->getForIncStmts()->at(0));
  ASSERT_NE(inc, nullptr);
  EXPECT_EQ(inc->getOpType(), vpiPostIncOp);
  ASSERT_NE(inc->getOperands(), nullptr);
  ASSERT_EQ(inc->getOperands()->size(), 1u);
  const hldb::RefObj *const i = any_cast<hldb::RefObj>(inc->getOperands()->at(0));
  ASSERT_NE(i, nullptr);
  EXPECT_EQ(i->getName(), "i");
  ASSERT_NE(getGenvarI(), nullptr);
  EXPECT_EQ(i->getActual(), getGenvarI()) << "'i' is the loop's genvar";
}

TEST_F(PartSelect4Test, LoopBodyIsBlockGenDfifoWithOneInstance) {
  const hldb::GenBlock *const gb = getGenDfifo();
  ASSERT_NE(gb, nullptr) << "27.4: the loop body is a generate block";
  EXPECT_EQ(gb->getName(), "gen_dfifo") << "the block is labeled 'gen_dfifo'";
  ASSERT_NE(gb->getStmts(), nullptr);
  EXPECT_EQ(gb->getStmts()->size(), 1u) << "gen_dfifo holds only the instance fifo_d";
  const hldb::RefInstance *const inst = getFifoD();
  ASSERT_NE(inst, nullptr) << "instance 'fifo_d' not found";
  ASSERT_NE(inst->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr);
  ASSERT_NE(getModule("tlul_fifo_sync"), nullptr);
  EXPECT_EQ(mt->getModule(), getModule("tlul_fifo_sync")) << "fifo_d instantiates tlul_fifo_sync";
  EXPECT_TRUE(inst->getArguments() == nullptr || inst->getArguments()->empty()) << "'fifo_d()' connects no port";
}

TEST_F(PartSelect4Test, FifoDOverridesReqDepthByName) {
  const std::vector<hldb::ParamAssign *> *const overrides = getOverrides(getFifoD());
  ASSERT_NE(overrides, nullptr) << "fifo_d writes a parameter_value_assignment";
  ASSERT_EQ(overrides->size(), 1u);
  const hldb::ParamAssign *const pa = overrides->at(0);
  EXPECT_EQ(getLhsName(pa), "ReqDepth");
  EXPECT_TRUE(pa->getConnByName()) << "23.10.2.2: '.ReqDepth(...)' assigns by name";
  ASSERT_NE(getParam(getModule("tlul_fifo_sync"), "ReqDepth"), nullptr);
  EXPECT_EQ(getLhsActual(pa), getParam(getModule("tlul_fifo_sync"), "ReqDepth"))
      << "23.10.2.2: the name is tlul_fifo_sync's parameter";
}

TEST_F(PartSelect4Test, ReqDepthOverrideIsIndexedPartSelectOfDReqDepth) {
  const std::vector<hldb::ParamAssign *> *const overrides = getOverrides(getFifoD());
  ASSERT_NE(overrides, nullptr);
  ASSERT_EQ(overrides->size(), 1u);
  const hldb::IndexedPartSelect *const ps = overrides->at(0)->getRhs<hldb::IndexedPartSelect>();
  ASSERT_NE(ps, nullptr) << "11.5.1: 'DReqDepth[i*4+:4]' is an indexed part-select";
  const hldb::RefObj *const prefix = ps->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr) << "the part-select applies to the name DReqDepth";
  EXPECT_EQ(prefix->getName(), "DReqDepth");
  ASSERT_NE(getParam(getModule("tlul_socket_1n"), "DReqDepth"), nullptr);
  EXPECT_EQ(prefix->getActual(), getParam(getModule("tlul_socket_1n"), "DReqDepth"))
      << "6.20.2: a part-select of tlul_socket_1n's parameter DReqDepth";
  const hldb::IndexedRange *const range = ps->getRange();
  ASSERT_NE(range, nullptr) << "37.59: an indexed part select has a base and a width";
  EXPECT_EQ(range->getIndexedPartSelectType(), vpiPosIndexed) << "37.59, Annex K: vpiPosIndexed is '+:'";
  const hldb::Operation *const base = range->getBaseExpr<hldb::Operation>();
  ASSERT_NE(base, nullptr) << "unreduced, the base is the expression i*4";
  EXPECT_EQ(base->getOpType(), vpiMultOp);
  ASSERT_NE(base->getOperands(), nullptr);
  ASSERT_EQ(base->getOperands()->size(), 2u);
  const hldb::RefObj *const i = getGenDfifoI();
  ASSERT_NE(i, nullptr) << "the first operand of '*' is the name i";
  EXPECT_EQ(i->getName(), "i");
  ExpectDecimalLiteral(base->getOperands()->at(1), "4", "the second operand of '*'");
  ExpectDecimalLiteral(range->getWidthExpr(), "4", "11.5.1: the width");
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (reference to
// genvar inside generate block) in the file header.
TEST_F(PartSelect4Test, GenDfifoIIsImplicitLocalparam) {
  const hldb::GenBlock *const gb = getGenDfifo();
  ASSERT_NE(gb, nullptr);
  const hldb::RefObj *const i = getGenDfifoI();
  ASSERT_NE(i, nullptr) << "'DReqDepth[i*4+:4]' references the name i";
  ASSERT_NE(i->getActual(), nullptr) << "'i' is declared";
  const hldb::Parameter *const lp = any_cast<hldb::Parameter>(i->getActual());
  ASSERT_NE(lp, nullptr) << "27.4: a reference to i inside the loop generate block is a reference to the "
                            "localparam, not to the genvar";
  EXPECT_EQ(lp->getName(), "i") << "27.4: the localparam has the same name as the loop index";
  EXPECT_TRUE(lp->getLocalParam()) << "27.4: an implicit localparam declaration";
  EXPECT_EQ(getParam(gb, "i"), lp) << "27.4: the localparam is declared within the generate block gen_dfifo";
}

// ---------------------------------------------------------------------------
// module xbar_main
// ---------------------------------------------------------------------------

TEST_F(PartSelect4Test, XbarMainInstantiatesSocketOnce) {
  const hldb::Module *const m = getModule("xbar_main");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getRefInstances(), nullptr);
  EXPECT_EQ(m->getRefInstances()->size(), 1u);
  const hldb::RefInstance *const inst = getUS1n25();
  ASSERT_NE(inst, nullptr) << "instance 'u_s1n_25' not found";
  ASSERT_NE(inst->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr);
  ASSERT_NE(getModule("tlul_socket_1n"), nullptr);
  EXPECT_EQ(mt->getModule(), getModule("tlul_socket_1n")) << "u_s1n_25 instantiates tlul_socket_1n";
  EXPECT_TRUE(inst->getArguments() == nullptr || inst->getArguments()->empty()) << "'u_s1n_25()' connects no port";
}

TEST_F(PartSelect4Test, USocketOverridesDReqDepthAndNByName) {
  const std::vector<hldb::ParamAssign *> *const overrides = getOverrides(getUS1n25());
  ASSERT_NE(overrides, nullptr) << "u_s1n_25 writes a parameter_value_assignment";
  ASSERT_EQ(overrides->size(), 2u);
  const hldb::Module *const socket = getModule("tlul_socket_1n");
  ASSERT_NE(socket, nullptr);
  const std::string_view names[] = {"DReqDepth", "N"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::ParamAssign *const pa = overrides->at(i);
    EXPECT_EQ(getLhsName(pa), names[i]) << "override " << i << " in source order";
    EXPECT_TRUE(pa->getConnByName()) << "23.10.2.2: override " << i << " assigns by name";
    ASSERT_NE(getParam(socket, names[i]), nullptr);
    EXPECT_EQ(getLhsActual(pa), getParam(socket, names[i]))
        << "23.10.2.2: '" << names[i] << "' is tlul_socket_1n's parameter";
  }
  ExpectWideHexLiteral(overrides->at(0)->getRhs(), "the DReqDepth override");
  ExpectDecimalLiteral(overrides->at(1)->getRhs(), "8", "the N override");
}

// ---------------------------------------------------------------------------
// module top
// ---------------------------------------------------------------------------

TEST_F(PartSelect4Test, TopDeclaresOverridableAAndB) {
  const hldb::Module *const m = getModule("top");
  ASSERT_NE(m, nullptr) << "module 'top' not found";
  ASSERT_NE(m->getParameters(), nullptr);
  ASSERT_EQ(m->getParameters()->size(), 2u);
  ASSERT_NE(m->getParamAssigns(), nullptr);
  EXPECT_EQ(m->getParamAssigns()->size(), 2u) << "A.2.4: top writes 2 param_assignments";
  const std::string_view names[] = {"A", "B"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::Parameter *const p = any_cast<hldb::Parameter>(m->getParameters()->at(i));
    ASSERT_NE(p, nullptr) << "parameter " << i << " is a value parameter";
    EXPECT_EQ(p->getName(), names[i]) << "parameter " << i << " follows the order of declaration";
    EXPECT_FALSE(p->getLocalParam()) << "6.20.1: top has no parameter port list, so " << names[i] << " is overridable";
  }
}

TEST_F(PartSelect4Test, AIsThe68BitHexLiteral) {
  const hldb::ParamAssign *const pa = getParamAssign(getModule("top"), "A");
  ASSERT_NE(pa, nullptr) << "A's param_assignment not found";
  ExpectWideHexLiteral(pa->getRhs(), "A's value");
}

TEST_F(PartSelect4Test, BIsIndexedPartSelectOfA) {
  const hldb::Module *const m = getModule("top");
  ASSERT_NE(m, nullptr);
  const hldb::ParamAssign *const pa = getParamAssign(m, "B");
  ASSERT_NE(pa, nullptr) << "B's param_assignment not found";
  const hldb::IndexedPartSelect *const ps = pa->getRhs<hldb::IndexedPartSelect>();
  ASSERT_NE(ps, nullptr) << "11.5.1: unreduced, 'A[8*4+:4]' is an indexed part-select";
  const hldb::RefObj *const prefix = ps->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr) << "the part-select applies to the name A";
  EXPECT_EQ(prefix->getName(), "A");
  ASSERT_NE(getParam(m, "A"), nullptr);
  EXPECT_EQ(prefix->getActual(), getParam(m, "A")) << "6.20.2: a part-select of top's parameter A";
  const hldb::IndexedRange *const range = ps->getRange();
  ASSERT_NE(range, nullptr) << "37.59: an indexed part select has a base and a width";
  EXPECT_EQ(range->getIndexedPartSelectType(), vpiPosIndexed) << "37.59, Annex K: vpiPosIndexed is '+:'";
  const hldb::Operation *const base = range->getBaseExpr<hldb::Operation>();
  ASSERT_NE(base, nullptr) << "unreduced, the base is the expression 8*4";
  EXPECT_EQ(base->getOpType(), vpiMultOp);
  ASSERT_NE(base->getOperands(), nullptr);
  ASSERT_EQ(base->getOperands()->size(), 2u);
  ExpectDecimalLiteral(base->getOperands()->at(0), "8", "the first operand of '*'");
  ExpectDecimalLiteral(base->getOperands()->at(1), "4", "the second operand of '*'");
  ExpectDecimalLiteral(range->getWidthExpr(), "4", "11.5.1: the width");
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(PartSelect4Test, TopLevelInstancesAreXbarMainAndTop) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    EXPECT_EQ(m_design->getTopModules()->size(), 2u)
        << "23.3.1: xbar_main and top are never instantiated; 6.20.1 forbids implicitly instantiating "
           "tlul_fifo_sync and tlul_socket_1n";
    EXPECT_NE(getElabTop("xbar_main"), nullptr) << "23.3.1: xbar_main is a top-level instance";
    EXPECT_NE(getElabTop("top"), nullptr) << "23.3.1: top is a top-level instance";
  }
}

TEST_F(PartSelect4Test, TopAIsLogic67To0AndBReducesToZero) {
  if (m_design->getElaborated()) {
    const hldb::Module *const top = getElabTop("top");
    ASSERT_NE(top, nullptr) << "the elaborated top not found";
    const hldb::Parameter *const a = getParam(top, "A");
    ASSERT_NE(a, nullptr);
    ASSERT_NE(a->getTypespec(), nullptr);
    const hldb::LogicTypespec *const at = a->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(at, nullptr) << "6.20.2: an integral value makes A a logic vector";
    ASSERT_NE(at->getRanges(), nullptr);
    ASSERT_EQ(at->getRanges()->size(), 1u);
    uint64_t v = 0;
    const hldb::Constant *const aLeft = at->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
    ASSERT_NE(aLeft, nullptr);
    ASSERT_TRUE(getIntValue(aLeft, &v)) << "unparsable constant '" << aLeft->getDecompile() << "'";
    EXPECT_EQ(v, 67u) << "6.20.2: range [size-1:0] of the 68-bit value";
    const hldb::Parameter *const b = getParam(top, "B");
    ASSERT_NE(b, nullptr);
    ASSERT_NE(b->getTypespec(), nullptr);
    const hldb::LogicTypespec *const bt = b->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(bt, nullptr) << "6.20.2: an integral value makes B a logic vector";
    ASSERT_NE(bt->getRanges(), nullptr);
    ASSERT_EQ(bt->getRanges()->size(), 1u);
    const hldb::Constant *const bLeft = bt->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
    ASSERT_NE(bLeft, nullptr);
    ASSERT_TRUE(getIntValue(bLeft, &v)) << "unparsable constant '" << bLeft->getDecompile() << "'";
    EXPECT_EQ(v, 3u) << "11.5.1: the part-select is 4 bits wide, so B is [3:0]";
    const hldb::ParamAssign *const pa = getParamAssign(top, "B");
    ASSERT_NE(pa, nullptr);
    const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr) << "6.20.2: B should be reduced to a Constant";
    ASSERT_TRUE(getIntValue(c, &v)) << "unparsable constant '" << c->getDecompile() << "'";
    EXPECT_EQ(v, 0u) << "11.5.1: A[35:32] of 2**57 is 0";
  }
}

TEST_F(PartSelect4Test, USocketReceivesNEightAndDReqDepthZero) {
  if (m_design->getElaborated()) {
    const hldb::Module *const xbar = getElabTop("xbar_main");
    ASSERT_NE(xbar, nullptr) << "the elaborated xbar_main not found";
    const hldb::Module *const socket = hldb::findByName<hldb::Module>("u_s1n_25", xbar->getModules());
    ASSERT_NE(socket, nullptr) << "the instance u_s1n_25 is elaborated";
    uint64_t v = 0;
    const hldb::ParamAssign *const n = getParamAssign(socket, "N");
    ASSERT_NE(n, nullptr);
    const hldb::Constant *const nc = n->getRhs<hldb::Constant>();
    ASSERT_NE(nc, nullptr) << "6.20.2: N should be reduced to a Constant";
    ASSERT_TRUE(getIntValue(nc, &v)) << "unparsable constant '" << nc->getDecompile() << "'";
    EXPECT_EQ(v, 8u) << "23.10.2.2: '.N(8)'";
    const hldb::ParamAssign *const d = getParamAssign(socket, "DReqDepth");
    ASSERT_NE(d, nullptr);
    const hldb::Constant *const dc = d->getRhs<hldb::Constant>();
    ASSERT_NE(dc, nullptr) << "6.20.2: DReqDepth should be reduced to a Constant";
    ASSERT_TRUE(getIntValue(dc, &v)) << "unparsable constant '" << dc->getDecompile() << "'";
    EXPECT_EQ(v, 0u) << "6.11.2: 2**57 converted to bit [31:0] keeps its low 32 bits, all 0";
  }
}

TEST_F(PartSelect4Test, GenDfifoHasEightInstancesWithReqDepthZero) {
  if (m_design->getElaborated()) {
    const hldb::Module *const xbar = getElabTop("xbar_main");
    ASSERT_NE(xbar, nullptr) << "the elaborated xbar_main not found";
    const hldb::Module *const socket = hldb::findByName<hldb::Module>("u_s1n_25", xbar->getModules());
    ASSERT_NE(socket, nullptr) << "the instance u_s1n_25 is elaborated";
    ASSERT_NE(socket->getGenScopeArrays(), nullptr) << "27.4: gen_dfifo declares an array of generate block instances";
    std::vector<const hldb::GenScope *> scopes;
    for (const hldb::GenScopeArray *const gsa : *socket->getGenScopeArrays()) {
      if ((gsa->getName().substr(0, 9) != "gen_dfifo") || (gsa->getGenScopes() == nullptr)) continue;
      for (const hldb::GenScope *const gs : *gsa->getGenScopes()) scopes.emplace_back(gs);
    }
    EXPECT_EQ(scopes.size(), 8u) << "27.4: i takes the values 0 to 7 because N = 8";
    for (const hldb::GenScope *const gs : scopes) {
      const hldb::Module *const fifo = hldb::findByName<hldb::Module>("fifo_d", gs->getModules());
      ASSERT_NE(fifo, nullptr) << "each gen_dfifo instance holds fifo_d";
      const hldb::ParamAssign *const pa = getParamAssign(fifo, "ReqDepth");
      ASSERT_NE(pa, nullptr);
      const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
      ASSERT_NE(c, nullptr) << "6.20.2: ReqDepth should be reduced to a Constant";
      uint64_t v = 0;
      ASSERT_TRUE(getIntValue(c, &v)) << "unparsable constant '" << c->getDecompile() << "'";
      EXPECT_EQ(v, 0u) << "11.5.1: every 4-bit slice of DReqDepth = 0 is 0";
    }
  }
}

TEST_F(PartSelect4Test, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "the file is legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
