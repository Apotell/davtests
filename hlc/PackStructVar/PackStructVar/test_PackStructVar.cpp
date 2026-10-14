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

// Validates the HLDB model built for tests/PackStructVar/dut.sv:
//
//   package top_pkg;
//     localparam int TL_AW=32;
//     localparam int TL_DW=32;    // = TL_DBW * 8; TL_DBW must be a power-of-two
//     localparam int TL_AIW=8;    // a_source, d_source
//     localparam int TL_DIW=1;    // d_sink
//     localparam int TL_DUW=16;   // d_user
//     localparam int TL_DBW=(TL_DW>>3);
//     localparam int TL_SZW=$clog2($clog2(TL_DBW)+1);
//     localparam int FLASH_BANKS=2;
//     localparam int FLASH_PAGES_PER_BANK=256;
//     localparam int FLASH_WORDS_PER_PAGE=256;
//     localparam int FLASH_BYTES_PER_WORD=4;
//   endpackage
//
//   package tlul_pkg;
//     typedef enum logic [2:0] {
//       AccessAck     = 3'h 0,
//       AccessAckData = 3'h 1
//     } tl_d_op_e;
//     typedef struct packed {
//       logic                         d_valid;
//       tl_d_op_e                     d_opcode;
//       logic                  [2:0]  d_param;
//       logic  [top_pkg::TL_SZW-1:0]  d_size;   // Bouncing back a_size
//       logic  [top_pkg::TL_AIW-1:0]  d_source;
//       logic  [top_pkg::TL_DIW-1:0]  d_sink;
//       logic   [top_pkg::TL_DW-1:0]  d_data;
//       logic  [top_pkg::TL_DUW-1:0]  d_user;
//       logic                         d_error;
//       logic                         a_ready;
//     } tl_h2d_t;
//   endpackage
//
//   module flash_ctrl ();
//     tlul_pkg::tl_h2d_t tl_fifo_h2d [2];
//   endmodule
//
// The point of the fixture is a module variable that is an unpacked array of
// a packed structure from one package, whose member widths are computed from
// parameters of another package through the package scope resolution
// operator (IEEE 1800-2023 26.3). The regression this file exists to catch is
// HLC losing those scoped parameter references in the member ranges.
//
// What is checked, and why:
//   Package top_pkg
//     - exactly 11 parameters, each a local parameter of type int
//     - TL_DBW is unreduced Operation vpiRShiftOp (11.4.10) over RefObj
//       TL_DW and Constant "3"; TL_SZW is unreduced the SysFuncCall
//       "$clog2" (20.8.1) of Operation vpiAddOp over a nested $clog2 of
//       TL_DBW and Constant "1"; the other nine are the literals written
//     - a parameter value is a constant expression (6.20.2), so on an
//       elaborated design TL_DBW = 32 >> 3 = 4 and
//       TL_SZW = $clog2($clog2(4) + 1) = $clog2(3) = 2
//   Package tlul_pkg
//     - exactly 2 typedefs. tl_d_op_e is an enum (6.19) with base type
//       logic [2:0] and exactly 2 names, AccessAck = 0 and AccessAckData =
//       1. White space between the base format and the digits ("3'h 0") is
//       legal (5.7.1), so the values are read numerically
//     - tl_h2d_t is a packed StructTypespec (7.2.1) with exactly 10 members
//       in source order. d_valid, d_error and a_ready are 1-bit logic;
//       d_opcode is typed by tl_d_op_e; d_param is logic [2:0]; and d_size,
//       d_source, d_sink, d_data and d_user are logic with the single packed
//       range [top_pkg::<P>-1:0], whose left bound is Operation vpiSubOp over
//       a package-scoped RefObj path to top_pkg's Parameter <P> and Constant
//       "1", for P = TL_SZW, TL_AIW, TL_DIW, TL_DW, TL_DUW
//   Module flash_ctrl (23.2)
//     - '()' declares one null port (37.14 detail 10, "module M();"): no
//       name (detail 8), port index 0 (detail 9), and no low connection
//     - exactly 1 variable, tl_fifo_h2d, an unpacked array (7.4.2) of 2
//       elements ('[2]' means '[0:1]') whose element type is the
//       package-scoped name tlul_pkg::tl_h2d_t
//   Elaboration (23.3.1)
//     - flash_ctrl appears in no instantiation, so on an elaborated design
//       it is the only top-level instance
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics
//
// Reduction and elaboration: TL_DBW and TL_SZW are checked in both modes,
// unreduced on Design::getAllPackages() and reduced, only when the design is
// elaborated, on Design::getTopPackages().
//
// KNOWN COMPILER BUG (null port missing), not a defect in this test: HLC
// models 'module flash_ctrl ();' with no port at all, although 37.14 detail
// 10 names "module M();" as declaring a null port. FlashCtrlHasOneNullPort is
// expected to fail until HLC is fixed; it is intentionally not skipped or
// relaxed.
//
// KNOWN COMPILER BUG (unpacked [size] dimension malformed), not a defect in
// this test: HLC records a '[2]' dimension as a range whose left bound is a
// vpiSubOp Operation with the single operand 2 and which has no right bound.
// 7.4.2 says "[size] shall mean the same as [0:size-1]", but neither those
// bounds nor the element count can be read from that range.
// TlFifoH2dIsTwoElementUnpackedArray is expected to fail until HLC is fixed;
// it is intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - The width of tl_h2d_t (68 bits once TL_SZW is 2). Nothing in the
//     source computes it, so there is no expression whose value could be
//     asserted.
//   - How a scoped type or parameter path records its package element (a
//     RefObj or a RefTypespec wrapping one) is a model convention; either is
//     accepted.
//   - How a RefTypespec refers to a typedef: it may resolve to the
//     TypedefTypespec or to the typespec it aliases. Both are that type
//     (6.18), so either is accepted.
//   - Whether other packages (for example a built-in one) also appear in
//     Design::getAllPackages() is a tool convention; packages are looked up
//     by name.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.
//   - The comments in the fixture are not design objects (5.4).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

namespace hlc {

class PackStructVarTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackStructVar.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg(std::string_view name) {
    return hldb::findByName<hldb::Package>(name, m_design->getAllPackages());
  }

  static const hldb::Parameter *getTopPkgParam(std::string_view name) {
    const hldb::Package *const pkg = getPkg("top_pkg");
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, pkg->getParameters());
  }

  static const hldb::ParamAssign *getParamAssign(const hldb::Package *pkg, std::string_view name) {
    if (pkg == nullptr || pkg->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *pkg->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == name)) return pa;
    }
    return nullptr;
  }

  static const hldb::Typedef *getTlulTypedef(std::string_view name) {
    const hldb::Package *const pkg = getPkg("tlul_pkg");
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>(name, pkg->getTypedefs());
  }

  static const hldb::Struct *getH2dStruct() {
    const hldb::Typedef *const td = getTlulTypedef("tl_h2d_t");
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
    if (st == nullptr) return nullptr;
    return st->getStruct();
  }

  static const hldb::Module *getFlashCtrl() {
    return hldb::findByDefName<hldb::Module>("flash_ctrl", m_design->getAllModules());
  }

  // Whether 'rts' names 'td', directly or as the last element of a
  // package-scoped path.
  static bool namesTypedef(const hldb::RefTypespec *rts, const hldb::Typedef *td) {
    if (rts == nullptr || td == nullptr || td->getAlias() == nullptr) return false;
    const hldb::Typespec *actual = rts->getActual();
    if ((actual == nullptr) && (rts->getPathElems() != nullptr) && !rts->getPathElems()->empty()) {
      const hldb::RefTypespec *const last = any_cast<hldb::RefTypespec>(rts->getPathElems()->back());
      if (last != nullptr) actual = last->getActual();
    }
    if (actual == nullptr) return false;
    const hldb::TypedefTypespec *const viaTypedef = any_cast<hldb::TypedefTypespec>(actual);
    return ((viaTypedef != nullptr) && (viaTypedef->getTypedef() == td)) || (actual == td->getAlias()->getActual());
  }

  // Integer value of a Constant, from its decompiled text or from a sized or
  // based literal; white space and '_' are ignored (5.7.1).
  static bool getIntValue(const hldb::Constant *c, uint64_t *value) {
    std::string text;
    for (char ch : c->getDecompile()) {
      if ((ch != '_') && (ch != ' ')) text.push_back(ch);
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

  // Verifies 'expr' is a Constant whose integer value is 'expected'.
  static void ExpectIntConstant(const hldb::Any *expr, uint64_t expected, std::string_view what) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(expr);
    ASSERT_NE(c, nullptr) << what << " should be a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(c, &v)) << "unparsable constant '" << c->getDecompile() << "'";
    EXPECT_EQ(v, expected) << what;
  }

  // Verifies member 'name' of tl_h2d_t is logic [top_pkg::<param>-1:0].
  static void ExpectScopedWidthMember(std::string_view name, std::string_view param) {
    const hldb::Struct *const s = getH2dStruct();
    ASSERT_NE(s, nullptr);
    const hldb::TypespecMember *const m = hldb::findByName<hldb::TypespecMember>(name, s->getMembers());
    ASSERT_NE(m, nullptr) << "member '" << name << "' not found";
    ASSERT_NE(m->getTypespec(), nullptr);
    const hldb::LogicTypespec *const lt = m->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr) << "'" << name << "' is declared 'logic [...]'";
    ASSERT_NE(lt->getRanges(), nullptr);
    ASSERT_EQ(lt->getRanges()->size(), 1u) << "'" << name << "'";
    const hldb::Operation *const left = lt->getRanges()->at(0)->getLeftExpr<hldb::Operation>();
    ASSERT_NE(left, nullptr) << "'" << name << "': the left bound 'top_pkg::" << param << "-1' is an expression";
    EXPECT_EQ(left->getOpType(), vpiSubOp);
    ASSERT_NE(left->getOperands(), nullptr);
    ASSERT_EQ(left->getOperands()->size(), 2u);
    const hldb::RefObj *const path = any_cast<hldb::RefObj>(left->getOperands()->at(0));
    ASSERT_NE(path, nullptr) << "'top_pkg::" << param << "' should be a package-scoped RefObj path";
    ASSERT_NE(path->getPathElems(), nullptr);
    ASSERT_EQ(path->getPathElems()->size(), 2u) << "the package, then the parameter";
    const hldb::RefObj *const scope = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
    ASSERT_NE(scope, nullptr);
    EXPECT_EQ(scope->getActual<hldb::Package>(), getPkg("top_pkg")) << "26.3: the scope prefix names top_pkg";
    const hldb::RefObj *const item = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->getName(), param);
    ASSERT_NE(getTopPkgParam(param), nullptr);
    EXPECT_EQ(item->getActual(), getTopPkgParam(param)) << "26.3: '" << param << "' is declared in top_pkg";
    const hldb::Constant *const one = any_cast<hldb::Constant>(left->getOperands()->at(1));
    ASSERT_NE(one, nullptr);
    EXPECT_EQ(one->getDecompile(), "1");
    const hldb::Constant *const right = lt->getRanges()->at(0)->getRightExpr<hldb::Constant>();
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(right->getDecompile(), "0");
  }
};

// ---------------------------------------------------------------------------
// package top_pkg;
// ---------------------------------------------------------------------------

TEST_F(PackStructVarTest, TopPkgHasElevenLocalIntParameters) {
  const hldb::Package *const pkg = getPkg("top_pkg");
  ASSERT_NE(pkg, nullptr) << "package 'top_pkg' not found";
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 11u);
  for (std::string_view name : {"TL_AW", "TL_DW", "TL_AIW", "TL_DIW", "TL_DUW", "TL_DBW", "TL_SZW", "FLASH_BANKS",
                                "FLASH_PAGES_PER_BANK", "FLASH_WORDS_PER_PAGE", "FLASH_BYTES_PER_WORD"}) {
    const hldb::Parameter *const p = getTopPkgParam(name);
    ASSERT_NE(p, nullptr) << "parameter '" << name << "' not found";
    EXPECT_TRUE(p->getLocalParam()) << "'" << name << "' is declared 'localparam'";
    ASSERT_NE(p->getTypespec(), nullptr);
    EXPECT_NE(p->getTypespec()->getActual<hldb::IntTypespec>(), nullptr) << "'" << name << "' is declared 'int'";
  }
}

TEST_F(PackStructVarTest, TopPkgLiteralParameters) {
  const hldb::Package *const pkg = getPkg("top_pkg");
  ASSERT_NE(pkg, nullptr);
  struct Literal final {
    std::string_view m_name;
    std::string_view m_text;
  };
  const Literal literals[] = {{"TL_AW", "32"},
                              {"TL_DW", "32"},
                              {"TL_AIW", "8"},
                              {"TL_DIW", "1"},
                              {"TL_DUW", "16"},
                              {"FLASH_BANKS", "2"},
                              {"FLASH_PAGES_PER_BANK", "256"},
                              {"FLASH_WORDS_PER_PAGE", "256"},
                              {"FLASH_BYTES_PER_WORD", "4"}};
  for (const Literal &lit : literals) {
    const hldb::ParamAssign *const pa = getParamAssign(pkg, lit.m_name);
    ASSERT_NE(pa, nullptr) << "no ParamAssign for '" << lit.m_name << "'";
    const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
    ASSERT_NE(rhs, nullptr) << "'" << lit.m_name << "' is assigned a literal";
    EXPECT_EQ(rhs->getDecompile(), lit.m_text) << "'" << lit.m_name << "'";
  }
}

TEST_F(PackStructVarTest, TlDbwIsTlDwShiftedRightByThree) {
  const hldb::ParamAssign *const pa = getParamAssign(getPkg("top_pkg"), "TL_DBW");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const shift = pa->getRhs<hldb::Operation>();
  ASSERT_NE(shift, nullptr) << "unreduced, '(TL_DW>>3)' is an Operation";
  EXPECT_EQ(shift->getOpType(), vpiRShiftOp) << "11.4.10: '>>' is the logical right shift";
  ASSERT_NE(shift->getOperands(), nullptr);
  ASSERT_EQ(shift->getOperands()->size(), 2u);
  const hldb::RefObj *const dw = any_cast<hldb::RefObj>(shift->getOperands()->at(0));
  ASSERT_NE(dw, nullptr);
  EXPECT_EQ(dw->getName(), "TL_DW");
  EXPECT_EQ(dw->getActual(), getTopPkgParam("TL_DW"));
  const hldb::Constant *const three = any_cast<hldb::Constant>(shift->getOperands()->at(1));
  ASSERT_NE(three, nullptr);
  EXPECT_EQ(three->getDecompile(), "3");
  if (m_design->getElaborated()) {
    const hldb::ParamAssign *const elab =
        getParamAssign(hldb::findByName<hldb::Package>("top_pkg", m_design->getTopPackages()), "TL_DBW");
    ASSERT_NE(elab, nullptr) << "no ParamAssign for 'TL_DBW' in the elaborated top_pkg";
    ExpectIntConstant(elab->getRhs(), 4, "6.20.2: TL_DBW = 32 >> 3 = 4");
  }
}

TEST_F(PackStructVarTest, TlSzwIsNestedClog2) {
  const hldb::ParamAssign *const pa = getParamAssign(getPkg("top_pkg"), "TL_SZW");
  ASSERT_NE(pa, nullptr);
  const hldb::SysFuncCall *const outer = pa->getRhs<hldb::SysFuncCall>();
  ASSERT_NE(outer, nullptr) << "unreduced, the value is a $clog2 call";
  EXPECT_EQ(outer->getName(), "$clog2");
  ASSERT_NE(outer->getArguments(), nullptr);
  ASSERT_EQ(outer->getArguments()->size(), 1u);
  const hldb::Operation *const add = any_cast<hldb::Operation>(outer->getArguments()->at(0));
  ASSERT_NE(add, nullptr) << "the argument '$clog2(TL_DBW)+1' is an Operation";
  EXPECT_EQ(add->getOpType(), vpiAddOp);
  ASSERT_NE(add->getOperands(), nullptr);
  ASSERT_EQ(add->getOperands()->size(), 2u);
  const hldb::SysFuncCall *const inner = any_cast<hldb::SysFuncCall>(add->getOperands()->at(0));
  ASSERT_NE(inner, nullptr);
  EXPECT_EQ(inner->getName(), "$clog2");
  ASSERT_NE(inner->getArguments(), nullptr);
  ASSERT_EQ(inner->getArguments()->size(), 1u);
  const hldb::RefObj *const dbw = any_cast<hldb::RefObj>(inner->getArguments()->at(0));
  ASSERT_NE(dbw, nullptr);
  EXPECT_EQ(dbw->getActual(), getTopPkgParam("TL_DBW"));
  if (m_design->getElaborated()) {
    const hldb::ParamAssign *const elab =
        getParamAssign(hldb::findByName<hldb::Package>("top_pkg", m_design->getTopPackages()), "TL_SZW");
    ASSERT_NE(elab, nullptr) << "no ParamAssign for 'TL_SZW' in the elaborated top_pkg";
    ExpectIntConstant(elab->getRhs(), 2, "20.8.1: $clog2($clog2(4) + 1) = $clog2(3) = 2");
  }
}

// ---------------------------------------------------------------------------
// package tlul_pkg;
// ---------------------------------------------------------------------------

TEST_F(PackStructVarTest, TlDOpEIsLogic2To0EnumWithTwoNames) {
  const hldb::Package *const pkg = getPkg("tlul_pkg");
  ASSERT_NE(pkg, nullptr) << "package 'tlul_pkg' not found";
  ASSERT_NE(pkg->getTypedefs(), nullptr);
  EXPECT_EQ(pkg->getTypedefs()->size(), 2u) << "'tl_d_op_e' and 'tl_h2d_t'";
  const hldb::Typedef *const td = getTlulTypedef("tl_d_op_e");
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::EnumTypespec *const et = td->getAlias()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(et, nullptr) << "6.19: 'tl_d_op_e' names an enumerated type";
  ASSERT_NE(et->getEnum(), nullptr);
  ASSERT_NE(et->getEnum()->getEnumConsts(), nullptr);
  ASSERT_EQ(et->getEnum()->getEnumConsts()->size(), 2u);
  const char *const names[] = {"AccessAck", "AccessAckData"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::EnumConst *const ec = et->getEnum()->getEnumConsts()->at(i);
    ASSERT_NE(ec, nullptr);
    EXPECT_EQ(ec->getName(), names[i]) << "enum name " << i << ", in source order";
    ExpectIntConstant(ec->getValue(), i, "5.7.1: '3'h " + std::to_string(i) + "' has the value " + std::to_string(i));
  }
}

TEST_F(PackStructVarTest, TlH2dTIsPackedStructOfTenMembers) {
  const hldb::Struct *const s = getH2dStruct();
  ASSERT_NE(s, nullptr) << "6.18: typedef 'tl_h2d_t' should alias a structure type";
  EXPECT_TRUE(s->getPacked()) << "7.2.1: declared 'struct packed'";
  ASSERT_NE(s->getMembers(), nullptr);
  ASSERT_EQ(s->getMembers()->size(), 10u);
  const char *const names[] = {"d_valid", "d_opcode", "d_param", "d_size",  "d_source",
                               "d_sink",  "d_data",   "d_user",  "d_error", "a_ready"};
  for (size_t i = 0; i < 10; ++i) {
    ASSERT_NE(s->getMembers()->at(i), nullptr);
    EXPECT_EQ(s->getMembers()->at(i)->getName(), names[i]) << "member " << i << ", in source order";
  }
}

TEST_F(PackStructVarTest, FixedWidthMembers) {
  const hldb::Struct *const s = getH2dStruct();
  ASSERT_NE(s, nullptr);
  for (std::string_view name : {"d_valid", "d_error", "a_ready"}) {
    const hldb::TypespecMember *const m = hldb::findByName<hldb::TypespecMember>(name, s->getMembers());
    ASSERT_NE(m, nullptr);
    ASSERT_NE(m->getTypespec(), nullptr);
    const hldb::LogicTypespec *const lt = m->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr) << "'" << name << "' is declared 'logic'";
    EXPECT_TRUE(lt->getRanges() == nullptr || lt->getRanges()->empty()) << "'" << name << "' is a single bit";
  }
  const hldb::TypespecMember *const opcode = hldb::findByName<hldb::TypespecMember>("d_opcode", s->getMembers());
  ASSERT_NE(opcode, nullptr);
  EXPECT_TRUE(namesTypedef(opcode->getTypespec(), getTlulTypedef("tl_d_op_e"))) << "'d_opcode' is typed by tl_d_op_e";
  const hldb::TypespecMember *const param = hldb::findByName<hldb::TypespecMember>("d_param", s->getMembers());
  ASSERT_NE(param, nullptr);
  ASSERT_NE(param->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = param->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "'d_param' is declared 'logic [2:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  const hldb::Constant *const left = lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "2");
}

TEST_F(PackStructVarTest, ScopedWidthMembersUseTopPkgParameters) {
  ExpectScopedWidthMember("d_size", "TL_SZW");
  ExpectScopedWidthMember("d_source", "TL_AIW");
  ExpectScopedWidthMember("d_sink", "TL_DIW");
  ExpectScopedWidthMember("d_data", "TL_DW");
  ExpectScopedWidthMember("d_user", "TL_DUW");
}

// ---------------------------------------------------------------------------
// module flash_ctrl (); tlul_pkg::tl_h2d_t tl_fifo_h2d [2];
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (null port
// missing) in the file header.
TEST_F(PackStructVarTest, FlashCtrlHasOneNullPort) {
  const hldb::Module *const m = getFlashCtrl();
  ASSERT_NE(m, nullptr) << "module 'flash_ctrl' not found";
  ASSERT_NE(m->getPorts(), nullptr) << "37.14 detail 10: '()' declares a null port";
  ASSERT_EQ(m->getPorts()->size(), 1u) << "37.14 detail 10: '()' declares exactly one null port";
  const hldb::Port *const port = m->getPorts()->at(0);
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), "") << "37.14 detail 8: a null port has no name";
  EXPECT_EQ(port->getPortIndex(), 0) << "37.14 detail 9: the first port has index 0";
  EXPECT_EQ(port->getLowConn(), nullptr) << "37.14 detail 10: a null port has no low connection";
}

TEST_F(PackStructVarTest, TlFifoH2dIsUnpackedArrayOfScopedTlH2dT) {
  const hldb::Module *const m = getFlashCtrl();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getVariables(), nullptr);
  ASSERT_EQ(m->getVariables()->size(), 1u) << "'tl_fifo_h2d' is the module's only variable";
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("tl_fifo_h2d", m->getVariables());
  ASSERT_NE(v, nullptr);
  ASSERT_NE(v->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = v->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr) << "'[2]' after the name declares an unpacked array";
  EXPECT_FALSE(at->getPacked()) << "7.4.2: a dimension after the name is unpacked";
  EXPECT_TRUE(namesTypedef(at->getElemTypespec(), getTlulTypedef("tl_h2d_t")))
      << "26.3: the element type is tlul_pkg's tl_h2d_t";
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (unpacked
// [size] dimension malformed) in the file header.
TEST_F(PackStructVarTest, TlFifoH2dIsTwoElementUnpackedArray) {
  const hldb::Module *const m = getFlashCtrl();
  ASSERT_NE(m, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("tl_fifo_h2d", m->getVariables());
  ASSERT_NE(v, nullptr);
  ASSERT_NE(v->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = v->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  const hldb::Range *const r = at->getRange();
  ASSERT_NE(r, nullptr);
  const hldb::Constant *const left = r->getLeftExpr<hldb::Constant>();
  const hldb::Constant *const right = r->getRightExpr<hldb::Constant>();
  int64_t count = r->getSize();
  if ((left != nullptr) && (right != nullptr)) {
    uint64_t l = 0;
    uint64_t rr = 0;
    ASSERT_TRUE(getIntValue(left, &l) && getIntValue(right, &rr));
    count = std::llabs(static_cast<int64_t>(l) - static_cast<int64_t>(rr)) + 1;
  }
  EXPECT_EQ(count, 2) << "7.4.2: '[2]' means '[0:1]', two elements";
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(PackStructVarTest, FlashCtrlIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: 'flash_ctrl' appears in no instantiation";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getDefName(), "flash_ctrl");
  }
}

TEST_F(PackStructVarTest, NoFatalSyntaxOrErrorDiagnostics) {
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
