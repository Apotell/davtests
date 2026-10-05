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

// Tests for dut.sv (tags: LargeValue2Struct)
//   package pack;
//     parameter int SramKeyWidth      = 128;
//     typedef logic [SramKeyWidth-1:0]   sram_key_t;
//     typedef logic [SramKeyWidth-1:0] sram_nonce_t;
//   typedef struct packed {
//       sram_key_t   key;
//       sram_nonce_t nonce;
//     } scrmbl_key_init_t;
//   localparam scrmbl_key_init_t RndCnstScrmblKeyInitDefault =
//         256'hcebeb96ffe0eced795f8b2cfe23c1e519e4fa08047a6bcfb811b04f0a479006e;
//   endpackage
//
//   module prim_sec_anchor_flop #(
//     parameter int               Width      = 1,
//     parameter logic [Width-1:0] ResetValue = 0) ();
//   endmodule
//
//   module top();
//   import pack::*;
//   parameter scrmbl_key_init_t RndCnstScrmblKeyInit = RndCnstScrmblKeyInitDefault;
//   parameter Width = SramKeyWidth;
//   prim_sec_anchor_flop #(
//       .Width(Width),
//       .ResetValue(RndCnstScrmblKeyInit.key)
//     ) u_key_out_anchor (
//     );
//   endmodule
//
// What is checked (IEEE 1800-2023):
//   - package 'pack': 6.20.1 "All param_assignments appearing within a ...
//     package ... shall become localparam declarations" -> SramKeyWidth is
//     local (type int, signed per 6.11.3), RndCnstScrmblKeyInitDefault is an
//     explicit localparam typed scrmbl_key_init_t
//   - sram_key_t / sram_nonce_t are typedefs of logic [SramKeyWidth-1:0]
//   - scrmbl_key_init_t is a packed struct (7.2.1) with members key
//     (sram_key_t) and nonce (sram_nonce_t)
//   - RndCnstScrmblKeyInitDefault's value is a 256-bit sized hex literal
//     (5.7.1): vpiHexConst, size 256 -- a packed struct may be assigned an
//     integral value (7.2.1)
//   - prim_sec_anchor_flop: Width (int) and ResetValue (logic [Width-1:0])
//     are overridable parameter ports; ResetValue's range references Width
//   - top has no parameter_port_list, so its body parameters
//     RndCnstScrmblKeyInit and Width are overridable (not local, 6.20.1);
//     their initializers reference the package parameters made visible by
//     the wildcard import (26.3)
//   - u_key_out_anchor is an instance of prim_sec_anchor_flop with by-name
//     parameter value assignments (23.10.2.2): in .Width(Width) the formal
//     is prim_sec_anchor_flop's Width while the actual expression is
//     evaluated in the instantiating scope, i.e. top's Width; and
//     .ResetValue(RndCnstScrmblKeyInit.key) selects member key of top's
//     struct parameter (7.2)
//
// What is NOT checked and why:
//   - evaluated values / widths (e.g. that ResetValue becomes 128 bits wide
//     holding the upper half of the 256-bit literal): needs elaboration,
//     which this .hlc does not run.
//   - Module::getName(): HLC decorates names of parameterized definitions;
//     modules are looked up by getDefName().

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/import_typespec.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class LargeValue2StructTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LargeValue2Struct.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPack() { return hldb::findByName<hldb::Package>("pack", m_design->getAllPackages()); }

  static const hldb::Module *getModuleDef(std::string_view defName) {
    if (m_design->getAllModules() == nullptr) return nullptr;
    for (const hldb::Module *const m : *m_design->getAllModules()) {
      if (m->getDefName() == defName) return m;
    }
    return nullptr;
  }

  static const hldb::Parameter *getParam(const hldb::Instance *scope, std::string_view name) {
    return hldb::findByName<hldb::Parameter>(name, scope->getParameters());
  }

  static const hldb::ModuleTypespec *getInstTypespec() {
    const hldb::Module *const top = getModuleDef("top");
    if ((top == nullptr) || (top->getRefInstances() == nullptr) || (top->getRefInstances()->size() != 1)) {
      return nullptr;
    }
    const hldb::RefInstance *const inst = top->getRefInstances()->at(0);
    if (inst->getTypespec() == nullptr) return nullptr;
    return inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  }

  // Checks a logic typespec with one range [<param>-1:0].
  static void checkLogicRange(const hldb::Typespec *ts, std::string_view param) {
    const hldb::LogicTypespec *const lt = any_cast<hldb::LogicTypespec>(ts);
    ASSERT_NE(lt, nullptr);
    ASSERT_NE(lt->getRanges(), nullptr);
    ASSERT_EQ(lt->getRanges()->size(), 1u);
    const hldb::Range *const r = lt->getRanges()->at(0);
    ASSERT_NE(r, nullptr);
    const hldb::Operation *const left = r->getLeftExpr<hldb::Operation>();
    ASSERT_NE(left, nullptr);
    EXPECT_EQ(left->getOpType(), vpiSubOp);
    ASSERT_NE(left->getOperands(), nullptr);
    ASSERT_EQ(left->getOperands()->size(), 2u);
    const hldb::RefObj *const p = any_cast<hldb::RefObj>(left->getOperands()->at(0));
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->getName(), param);
    ASSERT_NE(p->getActual(), nullptr);
    EXPECT_EQ(p->getActual()->getAnyType(), hldb::AnyType::Parameter);
    const hldb::Constant *const right = r->getRightExpr<hldb::Constant>();
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(right->getDecompile(), "0");
  }
};

// ===========================================================================
// package pack
// ===========================================================================

TEST_F(LargeValue2StructTest, PackageParametersAreLocal) {
  const hldb::Package *const pack = getPack();
  ASSERT_NE(pack, nullptr);
  const hldb::Parameter *const w = getParam(pack, "SramKeyWidth");
  ASSERT_NE(w, nullptr);
  EXPECT_TRUE(w->getLocalParam()) << "6.20.1: package parameters become localparam";
  ASSERT_NE(w->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = w->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr) << "SramKeyWidth is declared 'int'";
  EXPECT_TRUE(it->getSigned()) << "6.11.3: int is signed";

  const hldb::Parameter *const d = getParam(pack, "RndCnstScrmblKeyInitDefault");
  ASSERT_NE(d, nullptr);
  EXPECT_TRUE(d->getLocalParam());
  ASSERT_NE(d->getTypespec(), nullptr);
  EXPECT_EQ(d->getTypespec()->getName(), "scrmbl_key_init_t");
}

TEST_F(LargeValue2StructTest, KeyAndNonceTypedefsAreLogicVectors) {
  const hldb::Package *const pack = getPack();
  ASSERT_NE(pack, nullptr);
  for (std::string_view name : {"sram_key_t", "sram_nonce_t"}) {
    const hldb::Typedef *const td = hldb::findByName<hldb::Typedef>(name, pack->getTypedefs());
    ASSERT_NE(td, nullptr) << name;
    ASSERT_NE(td->getAlias(), nullptr) << name;
    checkLogicRange(td->getAlias()->getActual(), "SramKeyWidth");
  }
}

TEST_F(LargeValue2StructTest, ScrmblKeyInitIsPackedStruct) {
  const hldb::Package *const pack = getPack();
  ASSERT_NE(pack, nullptr);
  const hldb::Typedef *const td = hldb::findByName<hldb::Typedef>("scrmbl_key_init_t", pack->getTypedefs());
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr);
  const hldb::Struct *const s = st->getStruct();
  ASSERT_NE(s, nullptr);
  EXPECT_TRUE(s->getPacked()) << "7.2.1";
  ASSERT_NE(s->getMembers(), nullptr);
  ASSERT_EQ(s->getMembers()->size(), 2u);
  const hldb::TypespecMember *const key = s->getMembers()->at(0);
  const hldb::TypespecMember *const nonce = s->getMembers()->at(1);
  ASSERT_NE(key, nullptr);
  ASSERT_NE(nonce, nullptr);
  EXPECT_EQ(key->getName(), "key");
  EXPECT_EQ(nonce->getName(), "nonce");
  ASSERT_NE(key->getTypespec(), nullptr);
  ASSERT_NE(nonce->getTypespec(), nullptr);
  EXPECT_EQ(key->getTypespec()->getName(), "sram_key_t");
  EXPECT_EQ(nonce->getTypespec()->getName(), "sram_nonce_t");
}

TEST_F(LargeValue2StructTest, DefaultValueIs256BitHexLiteral) {
  const hldb::Package *const pack = getPack();
  ASSERT_NE(pack, nullptr);
  const hldb::ParamAssign *const pa =
      hldb::findByName<hldb::ParamAssign>("RndCnstScrmblKeyInitDefault", pack->getParamAssigns());
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), vpiHexConst);
  EXPECT_EQ(c->getSize(), 256) << "5.7.1: the size constant gives the exact width";
  EXPECT_EQ(c->getDecompile(), "256'hcebeb96ffe0eced795f8b2cfe23c1e519e4fa08047a6bcfb811b04f0a479006e");
}

// ===========================================================================
// prim_sec_anchor_flop
// ===========================================================================

TEST_F(LargeValue2StructTest, AnchorFlopParameterPorts) {
  const hldb::Module *const m = getModuleDef("prim_sec_anchor_flop");
  ASSERT_NE(m, nullptr);
  const hldb::Parameter *const w = getParam(m, "Width");
  const hldb::Parameter *const rv = getParam(m, "ResetValue");
  ASSERT_NE(w, nullptr);
  ASSERT_NE(rv, nullptr);
  EXPECT_FALSE(w->getLocalParam());
  EXPECT_FALSE(rv->getLocalParam());
  ASSERT_NE(w->getTypespec(), nullptr);
  EXPECT_NE(w->getTypespec()->getActual<hldb::IntTypespec>(), nullptr) << "Width is declared 'int'";
  ASSERT_NE(rv->getTypespec(), nullptr);
  checkLogicRange(rv->getTypespec()->getActual(), "Width");
}

// ===========================================================================
// top
// ===========================================================================

TEST_F(LargeValue2StructTest, TopImportsPack) {
  const hldb::Module *const top = getModuleDef("top");
  ASSERT_NE(top, nullptr);
  const hldb::ImportTypespec *imp = nullptr;
  if (top->getTypespecs() != nullptr) {
    for (const hldb::Typespec *const ts : *top->getTypespecs()) {
      if (const hldb::ImportTypespec *const it = any_cast<hldb::ImportTypespec>(ts)) {
        imp = it;
        break;
      }
    }
  }
  ASSERT_NE(imp, nullptr) << "26.3: 'import pack::*;'";
  EXPECT_EQ(imp->getName(), "pack");
  ASSERT_NE(imp->getItem(), nullptr);
  EXPECT_EQ(imp->getItem()->getDecompile(), "*");
}

TEST_F(LargeValue2StructTest, TopBodyParametersAreOverridable) {
  const hldb::Module *const top = getModuleDef("top");
  ASSERT_NE(top, nullptr);
  const hldb::Parameter *const k = getParam(top, "RndCnstScrmblKeyInit");
  const hldb::Parameter *const w = getParam(top, "Width");
  ASSERT_NE(k, nullptr);
  ASSERT_NE(w, nullptr);
  EXPECT_FALSE(k->getLocalParam()) << "6.20.1: top has no parameter_port_list";
  EXPECT_FALSE(w->getLocalParam()) << "6.20.1: top has no parameter_port_list";
  ASSERT_NE(k->getTypespec(), nullptr);
  EXPECT_EQ(k->getTypespec()->getName(), "scrmbl_key_init_t");
}

TEST_F(LargeValue2StructTest, TopParametersBindToImportedPackageParameters) {
  const hldb::Module *const top = getModuleDef("top");
  const hldb::Package *const pack = getPack();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(pack, nullptr);

  const hldb::ParamAssign *const k =
      hldb::findByName<hldb::ParamAssign>("RndCnstScrmblKeyInit", top->getParamAssigns());
  ASSERT_NE(k, nullptr);
  const hldb::RefObj *const kr = k->getRhs<hldb::RefObj>();
  ASSERT_NE(kr, nullptr);
  EXPECT_EQ(kr->getName(), "RndCnstScrmblKeyInitDefault");
  EXPECT_EQ(kr->getActual(), getParam(pack, "RndCnstScrmblKeyInitDefault"))
      << "26.3: resolves to the package parameter through the wildcard import";

  const hldb::ParamAssign *const w = hldb::findByName<hldb::ParamAssign>("Width", top->getParamAssigns());
  ASSERT_NE(w, nullptr);
  const hldb::RefObj *const wr = w->getRhs<hldb::RefObj>();
  ASSERT_NE(wr, nullptr);
  EXPECT_EQ(wr->getName(), "SramKeyWidth");
  EXPECT_EQ(wr->getActual(), getParam(pack, "SramKeyWidth"));
}

TEST_F(LargeValue2StructTest, InstanceOfAnchorFlop) {
  const hldb::Module *const top = getModuleDef("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  ASSERT_EQ(top->getRefInstances()->size(), 1u);
  const hldb::RefInstance *const inst = top->getRefInstances()->at(0);
  ASSERT_NE(inst, nullptr);
  EXPECT_EQ(inst->getName(), "u_key_out_anchor");
  ASSERT_NE(inst->getTypespec(), nullptr);
  EXPECT_EQ(inst->getTypespec()->getName(), "prim_sec_anchor_flop");
  const hldb::ModuleTypespec *const mt = getInstTypespec();
  ASSERT_NE(mt, nullptr);
  ASSERT_NE(mt->getParamAssigns(), nullptr);
  EXPECT_EQ(mt->getParamAssigns()->size(), 2u);
}

TEST_F(LargeValue2StructTest, WidthOverrideFormalAndActualScopes) {
  const hldb::Module *const top = getModuleDef("top");
  const hldb::Module *const flop = getModuleDef("prim_sec_anchor_flop");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(flop, nullptr);
  const hldb::ModuleTypespec *const mt = getInstTypespec();
  ASSERT_NE(mt, nullptr);
  const hldb::ParamAssign *const pa = hldb::findByName<hldb::ParamAssign>("Width", mt->getParamAssigns());
  ASSERT_NE(pa, nullptr);
  EXPECT_TRUE(pa->getConnByName());
  EXPECT_TRUE(pa->getOverridden());

  const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getActual(), getParam(flop, "Width")) << "the formal is prim_sec_anchor_flop's Width";

  const hldb::RefObj *const rhs = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "Width");
  EXPECT_EQ(rhs->getActual(), getParam(top, "Width"))
      << "23.10.2.2: the actual expression is evaluated in the instantiating scope (top)";
}

TEST_F(LargeValue2StructTest, ResetValueOverrideSelectsStructMember) {
  const hldb::Module *const top = getModuleDef("top");
  const hldb::Module *const flop = getModuleDef("prim_sec_anchor_flop");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(flop, nullptr);
  const hldb::ModuleTypespec *const mt = getInstTypespec();
  ASSERT_NE(mt, nullptr);
  const hldb::ParamAssign *const pa = hldb::findByName<hldb::ParamAssign>("ResetValue", mt->getParamAssigns());
  ASSERT_NE(pa, nullptr);
  EXPECT_TRUE(pa->getConnByName());
  EXPECT_TRUE(pa->getOverridden());

  const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getActual(), getParam(flop, "ResetValue"));

  const hldb::RefObj *const rhs = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "RndCnstScrmblKeyInit.key";
  ASSERT_NE(rhs->getPathElems(), nullptr);
  ASSERT_EQ(rhs->getPathElems()->size(), 2u);
  const hldb::RefObj *const base = any_cast<hldb::RefObj>(rhs->getPathElems()->at(0));
  const hldb::RefObj *const member = any_cast<hldb::RefObj>(rhs->getPathElems()->at(1));
  ASSERT_NE(base, nullptr);
  ASSERT_NE(member, nullptr);
  EXPECT_EQ(base->getName(), "RndCnstScrmblKeyInit");
  EXPECT_EQ(base->getActual(), getParam(top, "RndCnstScrmblKeyInit"));
  EXPECT_EQ(member->getName(), "key");
  ASSERT_NE(member->getActual(), nullptr);
  EXPECT_EQ(member->getActual()->getAnyType(), hldb::AnyType::TypespecMember) << "7.2: member 'key' of the struct";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
