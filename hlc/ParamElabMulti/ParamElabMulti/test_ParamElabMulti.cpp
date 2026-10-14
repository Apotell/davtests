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

// Validates the HLDB model built for tests/ParamElabMulti/dut.sv:
//
//   package prim_pad_wrapper_pkg;
//      typedef enum  logic [2:0] {
//         A = 3'h0,
//         B = 3'h1
//      } pad_type_e;
//   endpackage : prim_pad_wrapper_pkg
//
//   package pinmux_pkg;
//      import prim_pad_wrapper_pkg::*;
//
//      parameter int NDioPads = 1;
//
//      typedef struct packed {
//         pad_type_e [NDioPads-1:0] dio_pad_type;
//      } target_cfg_t;
//
//      parameter target_cfg_t DefaultTargetCfg = '{
//         dio_pad_type: {NDioPads{A}}
//      };
//   endpackage : pinmux_pkg
//
//   module prim_generic_pad_attr();
//      import prim_pad_wrapper_pkg::*;
//      parameter pad_type_e PadType = A;
//      if (PadType == B) begin : gen_assign
//         assign a = 1;
//      end
//   endmodule : prim_generic_pad_attr
//
//   module prim_pad_attr();
//      import prim_pad_wrapper_pkg::*;
//      parameter pad_type_e PadType = A;
//
//      if (1) begin : gen_generic
//         prim_generic_pad_attr #(
//            .PadType(PadType)
//         ) u_impl_generic();
//      end
//   endmodule
//
//   module sub_top();
//      import pinmux_pkg::*;
//      parameter target_cfg_t TargetCfg = DefaultTargetCfg;
//
//     for (genvar k = 0; k < NDioPads; k++) begin : gen_dio_attr
//        prim_pad_attr #(
//           .PadType(TargetCfg.dio_pad_type[k])
//        ) u_prim_pad_attr();
//     end
//   endmodule
//
//   module top();
//      localparam pinmux_pkg::target_cfg_t PinmuxTargetCfg = '{
//         dio_pad_type: {pinmux_pkg::NDioPads{prim_pad_wrapper_pkg::B}}
//      };
//
//      sub_top #(
//         .TargetCfg(PinmuxTargetCfg)
//      ) u_sub();
//
//   endmodule
//
// The point of the fixture is a parameter value carried through several
// levels of instances and generate blocks (IEEE 1800-2023 23.10.2.2, 27.4,
// 27.5): top overrides sub_top's struct parameter TargetCfg; sub_top passes
// one element of it, per loop iteration, to prim_pad_attr's PadType;
// prim_pad_attr passes PadType on to prim_generic_pad_attr; and there it
// selects the if-generate block gen_assign. The regression this file exists
// to catch is HLC elaborating a lower level with a default value (A) instead
// of the value pushed down from top (B), which would drop gen_assign.
//
// Values: NDioPads = 1, so the loop runs once, for k = 0. PinmuxTargetCfg's
// only member is {1{B}}, so TargetCfg = 3'h1 = 1 in u_sub, and PadType = B =
// 1 in both u_prim_pad_attr and u_impl_generic. PadType == B holds, so
// gen_assign is instantiated.
//
// What is checked, and why:
//   Package prim_pad_wrapper_pkg
//     - exactly 1 Typedef, pad_type_e, an enum (6.19) with base type logic
//       [2:0] and exactly 2 names, A = 3'h0 and B = 3'h1
//   Package pinmux_pkg
//     - exactly 2 parameters, NDioPads and DefaultTargetCfg, both local
//       (6.20.4); NDioPads is "1"
//     - exactly 1 Typedef, target_cfg_t, a packed struct (7.2.1) with the one
//       member dio_pad_type
//     - DefaultTargetCfg's unreduced value is Operation vpiAssignmentPatternOp
//       (10.9.2) with one item, keyed dio_pad_type, whose value is Operation
//       vpiMultiConcatOp (11.4.12.1) with the multiplier RefObj 'NDioPads',
//       bound to the Parameter, and the element RefObj 'A', bound through the
//       import (26.3) to prim_pad_wrapper_pkg's EnumConst
//   Modules prim_generic_pad_attr and prim_pad_attr
//     - each has exactly 1 parameter, PadType, of type pad_type_e, declared
//       in a body with no parameter port list so overridable (6.20.1), with
//       the default RefObj 'A' bound to the EnumConst
//     - prim_generic_pad_attr has exactly 1 generate construct, an
//       if-generate (27.5) whose condition is Operation vpiEqOp (11.4.5) over
//       RefObj 'PadType', bound to the module's Parameter, and RefObj 'B',
//       bound to the EnumConst; its block is named gen_assign and holds a
//       continuous assignment to 'a'
//     - prim_pad_attr has exactly 1 generate construct, an if-generate whose
//       condition is the Constant "1" and whose block, gen_generic, holds the
//       instance u_impl_generic, overriding PadType with RefObj 'PadType'
//       bound to prim_pad_attr's Parameter
//   Module sub_top
//     - exactly 1 parameter, TargetCfg, overridable (6.20.1), with the
//       default RefObj 'DefaultTargetCfg' bound to pinmux_pkg's Parameter
//     - exactly 1 generate construct, a loop generate (27.4) whose condition
//       is Operation vpiLtOp over RefObj 'k' and RefObj 'NDioPads', bound to
//       pinmux_pkg's Parameter; its block, gen_dio_attr, holds the instance
//       u_prim_pad_attr, which overrides PadType
//   Module top
//     - exactly 1 parameter, PinmuxTargetCfg, a localparam
//     - exactly 1 instance, u_sub, overriding TargetCfg with RefObj
//       'PinmuxTargetCfg' bound to top's Parameter
//   Every '()' port list declares one null port (37.14 detail 10, "module
//   M();")
//   Elaboration
//     - every other module is instantiated, inside generate blocks or not, so
//       top is the only top-level instance (23.3.1)
//     - top.u_sub has TargetCfg = 1
//     - top.u_sub.gen_dio_attr has exactly 1 instance, for k = 0, holding
//       u_prim_pad_attr with PadType = 1
//     - u_prim_pad_attr.gen_generic has exactly 1 instance, holding
//       u_impl_generic with PadType = 1
//     - u_impl_generic.gen_assign has exactly 1 instance, holding exactly 1
//       continuous assignment whose target 'a' is an implicit net (6.10) of
//       the default net type, wire (22.8), declared in that generate block
//   Diagnostics
//     - there is no syntax error: zero syntax and zero fatal diagnostics
//
// Reduction and elaboration: the parameter values and the generate blocks
// are checked only when the design is elaborated, on
// Design::getTopModules(); the unreduced values, conditions and overrides
// are asserted on the definitions in every run.
//
// KNOWN COMPILER BUG (null port missing), not a defect in this test: HLC
// models an empty port list with no port at all, although 37.14 detail 10
// names "module M();" as declaring a null port.
// EmptyPortListsDeclareOneNullPort is expected to fail until HLC is fixed;
// it is intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - The error count. dio_pad_type is a packed array whose elements are of
//     enum type, and both patterns assign it a replication, which is a plain
//     vector (11.4.12). 6.19.3 requires a cast for assignment to "an enum
//     variable" and 6.22.3 makes "all integral types" assignment compatible;
//     the standard does not say which rule governs a packed array of enum,
//     so whether a diagnostic is due is left open.
//   - The shape of 'TargetCfg.dio_pad_type[k]' in the unelaborated model is a
//     model convention; its value is asserted through the elaborated
//     PadType.
//   - How HLC records the type of dio_pad_type, and the parameter override of
//     an unelaborated instance, are model conventions; the override is read
//     from the instance's module typespec.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/enum.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/gen_for.h>
#include <hldb/gen_if.h>
#include <hldb/gen_scope.h>
#include <hldb/gen_scope_array.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
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
#include <hldb/vpi_user.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

namespace hlc {

class ParamElabMultiTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamElabMulti.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg(std::string_view name) {
    return hldb::findByName<hldb::Package>(name, m_design->getAllPackages());
  }

  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Parameter *getParam(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, scope->getParameters());
  }

  // The ParamAssign in 'scope' whose LHS names 'name'.
  static const hldb::ParamAssign *getParamAssign(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr || scope->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *scope->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == name)) return pa;
      const hldb::Parameter *const p = pa->getLhs<hldb::Parameter>();
      if ((p != nullptr) && (p->getName() == name)) return pa;
    }
    return nullptr;
  }

  static const hldb::Typedef *getPadTypeE() {
    const hldb::Package *const pkg = getPkg("prim_pad_wrapper_pkg");
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("pad_type_e", pkg->getTypedefs());
  }

  static const hldb::EnumConst *getEnumConst(std::string_view name) {
    const hldb::Typedef *const td = getPadTypeE();
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::EnumTypespec *const et = td->getAlias()->getActual<hldb::EnumTypespec>();
    if (et == nullptr || et->getEnum() == nullptr) return nullptr;
    return hldb::findByName<hldb::EnumConst>(name, et->getEnum()->getEnumConsts());
  }

  static const hldb::TypespecMember *getTargetCfgMember(std::string_view name) {
    const hldb::Package *const pkg = getPkg("pinmux_pkg");
    if (pkg == nullptr) return nullptr;
    const hldb::Typedef *const td = hldb::findByName<hldb::Typedef>("target_cfg_t", pkg->getTypedefs());
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
    if (st == nullptr || st->getStruct() == nullptr) return nullptr;
    return hldb::findByName<hldb::TypespecMember>(name, st->getStruct()->getMembers());
  }

  // Whether 'rts' names pad_type_e.
  static bool namesPadTypeE(const hldb::RefTypespec *rts) {
    const hldb::Typedef *const td = getPadTypeE();
    if (rts == nullptr || td == nullptr || td->getAlias() == nullptr || rts->getActual() == nullptr) return false;
    const hldb::TypedefTypespec *const viaTypedef = rts->getActual<hldb::TypedefTypespec>();
    return ((viaTypedef != nullptr) && (viaTypedef->getTypedef() == td)) ||
           (rts->getActual() == td->getAlias()->getActual());
  }

  // The override expression for 'param' that the unelaborated instance
  // 'inst' writes.
  static const hldb::Any *getOverride(const hldb::RefInstance *inst, std::string_view param) {
    if (inst == nullptr || inst->getTypespec() == nullptr) return nullptr;
    const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
    if (mt == nullptr || mt->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *mt->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == param)) return pa->getRhs();
    }
    return nullptr;
  }

  // The only generate construct of module 'name'.
  static const hldb::Any *getOnlyGenStmt(std::string_view name) {
    const hldb::Module *const m = getModule(name);
    if (m == nullptr || m->getGenStmts() == nullptr || m->getGenStmts()->size() != 1u) return nullptr;
    return m->getGenStmts()->at(0);
  }

  // The instance named 'name' in 'block'.
  static const hldb::RefInstance *findInstance(const hldb::Begin *block, std::string_view name) {
    if (block == nullptr || block->getStmts() == nullptr) return nullptr;
    for (const hldb::Any *const item : *block->getStmts()) {
      const hldb::RefInstance *const inst = any_cast<hldb::RefInstance>(item);
      if ((inst != nullptr) && (inst->getName() == name)) return inst;
    }
    return nullptr;
  }

  // The single generate scope of the generate block 'name' directly in
  // 'scope', or nullptr unless there is exactly one.
  template <typename T>
  static const hldb::GenScope *getOnlyGenScope(const T *scope, std::string_view name) {
    if (scope == nullptr) return nullptr;
    const hldb::GenScopeArray *const gsa = hldb::findByName<hldb::GenScopeArray>(name, scope->getGenScopeArrays());
    if (gsa == nullptr || gsa->getGenScopes() == nullptr || gsa->getGenScopes()->size() != 1u) return nullptr;
    return gsa->getGenScopes()->at(0);
  }

  // Integer value of a Constant, from its decompiled text or from a sized or
  // based literal. The radix HLC picks for a folded value is a tool
  // convention, so every integer form is accepted.
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

  // Verifies the reduced value of parameter 'name' in the elaborated scope
  // 'scope' is 'expected'.
  static void ExpectElabParam(const hldb::Scope *scope, std::string_view name, uint64_t expected,
                              std::string_view why) {
    ASSERT_NE(scope, nullptr);
    const hldb::ParamAssign *const pa = getParamAssign(scope, name);
    ASSERT_NE(pa, nullptr) << "no ParamAssign for '" << name << "' in the elaborated scope";
    const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr) << "6.20.2: '" << name << "' should be reduced to a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(c, &v)) << "unparsable constant '" << c->getDecompile() << "'";
    EXPECT_EQ(v, expected) << why;
  }

  // Verifies module 'name' declares exactly 1 parameter, PadType, of
  // pad_type_e, overridable, with the default A.
  static void ExpectPadTypeParameter(std::string_view name) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << "module '" << name << "' not found";
    ASSERT_NE(m->getParameters(), nullptr);
    ASSERT_EQ(m->getParameters()->size(), 1u) << name;
    const hldb::Parameter *const p = getParam(m, "PadType");
    ASSERT_NE(p, nullptr) << name;
    EXPECT_FALSE(p->getLocalParam()) << "6.20.1: " << name
                                     << " has no parameter port list, so 'PadType' is overridable";
    EXPECT_TRUE(namesPadTypeE(p->getTypespec())) << "26.3: " << name << "'s 'PadType' is the imported pad_type_e";
    const hldb::ParamAssign *const pa = getParamAssign(m, "PadType");
    ASSERT_NE(pa, nullptr);
    const hldb::RefObj *const a = pa->getRhs<hldb::RefObj>();
    ASSERT_NE(a, nullptr) << name << ": the default is 'A'";
    EXPECT_EQ(a->getName(), "A");
    ASSERT_NE(getEnumConst("A"), nullptr);
    EXPECT_EQ(a->getActual(), getEnumConst("A")) << "26.3: 'A' is imported from prim_pad_wrapper_pkg";
  }
};

// ---------------------------------------------------------------------------
// packages prim_pad_wrapper_pkg and pinmux_pkg
// ---------------------------------------------------------------------------

TEST_F(ParamElabMultiTest, PadTypeEIsThreeBitEnumOfAAndB) {
  const hldb::Package *const pkg = getPkg("prim_pad_wrapper_pkg");
  ASSERT_NE(pkg, nullptr) << "package 'prim_pad_wrapper_pkg' not found";
  EXPECT_EQ(pkg->getEndLabel(), "prim_pad_wrapper_pkg");
  ASSERT_NE(pkg->getTypedefs(), nullptr);
  EXPECT_EQ(pkg->getTypedefs()->size(), 1u);
  const hldb::Typedef *const td = getPadTypeE();
  ASSERT_NE(td, nullptr) << "typedef 'pad_type_e' not found";
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::EnumTypespec *const et = td->getAlias()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(et, nullptr) << "6.19: 'pad_type_e' names an enumerated type";
  ASSERT_NE(et->getEnum(), nullptr);
  ASSERT_NE(et->getEnum()->getBaseTypespec(), nullptr);
  const hldb::LogicTypespec *const base = et->getEnum()->getBaseTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(base, nullptr) << "6.19: the base type is 'logic [2:0]'";
  ASSERT_NE(base->getRanges(), nullptr);
  ASSERT_EQ(base->getRanges()->size(), 1u);
  ASSERT_NE(base->getRanges()->at(0)->getLeftExpr<hldb::Constant>(), nullptr);
  EXPECT_EQ(base->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "2");
  ASSERT_NE(et->getEnum(), nullptr);
  ASSERT_NE(et->getEnum()->getEnumConsts(), nullptr);
  ASSERT_EQ(et->getEnum()->getEnumConsts()->size(), 2u);
  const char *const names[] = {"A", "B"};
  const char *const values[] = {"3'h0", "3'h1"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::EnumConst *const ec = et->getEnum()->getEnumConsts()->at(i);
    ASSERT_NE(ec, nullptr);
    EXPECT_EQ(ec->getName(), names[i]) << "enum name " << i << ", in source order";
    const hldb::Constant *const value = ec->getValue<hldb::Constant>();
    ASSERT_NE(value, nullptr) << "'" << names[i] << "' is given an explicit value";
    EXPECT_EQ(value->getDecompile(), values[i]);
  }
}

TEST_F(ParamElabMultiTest, PinmuxPkgDeclaresLocalNDioPadsOfOne) {
  const hldb::Package *const pkg = getPkg("pinmux_pkg");
  ASSERT_NE(pkg, nullptr) << "package 'pinmux_pkg' not found";
  EXPECT_EQ(pkg->getEndLabel(), "pinmux_pkg");
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 2u);
  for (std::string_view name : {"NDioPads", "DefaultTargetCfg"}) {
    const hldb::Parameter *const p = getParam(pkg, name);
    ASSERT_NE(p, nullptr) << "parameter '" << name << "' not found";
    EXPECT_TRUE(p->getLocalParam()) << "6.20.4: in a package, 'parameter' is a synonym for 'localparam'";
  }
  const hldb::ParamAssign *const pa = getParamAssign(pkg, "NDioPads");
  ASSERT_NE(pa, nullptr);
  ASSERT_NE(pa->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(pa->getRhs<hldb::Constant>()->getDecompile(), "1");
}

TEST_F(ParamElabMultiTest, TargetCfgTIsPackedStructOfDioPadType) {
  const hldb::Package *const pkg = getPkg("pinmux_pkg");
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getTypedefs(), nullptr);
  EXPECT_EQ(pkg->getTypedefs()->size(), 1u);
  const hldb::Typedef *const td = hldb::findByName<hldb::Typedef>("target_cfg_t", pkg->getTypedefs());
  ASSERT_NE(td, nullptr) << "typedef 'target_cfg_t' not found";
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr) << "6.18: 'target_cfg_t' names a structure type";
  ASSERT_NE(st->getStruct(), nullptr);
  EXPECT_TRUE(st->getStruct()->getPacked()) << "7.2.1: declared 'struct packed'";
  ASSERT_NE(st->getStruct()->getMembers(), nullptr);
  ASSERT_EQ(st->getStruct()->getMembers()->size(), 1u);
  ASSERT_NE(st->getStruct()->getMembers()->at(0), nullptr);
  EXPECT_EQ(st->getStruct()->getMembers()->at(0)->getName(), "dio_pad_type");
}

TEST_F(ParamElabMultiTest, DefaultTargetCfgReplicatesA) {
  const hldb::Package *const pkg = getPkg("pinmux_pkg");
  ASSERT_NE(pkg, nullptr);
  const hldb::ParamAssign *const pa = getParamAssign(pkg, "DefaultTargetCfg");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const pattern = pa->getRhs<hldb::Operation>();
  ASSERT_NE(pattern, nullptr) << "unreduced, the value is an Operation";
  EXPECT_EQ(pattern->getOpType(), vpiAssignmentPatternOp) << "10.9.2: a structure assignment pattern";
  ASSERT_NE(pattern->getOperands(), nullptr);
  ASSERT_EQ(pattern->getOperands()->size(), 1u);
  const hldb::TaggedPattern *const item = any_cast<hldb::TaggedPattern>(pattern->getOperands()->at(0));
  ASSERT_NE(item, nullptr) << "the item is a 'member: value' item";
  const hldb::RefObj *const tag = item->getTag<hldb::RefObj>();
  ASSERT_NE(tag, nullptr) << "10.9.2: the item is keyed by a member name";
  EXPECT_EQ(tag->getName(), "dio_pad_type");
  ASSERT_NE(getTargetCfgMember("dio_pad_type"), nullptr);
  EXPECT_EQ(tag->getActual(), getTargetCfgMember("dio_pad_type"))
      << "10.9.2: the key is the member dio_pad_type of target_cfg_t";
  const hldb::Operation *const repl = item->getPattern<hldb::Operation>();
  ASSERT_NE(repl, nullptr);
  EXPECT_EQ(repl->getOpType(), vpiMultiConcatOp) << "11.4.12.1: '{NDioPads{A}}' is a replication";
  ASSERT_NE(repl->getOperands(), nullptr);
  ASSERT_EQ(repl->getOperands()->size(), 2u) << "the multiplier, then the replicated concatenation";
  const hldb::RefObj *const count = any_cast<hldb::RefObj>(repl->getOperands()->at(0));
  ASSERT_NE(count, nullptr);
  EXPECT_EQ(count->getName(), "NDioPads");
  EXPECT_EQ(count->getActual(), getParam(pkg, "NDioPads")) << "'NDioPads' is pinmux_pkg's own parameter";
  const hldb::Any *element = repl->getOperands()->at(1);
  const hldb::Operation *const inner = any_cast<hldb::Operation>(element);
  if ((inner != nullptr) && (inner->getOpType() == vpiConcatOp) && (inner->getOperands() != nullptr) &&
      (inner->getOperands()->size() == 1u)) {
    element = inner->getOperands()->at(0);
  }
  const hldb::RefObj *const a = any_cast<hldb::RefObj>(element);
  ASSERT_NE(a, nullptr) << "the replicated element is 'A'";
  EXPECT_EQ(a->getName(), "A");
  ASSERT_NE(getEnumConst("A"), nullptr);
  EXPECT_EQ(a->getActual(), getEnumConst("A")) << "26.3: 'A' is imported from prim_pad_wrapper_pkg";
}

// ---------------------------------------------------------------------------
// modules prim_generic_pad_attr, prim_pad_attr, sub_top and top
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (null port
// missing) in the file header.
TEST_F(ParamElabMultiTest, EmptyPortListsDeclareOneNullPort) {
  for (std::string_view name : {"prim_generic_pad_attr", "prim_pad_attr", "sub_top", "top"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << "module '" << name << "' not found";
    ASSERT_NE(m->getPorts(), nullptr) << "37.14 detail 10: the empty port list of " << name << " is a null port";
    ASSERT_EQ(m->getPorts()->size(), 1u) << "37.14 detail 10: exactly one null port";
    EXPECT_EQ(m->getPorts()->at(0)->getName(), "") << "37.14 detail 8: a null port has no name";
    EXPECT_EQ(m->getPorts()->at(0)->getLowConn(), nullptr) << "37.14 detail 10: no low connection";
  }
}

TEST_F(ParamElabMultiTest, PadAttrModulesDeclareOverridablePadType) {
  ExpectPadTypeParameter("prim_generic_pad_attr");
  ExpectPadTypeParameter("prim_pad_attr");
}

TEST_F(ParamElabMultiTest, GenAssignIsGuardedByPadTypeEqualsB) {
  const hldb::GenIf *const genIf = any_cast<hldb::GenIf>(getOnlyGenStmt("prim_generic_pad_attr"));
  ASSERT_NE(genIf, nullptr) << "27.5: prim_generic_pad_attr holds exactly one if-generate";
  const hldb::Operation *const eq = genIf->getCondition<hldb::Operation>();
  ASSERT_NE(eq, nullptr);
  EXPECT_EQ(eq->getOpType(), vpiEqOp) << "11.4.5: 'PadType == B'";
  ASSERT_NE(eq->getOperands(), nullptr);
  ASSERT_EQ(eq->getOperands()->size(), 2u);
  const hldb::RefObj *const padType = any_cast<hldb::RefObj>(eq->getOperands()->at(0));
  ASSERT_NE(padType, nullptr);
  EXPECT_EQ(padType->getName(), "PadType");
  EXPECT_EQ(padType->getActual(), getParam(getModule("prim_generic_pad_attr"), "PadType"));
  const hldb::RefObj *const b = any_cast<hldb::RefObj>(eq->getOperands()->at(1));
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getName(), "B");
  ASSERT_NE(getEnumConst("B"), nullptr);
  EXPECT_EQ(b->getActual(), getEnumConst("B")) << "26.3: 'B' is imported from prim_pad_wrapper_pkg";
  const hldb::Begin *const block = genIf->getStmt<hldb::Begin>();
  ASSERT_NE(block, nullptr) << "27.5: 'begin : gen_assign ... end' is the generate block";
  EXPECT_EQ(block->getName(), "gen_assign");
  const hldb::ContAssign *assign = nullptr;
  if (block->getStmts() != nullptr) {
    for (const hldb::Any *const item : *block->getStmts()) {
      if (const hldb::ContAssign *const ca = any_cast<hldb::ContAssign>(item)) assign = ca;
    }
  }
  ASSERT_NE(assign, nullptr) << "10.3: the block holds 'assign a = 1;'";
  ASSERT_NE(assign->getLhs<hldb::RefObj>(), nullptr);
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "a");
}

TEST_F(ParamElabMultiTest, GenGenericPassesPadTypeToUImplGeneric) {
  const hldb::GenIf *const genIf = any_cast<hldb::GenIf>(getOnlyGenStmt("prim_pad_attr"));
  ASSERT_NE(genIf, nullptr) << "27.5: prim_pad_attr holds exactly one if-generate";
  const hldb::Constant *const one = genIf->getCondition<hldb::Constant>();
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->getDecompile(), "1");
  const hldb::Begin *const block = genIf->getStmt<hldb::Begin>();
  ASSERT_NE(block, nullptr);
  EXPECT_EQ(block->getName(), "gen_generic");
  const hldb::RefInstance *const inst = findInstance(block, "u_impl_generic");
  ASSERT_NE(inst, nullptr) << "gen_generic holds the instance u_impl_generic";
  const hldb::RefObj *const value = any_cast<hldb::RefObj>(getOverride(inst, "PadType"));
  ASSERT_NE(value, nullptr) << "23.10.2.2: '.PadType(PadType)' overrides by name";
  EXPECT_EQ(value->getName(), "PadType");
  EXPECT_EQ(value->getActual(), getParam(getModule("prim_pad_attr"), "PadType"))
      << "the value is prim_pad_attr's own parameter";
}

TEST_F(ParamElabMultiTest, SubTopDefaultsTargetCfgToDefaultTargetCfg) {
  const hldb::Module *const sub = getModule("sub_top");
  ASSERT_NE(sub, nullptr) << "module 'sub_top' not found";
  ASSERT_NE(sub->getParameters(), nullptr);
  ASSERT_EQ(sub->getParameters()->size(), 1u);
  const hldb::Parameter *const p = getParam(sub, "TargetCfg");
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->getLocalParam()) << "6.20.1: sub_top has no parameter port list, so 'TargetCfg' is overridable";
  const hldb::ParamAssign *const pa = getParamAssign(sub, "TargetCfg");
  ASSERT_NE(pa, nullptr);
  const hldb::RefObj *const value = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getName(), "DefaultTargetCfg");
  ASSERT_NE(getParam(getPkg("pinmux_pkg"), "DefaultTargetCfg"), nullptr);
  EXPECT_EQ(value->getActual(), getParam(getPkg("pinmux_pkg"), "DefaultTargetCfg"))
      << "26.3: 'DefaultTargetCfg' is imported from pinmux_pkg";
}

TEST_F(ParamElabMultiTest, GenDioAttrLoopsOverNDioPads) {
  const hldb::GenFor *const loop = any_cast<hldb::GenFor>(getOnlyGenStmt("sub_top"));
  ASSERT_NE(loop, nullptr) << "27.4: sub_top holds exactly one loop generate";
  const hldb::Operation *const lt = loop->getCondition<hldb::Operation>();
  ASSERT_NE(lt, nullptr);
  EXPECT_EQ(lt->getOpType(), vpiLtOp) << "11.4.4: 'k < NDioPads'";
  ASSERT_NE(lt->getOperands(), nullptr);
  ASSERT_EQ(lt->getOperands()->size(), 2u);
  const hldb::RefObj *const k = any_cast<hldb::RefObj>(lt->getOperands()->at(0));
  ASSERT_NE(k, nullptr);
  EXPECT_EQ(k->getName(), "k");
  const hldb::RefObj *const n = any_cast<hldb::RefObj>(lt->getOperands()->at(1));
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(n->getName(), "NDioPads");
  ASSERT_NE(getParam(getPkg("pinmux_pkg"), "NDioPads"), nullptr);
  EXPECT_EQ(n->getActual(), getParam(getPkg("pinmux_pkg"), "NDioPads")) << "26.3: imported from pinmux_pkg";
  const hldb::Begin *const block = loop->getStmt<hldb::Begin>();
  ASSERT_NE(block, nullptr);
  EXPECT_EQ(block->getName(), "gen_dio_attr");
  const hldb::RefInstance *const inst = findInstance(block, "u_prim_pad_attr");
  ASSERT_NE(inst, nullptr) << "gen_dio_attr holds the instance u_prim_pad_attr";
  EXPECT_NE(getOverride(inst, "PadType"), nullptr) << "23.10.2.2: '.PadType(TargetCfg.dio_pad_type[k])'";
}

TEST_F(ParamElabMultiTest, TopPassesPinmuxTargetCfgToUSub) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  ASSERT_NE(top->getParameters(), nullptr);
  ASSERT_EQ(top->getParameters()->size(), 1u);
  const hldb::Parameter *const p = getParam(top, "PinmuxTargetCfg");
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(p->getLocalParam()) << "6.20.4: declared 'localparam'";
  ASSERT_NE(top->getRefInstances(), nullptr);
  ASSERT_EQ(top->getRefInstances()->size(), 1u) << "top holds exactly one instance";
  const hldb::RefInstance *const inst = top->getRefInstances()->at(0);
  ASSERT_NE(inst, nullptr);
  EXPECT_EQ(inst->getName(), "u_sub");
  const hldb::RefObj *const value = any_cast<hldb::RefObj>(getOverride(inst, "TargetCfg"));
  ASSERT_NE(value, nullptr) << "23.10.2.2: '.TargetCfg(PinmuxTargetCfg)' overrides by name";
  EXPECT_EQ(value->getName(), "PinmuxTargetCfg");
  EXPECT_EQ(value->getActual(), p) << "the value is top's localparam";
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(ParamElabMultiTest, TopIsTheOnlyTopLevelInstance) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u)
        << "23.3.1: every other module is instantiated, inside a generate block or not";
    EXPECT_EQ(m_design->getTopModules()->at(0)->getDefName(), "top");
  }
}

TEST_F(ParamElabMultiTest, USubReceivesTargetCfgOfB) {
  if (m_design->getElaborated()) {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
    ASSERT_NE(top, nullptr) << "the elaborated top not found";
    const hldb::Module *const sub = hldb::findByName<hldb::Module>("u_sub", top->getModules());
    ASSERT_NE(sub, nullptr) << "the instance u_sub is elaborated";
    ExpectElabParam(sub, "TargetCfg", 1, "23.10.2.2: PinmuxTargetCfg = '{dio_pad_type: {1{B}}} = 3'h1");
  }
}

TEST_F(ParamElabMultiTest, PadTypeBReachesUImplGeneric) {
  if (m_design->getElaborated()) {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
    ASSERT_NE(top, nullptr);
    const hldb::Module *const sub = hldb::findByName<hldb::Module>("u_sub", top->getModules());
    ASSERT_NE(sub, nullptr);
    const hldb::GenScope *const dio = getOnlyGenScope(sub, "gen_dio_attr");
    ASSERT_NE(dio, nullptr) << "27.4: NDioPads = 1, so gen_dio_attr is instantiated exactly once, for k = 0";
    const hldb::Module *const padAttr = hldb::findByName<hldb::Module>("u_prim_pad_attr", dio->getModules());
    ASSERT_NE(padAttr, nullptr) << "gen_dio_attr[0] holds u_prim_pad_attr";
    ExpectElabParam(padAttr, "PadType", 1, "23.10.2.2: TargetCfg.dio_pad_type[0] = B = 3'h1");
    const hldb::GenScope *const generic = getOnlyGenScope(padAttr, "gen_generic");
    ASSERT_NE(generic, nullptr) << "27.5: 'if (1)' selects gen_generic";
    const hldb::Module *const impl = hldb::findByName<hldb::Module>("u_impl_generic", generic->getModules());
    ASSERT_NE(impl, nullptr) << "gen_generic holds u_impl_generic";
    ExpectElabParam(impl, "PadType", 1, "23.10.2.2: '.PadType(PadType)' passes B = 3'h1 on");
  }
}

TEST_F(ParamElabMultiTest, GenAssignIsGeneratedWithImplicitNetA) {
  if (m_design->getElaborated()) {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
    ASSERT_NE(top, nullptr);
    const hldb::Module *const sub = hldb::findByName<hldb::Module>("u_sub", top->getModules());
    ASSERT_NE(sub, nullptr);
    const hldb::GenScope *const dio = getOnlyGenScope(sub, "gen_dio_attr");
    ASSERT_NE(dio, nullptr);
    const hldb::Module *const padAttr = hldb::findByName<hldb::Module>("u_prim_pad_attr", dio->getModules());
    ASSERT_NE(padAttr, nullptr);
    const hldb::GenScope *const generic = getOnlyGenScope(padAttr, "gen_generic");
    ASSERT_NE(generic, nullptr);
    const hldb::Module *const impl = hldb::findByName<hldb::Module>("u_impl_generic", generic->getModules());
    ASSERT_NE(impl, nullptr);
    const hldb::GenScope *const assignScope = getOnlyGenScope(impl, "gen_assign");
    ASSERT_NE(assignScope, nullptr) << "27.5: PadType == B holds in u_impl_generic, so gen_assign is instantiated";
    ASSERT_NE(assignScope->getContAssigns(), nullptr);
    ASSERT_EQ(assignScope->getContAssigns()->size(), 1u) << "gen_assign holds 'assign a = 1;'";
    const hldb::RefObj *const lhs = assignScope->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getName(), "a");
    const hldb::Net *const net = lhs->getActual<hldb::Net>();
    ASSERT_NE(net, nullptr) << "6.10: an undeclared continuous-assignment target is an implicit net";
    EXPECT_EQ(net->getNetType(), vpiWire) << "22.8: the default net type is wire";
    EXPECT_EQ(net, hldb::findByName<hldb::Net>("a", assignScope->getNets()))
        << "6.10: the implicit net belongs to the generate block where the reference appears";
  }
}

TEST_F(ParamElabMultiTest, NoSyntaxOrFatalDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "the file is syntactically well formed";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
