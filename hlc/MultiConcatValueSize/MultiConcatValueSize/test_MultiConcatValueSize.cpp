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

// Tests for dut.sv (tags: MultiConcatValueSize)
//   package prim_pad_wrapper_pkg;
//      typedef enum logic [2:0] { A = 3'h0, B = 3'h1 } pad_type_e;
//   endpackage
//   package pinmux_pkg;
//      import prim_pad_wrapper_pkg::*;
//      parameter int NDioPads = 1;
//      typedef struct packed {
//         pad_type_e [NDioPads-1:0] dio_pad_type;
//      } target_cfg_t;
//      parameter target_cfg_t DefaultTargetCfg = '{
//         dio_pad_type: {NDioPads{A}}
//      };
//   endpackage
//   module prim_generic_pad_attr(output int a);
//      import prim_pad_wrapper_pkg::*;
//      parameter pad_type_e PadType = A;
//      if (PadType == B) begin : gen_assign assign a = 1; end
//   endmodule
//   module prim_pad_attr(output int b); ... gen_generic: instance of
//      prim_generic_pad_attr #(.PadType(PadType)) u_impl_generic(.a(b));
//   module sub_top(output int c);
//      import pinmux_pkg::*;
//      parameter target_cfg_t TargetCfg = DefaultTargetCfg;
//      for (genvar k = 0; k < NDioPads; k++) begin : gen_dio_attr
//         prim_pad_attr #(.PadType(TargetCfg.dio_pad_type[k])) u_prim_pad_attr(.b(c));
//      end
//   module top(output int o);
//      localparam pinmux_pkg::target_cfg_t PinmuxTargetCfg = '{
//         dio_pad_type: {pinmux_pkg::NDioPads{prim_pad_wrapper_pkg::B}}
//      };
//      sub_top #(.TargetCfg(PinmuxTargetCfg)) u_sub(.c(o));
//
// The construct under test is the replication (multiple concatenation)
// '{N{enum_const}}' used as the value of a packed-array struct member inside
// an assignment pattern, whose multiplier is a package parameter and whose
// size must equal the member width NDioPads * $bits(pad_type_e).
//
// What is checked (IEEE 1800-2023):
//   - 6.19: 'pad_type_e' is an enum with base type logic [2:0] and constants
//     A (3'h0) and B (3'h1), each sized 3 bits.
//   - 6.20.1: "All param_assignments appearing within a ... package ...
//     shall become localparam declarations" -> 'NDioPads' and
//     'DefaultTargetCfg' are localparams; 'NDioPads' is typed 'int' with
//     value 1.
//   - 7.2.1 / 7.4.1: 'target_cfg_t' is a packed struct with one member
//     'dio_pad_type', a packed array [NDioPads-1:0] of 'pad_type_e'; the
//     left bound is the expression 'NDioPads-1' (vpiSubOp) referencing the
//     package parameter.
//   - 10.9.2 / 11.4.12.1: 'DefaultTargetCfg' is an assignment pattern with
//     one member:value item; the key binds to struct member 'dio_pad_type'
//     and the value is a replication (vpiMultiConcatOp) whose multiplier
//     binds to 'NDioPads' and whose replicated operand is a concatenation
//     (vpiConcatOp) of the single enum constant 'A' (bound through the
//     wildcard import, 26.3).
//   - Same in 'top', with package-scoped names (26.3 'pkg::name'):
//     the multiplier binds to pinmux_pkg's 'NDioPads', the replicated item
//     binds to prim_pad_wrapper_pkg's 'B', and the type
//     'pinmux_pkg::target_cfg_t' resolves to pinmux_pkg's typedef.
//   - 23.2.2.3: 'output int a/b/c/o' have an explicit data type, so each
//     port is a variable (not a net) of direction output.
//   - 6.20.2 / 27.5: in 'prim_generic_pad_attr' 'PadType' is a non-local
//     parameter of type 'pad_type_e' defaulting to 'A'; the conditional
//     generate tests 'PadType == B' (vpiEqOp) and its named block
//     'gen_assign' contains one continuous assignment to 'a'.
//   - 23.10: parameter overrides by name in the instantiations bind their
//     formal and actual names: '.PadType(TargetCfg.dio_pad_type[k])' in
//     'sub_top' (member + select path rooted at parameter 'TargetCfg', index
//     bound to genvar 'k') and '.TargetCfg(PinmuxTargetCfg)' in 'top'.
//   - 27.4: 'sub_top' has a loop generate with condition 'k < NDioPads'.
//   - no COMP_FAILED_TO_BIND for any of the names above.
//
// What is NOT checked and why:
//   - the evaluated value / width of the replication (3 bits for N=1), the
//     resulting per-instance PadType, or which 'gen_assign' block exists:
//     this flow does not elaborate (no parameter propagation through the
//     hierarchy), so no evaluated values exist to assert against.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/enum.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/gen_for.h>
#include <hldb/gen_if.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/tagged_pattern.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class MultiConcatValueSizeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "MultiConcatValueSize.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg(std::string_view name) {
    return hldb::findByName<hldb::Package>(name, m_design->getAllPackages());
  }

  static const hldb::Module *getMod(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Parameter *findParam(const hldb::Scope *s, std::string_view name) {
    if (s == nullptr || s->getParameters() == nullptr) return nullptr;
    for (const hldb::Any *const p : *s->getParameters()) {
      const hldb::Parameter *const param = any_cast<hldb::Parameter>(p);
      if (param != nullptr && param->getName() == name) return param;
    }
    return nullptr;
  }

  static const hldb::ParamAssign *findParamAssign(const hldb::Scope *s, std::string_view name) {
    return (s == nullptr) ? nullptr : hldb::findByName<hldb::ParamAssign>(name, s->getParamAssigns());
  }

  static const hldb::Enum *getPadEnum() {
    const hldb::Package *const p = getPkg("prim_pad_wrapper_pkg");
    if (p == nullptr) return nullptr;
    const hldb::Typedef *const td = hldb::findByName<hldb::Typedef>("pad_type_e", p->getTypedefs());
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::EnumTypespec *const ets = any_cast<hldb::EnumTypespec>(td->getAlias()->getActual());
    return (ets == nullptr) ? nullptr : ets->getEnum();
  }

  static const hldb::EnumConst *getEnumConst(std::string_view name) {
    const hldb::Enum *const e = getPadEnum();
    return (e == nullptr) ? nullptr : hldb::findByName<hldb::EnumConst>(name, e->getEnumConsts());
  }

  static const hldb::Typedef *getCfgTypedef() {
    const hldb::Package *const p = getPkg("pinmux_pkg");
    return (p == nullptr) ? nullptr : hldb::findByName<hldb::Typedef>("target_cfg_t", p->getTypedefs());
  }

  static const hldb::Struct *getCfgStruct() {
    const hldb::Typedef *const td = getCfgTypedef();
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::StructTypespec *const sts = any_cast<hldb::StructTypespec>(td->getAlias()->getActual());
    return (sts == nullptr) ? nullptr : sts->getStruct();
  }

  static const hldb::TypespecMember *getDioMember() {
    const hldb::Struct *const st = getCfgStruct();
    return (st == nullptr) ? nullptr : hldb::findByName<hldb::TypespecMember>("dio_pad_type", st->getMembers());
  }

  static const hldb::Parameter *getNDioPads() { return findParam(getPkg("pinmux_pkg"), "NDioPads"); }

  // Checks "'{ dio_pad_type: {N{X}} }" where N must bind to NDioPads and X to
  // the enum constant 'item'.
  static void checkPattern(const hldb::ParamAssign *pa, std::string_view item) {
    ASSERT_NE(pa, nullptr);
    const hldb::Operation *const pat = any_cast<hldb::Operation>(pa->getRhs());
    ASSERT_NE(pat, nullptr) << "RHS is an assignment pattern";
    EXPECT_EQ(pat->getOpType(), vpiAssignmentPatternOp);
    ASSERT_NE(pat->getOperands(), nullptr);
    ASSERT_EQ(pat->getOperands()->size(), 1u) << "one member:value item";
    const hldb::TaggedPattern *const tp = any_cast<hldb::TaggedPattern>(pat->getOperands()->at(0));
    ASSERT_NE(tp, nullptr) << "10.9.2: 'member: value' item";
    const hldb::RefObj *const tag = any_cast<hldb::RefObj>(tp->getTag());
    ASSERT_NE(tag, nullptr);
    EXPECT_EQ(tag->getName(), "dio_pad_type");
    ASSERT_NE(tag->getActual(), nullptr) << "member key must bind to the struct member";
    ASSERT_NE(getDioMember(), nullptr);
    EXPECT_EQ(tag->getActual(), getDioMember());

    const hldb::Operation *const rep = any_cast<hldb::Operation>(tp->getPattern());
    ASSERT_NE(rep, nullptr);
    EXPECT_EQ(rep->getOpType(), vpiMultiConcatOp) << "11.4.12.1: replication";
    ASSERT_NE(rep->getOperands(), nullptr);
    ASSERT_EQ(rep->getOperands()->size(), 2u) << "multiplier and replicated concatenation";

    const hldb::RefObj *const mult = any_cast<hldb::RefObj>(rep->getOperands()->at(0));
    ASSERT_NE(mult, nullptr);
    ASSERT_NE(mult->getActual(), nullptr) << "multiplier must bind";
    ASSERT_NE(getNDioPads(), nullptr);
    EXPECT_EQ(mult->getActual(), getNDioPads());

    const hldb::Operation *const cat = any_cast<hldb::Operation>(rep->getOperands()->at(1));
    ASSERT_NE(cat, nullptr);
    EXPECT_EQ(cat->getOpType(), vpiConcatOp);
    ASSERT_NE(cat->getOperands(), nullptr);
    ASSERT_EQ(cat->getOperands()->size(), 1u);
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(cat->getOperands()->at(0));
    ASSERT_NE(ref, nullptr);
    ASSERT_NE(ref->getActual(), nullptr) << "replicated item '" << item << "' must bind";
    ASSERT_NE(getEnumConst(item), nullptr);
    EXPECT_EQ(ref->getActual(), getEnumConst(item));
  }

  static void checkOutputIntVariablePort(std::string_view mod, std::string_view name) {
    const hldb::Module *const m = getMod(mod);
    ASSERT_NE(m, nullptr) << mod;
    const hldb::Port *const p = hldb::findByName<hldb::Port>(name, m->getPorts());
    ASSERT_NE(p, nullptr) << mod << "." << name;
    EXPECT_EQ(p->getDirection(), vpiOutput);
    const hldb::Variable *const v = hldb::findByName<hldb::Variable>(name, m->getVariables());
    ASSERT_NE(v, nullptr) << "23.2.2.3: output port with explicit data type 'int' is a variable";
    ASSERT_NE(v->getTypespec(), nullptr);
    ASSERT_NE(v->getTypespec()->getActual(), nullptr);
    EXPECT_EQ(v->getTypespec()->getActual()->getAnyType(), hldb::AnyType::IntTypespec);
    const hldb::RefObj *const low = any_cast<hldb::RefObj>(p->getLowConn());
    ASSERT_NE(low, nullptr);
    EXPECT_EQ(low->getActual(), v);
  }
};

// ===========================================================================
// 6.19: enum pad_type_e
// ===========================================================================

TEST_F(MultiConcatValueSizeTest, PadTypeEnumHasLogic3BaseAndTwoConsts) {
  const hldb::Enum *const e = getPadEnum();
  ASSERT_NE(e, nullptr) << "typedef enum 'pad_type_e' not found";
  ASSERT_NE(e->getBaseTypespec(), nullptr);
  const hldb::LogicTypespec *const base = any_cast<hldb::LogicTypespec>(e->getBaseTypespec()->getActual());
  ASSERT_NE(base, nullptr) << "explicit base type logic [2:0]";
  ASSERT_NE(base->getRanges(), nullptr);
  EXPECT_EQ(base->getRanges()->size(), 1u);
  ASSERT_NE(e->getEnumConsts(), nullptr);
  ASSERT_EQ(e->getEnumConsts()->size(), 2u);
  EXPECT_EQ(e->getEnumConsts()->at(0)->getName(), "A");
  EXPECT_EQ(e->getEnumConsts()->at(1)->getName(), "B");
}

TEST_F(MultiConcatValueSizeTest, EnumConstValuesAreSized3Bit) {
  for (std::string_view name : {"A", "B"}) {
    const hldb::EnumConst *const c = getEnumConst(name);
    ASSERT_NE(c, nullptr) << name;
    const hldb::Constant *const v = any_cast<hldb::Constant>(c->getValue());
    ASSERT_NE(v, nullptr) << name;
    EXPECT_EQ(v->getSize(), 3) << "5.7.1: 3'hN is 3 bits wide";
    EXPECT_EQ(v->getConstType(), vpiHexConst);
  }
  const hldb::EnumConst *const a = getEnumConst("A");
  const hldb::EnumConst *const b = getEnumConst("B");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  ASSERT_NE(any_cast<hldb::Constant>(a->getValue()), nullptr);
  ASSERT_NE(any_cast<hldb::Constant>(b->getValue()), nullptr);
  EXPECT_EQ(any_cast<hldb::Constant>(a->getValue())->getDecompile(), "3'h0");
  EXPECT_EQ(any_cast<hldb::Constant>(b->getValue())->getDecompile(), "3'h1");
}

// ===========================================================================
// 6.20.1: package parameters become localparams
// ===========================================================================

TEST_F(MultiConcatValueSizeTest, NDioPadsIsLocalIntParamEqualTo1) {
  const hldb::Parameter *const n = getNDioPads();
  ASSERT_NE(n, nullptr);
  EXPECT_TRUE(n->getLocalParam()) << "6.20.1: param_assignments in a package become localparam";
  ASSERT_NE(n->getTypespec(), nullptr);
  ASSERT_NE(n->getTypespec()->getActual(), nullptr);
  EXPECT_EQ(n->getTypespec()->getActual()->getAnyType(), hldb::AnyType::IntTypespec);
  const hldb::ParamAssign *const pa = findParamAssign(getPkg("pinmux_pkg"), "NDioPads");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const v = any_cast<hldb::Constant>(pa->getRhs());
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getDecompile(), "1");
}

TEST_F(MultiConcatValueSizeTest, DefaultTargetCfgIsLocalParamOfTargetCfgT) {
  const hldb::Parameter *const p = findParam(getPkg("pinmux_pkg"), "DefaultTargetCfg");
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(p->getLocalParam()) << "6.20.1";
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::TypedefTypespec *const tts = any_cast<hldb::TypedefTypespec>(p->getTypespec()->getActual());
  ASSERT_NE(tts, nullptr);
  EXPECT_EQ(tts->getTypedef(), getCfgTypedef());
}

// ===========================================================================
// 7.2.1 / 7.4.1: target_cfg_t
// ===========================================================================

TEST_F(MultiConcatValueSizeTest, TargetCfgIsPackedStructOfPackedEnumArray) {
  const hldb::Struct *const st = getCfgStruct();
  ASSERT_NE(st, nullptr);
  EXPECT_TRUE(st->getPacked());
  ASSERT_NE(st->getMembers(), nullptr);
  ASSERT_EQ(st->getMembers()->size(), 1u);
  const hldb::TypespecMember *const m = getDioMember();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = any_cast<hldb::ArrayTypespec>(m->getTypespec()->getActual());
  ASSERT_NE(at, nullptr) << "'pad_type_e [NDioPads-1:0]' is a packed array";
  EXPECT_TRUE(at->getPacked());
  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::TypedefTypespec *const elem = any_cast<hldb::TypedefTypespec>(at->getElemTypespec()->getActual());
  ASSERT_NE(elem, nullptr);
  EXPECT_EQ(elem->getName(), "pad_type_e");

  ASSERT_NE(at->getRange(), nullptr);
  const hldb::Operation *const left = any_cast<hldb::Operation>(at->getRange()->getLeftExpr());
  ASSERT_NE(left, nullptr) << "left bound is 'NDioPads-1'";
  EXPECT_EQ(left->getOpType(), vpiSubOp);
  ASSERT_NE(left->getOperands(), nullptr);
  ASSERT_EQ(left->getOperands()->size(), 2u);
  const hldb::RefObj *const n = any_cast<hldb::RefObj>(left->getOperands()->at(0));
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(n->getActual(), getNDioPads());
  const hldb::Constant *const right = any_cast<hldb::Constant>(at->getRange()->getRightExpr());
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
}

// ===========================================================================
// 10.9.2 / 11.4.12.1: '{dio_pad_type: {N{X}}}
// ===========================================================================

TEST_F(MultiConcatValueSizeTest, DefaultTargetCfgIsPatternWithReplicationOfA) {
  checkPattern(findParamAssign(getPkg("pinmux_pkg"), "DefaultTargetCfg"), "A");
}

TEST_F(MultiConcatValueSizeTest, PinmuxTargetCfgIsPatternWithReplicationOfB) {
  checkPattern(findParamAssign(getMod("top"), "PinmuxTargetCfg"), "B");
}

TEST_F(MultiConcatValueSizeTest, PinmuxTargetCfgIsLocalParamOfPkgScopedType) {
  const hldb::Parameter *const p = findParam(getMod("top"), "PinmuxTargetCfg");
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(p->getLocalParam());
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::TypedefTypespec *const tts = any_cast<hldb::TypedefTypespec>(p->getTypespec()->getActual());
  ASSERT_NE(tts, nullptr) << "'pinmux_pkg::target_cfg_t' must resolve";
  EXPECT_EQ(tts->getTypedef(), getCfgTypedef());
}

// ===========================================================================
// 23.2.2.3: output int ports are variables
// ===========================================================================

TEST_F(MultiConcatValueSizeTest, OutputIntPortsAreVariables) {
  checkOutputIntVariablePort("prim_generic_pad_attr", "a");
  checkOutputIntVariablePort("prim_pad_attr", "b");
  checkOutputIntVariablePort("sub_top", "c");
  checkOutputIntVariablePort("top", "o");
}

// ===========================================================================
// prim_generic_pad_attr: parameter + conditional generate
// ===========================================================================

TEST_F(MultiConcatValueSizeTest, PadTypeParameterDefaultsToA) {
  const hldb::Module *const m = getMod("prim_generic_pad_attr");
  ASSERT_NE(m, nullptr);
  const hldb::Parameter *const p = findParam(m, "PadType");
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->getLocalParam()) << "module without parameter_port_list: 'parameter' is overridable";
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::TypedefTypespec *const tts = any_cast<hldb::TypedefTypespec>(p->getTypespec()->getActual());
  ASSERT_NE(tts, nullptr);
  EXPECT_EQ(tts->getName(), "pad_type_e");
  const hldb::ParamAssign *const pa = findParamAssign(m, "PadType");
  ASSERT_NE(pa, nullptr);
  const hldb::RefObj *const rhs = any_cast<hldb::RefObj>(pa->getRhs());
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getActual(), nullptr);
  EXPECT_EQ(rhs->getActual(), getEnumConst("A")) << "26.3: 'A' visible through the wildcard import";
}

TEST_F(MultiConcatValueSizeTest, GenIfTestsPadTypeEqualsB) {
  const hldb::Module *const m = getMod("prim_generic_pad_attr");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getGenStmts(), nullptr);
  ASSERT_EQ(m->getGenStmts()->size(), 1u);
  const hldb::GenIf *const gi = any_cast<hldb::GenIf>(m->getGenStmts()->at(0));
  ASSERT_NE(gi, nullptr) << "27.5: if-generate without else";
  const hldb::Operation *const cond = any_cast<hldb::Operation>(gi->getCondition());
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiEqOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  const hldb::RefObj *const rhs = any_cast<hldb::RefObj>(cond->getOperands()->at(1));
  ASSERT_NE(lhs, nullptr);
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(lhs->getActual(), findParam(m, "PadType"));
  EXPECT_EQ(rhs->getActual(), getEnumConst("B"));

  const hldb::Begin *const blk = any_cast<hldb::Begin>(gi->getStmt());
  ASSERT_NE(blk, nullptr);
  EXPECT_EQ(blk->getName(), "gen_assign");
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 1u);
  const hldb::ContAssign *const ca = any_cast<hldb::ContAssign>(blk->getStmts()->at(0));
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const a = any_cast<hldb::RefObj>(ca->getLhs());
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getActual(), hldb::findByName<hldb::Variable>("a", m->getVariables()));
}

// ===========================================================================
// sub_top: default from package, loop generate, by-name override
// ===========================================================================

TEST_F(MultiConcatValueSizeTest, SubTopTargetCfgDefaultsToPackageParam) {
  const hldb::Module *const m = getMod("sub_top");
  ASSERT_NE(m, nullptr);
  const hldb::Parameter *const p = findParam(m, "TargetCfg");
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->getLocalParam());
  const hldb::ParamAssign *const pa = findParamAssign(m, "TargetCfg");
  ASSERT_NE(pa, nullptr);
  const hldb::RefObj *const rhs = any_cast<hldb::RefObj>(pa->getRhs());
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getActual(), nullptr);
  EXPECT_EQ(rhs->getActual(), findParam(getPkg("pinmux_pkg"), "DefaultTargetCfg"));
}

TEST_F(MultiConcatValueSizeTest, SubTopLoopOverrideBindsMemberSelect) {
  const hldb::Module *const m = getMod("sub_top");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getGenStmts(), nullptr);
  ASSERT_EQ(m->getGenStmts()->size(), 1u);
  const hldb::GenFor *const gf = any_cast<hldb::GenFor>(m->getGenStmts()->at(0));
  ASSERT_NE(gf, nullptr);
  const hldb::Variable *const k = hldb::findByName<hldb::Variable>("k", gf->getVariables());
  ASSERT_NE(k, nullptr);
  const hldb::Operation *const cond = any_cast<hldb::Operation>(gf->getCondition());
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiLtOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const n = any_cast<hldb::RefObj>(cond->getOperands()->at(1));
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(n->getActual(), getNDioPads()) << "26.3: 'NDioPads' via import pinmux_pkg::*";

  const hldb::Begin *const blk = any_cast<hldb::Begin>(gf->getStmt());
  ASSERT_NE(blk, nullptr);
  EXPECT_EQ(blk->getName(), "gen_dio_attr");
  ASSERT_NE(blk->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *mts = nullptr;
  for (const hldb::Typespec *const ts : *blk->getTypespecs()) {
    if (const hldb::ModuleTypespec *const t = any_cast<hldb::ModuleTypespec>(ts)) mts = t;
  }
  ASSERT_NE(mts, nullptr) << "instantiation of prim_pad_attr";
  EXPECT_EQ(mts->getModule(), getMod("prim_pad_attr"));
  ASSERT_NE(mts->getParamAssigns(), nullptr);
  ASSERT_EQ(mts->getParamAssigns()->size(), 1u);
  const hldb::ParamAssign *const pa = mts->getParamAssigns()->at(0);
  EXPECT_TRUE(pa->getConnByName());
  const hldb::RefObj *const formal = any_cast<hldb::RefObj>(pa->getLhs());
  ASSERT_NE(formal, nullptr);
  EXPECT_EQ(formal->getActual(), findParam(getMod("prim_pad_attr"), "PadType"));

  const hldb::RefObj *const actual = any_cast<hldb::RefObj>(pa->getRhs());
  ASSERT_NE(actual, nullptr) << "'TargetCfg.dio_pad_type[k]'";
  ASSERT_NE(actual->getPathElems(), nullptr);
  ASSERT_EQ(actual->getPathElems()->size(), 2u);
  const hldb::RefObj *const head = any_cast<hldb::RefObj>(actual->getPathElems()->at(0));
  ASSERT_NE(head, nullptr);
  EXPECT_EQ(head->getActual(), findParam(m, "TargetCfg"));
  const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(actual->getPathElems()->at(1));
  ASSERT_NE(sel, nullptr);
  const hldb::RefObj *const member = any_cast<hldb::RefObj>(sel->getPrefix());
  ASSERT_NE(member, nullptr);
  EXPECT_EQ(member->getActual(), getDioMember());
  const hldb::RefObj *const idx = any_cast<hldb::RefObj>(sel->getIndex());
  ASSERT_NE(idx, nullptr);
  EXPECT_EQ(idx->getActual(), k);
}

// ===========================================================================
// top: sub_top #(.TargetCfg(PinmuxTargetCfg)) u_sub(.c(o));
// ===========================================================================

TEST_F(MultiConcatValueSizeTest, TopOverridesTargetCfgByName) {
  const hldb::Module *const top = getMod("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  ASSERT_EQ(top->getRefInstances()->size(), 1u);
  EXPECT_EQ(top->getRefInstances()->at(0)->getName(), "u_sub");

  ASSERT_NE(top->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *mts = nullptr;
  for (const hldb::Typespec *const ts : *top->getTypespecs()) {
    if (const hldb::ModuleTypespec *const t = any_cast<hldb::ModuleTypespec>(ts)) mts = t;
  }
  ASSERT_NE(mts, nullptr);
  EXPECT_EQ(mts->getModule(), getMod("sub_top"));
  ASSERT_NE(mts->getParamAssigns(), nullptr);
  ASSERT_EQ(mts->getParamAssigns()->size(), 1u);
  const hldb::ParamAssign *const pa = mts->getParamAssigns()->at(0);
  EXPECT_TRUE(pa->getConnByName());
  const hldb::RefObj *const formal = any_cast<hldb::RefObj>(pa->getLhs());
  const hldb::RefObj *const actual = any_cast<hldb::RefObj>(pa->getRhs());
  ASSERT_NE(formal, nullptr);
  ASSERT_NE(actual, nullptr);
  EXPECT_EQ(formal->getActual(), findParam(getMod("sub_top"), "TargetCfg"));
  EXPECT_EQ(actual->getActual(), findParam(top, "PinmuxTargetCfg"));
}

TEST_F(MultiConcatValueSizeTest, NoBindingErrors) {
  for (std::string_view name : {"A", "B", "NDioPads", "dio_pad_type", "DefaultTargetCfg", "TargetCfg",
                                "PinmuxTargetCfg", "PadType", "pad_type_e", "target_cfg_t", "k"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, name), nullptr) << name;
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
