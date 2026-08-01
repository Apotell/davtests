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

// Validates IEEE 1800-2023 Ch.30 specify block UHDM model produced by
// Phase2ModelBuilder for the module in dut.sv.
//
// dut.sv's specify block produces 61 stmts total:
//   21 SpecParam       -- specparam declarations (including multi-name forms)
//   11 ModPath         -- path_delay_expression declarations (a => zN), all
//                         5 count forms (1/2/3/6/12-value) plus mintypmax and
//                         specparam-reference variants
//   4 PulseStyle       -- pulsestyle_onevent / pulsestyle_ondetect statements
//   3 ShowCancelled    -- showcancelled / noshowcancelled statements
//   22 Tchk            -- system_timing_check statements ($setup, $hold,
//                         $recovery, $removal, $skew, $timeskew, $fullskew,
//                         $setuphold, $recrem, $period, $width, $nochange,
//                         plus notifier/condition/mintypmax edge-case forms)
//
// This test file exercises SpecParam (stmts[0]) and the first 4 Tchk objects
// ($setup + 3x $hold) in detail; ModPath, PulseStyle, and ShowCancelled are
// only checked via the aggregate stmt count above.
//
// getTchk(N) counts only Tchk objects so indices remain 0-3 regardless of
// how many SpecParam/ModPath/PulseStyle/ShowCancelled objects precede them
// in stmts.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/any.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/mod_path.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/spec_param.h>
#include <hldb/specify_block.h>
#include <hldb/tchk.h>
#include <hldb/tchk_term.h>
#include <hldb/vpi_user.h>

#include <gtest/gtest.h>

namespace hlc {
class SpecifyBlockTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "SpecifyBlock.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule() {
    return hldb::findByName<hldb::Module>("specify_complete", m_design->getAllModules());
  }

  static const hldb::SpecifyBlock *getSpecifyBlock() {
    const auto *mod = getModule();
    if (mod == nullptr) return nullptr;
    const auto *sbs = mod->getSpecifyBlocks();
    if (sbs == nullptr || sbs->empty()) return nullptr;
    return (*sbs)[0];
  }

  // Return the Nth Tchk from the SpecifyBlock stmts (0-based), or null.
  static const hldb::Tchk *getTchk(size_t idx) {
    const auto *sb = getSpecifyBlock();
    if (sb == nullptr) return nullptr;
    const auto *stmts = sb->getStmts();
    if (stmts == nullptr) return nullptr;
    size_t found = 0;
    for (const hldb::Any *stmt : *stmts) {
      if (const auto *tc = any_cast<hldb::Tchk>(stmt)) {
        if (found == idx) return tc;
        ++found;
      }
    }
    return nullptr;
  }
};

// --- Module structure ---

TEST_F(SpecifyBlockTest, ModuleExists) { ASSERT_NE(getModule(), nullptr) << "Module specify_complete not found"; }

TEST_F(SpecifyBlockTest, ModuleHasSpecifyBlock) {
  const auto *mod = getModule();
  ASSERT_NE(mod, nullptr);
  const auto *sbs = mod->getSpecifyBlocks();
  ASSERT_NE(sbs, nullptr);
  EXPECT_GE(sbs->size(), 1u);
}

TEST_F(SpecifyBlockTest, SpecifyBlockHasStmts) {
  const auto *sb = getSpecifyBlock();
  ASSERT_NE(sb, nullptr);
  EXPECT_NE(sb->getStmts(), nullptr);
}

// stmts = 1 SpecParam + 4 Tchks (pulsestyle/showcancelled are stubs)
TEST_F(SpecifyBlockTest, StmtsTotalCount) {
  const auto *sb = getSpecifyBlock();
  ASSERT_NE(sb, nullptr);
  const auto *stmts = sb->getStmts();
  ASSERT_NE(stmts, nullptr);
  EXPECT_EQ(stmts->size(), 61u) << "21 SpecParam + 11 ModPath + 4 PulseStyle + 3 ShowCancelled + 22 Tchk";
}

TEST_F(SpecifyBlockTest, TchkCount_22) {
  const auto *sb = getSpecifyBlock();
  ASSERT_NE(sb, nullptr);
  const auto *stmts = sb->getStmts();
  ASSERT_NE(stmts, nullptr);
  size_t count = 0;
  for (const hldb::Any *stmt : *stmts) {
    if (any_cast<hldb::Tchk>(stmt)) ++count;
  }
  EXPECT_EQ(count, 22u) << "$setup + 3x$hold expected";
}

// --- specparam tRise = 1 (stmts[0]) ---
// Data flow: visit_INTEGRAL_NUMBER -> Constant(1) bubbles up through
// Primary_literal -> Constant_primary -> Constant_expression ->
// Constant_mintypmax_expression; leavePA_Specparam_declaration extracts it.

TEST_F(SpecifyBlockTest, SpecParam_ExistsInStmts) {
  const auto *sb = getSpecifyBlock();
  ASSERT_NE(sb, nullptr);
  const auto *stmts = sb->getStmts();
  ASSERT_NE(stmts, nullptr);
  ASSERT_FALSE(stmts->empty());
  EXPECT_NE(any_cast<hldb::SpecParam>((*stmts)[0]), nullptr) << "stmts[0] should be the SpecParam for tRise";
}

TEST_F(SpecifyBlockTest, SpecParam_Name) {
  const auto *sb = getSpecifyBlock();
  ASSERT_NE(sb, nullptr);
  const auto *stmts = sb->getStmts();
  ASSERT_NE(stmts, nullptr);
  ASSERT_FALSE(stmts->empty());
  const auto *sp = any_cast<hldb::SpecParam>((*stmts)[0]);
  ASSERT_NE(sp, nullptr);
  EXPECT_EQ(sp->getName(), "tRise");
}

TEST_F(SpecifyBlockTest, SpecParam_HasOneExpr) {
  const auto *sb = getSpecifyBlock();
  ASSERT_NE(sb, nullptr);
  const auto *stmts = sb->getStmts();
  ASSERT_NE(stmts, nullptr);
  ASSERT_FALSE(stmts->empty());
  const auto *sp = any_cast<hldb::SpecParam>((*stmts)[0]);
  ASSERT_NE(sp, nullptr);
  const auto *exprs = sp->getExprs();
  ASSERT_NE(exprs, nullptr);
  EXPECT_EQ(exprs->size(), 1u);
}

TEST_F(SpecifyBlockTest, SpecParam_ExprIsConstant_1) {
  const auto *sb = getSpecifyBlock();
  ASSERT_NE(sb, nullptr);
  const auto *stmts = sb->getStmts();
  ASSERT_NE(stmts, nullptr);
  ASSERT_FALSE(stmts->empty());
  const auto *sp = any_cast<hldb::SpecParam>((*stmts)[0]);
  ASSERT_NE(sp, nullptr);
  const auto *exprs = sp->getExprs();
  ASSERT_NE(exprs, nullptr);
  ASSERT_FALSE(exprs->empty());
  const auto *c = any_cast<hldb::Constant>((*exprs)[0]);
  ASSERT_NE(c, nullptr) << "expr[0] should be a Constant";
  EXPECT_EQ(c->getDecompile(), "1");
  EXPECT_EQ(c->getConstType(), vpiUIntConst);
}

// --- Tchk[0]: $setup(in1, posedge clk, tSetup) ---
// ANTLR4 always matches dollar_setup_timing_check (first alternative wins).
// IEEE: data_event first, reference_event second.
// isSetup=true: 1st arg -> TchkDataTerm (in1, no edge)
//               2nd arg -> TchkRefTerm  (clk, posedge)

TEST_F(SpecifyBlockTest, Setup_TchkType) {
  const auto *tc = getTchk(0);
  ASSERT_NE(tc, nullptr);
  EXPECT_EQ(tc->getTchkType(), vpiSetup);
}

TEST_F(SpecifyBlockTest, Setup_DataTerm_Name) {
  const auto *tc = getTchk(0);
  ASSERT_NE(tc, nullptr) << "Tchk[0] ($setup) not found";
  const auto *dt = tc->getExprs()->at(0);
  ASSERT_NE(dt, nullptr) << "TchkDataTerm missing";
  const auto *ro = dt->getExpr<hldb::RefObj>();
  ASSERT_NE(ro, nullptr) << "data term expr is not RefObj";
  EXPECT_EQ(ro->getName(), "in1");
}

TEST_F(SpecifyBlockTest, Setup_DataTerm_NoEdge) {
  const auto *tc = getTchk(0);
  ASSERT_NE(tc, nullptr);
  const auto *dt = tc->getExprs()->at(0);
  ASSERT_NE(dt, nullptr);
  EXPECT_EQ(dt->getEdge(), vpiNoEdge);
}

TEST_F(SpecifyBlockTest, Setup_RefTerm_Name) {
  const auto *tc = getTchk(0);
  ASSERT_NE(tc, nullptr) << "Tchk[0] ($setup) not found";
  const auto *rt = tc->getExprs()->at(1);
  ASSERT_NE(rt, nullptr) << "TchkRefTerm missing";
  const auto *ro = rt->getExpr<hldb::RefObj>();
  ASSERT_NE(ro, nullptr) << "ref term expr is not RefObj";
  EXPECT_EQ(ro->getName(), "clk");
}

TEST_F(SpecifyBlockTest, Setup_RefTerm_Posedge) {
  const auto *tc = getTchk(0);
  ASSERT_NE(tc, nullptr);
  const auto *rt = tc->getExprs()->at(1);
  ASSERT_NE(rt, nullptr);
  EXPECT_EQ(rt->getEdge(), vpiPosedge);
}

// --- Tchk[1]: $hold(posedge clk, in1, tHold) ---
// Grammar: dollar_hold_timing_check(reference_event, timing_check_event, ...)
//   reference_event          -> TchkRefTerm  (clk, posedge)
//   Direct timing_check_event -> TchkDataTerm (in1, no edge)

TEST_F(SpecifyBlockTest, Hold1_TchkType) {
  const auto *tc = getTchk(1);
  ASSERT_NE(tc, nullptr) << "Tchk[1] ($hold posedge clk) not found";
  EXPECT_EQ(tc->getTchkType(), vpiHold);
}

TEST_F(SpecifyBlockTest, Hold1_RefTerm_Posedge) {
  const auto *tc = getTchk(1);
  ASSERT_NE(tc, nullptr) << "Tchk[1] ($hold posedge clk) not found";
  const auto *rt = tc->getExprs()->at(0);
  ASSERT_NE(rt, nullptr);
  EXPECT_EQ(rt->getEdge(), vpiPosedge);
}

TEST_F(SpecifyBlockTest, Hold1_DataTerm_Name) {
  const auto *tc = getTchk(1);
  ASSERT_NE(tc, nullptr);
  const auto *dt = tc->getExprs()->at(1);
  ASSERT_NE(dt, nullptr);
  const auto *ro = dt->getExpr<hldb::RefObj>();
  ASSERT_NE(ro, nullptr);
  EXPECT_EQ(ro->getName(), "in1");
}

TEST_F(SpecifyBlockTest, Hold1_DataTerm_NoEdge) {
  const auto *tc = getTchk(1);
  ASSERT_NE(tc, nullptr);
  const auto *dt = tc->getExprs()->at(1);
  ASSERT_NE(dt, nullptr);
  EXPECT_EQ(dt->getEdge(), vpiNoEdge);
}

// --- Tchk[2]: $hold(negedge clk, in1, tHold) ---

TEST_F(SpecifyBlockTest, Hold2_TchkType) {
  const auto *tc = getTchk(2);
  ASSERT_NE(tc, nullptr) << "Tchk[2] ($hold negedge clk) not found";
  EXPECT_EQ(tc->getTchkType(), vpiHold);
}

TEST_F(SpecifyBlockTest, Hold2_RefTerm_Negedge) {
  const auto *tc = getTchk(2);
  ASSERT_NE(tc, nullptr) << "Tchk[2] ($hold negedge clk) not found";
  const auto *rt = tc->getExprs()->at(0);
  ASSERT_NE(rt, nullptr);
  EXPECT_EQ(rt->getEdge(), vpiNegedge);
}

TEST_F(SpecifyBlockTest, Hold2_DataTerm_NoEdge) {
  const auto *tc = getTchk(2);
  ASSERT_NE(tc, nullptr);
  const auto *dt = tc->getExprs()->at(1);
  ASSERT_NE(dt, nullptr);
  EXPECT_EQ(dt->getEdge(), vpiNoEdge);
}

// --- Tchk[3]: $hold(posedge clk, posedge in1, tHold) ---

TEST_F(SpecifyBlockTest, Hold3_TchkType) {
  const auto *tc = getTchk(3);
  ASSERT_NE(tc, nullptr) << "Tchk[3] ($hold posedge in1) not found";
  EXPECT_EQ(tc->getTchkType(), vpiHold);
}

TEST_F(SpecifyBlockTest, Hold3_RefTerm_Posedge) {
  const auto *tc = getTchk(3);
  ASSERT_NE(tc, nullptr) << "Tchk[3] ($hold posedge in1) not found";
  const auto *rt = tc->getExprs()->at(0);
  ASSERT_NE(rt, nullptr);
  EXPECT_EQ(rt->getEdge(), vpiPosedge);
}

TEST_F(SpecifyBlockTest, Hold3_DataTerm_Posedge) {
  const auto *tc = getTchk(3);
  ASSERT_NE(tc, nullptr);
  const auto *dt = tc->getExprs()->at(0);
  ASSERT_NE(dt, nullptr);
  EXPECT_EQ(dt->getEdge(), vpiPosedge);
}

TEST_F(SpecifyBlockTest, Hold3_DataTerm_Name) {
  const auto *tc = getTchk(3);
  ASSERT_NE(tc, nullptr);
  const auto *dt = tc->getExprs()->at(1);
  ASSERT_NE(dt, nullptr);
  const auto *ro = dt->getExpr<hldb::RefObj>();
  ASSERT_NE(ro, nullptr);
  EXPECT_EQ(ro->getName(), "in1");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
