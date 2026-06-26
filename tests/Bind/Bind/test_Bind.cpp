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

// Validates IEEE 1800-2017 §23.11 bind_directive HLDB model for all
// permutations defined in dut.sv (T1-T16 top-level, M1-M5 module-level,
// G1-G3 generate-level).

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/any.h>
#include <hldb/begin.h>
#include <hldb/bind_directive.h>
#include <hldb/bit_select.h>
#include <hldb/checker_inst.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_if.h>
#include <hldb/module.h>
#include <hldb/ref_module.h>
#include <hldb/ref_obj.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

namespace hlc {

class BindDirectiveAll : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-nobuiltin", "dut.sv"});
    ASSERT_NE(m_session, nullptr);
    ASSERT_NE(m_design, nullptr);
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }

 protected:
  // Find a BindDirective in `col` whose source instance has the given name.
  static const hldb::BindDirective* findBD(
      const hldb::BindDirectiveCollection* col, std::string_view srcName) {
    if (col == nullptr) return nullptr;
    for (const hldb::BindDirective* bd : *col) {
      if (const hldb::Any* src = bd->getBindSourceInstance()) {
        if (src->getName() == srcName) return bd;
      }
    }
    return nullptr;
  }

  // Collect BindDirectives from generate-if blocks inside a module.
  // At parse time (pre-elaboration) they live in:
  //   module->getGenStmts() → GenIf → getStmt<Begin>() → getStmts() → BindDirective
  static std::vector<const hldb::BindDirective*> genScopeBinds(
      const hldb::Module* mod) {
    std::vector<const hldb::BindDirective*> result;
    if (mod == nullptr) return result;
    const auto* genStmts = mod->getGenStmts();
    if (genStmts == nullptr) return result;
    for (const hldb::Any* genStmt : *genStmts) {
      if (const auto* genIf = any_cast<hldb::GenIf>(genStmt)) {
        const auto* begin = genIf->getStmt<hldb::Begin>();
        if (begin == nullptr) continue;
        const auto* stmts = begin->getStmts();
        if (stmts == nullptr) continue;
        for (const hldb::Any* stmt : *stmts) {
          if (const auto* bd = any_cast<hldb::BindDirective>(stmt)) {
            result.push_back(bd);
          }
        }
      }
    }
    return result;
  }
};

// ─── Counts ──────────────────────────────────────────────────────────────────

TEST_F(BindDirectiveAll, DesignLevelCount) {
  const auto* bds = m_design->getBindDirectives();
  ASSERT_NE(bds, nullptr);
  EXPECT_EQ(bds->size(), 16u) << "Expected T1-T16 as top-level bind directives";
}

TEST_F(BindDirectiveAll, ModuleLevelCount) {
  const auto* mod = hldb::findByName<hldb::Module>(
      "work@top_module_binds", m_design->getAllModules());
  ASSERT_NE(mod, nullptr) << "Module top_module_binds not found";
  const auto* bds = mod->getBindDirectives();
  ASSERT_NE(bds, nullptr);
  EXPECT_EQ(bds->size(), 5u) << "Expected M1-M5 inside top_module_binds";
}

TEST_F(BindDirectiveAll, GenerateLevelCount) {
  const auto* mod = hldb::findByName<hldb::Module>(
      "work@top_generate_binds", m_design->getAllModules());
  ASSERT_NE(mod, nullptr) << "Module top_generate_binds not found";
  const auto bds = genScopeBinds(mod);
  EXPECT_EQ(bds.size(), 3u) << "Expected G1-G3 across generate blocks";
}

// ─── T1–T6: Form 1, scope-only (no instance list) ────────────────────────────

TEST_F(BindDirectiveAll, T1_ScopeOnly_NamedPorts) {
  const auto* bd = findBD(m_design->getBindDirectives(), "bd_t1");
  ASSERT_NE(bd, nullptr);
  EXPECT_EQ(bd->getBindTargetScope(), "TargetMod");
  EXPECT_TRUE(bd->getBindTargetInstances() == nullptr);
  ASSERT_NE(any_cast<hldb::RefModule>(bd->getBindSourceInstance()), nullptr);
  EXPECT_EQ(bd->getBindSourceInstance()->getName(), "bd_t1");
}

TEST_F(BindDirectiveAll, T2_ScopeOnly_OrderedPorts) {
  const auto* bd = findBD(m_design->getBindDirectives(), "bd_t2");
  ASSERT_NE(bd, nullptr);
  EXPECT_EQ(bd->getBindTargetScope(), "TargetMod");
  EXPECT_TRUE(bd->getBindTargetInstances() == nullptr);
  EXPECT_EQ(bd->getBindSourceInstance()->getName(), "bd_t2");
}

TEST_F(BindDirectiveAll, T3_ScopeOnly_DotStar) {
  const auto* bd = findBD(m_design->getBindDirectives(), "bd_t3");
  ASSERT_NE(bd, nullptr);
  EXPECT_EQ(bd->getBindTargetScope(), "TargetMod");
  EXPECT_TRUE(bd->getBindTargetInstances() == nullptr);
  EXPECT_EQ(bd->getBindSourceInstance()->getName(), "bd_t3");
}

TEST_F(BindDirectiveAll, T4_ScopeOnly_NoPorts) {
  const auto* bd = findBD(m_design->getBindDirectives(), "bd_t4");
  ASSERT_NE(bd, nullptr);
  EXPECT_EQ(bd->getBindTargetScope(), "TargetMod");
  EXPECT_TRUE(bd->getBindTargetInstances() == nullptr);
  EXPECT_EQ(bd->getBindSourceInstance()->getName(), "bd_t4");
}

TEST_F(BindDirectiveAll, T5_ScopeOnly_ParamOverride) {
  const auto* bd = findBD(m_design->getBindDirectives(), "bd_t5");
  ASSERT_NE(bd, nullptr);
  EXPECT_EQ(bd->getBindTargetScope(), "TargetMod");
  EXPECT_TRUE(bd->getBindTargetInstances() == nullptr);
  EXPECT_EQ(bd->getBindSourceInstance()->getName(), "bd_t5");
}

// T6: checker_instantiation — grammar parses as module_instantiation at parse
// time; elaboration must reclassify to CheckerInst.
TEST_F(BindDirectiveAll, T6_ScopeOnly_CheckerBind) {
  const auto* bd = findBD(m_design->getBindDirectives(), "bd_t6");
  ASSERT_NE(bd, nullptr);
  EXPECT_EQ(bd->getBindTargetScope(), "TargetMod");
  EXPECT_TRUE(bd->getBindTargetInstances() == nullptr);
  ASSERT_NE(bd->getBindSourceInstance(), nullptr);
  EXPECT_EQ(bd->getBindSourceInstance()->getName(), "bd_t6");
}

// ─── T7–T12: Form 1, explicit instance lists ──────────────────────────────────

TEST_F(BindDirectiveAll, T7_OneInstance) {
  const auto* bd = findBD(m_design->getBindDirectives(), "bd_t7");
  ASSERT_NE(bd, nullptr);
  EXPECT_EQ(bd->getBindTargetScope(), "TargetMod");
  const auto* insts = bd->getBindTargetInstances();
  ASSERT_NE(insts, nullptr);
  ASSERT_EQ(insts->size(), 1u);
  const auto* ro = any_cast<hldb::RefObj>((*insts)[0]);
  ASSERT_NE(ro, nullptr);
  EXPECT_EQ(ro->getName(), "u0");
}

TEST_F(BindDirectiveAll, T8_TwoInstances) {
  const auto* bd = findBD(m_design->getBindDirectives(), "bd_t8");
  ASSERT_NE(bd, nullptr);
  EXPECT_EQ(bd->getBindTargetScope(), "TargetMod");
  const auto* insts = bd->getBindTargetInstances();
  ASSERT_NE(insts, nullptr);
  ASSERT_EQ(insts->size(), 2u);
  const auto* ro0 = any_cast<hldb::RefObj>((*insts)[0]);
  ASSERT_NE(ro0, nullptr);
  EXPECT_EQ(ro0->getName(), "u0");
  const auto* ro1 = any_cast<hldb::RefObj>((*insts)[1]);
  ASSERT_NE(ro1, nullptr);
  EXPECT_EQ(ro1->getName(), "u1");
}

TEST_F(BindDirectiveAll, T9_BitSelectSingle) {
  const auto* bd = findBD(m_design->getBindDirectives(), "bd_t9");
  ASSERT_NE(bd, nullptr);
  const auto* insts = bd->getBindTargetInstances();
  ASSERT_NE(insts, nullptr);
  ASSERT_EQ(insts->size(), 1u);
  // u_arr[0]
  const auto* bs = any_cast<hldb::BitSelect>((*insts)[0]);
  ASSERT_NE(bs, nullptr);
  ASSERT_NE(any_cast<hldb::Constant>(bs->getIndex()), nullptr);
  const auto* pfx = bs->getPrefix<hldb::RefObj>();
  ASSERT_NE(pfx, nullptr);
  EXPECT_EQ(pfx->getName(), "u_arr");
}

TEST_F(BindDirectiveAll, T10_TwoBitSelects) {
  const auto* bd = findBD(m_design->getBindDirectives(), "bd_t10");
  ASSERT_NE(bd, nullptr);
  const auto* insts = bd->getBindTargetInstances();
  ASSERT_NE(insts, nullptr);
  ASSERT_EQ(insts->size(), 2u);
  // u_arr[0]
  const auto* bs0 = any_cast<hldb::BitSelect>((*insts)[0]);
  ASSERT_NE(bs0, nullptr);
  const auto* idx0 = any_cast<hldb::Constant>(bs0->getIndex());
  ASSERT_NE(idx0, nullptr);
  EXPECT_EQ(idx0->getDecompile(), "0");
  const auto* pfx0 = bs0->getPrefix<hldb::RefObj>();
  ASSERT_NE(pfx0, nullptr);
  EXPECT_EQ(pfx0->getName(), "u_arr");
  // u_arr[1]
  const auto* bs1 = any_cast<hldb::BitSelect>((*insts)[1]);
  ASSERT_NE(bs1, nullptr);
  const auto* idx1 = any_cast<hldb::Constant>(bs1->getIndex());
  ASSERT_NE(idx1, nullptr);
  EXPECT_EQ(idx1->getDecompile(), "1");
  const auto* pfx1 = bs1->getPrefix<hldb::RefObj>();
  ASSERT_NE(pfx1, nullptr);
  EXPECT_EQ(pfx1->getName(), "u_arr");
}

TEST_F(BindDirectiveAll, T11_2DBitSelect) {
  // u_arr[1][0]  →  BitSelect{0, prefix=BitSelect{1, prefix=RefObj"u_arr"}}
  const auto* bd = findBD(m_design->getBindDirectives(), "bd_t11");
  ASSERT_NE(bd, nullptr);
  const auto* insts = bd->getBindTargetInstances();
  ASSERT_NE(insts, nullptr);
  ASSERT_EQ(insts->size(), 1u);
  const auto* outer = any_cast<hldb::BitSelect>((*insts)[0]);
  ASSERT_NE(outer, nullptr);
  const auto* outerIdx = any_cast<hldb::Constant>(outer->getIndex());
  ASSERT_NE(outerIdx, nullptr);
  EXPECT_EQ(outerIdx->getDecompile(), "0");
  const auto* inner = outer->getPrefix<hldb::BitSelect>();
  ASSERT_NE(inner, nullptr);
  const auto* innerIdx = any_cast<hldb::Constant>(inner->getIndex());
  ASSERT_NE(innerIdx, nullptr);
  EXPECT_EQ(innerIdx->getDecompile(), "1");
  const auto* base = inner->getPrefix<hldb::RefObj>();
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->getName(), "u_arr");
}

TEST_F(BindDirectiveAll, T12_MixedList) {
  // u0, u_arr[0], u_arr[1]
  const auto* bd = findBD(m_design->getBindDirectives(), "bd_t12");
  ASSERT_NE(bd, nullptr);
  const auto* insts = bd->getBindTargetInstances();
  ASSERT_NE(insts, nullptr);
  ASSERT_EQ(insts->size(), 3u);
  // [0] = RefObj "u0"
  const auto* ro = any_cast<hldb::RefObj>((*insts)[0]);
  ASSERT_NE(ro, nullptr);
  EXPECT_EQ(ro->getName(), "u0");
  // [1] = BitSelect u_arr[0]
  const auto* bs0 = any_cast<hldb::BitSelect>((*insts)[1]);
  ASSERT_NE(bs0, nullptr);
  const auto* pfx0 = bs0->getPrefix<hldb::RefObj>();
  ASSERT_NE(pfx0, nullptr);
  EXPECT_EQ(pfx0->getName(), "u_arr");
  // [2] = BitSelect u_arr[1]
  const auto* bs1 = any_cast<hldb::BitSelect>((*insts)[2]);
  ASSERT_NE(bs1, nullptr);
  const auto* pfx1 = bs1->getPrefix<hldb::RefObj>();
  ASSERT_NE(pfx1, nullptr);
  EXPECT_EQ(pfx1->getName(), "u_arr");
}

// ─── T13–T14: checker_instantiation with instance lists ──────────────────────

TEST_F(BindDirectiveAll, T13_CheckerWithOneInstance) {
  const auto* bd = findBD(m_design->getBindDirectives(), "bd_t13");
  ASSERT_NE(bd, nullptr);
  EXPECT_EQ(bd->getBindTargetScope(), "TargetMod");
  const auto* insts = bd->getBindTargetInstances();
  ASSERT_NE(insts, nullptr);
  ASSERT_EQ(insts->size(), 1u);
  const auto* ro = any_cast<hldb::RefObj>((*insts)[0]);
  ASSERT_NE(ro, nullptr);
  EXPECT_EQ(ro->getName(), "u0");
  ASSERT_NE(bd->getBindSourceInstance(), nullptr);
  EXPECT_EQ(bd->getBindSourceInstance()->getName(), "bd_t13");
}

TEST_F(BindDirectiveAll, T14_CheckerWithBitSelects) {
  const auto* bd = findBD(m_design->getBindDirectives(), "bd_t14");
  ASSERT_NE(bd, nullptr);
  const auto* insts = bd->getBindTargetInstances();
  ASSERT_NE(insts, nullptr);
  ASSERT_EQ(insts->size(), 2u);
  ASSERT_NE(any_cast<hldb::BitSelect>((*insts)[0]), nullptr);
  ASSERT_NE(any_cast<hldb::BitSelect>((*insts)[1]), nullptr);
  ASSERT_NE(bd->getBindSourceInstance(), nullptr);
  EXPECT_EQ(bd->getBindSourceInstance()->getName(), "bd_t14");
}

// ─── T15: Form 1/2 ambiguous — single identifier, no COLON ──────────────────

TEST_F(BindDirectiveAll, T15_Ambiguous_SingleIdentifier) {
  // At parse time: scope="u0", no instances. Elaboration resolves Form 1 vs 2.
  const auto* bd = findBD(m_design->getBindDirectives(), "bd_t15");
  ASSERT_NE(bd, nullptr);
  EXPECT_EQ(bd->getBindTargetScope(), "u0");
  EXPECT_TRUE(bd->getBindTargetInstances() == nullptr);
  ASSERT_NE(bd->getBindSourceInstance(), nullptr);
  EXPECT_EQ(bd->getBindSourceInstance()->getName(), "bd_t15");
}

// ─── T16: Form 2 — hierarchical path bind target ─────────────────────────────

TEST_F(BindDirectiveAll, T16_HierarchicalForm2) {
  // bind top_instances.u0 AsrtMod bd_t16(...)
  // Grammar: bind_target_instance → hierarchical_identifier constant_bit_select
  // The hierarchical path is not extracted into bind_target_scope at parse time;
  // elaboration resolves the full path top_instances.u0 to a specific instance.
  const auto* bd = findBD(m_design->getBindDirectives(), "bd_t16");
  ASSERT_NE(bd, nullptr);
  EXPECT_EQ(bd->getBindTargetScope(), "")
      << "Hierarchical Form 2 path not extracted at parse time";
  EXPECT_TRUE(bd->getBindTargetInstances() == nullptr);
  ASSERT_NE(bd->getBindSourceInstance(), nullptr);
  EXPECT_EQ(bd->getBindSourceInstance()->getName(), "bd_t16");
}

// ─── M1–M5: module-level bind directives ─────────────────────────────────────

TEST_F(BindDirectiveAll, M1_FormOneAllInstances) {
  const auto* mod = hldb::findByName<hldb::Module>(
      "work@top_module_binds", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto* bd = findBD(mod->getBindDirectives(), "bd_m1");
  ASSERT_NE(bd, nullptr);
  EXPECT_EQ(bd->getBindTargetScope(), "TargetMod");
  EXPECT_TRUE(bd->getBindTargetInstances() == nullptr);
  ASSERT_NE(bd->getBindSourceInstance(), nullptr);
  EXPECT_EQ(bd->getBindSourceInstance()->getName(), "bd_m1");
}

TEST_F(BindDirectiveAll, M2_TwoNamedInstances) {
  const auto* mod = hldb::findByName<hldb::Module>(
      "work@top_module_binds", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto* bd = findBD(mod->getBindDirectives(), "bd_m2");
  ASSERT_NE(bd, nullptr);
  EXPECT_EQ(bd->getBindTargetScope(), "TargetMod");
  const auto* insts = bd->getBindTargetInstances();
  ASSERT_NE(insts, nullptr);
  ASSERT_EQ(insts->size(), 2u);
  const auto* ro0 = any_cast<hldb::RefObj>((*insts)[0]);
  ASSERT_NE(ro0, nullptr);
  EXPECT_EQ(ro0->getName(), "ma0");
  const auto* ro1 = any_cast<hldb::RefObj>((*insts)[1]);
  ASSERT_NE(ro1, nullptr);
  EXPECT_EQ(ro1->getName(), "ma1");
}

TEST_F(BindDirectiveAll, M3_BitSelectInstance) {
  const auto* mod = hldb::findByName<hldb::Module>(
      "work@top_module_binds", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto* bd = findBD(mod->getBindDirectives(), "bd_m3");
  ASSERT_NE(bd, nullptr);
  const auto* insts = bd->getBindTargetInstances();
  ASSERT_NE(insts, nullptr);
  ASSERT_EQ(insts->size(), 1u);
  // ma_arr[0]
  const auto* bs = any_cast<hldb::BitSelect>((*insts)[0]);
  ASSERT_NE(bs, nullptr);
  ASSERT_NE(any_cast<hldb::Constant>(bs->getIndex()), nullptr);
  const auto* pfx = bs->getPrefix<hldb::RefObj>();
  ASSERT_NE(pfx, nullptr);
  EXPECT_EQ(pfx->getName(), "ma_arr");
}

TEST_F(BindDirectiveAll, M4_CheckerBind) {
  const auto* mod = hldb::findByName<hldb::Module>(
      "work@top_module_binds", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto* bd = findBD(mod->getBindDirectives(), "bd_m4");
  ASSERT_NE(bd, nullptr);
  EXPECT_EQ(bd->getBindTargetScope(), "TargetMod");
  EXPECT_TRUE(bd->getBindTargetInstances() == nullptr);
  ASSERT_NE(bd->getBindSourceInstance(), nullptr);
  EXPECT_EQ(bd->getBindSourceInstance()->getName(), "bd_m4");
}

TEST_F(BindDirectiveAll, M5_ParamOverride) {
  const auto* mod = hldb::findByName<hldb::Module>(
      "work@top_module_binds", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto* bd = findBD(mod->getBindDirectives(), "bd_m5");
  ASSERT_NE(bd, nullptr);
  EXPECT_EQ(bd->getBindTargetScope(), "TargetMod");
  EXPECT_TRUE(bd->getBindTargetInstances() == nullptr);
  ASSERT_NE(bd->getBindSourceInstance(), nullptr);
  EXPECT_EQ(bd->getBindSourceInstance()->getName(), "bd_m5");
}

// ─── G1–G3: bind directives inside generate blocks ───────────────────────────

TEST_F(BindDirectiveAll, G1_FormOneInGenerateIf) {
  const auto* mod = hldb::findByName<hldb::Module>(
      "work@top_generate_binds", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto bds = genScopeBinds(mod);
  const auto it = std::find_if(bds.begin(), bds.end(),
      [](const hldb::BindDirective* bd) {
        const auto* src = bd->getBindSourceInstance();
        return src && src->getName() == "bd_g1";
      });
  ASSERT_NE(it, bds.end());
  EXPECT_EQ((*it)->getBindTargetScope(), "TargetMod");
  EXPECT_TRUE((*it)->getBindTargetInstances() == nullptr);
}

TEST_F(BindDirectiveAll, G2_InstanceListInGenerateIf) {
  const auto* mod = hldb::findByName<hldb::Module>(
      "work@top_generate_binds", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto bds = genScopeBinds(mod);
  const auto it = std::find_if(bds.begin(), bds.end(),
      [](const hldb::BindDirective* bd) {
        const auto* src = bd->getBindSourceInstance();
        return src && src->getName() == "bd_g2";
      });
  ASSERT_NE(it, bds.end());
  EXPECT_EQ((*it)->getBindTargetScope(), "TargetMod");
  const auto* insts = (*it)->getBindTargetInstances();
  ASSERT_NE(insts, nullptr);
  ASSERT_EQ(insts->size(), 2u);
  const auto* ro0 = any_cast<hldb::RefObj>((*insts)[0]);
  ASSERT_NE(ro0, nullptr);
  EXPECT_EQ(ro0->getName(), "gb0");
  const auto* ro1 = any_cast<hldb::RefObj>((*insts)[1]);
  ASSERT_NE(ro1, nullptr);
  EXPECT_EQ(ro1->getName(), "gb1");
}

TEST_F(BindDirectiveAll, G3_CheckerInGenerateIf) {
  const auto* mod = hldb::findByName<hldb::Module>(
      "work@top_generate_binds", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto bds = genScopeBinds(mod);
  const auto it = std::find_if(bds.begin(), bds.end(),
      [](const hldb::BindDirective* bd) {
        const auto* src = bd->getBindSourceInstance();
        return src && src->getName() == "bd_g3";
      });
  ASSERT_NE(it, bds.end());
  EXPECT_EQ((*it)->getBindTargetScope(), "TargetMod");
  EXPECT_TRUE((*it)->getBindTargetInstances() == nullptr);
  EXPECT_EQ((*it)->getBindSourceInstance()->getName(), "bd_g3");
}

}  // namespace hlc

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
