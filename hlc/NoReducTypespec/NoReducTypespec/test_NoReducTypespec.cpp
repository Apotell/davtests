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

// Tests for tests/NoReducTypespec/dut.sv:
//
//   package axi_pkg;
//     typedef struct packed {
//       int unsigned   NoSlvPorts;
//       int unsigned   NoMstPorts;
//       int unsigned   AxiAddrWidth;
//     } xbar_cfg_t;
//   endpackage
//
//   module axi_xbar
//   #(
//     parameter axi_pkg::xbar_cfg_t Cfg                                   = '0,
//     parameter DEBUG = Cfg.NoSlvPorts,
//     parameter bit [Cfg.NoSlvPorts-1:0][Cfg.NoMstPorts-1:0] Connectivity = '1
//   ) ();
//   endmodule
//
//   package cheshire_pkg;
//     localparam axi_pkg::xbar_cfg_t AxiXbarCfg = '{
//       NoSlvPorts:         5,
//       NoMstPorts:         5,
//       AxiAddrWidth:       48
//     };
//   endpackage
//
//   module cheshire_soc import cheshire_pkg::*; #() ();
//     axi_xbar #(
//       .Cfg            ( AxiXbarCfg                    )
//     ) i_axi_xbar ();
//   endmodule
//
// What to check and why (IEEE 1800-2023):
//   - Sec 7.2.1: xbar_cfg_t is a packed structure of three 'int unsigned'
//     members (Sec 6.11: 'unsigned' overrides int's default signedness).
//   - Sec 26.3: "axi_pkg::xbar_cfg_t" resolves to the typedef in axi_pkg.
//   - Sec 5.7.1: '0 and '1 are unbased unsized single-bit literals that
//     fill the whole target with 0 / 1.
//   - Sec 6.20.2: "DEBUG = Cfg.NoSlvPorts" has no type or range, so it
//     takes the type of its final value: integral -> "a logic vector of the
//     same size with range [size-1:0]"; Cfg.NoSlvPorts is int unsigned, so
//     DEBUG is logic [31:0]. Its initializer is a member select (Sec 7.2)
//     of parameter Cfg.
//   - Sec 7.4.1: Connectivity's type "bit [Cfg.NoSlvPorts-1:0]
//     [Cfg.NoMstPorts-1:0]" has two packed dimensions whose bounds are
//     constant expressions over members of the (overridable) parameter Cfg;
//     the dimension bounds must keep those expressions (Sec 6.20.2: a
//     parameter's value can be overridden, so the bound cannot be reduced
//     using the default '0 at declaration time -- that is what this test is
//     named after).
//   - Sec 10.9.2: AxiXbarCfg's initializer is a structure assignment
//     pattern keyed by member names, with values 5, 5, 48.
//   - Sec 26.3 / 23.10.2.2: cheshire_soc wildcard-imports cheshire_pkg in
//     its header, so AxiXbarCfg in ".Cfg(AxiXbarCfg)" resolves to the
//     package localparam; the override is by name and targets axi_xbar's
//     Cfg parameter.
//
// What is NOT checked and why:
//   - Elaborated values (e.g. i_axi_xbar.DEBUG == 5): the .hlc does not
//     request elaboration.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bit_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
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
#include <hldb/sv_vpi_user.h>
#include <hldb/tagged_pattern.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/vpi_user.h>

namespace hlc {

class NoReducTypespecTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "NoReducTypespec.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg(std::string_view name) {
    return hldb::findByName<hldb::Package>(name, m_design->getAllPackages());
  }

  static const hldb::Module *getModuleByDef(std::string_view defName) {
    if (m_design->getAllModules() == nullptr) return nullptr;
    for (const hldb::Module *const m : *m_design->getAllModules()) {
      if (m->getDefName() == defName) return m;
    }
    return nullptr;
  }

  static const hldb::Parameter *getParam(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr || scope->getParameters() == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, scope->getParameters());
  }

  static const hldb::ParamAssign *getParamAssign(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr || scope->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *scope->getParamAssigns()) {
      const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(pa->getLhs());
      if (lhs != nullptr && lhs->getName() == name) return pa;
      const hldb::Parameter *const p = any_cast<hldb::Parameter>(pa->getLhs());
      if (p != nullptr && p->getName() == name) return pa;
    }
    return nullptr;
  }

  static const hldb::TypedefTypespec *getXbarCfgT() {
    const hldb::Package *const pkg = getPkg("axi_pkg");
    if (pkg == nullptr || pkg->getTypespecs() == nullptr) return nullptr;
    return hldb::findByName<hldb::TypedefTypespec>("xbar_cfg_t", pkg->getTypespecs());
  }

  // Checks 'ref' is "Cfg.<member>" with Cfg -> parameter and member bound.
  static void checkCfgMember(const hldb::Any *any, std::string_view member, const hldb::Parameter *cfg) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(any);
    ASSERT_NE(ref, nullptr);
    ASSERT_NE(ref->getPathElems(), nullptr);
    ASSERT_EQ(ref->getPathElems()->size(), 2u);
    const hldb::RefObj *const base = any_cast<hldb::RefObj>(ref->getPathElems()->at(0));
    ASSERT_NE(base, nullptr);
    EXPECT_EQ(base->getName(), std::string_view("Cfg"));
    EXPECT_EQ(base->getActual(), cfg);
    const hldb::RefObj *const mem = any_cast<hldb::RefObj>(ref->getPathElems()->at(1));
    ASSERT_NE(mem, nullptr);
    EXPECT_EQ(mem->getName(), member);
    ASSERT_NE(mem->getActual(), nullptr);
    EXPECT_EQ(mem->getActual()->getAnyType(), hldb::AnyType::TypespecMember);
  }
};

// ---------------------------------------------------------------------------
// Existence
// ---------------------------------------------------------------------------

TEST_F(NoReducTypespecTest, PackagesAndModulesExist) {
  EXPECT_NE(getPkg("axi_pkg"), nullptr);
  EXPECT_NE(getPkg("cheshire_pkg"), nullptr);
  EXPECT_NE(getModuleByDef("axi_xbar"), nullptr);
  EXPECT_NE(getModuleByDef("cheshire_soc"), nullptr);
}

// ---------------------------------------------------------------------------
// axi_pkg::xbar_cfg_t
// ---------------------------------------------------------------------------

TEST_F(NoReducTypespecTest, XbarCfgTIsPackedStructOfThreeIntUnsigned) {
  const hldb::TypedefTypespec *const tt = getXbarCfgT();
  ASSERT_NE(tt, nullptr);
  ASSERT_NE(tt->getTypedef(), nullptr);
  ASSERT_NE(tt->getTypedef()->getAlias(), nullptr);
  const hldb::StructTypespec *const st = tt->getTypedef()->getAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr);
  const hldb::Struct *const s = st->getStruct();
  ASSERT_NE(s, nullptr);
  EXPECT_TRUE(s->getPacked());
  ASSERT_NE(s->getMembers(), nullptr);
  ASSERT_EQ(s->getMembers()->size(), 3u);
  const char *const names[] = {"NoSlvPorts", "NoMstPorts", "AxiAddrWidth"};
  for (size_t i = 0; i < 3; ++i) {
    const hldb::TypespecMember *const m = s->getMembers()->at(i);
    EXPECT_EQ(m->getName(), std::string_view(names[i]));
    ASSERT_NE(m->getTypespec(), nullptr) << names[i];
    const hldb::IntTypespec *const it = m->getTypespec()->getActual<hldb::IntTypespec>();
    ASSERT_NE(it, nullptr) << names[i];
    EXPECT_FALSE(it->getSigned()) << names[i] << ": 'int unsigned' is unsigned";
  }
}

// ---------------------------------------------------------------------------
// axi_xbar parameters
// ---------------------------------------------------------------------------

TEST_F(NoReducTypespecTest, CfgTypedByPackageScopedTypedef) {
  const hldb::Parameter *const cfg = getParam(getModuleByDef("axi_xbar"), "Cfg");
  ASSERT_NE(cfg, nullptr);
  EXPECT_FALSE(cfg->getLocalParam());
  ASSERT_NE(cfg->getTypespec(), nullptr);
  EXPECT_EQ(cfg->getTypespec()->getActual(), getXbarCfgT()) << "Sec 26.3: axi_pkg::xbar_cfg_t";
}

TEST_F(NoReducTypespecTest, CfgDefaultIsUnbasedUnsizedZero) {
  const hldb::ParamAssign *const pa = getParamAssign(getModuleByDef("axi_xbar"), "Cfg");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const c = any_cast<hldb::Constant>(pa->getRhs());
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), std::string_view("'0"));
}

TEST_F(NoReducTypespecTest, DebugInitializerIsCfgNoSlvPorts) {
  const hldb::Module *const xbar = getModuleByDef("axi_xbar");
  const hldb::ParamAssign *const pa = getParamAssign(xbar, "DEBUG");
  ASSERT_NE(pa, nullptr);
  checkCfgMember(pa->getRhs(), "NoSlvPorts", getParam(xbar, "Cfg"));
}

TEST_F(NoReducTypespecTest, DebugIsLogicVector31To0) {
  const hldb::Parameter *const dbg = getParam(getModuleByDef("axi_xbar"), "DEBUG");
  ASSERT_NE(dbg, nullptr);
  EXPECT_FALSE(dbg->getLocalParam());
  ASSERT_NE(dbg->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = dbg->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "Sec 6.20.2: untyped integral parameter is a logic vector";
  ASSERT_NE(lt->getRanges(), nullptr) << "Sec 6.20.2: range [size-1:0] of int unsigned -> [31:0]";
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  const hldb::Constant *const l = any_cast<hldb::Constant>(lt->getRanges()->at(0)->getLeftExpr());
  const hldb::Constant *const r = any_cast<hldb::Constant>(lt->getRanges()->at(0)->getRightExpr());
  ASSERT_NE(l, nullptr);
  ASSERT_NE(r, nullptr);
  EXPECT_EQ(l->getDecompile(), std::string_view("31"));
  EXPECT_EQ(r->getDecompile(), std::string_view("0"));
}

TEST_F(NoReducTypespecTest, ConnectivityIsBitWithTwoUnreducedPackedDims) {
  const hldb::Module *const xbar = getModuleByDef("axi_xbar");
  const hldb::Parameter *const cfg = getParam(xbar, "Cfg");
  const hldb::Parameter *const conn = getParam(xbar, "Connectivity");
  ASSERT_NE(conn, nullptr);
  ASSERT_NE(conn->getTypespec(), nullptr);
  const hldb::BitTypespec *const bt = conn->getTypespec()->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  ASSERT_NE(bt->getRanges(), nullptr);
  ASSERT_EQ(bt->getRanges()->size(), 2u);
  const char *const members[] = {"NoSlvPorts", "NoMstPorts"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::Range *const rg = bt->getRanges()->at(i);
    const hldb::Operation *const sub = any_cast<hldb::Operation>(rg->getLeftExpr());
    ASSERT_NE(sub, nullptr) << "dimension " << i << " left bound must remain 'Cfg." << members[i] << "-1'";
    EXPECT_EQ(sub->getOpType(), vpiSubOp);
    ASSERT_NE(sub->getOperands(), nullptr);
    ASSERT_EQ(sub->getOperands()->size(), 2u);
    checkCfgMember(sub->getOperands()->at(0), members[i], cfg);
    const hldb::Constant *const one = any_cast<hldb::Constant>(sub->getOperands()->at(1));
    ASSERT_NE(one, nullptr);
    EXPECT_EQ(one->getDecompile(), std::string_view("1"));
    const hldb::Constant *const zero = any_cast<hldb::Constant>(rg->getRightExpr());
    ASSERT_NE(zero, nullptr);
    EXPECT_EQ(zero->getDecompile(), std::string_view("0"));
  }
}

TEST_F(NoReducTypespecTest, ConnectivityDefaultIsUnbasedUnsizedOne) {
  const hldb::ParamAssign *const pa = getParamAssign(getModuleByDef("axi_xbar"), "Connectivity");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const c = any_cast<hldb::Constant>(pa->getRhs());
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), std::string_view("'1"));
}

// ---------------------------------------------------------------------------
// cheshire_pkg::AxiXbarCfg
// ---------------------------------------------------------------------------

TEST_F(NoReducTypespecTest, AxiXbarCfgIsLocalParamOfXbarCfgT) {
  const hldb::Parameter *const p = getParam(getPkg("cheshire_pkg"), "AxiXbarCfg");
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(p->getLocalParam());
  ASSERT_NE(p->getTypespec(), nullptr);
  EXPECT_EQ(p->getTypespec()->getActual(), getXbarCfgT());
}

TEST_F(NoReducTypespecTest, AxiXbarCfgInitializerIsKeyedPattern) {
  const hldb::ParamAssign *const pa = getParamAssign(getPkg("cheshire_pkg"), "AxiXbarCfg");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const op = any_cast<hldb::Operation>(pa->getRhs());
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiAssignmentPatternOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 3u);
  const char *const keys[] = {"NoSlvPorts", "NoMstPorts", "AxiAddrWidth"};
  const char *const values[] = {"5", "5", "48"};
  for (size_t i = 0; i < 3; ++i) {
    const hldb::TaggedPattern *const tp = any_cast<hldb::TaggedPattern>(op->getOperands()->at(i));
    ASSERT_NE(tp, nullptr) << keys[i];
    const hldb::RefObj *const tag = any_cast<hldb::RefObj>(tp->getTag());
    ASSERT_NE(tag, nullptr) << keys[i];
    EXPECT_EQ(tag->getName(), std::string_view(keys[i]));
    ASSERT_NE(tag->getActual(), nullptr) << keys[i];
    EXPECT_EQ(tag->getActual()->getAnyType(), hldb::AnyType::TypespecMember);
    const hldb::Constant *const v = any_cast<hldb::Constant>(tp->getPattern());
    ASSERT_NE(v, nullptr) << keys[i];
    EXPECT_EQ(v->getDecompile(), std::string_view(values[i]));
  }
}

// ---------------------------------------------------------------------------
// cheshire_soc: axi_xbar #(.Cfg(AxiXbarCfg)) i_axi_xbar ();
// ---------------------------------------------------------------------------

TEST_F(NoReducTypespecTest, CheshireSocOverridesCfgByNameWithImportedParam) {
  const hldb::Module *const soc = getModuleByDef("cheshire_soc");
  ASSERT_NE(soc, nullptr);
  ASSERT_NE(soc->getRefInstances(), nullptr);
  const hldb::RefInstance *const ri = hldb::findByName<hldb::RefInstance>("i_axi_xbar", soc->getRefInstances());
  ASSERT_NE(ri, nullptr);
  ASSERT_NE(ri->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mts = ri->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mts, nullptr);
  EXPECT_EQ(mts->getModule(), getModuleByDef("axi_xbar"));
  ASSERT_NE(mts->getParamAssigns(), nullptr);
  ASSERT_EQ(mts->getParamAssigns()->size(), 1u);
  const hldb::ParamAssign *const pa = mts->getParamAssigns()->at(0);
  EXPECT_TRUE(pa->getConnByName());
  EXPECT_TRUE(pa->getOverridden());
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(pa->getLhs());
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view("Cfg"));
  EXPECT_EQ(lhs->getActual(), getParam(getModuleByDef("axi_xbar"), "Cfg"));
  const hldb::RefObj *const rhs = any_cast<hldb::RefObj>(pa->getRhs());
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), std::string_view("AxiXbarCfg"));
  EXPECT_EQ(rhs->getActual(), getParam(getPkg("cheshire_pkg"), "AxiXbarCfg"))
      << "Sec 26.3: wildcard import makes cheshire_pkg::AxiXbarCfg visible";
}

TEST_F(NoReducTypespecTest, NoBindingErrors) {
  for (const char *const sym : {"Cfg", "NoSlvPorts", "NoMstPorts", "AxiXbarCfg", "xbar_cfg_t", "axi_pkg"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, sym), nullptr) << sym;
  }
  EXPECT_EQ(findError(ErrorDefinition::ELAB_UNKNOWN_PARAMETER_OVERRIDE, "Cfg"), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
