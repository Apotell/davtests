/*
 Copyright 2026 Alain Dargelas

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

// Validates IEEE 1800-2017 Sec 23.11 bind_directive HLDB model for all
// permutations defined in dut.sv (T1-T16 top-level, M1-M5 module-level,
// G1-G3 generate-level).

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/any.h>
#include <hldb/bind_directive.h>
#include <hldb/bit_select.h>
#include <hldb/checker_inst.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_block.h>
#include <hldb/gen_if.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/unit.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

namespace hlc {

class BindTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Bind.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Find a BindDirective in `col` whose source instance has the given name.
  static const hldb::BindDirective *findBD(const hldb::BindDirectiveCollection *col, std::string_view srcName) {
    if (col == nullptr) return nullptr;
    for (const hldb::BindDirective *bd : *col) {
      if (const hldb::Any *src = bd->getBindSourceInstance()) {
        if (src->getName() == srcName) return bd;
      }
    }
    return nullptr;
  }

  // Collect BindDirectives from generate-if blocks inside a module.
  // At parse time (pre-elaboration) they live in:
  //   module->getGenStmts() -> GenIf -> getStmt<GenBlock>() -> getBindDirectives()
  // Each 'begin : gen_xxx ... end' is a generate block (GenBlock, a GenScope), and a bind
  // directive inside it is held by the GenScope's vpiBindDirective relation.
  static std::vector<const hldb::BindDirective *> genScopeBinds(const hldb::Module *mod) {
    std::vector<const hldb::BindDirective *> result;
    if (mod == nullptr) return result;
    const hldb::AnyCollection *const genStmts = mod->getGenStmts();
    if (genStmts == nullptr) return result;
    for (const hldb::Any *genStmt : *genStmts) {
      if (const hldb::GenIf *const genIf = any_cast<hldb::GenIf>(genStmt)) {
        const hldb::GenBlock *const block = genIf->getStmt<hldb::GenBlock>();
        if (block == nullptr) continue;
        const hldb::BindDirectiveCollection *const bds = block->getBindDirectives();
        if (bds == nullptr) continue;
        result.insert(result.end(), bds->cbegin(), bds->cend());
      }
    }
    return result;
  }
};

// --- Counts ---

TEST_F(BindTest, DesignLevelCount) {
  ASSERT_NE(m_unit, nullptr);
  const hldb::BindDirectiveCollection *bds = m_unit->getBindDirectives();
  ASSERT_NE(bds, nullptr);
  EXPECT_EQ(bds->size(), 16u) << "Expected T1-T16 as top-level bind directives";
}

TEST_F(BindTest, ModuleLevelCount) {
  const hldb::Module *mod = hldb::findByDefName<hldb::Module>("top_module_binds", m_design->getAllModules());
  ASSERT_NE(mod, nullptr) << "Module top_module_binds not found";
  const hldb::BindDirectiveCollection *bds = mod->getBindDirectives();
  ASSERT_NE(bds, nullptr);
  EXPECT_EQ(bds->size(), 5u) << "Expected M1-M5 inside top_module_binds";
}

TEST_F(BindTest, GenerateLevelCount) {
  const hldb::Module *mod = hldb::findByDefName<hldb::Module>("top_generate_binds", m_design->getAllModules());
  ASSERT_NE(mod, nullptr) << "Module top_generate_binds not found";
  const std::vector<const hldb::BindDirective *> bds = genScopeBinds(mod);
  EXPECT_EQ(bds.size(), 3u) << "Expected G1-G3 across generate blocks";
}

// --- T1-T6: Form 1, scope-only (no instance list) ---

TEST_F(BindTest, T1_ScopeOnly_NamedPorts) {
  ASSERT_NE(m_unit, nullptr);
  const hldb::BindDirective *const bd = findBD(m_unit->getBindDirectives(), "bd_t1");
  ASSERT_NE(bd, nullptr);
  const hldb::Any *const target = bd->getBindTargetScope();
  ASSERT_NE(target, nullptr);
  EXPECT_EQ(target->getName(), "TargetMod");
  EXPECT_EQ(bd->getBindTargetInstances(), nullptr);
  ASSERT_NE(any_cast<hldb::RefInstance>(bd->getBindSourceInstance()), nullptr);
  EXPECT_EQ(bd->getBindSourceInstance()->getName(), "bd_t1");
}

TEST_F(BindTest, T2_ScopeOnly_OrderedPorts) {
  ASSERT_NE(m_unit, nullptr);
  const hldb::BindDirective *const bd = findBD(m_unit->getBindDirectives(), "bd_t2");
  ASSERT_NE(bd, nullptr);
  const hldb::Any *const target = bd->getBindTargetScope();
  ASSERT_NE(target, nullptr);
  EXPECT_EQ(target->getName(), "TargetMod");
  EXPECT_EQ(bd->getBindTargetInstances(), nullptr);
  EXPECT_EQ(bd->getBindSourceInstance()->getName(), "bd_t2");
}

TEST_F(BindTest, T3_ScopeOnly_DotStar) {
  ASSERT_NE(m_unit, nullptr);
  const hldb::BindDirective *const bd = findBD(m_unit->getBindDirectives(), "bd_t3");
  ASSERT_NE(bd, nullptr);
  const hldb::Any *const target = bd->getBindTargetScope();
  ASSERT_NE(target, nullptr);
  EXPECT_EQ(target->getName(), "TargetMod");
  EXPECT_EQ(bd->getBindTargetInstances(), nullptr);
  EXPECT_EQ(bd->getBindSourceInstance()->getName(), "bd_t3");
}

TEST_F(BindTest, T4_ScopeOnly_NoPorts) {
  ASSERT_NE(m_unit, nullptr);
  const hldb::BindDirective *const bd = findBD(m_unit->getBindDirectives(), "bd_t4");
  ASSERT_NE(bd, nullptr);
  const hldb::Any *const target = bd->getBindTargetScope();
  ASSERT_NE(target, nullptr);
  EXPECT_EQ(target->getName(), "TargetMod");
  EXPECT_EQ(bd->getBindTargetInstances(), nullptr);
  EXPECT_EQ(bd->getBindSourceInstance()->getName(), "bd_t4");
}

TEST_F(BindTest, T5_ScopeOnly_ParamOverride) {
  ASSERT_NE(m_unit, nullptr);
  const hldb::BindDirective *const bd = findBD(m_unit->getBindDirectives(), "bd_t5");
  ASSERT_NE(bd, nullptr);
  const hldb::Any *const target = bd->getBindTargetScope();
  ASSERT_NE(target, nullptr);
  EXPECT_EQ(target->getName(), "TargetMod");
  EXPECT_EQ(bd->getBindTargetInstances(), nullptr);
  EXPECT_EQ(bd->getBindSourceInstance()->getName(), "bd_t5");
}

// T6: checker_instantiation -- grammar parses as module_instantiation at parse
// time; elaboration must reclassify to CheckerInst.
TEST_F(BindTest, T6_ScopeOnly_CheckerBind) {
  ASSERT_NE(m_unit, nullptr);
  const hldb::BindDirective *const bd = findBD(m_unit->getBindDirectives(), "bd_t6");
  ASSERT_NE(bd, nullptr);
  const hldb::Any *const target = bd->getBindTargetScope();
  ASSERT_NE(target, nullptr);
  EXPECT_EQ(target->getName(), "TargetMod");
  EXPECT_EQ(bd->getBindTargetInstances(), nullptr);
  ASSERT_NE(bd->getBindSourceInstance(), nullptr);
  EXPECT_EQ(bd->getBindSourceInstance()->getName(), "bd_t6");
}

// --- T7-T12: Form 1, explicit instance lists ---

TEST_F(BindTest, T7_OneInstance) {
  ASSERT_NE(m_unit, nullptr);
  const hldb::BindDirective *const bd = findBD(m_unit->getBindDirectives(), "bd_t7");
  ASSERT_NE(bd, nullptr);
  const hldb::Any *const target = bd->getBindTargetScope();
  ASSERT_NE(target, nullptr);
  EXPECT_EQ(target->getName(), "TargetMod");
  const hldb::AnyCollection *const insts = bd->getBindTargetInstances();
  ASSERT_NE(insts, nullptr);
  ASSERT_EQ(insts->size(), 1u);
  const hldb::RefObj *const ro = any_cast<hldb::RefObj>((*insts)[0]);
  ASSERT_NE(ro, nullptr);
  EXPECT_EQ(ro->getName(), "u0");
}

TEST_F(BindTest, T8_TwoInstances) {
  ASSERT_NE(m_unit, nullptr);
  const hldb::BindDirective *const bd = findBD(m_unit->getBindDirectives(), "bd_t8");
  ASSERT_NE(bd, nullptr);
  const hldb::Any *const target = bd->getBindTargetScope();
  ASSERT_NE(target, nullptr);
  EXPECT_EQ(target->getName(), "TargetMod");
  const hldb::AnyCollection *const insts = bd->getBindTargetInstances();
  ASSERT_NE(insts, nullptr);
  ASSERT_EQ(insts->size(), 2u);
  const hldb::RefObj *const ro0 = any_cast<hldb::RefObj>((*insts)[0]);
  ASSERT_NE(ro0, nullptr);
  EXPECT_EQ(ro0->getName(), "u0");
  const hldb::RefObj *const ro1 = any_cast<hldb::RefObj>((*insts)[1]);
  ASSERT_NE(ro1, nullptr);
  EXPECT_EQ(ro1->getName(), "u1");
}

TEST_F(BindTest, T9_BitSelectSingle) {
  ASSERT_NE(m_unit, nullptr);
  const hldb::BindDirective *const bd = findBD(m_unit->getBindDirectives(), "bd_t9");
  ASSERT_NE(bd, nullptr);
  const hldb::AnyCollection *const insts = bd->getBindTargetInstances();
  ASSERT_NE(insts, nullptr);
  ASSERT_EQ(insts->size(), 1u);
  // u_arr[0]
  const hldb::BitSelect *const bs = any_cast<hldb::BitSelect>((*insts)[0]);
  ASSERT_NE(bs, nullptr);
  ASSERT_NE(any_cast<hldb::Constant>(bs->getIndex()), nullptr);
  const hldb::RefObj *const pfx = bs->getPrefix<hldb::RefObj>();
  ASSERT_NE(pfx, nullptr);
  EXPECT_EQ(pfx->getName(), "u_arr");
}

TEST_F(BindTest, T10_TwoBitSelects) {
  ASSERT_NE(m_unit, nullptr);
  const hldb::BindDirective *const bd = findBD(m_unit->getBindDirectives(), "bd_t10");
  ASSERT_NE(bd, nullptr);
  const hldb::AnyCollection *const insts = bd->getBindTargetInstances();
  ASSERT_NE(insts, nullptr);
  ASSERT_EQ(insts->size(), 2u);
  // u_arr[0]
  const hldb::BitSelect *const bs0 = any_cast<hldb::BitSelect>((*insts)[0]);
  ASSERT_NE(bs0, nullptr);
  const hldb::Constant *const idx0 = bs0->getIndex<hldb::Constant>();
  ASSERT_NE(idx0, nullptr);
  EXPECT_EQ(idx0->getDecompile(), "0");
  const hldb::RefObj *const pfx0 = bs0->getPrefix<hldb::RefObj>();
  ASSERT_NE(pfx0, nullptr);
  EXPECT_EQ(pfx0->getName(), "u_arr");
  // u_arr[1]
  const hldb::BitSelect *const bs1 = any_cast<hldb::BitSelect>((*insts)[1]);
  ASSERT_NE(bs1, nullptr);
  const hldb::Constant *const idx1 = bs1->getIndex<hldb::Constant>();
  ASSERT_NE(idx1, nullptr);
  EXPECT_EQ(idx1->getDecompile(), "1");
  const hldb::RefObj *const pfx1 = bs1->getPrefix<hldb::RefObj>();
  ASSERT_NE(pfx1, nullptr);
  EXPECT_EQ(pfx1->getName(), "u_arr");
}

TEST_F(BindTest, T11_2DBitSelect) {
  ASSERT_NE(m_unit, nullptr);
  // u_arr[1][0]  ->  BitSelect{0, prefix=BitSelect{1, prefix=RefObj"u_arr"}}
  const hldb::BindDirective *const bd = findBD(m_unit->getBindDirectives(), "bd_t11");
  ASSERT_NE(bd, nullptr);
  const hldb::AnyCollection *const insts = bd->getBindTargetInstances();
  ASSERT_NE(insts, nullptr);
  ASSERT_EQ(insts->size(), 1u);
  const hldb::BitSelect *const outer = any_cast<hldb::BitSelect>((*insts)[0]);
  ASSERT_NE(outer, nullptr);
  const hldb::Constant *const outerIdx = outer->getIndex<hldb::Constant>();
  ASSERT_NE(outerIdx, nullptr);
  EXPECT_EQ(outerIdx->getDecompile(), "0");
  const hldb::BitSelect *inner = outer->getPrefix<hldb::BitSelect>();
  ASSERT_NE(inner, nullptr);
  const hldb::Constant *const innerIdx = inner->getIndex<hldb::Constant>();
  ASSERT_NE(innerIdx, nullptr);
  EXPECT_EQ(innerIdx->getDecompile(), "1");
  const hldb::RefObj *const base = inner->getPrefix<hldb::RefObj>();
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->getName(), "u_arr");
}

TEST_F(BindTest, T12_MixedList) {
  ASSERT_NE(m_unit, nullptr);
  // u0, u_arr[0], u_arr[1]
  const hldb::BindDirective *const bd = findBD(m_unit->getBindDirectives(), "bd_t12");
  ASSERT_NE(bd, nullptr);
  const hldb::AnyCollection *const insts = bd->getBindTargetInstances();
  ASSERT_NE(insts, nullptr);
  ASSERT_EQ(insts->size(), 3u);
  // [0] = RefObj "u0"
  const hldb::RefObj *const ro = any_cast<hldb::RefObj>((*insts)[0]);
  ASSERT_NE(ro, nullptr);
  EXPECT_EQ(ro->getName(), "u0");
  // [1] = BitSelect u_arr[0]
  const hldb::BitSelect *const bs0 = any_cast<hldb::BitSelect>((*insts)[1]);
  ASSERT_NE(bs0, nullptr);
  const hldb::RefObj *const pfx0 = bs0->getPrefix<hldb::RefObj>();
  ASSERT_NE(pfx0, nullptr);
  EXPECT_EQ(pfx0->getName(), "u_arr");
  // [2] = BitSelect u_arr[1]
  const hldb::BitSelect *const bs1 = any_cast<hldb::BitSelect>((*insts)[2]);
  ASSERT_NE(bs1, nullptr);
  const hldb::RefObj *const pfx1 = bs1->getPrefix<hldb::RefObj>();
  ASSERT_NE(pfx1, nullptr);
  EXPECT_EQ(pfx1->getName(), "u_arr");
}

// --- T13-T14: checker_instantiation with instance lists ---

TEST_F(BindTest, T13_CheckerWithOneInstance) {
  ASSERT_NE(m_unit, nullptr);
  const hldb::BindDirective *const bd = findBD(m_unit->getBindDirectives(), "bd_t13");
  ASSERT_NE(bd, nullptr);
  const hldb::Any *const scope = bd->getBindTargetScope();
  ASSERT_NE(scope, nullptr);
  EXPECT_EQ(scope->getName(), "TargetMod");
  const hldb::AnyCollection *const insts = bd->getBindTargetInstances();
  ASSERT_NE(insts, nullptr);
  ASSERT_EQ(insts->size(), 1u);
  const hldb::RefObj *const ro = any_cast<hldb::RefObj>((*insts)[0]);
  ASSERT_NE(ro, nullptr);
  EXPECT_EQ(ro->getName(), "u0");
  ASSERT_NE(bd->getBindSourceInstance(), nullptr);
  EXPECT_EQ(bd->getBindSourceInstance()->getName(), "bd_t13");
}

TEST_F(BindTest, T14_CheckerWithBitSelects) {
  ASSERT_NE(m_unit, nullptr);
  const hldb::BindDirective *const bd = findBD(m_unit->getBindDirectives(), "bd_t14");
  ASSERT_NE(bd, nullptr);
  const hldb::AnyCollection *const insts = bd->getBindTargetInstances();
  ASSERT_NE(insts, nullptr);
  ASSERT_EQ(insts->size(), 2u);
  ASSERT_NE(any_cast<hldb::BitSelect>((*insts)[0]), nullptr);
  ASSERT_NE(any_cast<hldb::BitSelect>((*insts)[1]), nullptr);
  ASSERT_NE(bd->getBindSourceInstance(), nullptr);
  EXPECT_EQ(bd->getBindSourceInstance()->getName(), "bd_t14");
}

// --- T15: Form 1/2 ambiguous -- single identifier, no COLON ---

TEST_F(BindTest, T15_Ambiguous_SingleIdentifier) {
  ASSERT_NE(m_unit, nullptr);
  // At parse time: scope="u0", no instances. Elaboration resolves Form 1 vs 2.
  const hldb::BindDirective *const bd = findBD(m_unit->getBindDirectives(), "bd_t15");
  ASSERT_NE(bd, nullptr);
  const hldb::Any *const scope = bd->getBindTargetScope();
  ASSERT_NE(scope, nullptr);
  EXPECT_EQ(scope->getName(), "u0");
  EXPECT_EQ(bd->getBindTargetInstances(), nullptr);
  ASSERT_NE(bd->getBindSourceInstance(), nullptr);
  EXPECT_EQ(bd->getBindSourceInstance()->getName(), "bd_t15");
}

// --- T16: Form 2 -- hierarchical path bind target ---

TEST_F(BindTest, T16_HierarchicalForm2) {
  // bind top_instances.u0 AsrtMod bd_t16(...)
  // Grammar: bind_target_instance -> hierarchical_identifier constant_bit_select
  // elaboration resolves the full path top_instances.u0 to a specific instance.
  ASSERT_NE(m_unit, nullptr);
  const hldb::BindDirective *const bd = findBD(m_unit->getBindDirectives(), "bd_t16");
  ASSERT_NE(bd, nullptr);
  const hldb::RefObj *const scope = bd->getBindTargetScope<hldb::RefObj>();
  ASSERT_NE(scope, nullptr) << "Hierarchical Form 2 path";
  ASSERT_NE(scope->getPathElems(), nullptr);
  ASSERT_EQ(scope->getPathElems()->size(), 2u);
  ASSERT_EQ(scope->getPathElems()->at(0)->getName(), "top_instances");
  ASSERT_EQ(scope->getPathElems()->at(1)->getName(), "u0");
  EXPECT_EQ(bd->getBindTargetInstances(), nullptr);
  ASSERT_NE(bd->getBindSourceInstance(), nullptr);
  EXPECT_EQ(bd->getBindSourceInstance()->getName(), "bd_t16");
}

// --- M1-M5: module-level bind directives ---

TEST_F(BindTest, M1_FormOneAllInstances) {
  const hldb::Module *const mod = hldb::findByDefName<hldb::Module>("top_module_binds", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const hldb::BindDirective *const bd = findBD(mod->getBindDirectives(), "bd_m1");
  ASSERT_NE(bd, nullptr);
  const hldb::Any *const scope = bd->getBindTargetScope();
  ASSERT_NE(scope, nullptr);
  EXPECT_EQ(scope->getName(), "TargetMod");
  EXPECT_EQ(bd->getBindTargetInstances(), nullptr);
  ASSERT_NE(bd->getBindSourceInstance(), nullptr);
  EXPECT_EQ(bd->getBindSourceInstance()->getName(), "bd_m1");
}

TEST_F(BindTest, M2_TwoNamedInstances) {
  const hldb::Module *const mod = hldb::findByDefName<hldb::Module>("top_module_binds", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const hldb::BindDirective *const bd = findBD(mod->getBindDirectives(), "bd_m2");
  ASSERT_NE(bd, nullptr);
  const hldb::Any *const scope = bd->getBindTargetScope();
  ASSERT_NE(scope, nullptr);
  EXPECT_EQ(scope->getName(), "TargetMod");
  const hldb::AnyCollection *const insts = bd->getBindTargetInstances();
  ASSERT_NE(insts, nullptr);
  ASSERT_EQ(insts->size(), 2u);
  const hldb::RefObj *const ro0 = any_cast<hldb::RefObj>((*insts)[0]);
  ASSERT_NE(ro0, nullptr);
  EXPECT_EQ(ro0->getName(), "ma0");
  const hldb::RefObj *const ro1 = any_cast<hldb::RefObj>((*insts)[1]);
  ASSERT_NE(ro1, nullptr);
  EXPECT_EQ(ro1->getName(), "ma1");
}

TEST_F(BindTest, M3_BitSelectInstance) {
  const hldb::Module *const mod = hldb::findByDefName<hldb::Module>("top_module_binds", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const hldb::BindDirective *const bd = findBD(mod->getBindDirectives(), "bd_m3");
  ASSERT_NE(bd, nullptr);
  const hldb::AnyCollection *const insts = bd->getBindTargetInstances();
  ASSERT_NE(insts, nullptr);
  ASSERT_EQ(insts->size(), 1u);
  // ma_arr[0]
  const hldb::BitSelect *const bs = any_cast<hldb::BitSelect>((*insts)[0]);
  ASSERT_NE(bs, nullptr);
  ASSERT_NE(any_cast<hldb::Constant>(bs->getIndex()), nullptr);
  const hldb::RefObj *const pfx = bs->getPrefix<hldb::RefObj>();
  ASSERT_NE(pfx, nullptr);
  EXPECT_EQ(pfx->getName(), "ma_arr");
}

TEST_F(BindTest, M4_CheckerBind) {
  const hldb::Module *const mod = hldb::findByDefName<hldb::Module>("top_module_binds", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const hldb::BindDirective *const bd = findBD(mod->getBindDirectives(), "bd_m4");
  ASSERT_NE(bd, nullptr);
  const hldb::Any *const scope = bd->getBindTargetScope();
  ASSERT_NE(scope, nullptr);
  EXPECT_EQ(scope->getName(), "TargetMod");
  EXPECT_EQ(bd->getBindTargetInstances(), nullptr);
  ASSERT_NE(bd->getBindSourceInstance(), nullptr);
  EXPECT_EQ(bd->getBindSourceInstance()->getName(), "bd_m4");
}

TEST_F(BindTest, M5_ParamOverride) {
  const hldb::Module *const mod = hldb::findByDefName<hldb::Module>("top_module_binds", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const hldb::BindDirective *const bd = findBD(mod->getBindDirectives(), "bd_m5");
  ASSERT_NE(bd, nullptr);
  const hldb::Any *const scope = bd->getBindTargetScope();
  ASSERT_NE(scope, nullptr);
  EXPECT_EQ(scope->getName(), "TargetMod");
  EXPECT_EQ(bd->getBindTargetInstances(), nullptr);
  ASSERT_NE(bd->getBindSourceInstance(), nullptr);
  EXPECT_EQ(bd->getBindSourceInstance()->getName(), "bd_m5");
}

// --- G1-G3: bind directives inside generate blocks ---

TEST_F(BindTest, G1_FormOneInGenerateIf) {
  const hldb::Module *const mod = hldb::findByDefName<hldb::Module>("top_generate_binds", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const std::vector<const hldb::BindDirective *> bds = genScopeBinds(mod);
  const auto it = std::find_if(bds.begin(), bds.end(), [](const hldb::BindDirective *bd) {
    const hldb::Any *const src = bd->getBindSourceInstance();
    return src && src->getName() == "bd_g1";
  });
  ASSERT_NE(it, bds.end());
  const hldb::Any *const scope = (*it)->getBindTargetScope();
  ASSERT_NE(scope, nullptr);
  EXPECT_EQ(scope->getName(), "TargetMod");
  EXPECT_TRUE((*it)->getBindTargetInstances() == nullptr);
}

TEST_F(BindTest, G2_InstanceListInGenerateIf) {
  const hldb::Module *const mod = hldb::findByDefName<hldb::Module>("top_generate_binds", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const std::vector<const hldb::BindDirective *> bds = genScopeBinds(mod);
  const auto it = std::find_if(bds.begin(), bds.end(), [](const hldb::BindDirective *bd) {
    const hldb::Any *const src = bd->getBindSourceInstance();
    return src && src->getName() == "bd_g2";
  });
  ASSERT_NE(it, bds.end());
  const hldb::Any *const scope = (*it)->getBindTargetScope();
  ASSERT_NE(scope, nullptr);
  EXPECT_EQ(scope->getName(), "TargetMod");
  const hldb::AnyCollection *const insts = (*it)->getBindTargetInstances();
  ASSERT_NE(insts, nullptr);
  ASSERT_EQ(insts->size(), 2u);
  const hldb::RefObj *const ro0 = any_cast<hldb::RefObj>((*insts)[0]);
  ASSERT_NE(ro0, nullptr);
  EXPECT_EQ(ro0->getName(), "gb0");
  const hldb::RefObj *const ro1 = any_cast<hldb::RefObj>((*insts)[1]);
  ASSERT_NE(ro1, nullptr);
  EXPECT_EQ(ro1->getName(), "gb1");
}

TEST_F(BindTest, G3_CheckerInGenerateIf) {
  const hldb::Module *const mod = hldb::findByDefName<hldb::Module>("top_generate_binds", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const std::vector<const hldb::BindDirective *> bds = genScopeBinds(mod);
  const auto it = std::find_if(bds.begin(), bds.end(), [](const hldb::BindDirective *bd) {
    const hldb::Any *const src = bd->getBindSourceInstance();
    return src && src->getName() == "bd_g3";
  });
  ASSERT_NE(it, bds.end());
  const hldb::Any *const scope = (*it)->getBindTargetScope();
  ASSERT_NE(scope, nullptr);
  EXPECT_EQ(scope->getName(), "TargetMod");
  EXPECT_TRUE((*it)->getBindTargetInstances() == nullptr);
  EXPECT_EQ((*it)->getBindSourceInstance()->getName(), "bd_g3");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
