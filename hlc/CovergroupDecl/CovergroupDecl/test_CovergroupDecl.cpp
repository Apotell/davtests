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

// Validates IEEE 1800-2017 §19 covergroup HLDB model built by Phase2ModelBuilder.
// Tests covergroup declarations nested inside a module body so they are
// reachable via module->getCoverGroups().
//
// DUT (dut.sv) declares inside module covergroup_dut:
//   cg_basic   — 3 coverpoints, 2 crosses, interleaved coverage_options
//   cg_clocked — covergroup with @(posedge clk) sampling event
//
// CoverGroup stores all coverage_spec_or_option items (coverpoint,
// cover_cross, coverage_option) in order via getStmts() AnyCollection.
// There are no separate typed getCoverPoints()/getCoverCross() accessors.
//
// CoverPoint stores cover_bin and coverage_option items in source order via
// getStmts() (bins_body_group, mirrors the covergroup_declaration stmt pattern).
// All bins and options are accessible at parse time.
//
// NOTE: cross_item_list stores RefObj placeholders at Phase2 parse time.
// CoverCross::getCrossItems() is only populated after elaboration (when RefObjs
// are resolved to actual CoverPoint objects). Tests with -parse only cannot
// verify cross item count; they verify cross existence/name instead.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/any.h>
#include <hldb/cover_bin.h>
#include <hldb/cover_cross.h>
#include <hldb/cover_group.h>
#include <hldb/cover_point.h>
#include <hldb/coverage_option.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>

namespace hlc {

class CovergroupDeclTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "CovergroupDecl.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::CoverGroup *findCG(const hldb::CoverGroupCollection *col, std::string_view name) {
    if (col == nullptr) return nullptr;
    for (const hldb::CoverGroup *cg : *col) {
      if (cg->getName() == name) return cg;
    }
    return nullptr;
  }

  // Find a CoverPoint by name inside the covergroup's ordered stmt list.
  static const hldb::CoverPoint *findCP(hldb::AnyCollection *stmts, std::string_view name) {
    if (stmts == nullptr) return nullptr;
    for (hldb::Any *item : *stmts) {
      if (const auto *cp = any_cast<hldb::CoverPoint>(item)) {
        if (cp->getName() == name) return cp;
      }
    }
    return nullptr;
  }

  // Find a CoverCross by name inside the covergroup's ordered stmt list.
  static const hldb::CoverCross *findCX(hldb::AnyCollection *stmts, std::string_view name) {
    if (stmts == nullptr) return nullptr;
    for (hldb::Any *item : *stmts) {
      if (const auto *cx = any_cast<hldb::CoverCross>(item)) {
        if (cx->getName() == name) return cx;
      }
    }
    return nullptr;
  }

  // Return the first CoverBin from a CoverPoint's stmt collection, or nullptr.
  static const hldb::CoverBin *firstBin(const hldb::CoverPoint *cp) {
    if (cp == nullptr) return nullptr;
    const auto *stmts = cp->getStmts();
    if (stmts == nullptr) return nullptr;
    for (hldb::Any *item : *stmts) {
      if (const auto *bin = any_cast<hldb::CoverBin>(item)) return bin;
    }
    return nullptr;
  }

  // Return the Nth CoverBin (0-based) from the stmt collection, or nullptr.
  static const hldb::CoverBin *getBinAt(const hldb::CoverPoint *cp, size_t idx) {
    if (cp == nullptr) return nullptr;
    const auto *stmts = cp->getStmts();
    if (stmts == nullptr) return nullptr;
    size_t count = 0;
    for (hldb::Any *item : *stmts) {
      if (const auto *bin = any_cast<hldb::CoverBin>(item)) {
        if (count++ == idx) return bin;
      }
    }
    return nullptr;
  }

  // Count CoverBin items in a CoverPoint's stmt collection.
  static size_t countBins(const hldb::CoverPoint *cp) {
    if (cp == nullptr) return 0;
    const auto *stmts = cp->getStmts();
    if (stmts == nullptr) return 0;
    size_t count = 0;
    for (hldb::Any *item : *stmts) {
      if (any_cast<hldb::CoverBin>(item)) ++count;
    }
    return count;
  }

  // Return the first CoverageOption from a CoverPoint's stmt collection, or nullptr.
  static const hldb::CoverageOption *firstOption(const hldb::CoverPoint *cp) {
    if (cp == nullptr) return nullptr;
    const auto *stmts = cp->getStmts();
    if (stmts == nullptr) return nullptr;
    for (hldb::Any *item : *stmts) {
      if (const auto *opt = any_cast<hldb::CoverageOption>(item)) return opt;
    }
    return nullptr;
  }

  // Count items of type T in the covergroup's ordered stmt list.
  template <typename T>
  static size_t countType(hldb::AnyCollection *stmts) {
    if (stmts == nullptr) return 0;
    size_t count = 0;
    for (hldb::Any *item : *stmts) {
      if (any_cast<T>(item)) ++count;
    }
    return count;
  }
};

// --- Module and covergroup presence ----------------------------------------

TEST_F(CovergroupDeclTest, ModuleExists) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@covergroup_dut", m_design->getAllModules());
  ASSERT_NE(mod, nullptr) << "Module covergroup_dut not found";
}

TEST_F(CovergroupDeclTest, TwoCoverGroups) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@covergroup_dut", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cgs = mod->getCoverGroups();
  ASSERT_NE(cgs, nullptr) << "Module has no covergroup declarations";
  EXPECT_EQ(cgs->size(), 2u) << "Expected cg_basic and cg_clocked in covergroup_dut";
}

TEST_F(CovergroupDeclTest, CgBasicFound) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@covergroup_dut", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_basic");
  ASSERT_NE(cg, nullptr) << "cg_basic not found in module";
}

TEST_F(CovergroupDeclTest, CgClockedFound) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@covergroup_dut", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_clocked");
  ASSERT_NE(cg, nullptr) << "cg_clocked not found in module";
}

// --- cg_basic structure: ordered stmt list ---------------------------------
// getStmts() is the AnyCollection holding coverage_option, cover_point, and
// cover_cross items in source order.

TEST_F(CovergroupDeclTest, CgBasicHasStmts) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@covergroup_dut", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_basic");
  ASSERT_NE(cg, nullptr);
  EXPECT_NE(cg->getStmts(), nullptr) << "cg_basic should have stmt items";
}

TEST_F(CovergroupDeclTest, CgBasicThreeCoverPoints) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@covergroup_dut", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_basic");
  ASSERT_NE(cg, nullptr);
  EXPECT_EQ(countType<hldb::CoverPoint>(cg->getStmts()), 3u) << "Expected cp_op, cp_addr, and cp_valid";
}

TEST_F(CovergroupDeclTest, CgBasicTwoCoverCrosses) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@covergroup_dut", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_basic");
  ASSERT_NE(cg, nullptr);
  EXPECT_EQ(countType<hldb::CoverCross>(cg->getStmts()), 2u) << "Expected cx_op_addr and cx_three_way";
}

// Coverage options are interleaved in stmts; verify total item count.
TEST_F(CovergroupDeclTest, CgBasicStmtTotalCount) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@covergroup_dut", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_basic");
  ASSERT_NE(cg, nullptr);
  const auto *stmts = cg->getStmts();
  ASSERT_NE(stmts, nullptr);
  // [0] coverage_option, [1] cp_op, [2] coverage_option, [3] cp_addr,
  // [4] cp_valid, [5] cx_op_addr, [6] cx_three_way = 7 items
  EXPECT_EQ(stmts->size(), 7u) << "cg_basic should have 7 ordered stmt items";
}

// --- cross_item_list: 2-way cross ------------------------------------------

TEST_F(CovergroupDeclTest, TwoWayCrossFound) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@covergroup_dut", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_basic");
  ASSERT_NE(cg, nullptr);
  const auto *cx = findCX(cg->getStmts(), "cx_op_addr");
  ASSERT_NE(cx, nullptr) << "2-way cross cx_op_addr not found";
}

// --- cross_item_list: 3-way cross ------------------------------------------
// cross_item_list grammar: cross_item COMMA cross_item (COMMA cross_item)*
// A 3-way cross exercises the repeating (COMMA cross_item)* portion.
// Each cross_item is a plain identifier naming a coverpoint (resolved at
// elaboration; getCrossItems() is empty at -parse-only time).

TEST_F(CovergroupDeclTest, ThreeWayCrossFound) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@covergroup_dut", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_basic");
  ASSERT_NE(cg, nullptr);
  const auto *cx = findCX(cg->getStmts(), "cx_three_way");
  ASSERT_NE(cx, nullptr) << "3-way cross cx_three_way not found";
}

TEST_F(CovergroupDeclTest, ThreeWayCrossName) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@covergroup_dut", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_basic");
  ASSERT_NE(cg, nullptr);
  const auto *cx = findCX(cg->getStmts(), "cx_three_way");
  ASSERT_NE(cx, nullptr);
  EXPECT_EQ(cx->getName(), "cx_three_way");
}

TEST_F(CovergroupDeclTest, CrossItemsTypedCollectionNullAtParseTime) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@covergroup_dut", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_basic");
  ASSERT_NE(cg, nullptr);
  const auto *cx = findCX(cg->getStmts(), "cx_three_way");
  ASSERT_NE(cx, nullptr);
  EXPECT_NE(cx->getCrossItems(), nullptr) << "getCrossItems() should hold RefObj collection";
}

// cross_item_list: RefObj placeholders stored in getStmts() at parse time.
// Two items: cp_op and cp_addr.
TEST_F(CovergroupDeclTest, TwoWayCrossHasTwoStmts) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@covergroup_dut", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_basic");
  ASSERT_NE(cg, nullptr);
  const auto *cx = findCX(cg->getStmts(), "cx_op_addr");
  ASSERT_NE(cx, nullptr);
  ASSERT_NE(cx->getCrossItems(), nullptr) << "cx_op_addr stmts should hold cross item RefObjs";
  EXPECT_EQ(cx->getCrossItems()->size(), 2u) << "2-way cross should have 2 RefObj items in stmts";
}

// Three items: cp_op, cp_addr, cp_valid — exercises the repeating grammar rule.
TEST_F(CovergroupDeclTest, ThreeWayCrossHasThreeStmts) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@covergroup_dut", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_basic");
  ASSERT_NE(cg, nullptr);
  const auto *cx = findCX(cg->getStmts(), "cx_three_way");
  ASSERT_NE(cx, nullptr);
  ASSERT_NE(cx->getCrossItems(), nullptr) << "cx_three_way cross-items should hold cross item RefObjs";
  EXPECT_EQ(cx->getCrossItems()->size(), 3u) << "3-way cross should have 3 RefObj items in cross-items";
}

// Validate the cross item names are correct and in source order.
TEST_F(CovergroupDeclTest, ThreeWayCrossStmtNames) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@covergroup_dut", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_basic");
  ASSERT_NE(cg, nullptr);
  const auto *cx = findCX(cg->getStmts(), "cx_three_way");
  ASSERT_NE(cx, nullptr);
  const auto *items = cx->getCrossItems();
  ASSERT_NE(items, nullptr);
  ASSERT_EQ(items->size(), 3u);
  EXPECT_EQ((*items)[0]->getName(), "cp_op") << "first cross item should be cp_op";
  EXPECT_EQ((*items)[1]->getName(), "cp_addr") << "second cross item should be cp_addr";
  EXPECT_EQ((*items)[2]->getName(), "cp_valid") << "third cross item should be cp_valid";
}

// --- cp_op coverpoint -------------------------------------------------------
// CoverPoint stores the first declared bin via getCoverBin() (card:1).
// cp_op's first bin is "rd" (regular bins, binType=0, isWildcard=false).

TEST_F(CovergroupDeclTest, CpOpFound) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@covergroup_dut", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_basic");
  ASSERT_NE(cg, nullptr);
  const auto *cp = findCP(cg->getStmts(), "cp_op");
  ASSERT_NE(cp, nullptr) << "cp_op not found";
}

TEST_F(CovergroupDeclTest, CpOpHasFirstBin) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@covergroup_dut", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_basic");
  ASSERT_NE(cg, nullptr);
  const auto *cp = findCP(cg->getStmts(), "cp_op");
  ASSERT_NE(cp, nullptr);
  EXPECT_NE(firstBin(cp), nullptr) << "cp_op should have at least one bin";
}

TEST_F(CovergroupDeclTest, CpOpFirstBinIsRegular) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@covergroup_dut", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_basic");
  ASSERT_NE(cg, nullptr);
  const auto *cp = findCP(cg->getStmts(), "cp_op");
  ASSERT_NE(cp, nullptr);
  const auto *bin = firstBin(cp);
  ASSERT_NE(bin, nullptr);
  EXPECT_EQ(bin->getBinType(), vpiBinsTypeBins) << "First bin 'rd' should be regular";
  EXPECT_FALSE(bin->getIsWildcard());
}

// --- cp_addr coverpoint -----------------------------------------------------
// cp_addr's first bin is "lo" (regular bins covering low nibble).

TEST_F(CovergroupDeclTest, CpAddrFound) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@covergroup_dut", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_basic");
  ASSERT_NE(cg, nullptr);
  const auto *cp = findCP(cg->getStmts(), "cp_addr");
  ASSERT_NE(cp, nullptr) << "cp_addr not found";
}

TEST_F(CovergroupDeclTest, CpAddrHasFirstBin) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@covergroup_dut", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_basic");
  ASSERT_NE(cg, nullptr);
  const auto *cp = findCP(cg->getStmts(), "cp_addr");
  ASSERT_NE(cp, nullptr);
  EXPECT_NE(firstBin(cp), nullptr) << "cp_addr should have at least one bin";
}

TEST_F(CovergroupDeclTest, CpAddrFirstBinIsRegular) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@covergroup_dut", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_basic");
  ASSERT_NE(cg, nullptr);
  const auto *cp = findCP(cg->getStmts(), "cp_addr");
  ASSERT_NE(cp, nullptr);
  const auto *bin = firstBin(cp);
  ASSERT_NE(bin, nullptr);
  EXPECT_EQ(bin->getBinType(), vpiBinsTypeBins) << "First bin 'lo' should be regular";
}

// --- cp_valid coverpoint ----------------------------------------------------

TEST_F(CovergroupDeclTest, CpValidFound) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@covergroup_dut", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_basic");
  ASSERT_NE(cg, nullptr);
  const auto *cp = findCP(cg->getStmts(), "cp_valid");
  ASSERT_NE(cp, nullptr) << "cp_valid not found";
}

TEST_F(CovergroupDeclTest, CpValidHasFirstBin) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@covergroup_dut", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_basic");
  ASSERT_NE(cg, nullptr);
  const auto *cp = findCP(cg->getStmts(), "cp_valid");
  ASSERT_NE(cp, nullptr);
  EXPECT_NE(firstBin(cp), nullptr) << "cp_valid should have at least one bin";
}

// --- cg_clocked ------------------------------------------------------------

TEST_F(CovergroupDeclTest, CgClockedHasSamplingEvent) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@covergroup_dut", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_clocked");
  ASSERT_NE(cg, nullptr);
  EXPECT_NE(cg->getCoverageEvent(), nullptr) << "cg_clocked should have a sampling event (@posedge clk)";
}

TEST_F(CovergroupDeclTest, CgClockedOneCoverPoint) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@covergroup_dut", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_clocked");
  ASSERT_NE(cg, nullptr);
  EXPECT_EQ(countType<hldb::CoverPoint>(cg->getStmts()), 1u) << "Expected only cp_state";
}

TEST_F(CovergroupDeclTest, CgClockedCpStateName) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@covergroup_dut", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_clocked");
  ASSERT_NE(cg, nullptr);
  const auto *cp = findCP(cg->getStmts(), "cp_state");
  ASSERT_NE(cp, nullptr) << "cp_state not found in cg_clocked";
}

// --- module top ------------------------------------------------------------
// module top declares covergroup cg @(posedge clk) with coverpoints a, b
// and a cross c whose body has 3 bins_selection items (select_expressions).
// Cross stmts: 2 RefObj cross items (a, b) + 3 CoverBin from body = 5 total.

TEST_F(CovergroupDeclTest, ModuleTopExists) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(mod, nullptr) << "Module top not found";
}

TEST_F(CovergroupDeclTest, ModuleTopCgFound) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg");
  ASSERT_NE(cg, nullptr) << "cg not found in module top";
}

TEST_F(CovergroupDeclTest, ModuleTopCgTwoCoverPoints) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg");
  ASSERT_NE(cg, nullptr);
  EXPECT_EQ(countType<hldb::CoverPoint>(cg->getStmts()), 2u) << "Expected coverpoints a and b";
}

TEST_F(CovergroupDeclTest, ModuleTopCgHasSamplingEvent) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg");
  ASSERT_NE(cg, nullptr);
  EXPECT_NE(cg->getCoverageEvent(), nullptr) << "cg in top should have @(posedge clk) event";
}

TEST_F(CovergroupDeclTest, ModuleTopCrossCBodyFiveStmts) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg");
  ASSERT_NE(cg, nullptr);
  const auto *cx = findCX(cg->getStmts(), "c");
  ASSERT_NE(cx, nullptr) << "cross c not found in cg";
  ASSERT_NE(cx->getStmts(), nullptr);
  // 3 CoverBin from bins_selection body (c1, c2, c3)
  EXPECT_EQ(cx->getStmts()->size(), 3u) << "Expected 2 cross-item RefObjs + 3 body CoverBins";
}

// Helper: find a CoverBin by name inside a cross's stmt collection.
static const hldb::CoverBin *findCrossBin(const hldb::CoverCross *cx, std::string_view name) {
  if (cx == nullptr || cx->getStmts() == nullptr) return nullptr;
  for (hldb::Any *item : *cx->getStmts()) {
    if (const auto *bin = any_cast<hldb::CoverBin>(item)) {
      if (bin->getName() == name) return bin;
    }
  }
  return nullptr;
}

// cross c:
//   bins c1 = ! binsof(a) intersect {[100:200]}
//     → value: NOT( INTERSECT(RefObj("a"), ListOp([100:200])) )
//   bins c2 = binsof(a.a2) || binsof(b.b2)
//     → value: LOGOR(RefObj("a.a2"), RefObj("b.b2"))
//   bins c3 = binsof(a.a1) && binsof(b.b4)
//     → value: LOGAND(RefObj("a.a1"), RefObj("b.b4"))
TEST_F(CovergroupDeclTest, ModuleTopCrossBinC1HasValue) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cx = findCX(findCG(mod->getCoverGroups(), "cg")->getStmts(), "c");
  ASSERT_NE(cx, nullptr);
  const auto *bin = findCrossBin(cx, "c1");
  ASSERT_NE(bin, nullptr) << "bins c1 not found";
  ASSERT_NE(bin->getValue(), nullptr) << "c1 should have a value (NOT INTERSECT expr)";
  // c1 = ! binsof(a) intersect {[100:200]} → NOT operation
  SCOPED_TRACE("c1 value AnyType=" + std::to_string((int)bin->getValue()->getAnyType()));
  const auto *notOp = any_cast<hldb::Operation>(bin->getValue());
  ASSERT_NE(notOp, nullptr) << "c1 value should be an Operation (NOT)";
  EXPECT_EQ(notOp->getOpType(), vpiNotOp) << "c1 outer op should be vpiNotOp";
}

TEST_F(CovergroupDeclTest, ModuleTopCrossBinC2HasValue) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cx = findCX(findCG(mod->getCoverGroups(), "cg")->getStmts(), "c");
  ASSERT_NE(cx, nullptr);
  const auto *bin = findCrossBin(cx, "c2");
  ASSERT_NE(bin, nullptr) << "bins c2 not found";
  ASSERT_NE(bin->getValue(), nullptr) << "c2 should have a value (binsof || binsof)";
  // c2 = binsof(a.a2) || binsof(b.b2) → logical-OR operation
  const auto *orOp = any_cast<hldb::Operation>(bin->getValue());
  ASSERT_NE(orOp, nullptr) << "c2 value should be an Operation (||)";
  EXPECT_EQ(orOp->getOpType(), vpiLogOrOp) << "c2 op should be vpiLogOrOp";
  ASSERT_NE(orOp->getOperands(), nullptr);
  ASSERT_EQ(orOp->getOperands()->size(), 2u) << "|| should have two operands";
  // left operand: RefObj("a.a2")
  const auto *lhs = any_cast<hldb::HierPath>((*orOp->getOperands())[0]);
  ASSERT_NE(lhs, nullptr) << "left operand of || should be RefObj";
  EXPECT_EQ(lhs->getName(), "a.a2");
  // right operand: RefObj("b.b2")
  const auto *rhs = any_cast<hldb::HierPath>((*orOp->getOperands())[1]);
  ASSERT_NE(rhs, nullptr) << "right operand of || should be RefObj";
  EXPECT_EQ(rhs->getName(), "b.b2");
}

TEST_F(CovergroupDeclTest, ModuleTopCrossBinC3HasValue) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cx = findCX(findCG(mod->getCoverGroups(), "cg")->getStmts(), "c");
  ASSERT_NE(cx, nullptr);
  const auto *bin = findCrossBin(cx, "c3");
  ASSERT_NE(bin, nullptr) << "bins c3 not found";
  ASSERT_NE(bin->getValue(), nullptr) << "c3 should have a value (binsof && binsof)";
  // c3 = binsof(a.a1) && binsof(b.b4) → logical-AND operation
  const auto *andOp = any_cast<hldb::Operation>(bin->getValue());
  ASSERT_NE(andOp, nullptr) << "c3 value should be an Operation (&&)";
  EXPECT_EQ(andOp->getOpType(), vpiLogAndOp) << "c3 op should be vpiLogAndOp";
  ASSERT_NE(andOp->getOperands(), nullptr);
  ASSERT_EQ(andOp->getOperands()->size(), 2u) << "&& should have two operands";
  // left operand: RefObj("a.a1")
  const auto *lhs = any_cast<hldb::HierPath>((*andOp->getOperands())[0]);
  ASSERT_NE(lhs, nullptr) << "left operand of && should be RefObj";
  EXPECT_EQ(lhs->getName(), "a.a1");
  // right operand: RefObj("b.b4")
  const auto *rhs = any_cast<hldb::HierPath>((*andOp->getOperands())[1]);
  ASSERT_NE(rhs, nullptr) << "right operand of && should be RefObj";
  EXPECT_EQ(rhs->getName(), "b.b4");
}

TEST_F(CovergroupDeclTest, ModuleTopCoverPointAHasFourBins) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg");
  ASSERT_NE(cg, nullptr);
  const auto *cp = findCP(cg->getStmts(), "a");
  ASSERT_NE(cp, nullptr) << "coverpoint 'a' not found";
  EXPECT_EQ(countBins(cp), 4u) << "Expected bins a1, a2, a3, a4";
}

TEST_F(CovergroupDeclTest, ModuleTopCoverPointBHasFourBins) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg");
  ASSERT_NE(cg, nullptr);
  const auto *cp = findCP(cg->getStmts(), "b");
  ASSERT_NE(cp, nullptr) << "coverpoint 'b' not found";
  EXPECT_EQ(countBins(cp), 4u) << "Expected bins b1, b2, b3, b4";
}

TEST_F(CovergroupDeclTest, ModuleTopCoverPointABinNames) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg");
  ASSERT_NE(cg, nullptr);
  const auto *cp = findCP(cg->getStmts(), "a");
  ASSERT_NE(cp, nullptr);
  ASSERT_EQ(countBins(cp), 4u);
  EXPECT_EQ(getBinAt(cp, 0)->getName(), "a1");
  EXPECT_EQ(getBinAt(cp, 1)->getName(), "a2");
  EXPECT_EQ(getBinAt(cp, 2)->getName(), "a3");
  EXPECT_EQ(getBinAt(cp, 3)->getName(), "a4");
}

TEST_F(CovergroupDeclTest, ModuleTopBinA1HasValue) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg");
  ASSERT_NE(cg, nullptr);
  const auto *cp = findCP(cg->getStmts(), "a");
  ASSERT_NE(cp, nullptr);
  const auto *b = getBinAt(cp, 0);
  ASSERT_NE(b, nullptr);
  // bin a1 = { [0:63] } — value is a vpiListOp Operation wrapping a Range
  EXPECT_NE(b->getValue(), nullptr) << "bins a1 should have a value (vpiListOp from range_list)";
}

TEST_F(CovergroupDeclTest, ModuleTopBinA2HasValue) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg");
  ASSERT_NE(cg, nullptr);
  const auto *cp = findCP(cg->getStmts(), "a");
  ASSERT_NE(cp, nullptr);
  const auto *b = getBinAt(cp, 1);
  ASSERT_NE(b, nullptr);
  EXPECT_NE(b->getValue(), nullptr) << "bins a2 should have a value (vpiListOp from range_list)";
}

// coverpoint a stmts order: a1(bin), option.weight(opt), a2(bin), a3(bin),
//                           option.at_least(opt), a4(bin) — 6 items total.
TEST_F(CovergroupDeclTest, ModuleTopCoverPointAStmtCount) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg");
  ASSERT_NE(cg, nullptr);
  const auto *cp = findCP(cg->getStmts(), "a");
  ASSERT_NE(cp, nullptr);
  ASSERT_NE(cp->getStmts(), nullptr);
  EXPECT_EQ(cp->getStmts()->size(), 6u) << "Expected 4 bins + 2 interleaved options";
}

TEST_F(CovergroupDeclTest, ModuleTopCoverPointAStmtOrderPreserved) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg");
  ASSERT_NE(cg, nullptr);
  const auto *cp = findCP(cg->getStmts(), "a");
  ASSERT_NE(cp, nullptr);
  const auto *stmts = cp->getStmts();
  ASSERT_NE(stmts, nullptr);
  ASSERT_EQ(stmts->size(), 6u);
  // [0] a1 (CoverBin)
  EXPECT_NE(any_cast<hldb::CoverBin>((*stmts)[0]), nullptr) << "[0] expected CoverBin (a1)";
  // [1] option.weight = 2 (CoverageOption)
  EXPECT_NE(any_cast<hldb::CoverageOption>((*stmts)[1]), nullptr) << "[1] expected CoverageOption";
  // [2] a2 (CoverBin)
  EXPECT_NE(any_cast<hldb::CoverBin>((*stmts)[2]), nullptr) << "[2] expected CoverBin (a2)";
  // [3] a3 (CoverBin)
  EXPECT_NE(any_cast<hldb::CoverBin>((*stmts)[3]), nullptr) << "[3] expected CoverBin (a3)";
  // [4] option.at_least = 1 (CoverageOption)
  EXPECT_NE(any_cast<hldb::CoverageOption>((*stmts)[4]), nullptr) << "[4] expected CoverageOption";
  // [5] a4 (CoverBin)
  EXPECT_NE(any_cast<hldb::CoverBin>((*stmts)[5]), nullptr) << "[5] expected CoverBin (a4)";
}

// coverpoint b stmts order: option.weight(opt), b1(bin), b2(bin),
//                           option.auto_bin_max(opt), b3(bin), b4(bin) — 6 items.
TEST_F(CovergroupDeclTest, ModuleTopCoverPointBStmtCount) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg");
  ASSERT_NE(cg, nullptr);
  const auto *cp = findCP(cg->getStmts(), "b");
  ASSERT_NE(cp, nullptr);
  ASSERT_NE(cp->getStmts(), nullptr);
  EXPECT_EQ(cp->getStmts()->size(), 6u) << "Expected 4 bins + 2 interleaved options";
}

TEST_F(CovergroupDeclTest, ModuleTopCoverPointBStmtOrderPreserved) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg");
  ASSERT_NE(cg, nullptr);
  const auto *cp = findCP(cg->getStmts(), "b");
  ASSERT_NE(cp, nullptr);
  const auto *stmts = cp->getStmts();
  ASSERT_NE(stmts, nullptr);
  ASSERT_EQ(stmts->size(), 6u);
  // [0] option.weight = 1 (CoverageOption — before first bin)
  EXPECT_NE(any_cast<hldb::CoverageOption>((*stmts)[0]), nullptr) << "[0] expected CoverageOption";
  // [1] b1 (CoverBin)
  EXPECT_NE(any_cast<hldb::CoverBin>((*stmts)[1]), nullptr) << "[1] expected CoverBin (b1)";
  // [2] b2 (CoverBin)
  EXPECT_NE(any_cast<hldb::CoverBin>((*stmts)[2]), nullptr) << "[2] expected CoverBin (b2)";
  // [3] option.auto_bin_max = 8 (CoverageOption — in middle)
  EXPECT_NE(any_cast<hldb::CoverageOption>((*stmts)[3]), nullptr) << "[3] expected CoverageOption";
  // [4] b3 (CoverBin)
  EXPECT_NE(any_cast<hldb::CoverBin>((*stmts)[4]), nullptr) << "[4] expected CoverBin (b3)";
  // [5] b4 (CoverBin)
  EXPECT_NE(any_cast<hldb::CoverBin>((*stmts)[5]), nullptr) << "[5] expected CoverBin (b4)";
}

// --- module cg_extra -------------------------------------------------------
// Helper: find a CoverCross with an empty name (unlabeled cross).
static const hldb::CoverCross *findUnlabeledCX(hldb::AnyCollection *stmts) {
  if (stmts == nullptr) return nullptr;
  for (hldb::Any *item : *stmts) {
    if (const auto *cx = any_cast<hldb::CoverCross>(item)) {
      if (cx->getName().empty()) return cx;
    }
  }
  return nullptr;
}
// Helper: find a CoverPoint with an empty name (unlabeled coverpoint).
static const hldb::CoverPoint *findUnlabeledCP(hldb::AnyCollection *stmts) {
  if (stmts == nullptr) return nullptr;
  for (hldb::Any *item : *stmts) {
    if (const auto *cp = any_cast<hldb::CoverPoint>(item)) {
      if (cp->getName().empty()) return cp;
    }
  }
  return nullptr;
}

TEST_F(CovergroupDeclTest, CgExtraModuleExists) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@cg_extra", m_design->getAllModules());
  ASSERT_NE(mod, nullptr) << "Module cg_extra not found";
}

TEST_F(CovergroupDeclTest, CgExtraFiveCovergroups) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@cg_extra", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cgs = mod->getCoverGroups();
  ASSERT_NE(cgs, nullptr);
  EXPECT_EQ(cgs->size(), 5u) << "Expected cg_params, cg_typed, cg_iff, cg_bins, cg_sampled";
}

// cg_params: tf_port_item_list + @(posedge clk) clocking event
TEST_F(CovergroupDeclTest, CgParamsHasSamplingEvent) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@cg_extra", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_params");
  ASSERT_NE(cg, nullptr) << "cg_params not found";
  EXPECT_NE(cg->getCoverageEvent(), nullptr) << "cg_params should have @(posedge clk) sampling event";
}

// cg_typed: data_type_or_implicit id COLON COVERPOINT (typed coverpoint)
TEST_F(CovergroupDeclTest, CpTypedHasTypespec) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@cg_extra", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_typed");
  ASSERT_NE(cg, nullptr) << "cg_typed not found";
  const auto *cp = findCP(cg->getStmts(), "cp_typed");
  ASSERT_NE(cp, nullptr) << "cp_typed not found";
  EXPECT_NE(cp->getTypespec(), nullptr) << "Typed coverpoint cp_typed should have a RefTypespec";
}

// cg_iff: IFF on coverpoint, IFF on cross, unlabeled coverpoint, unlabeled cross
TEST_F(CovergroupDeclTest, CpGuardedHasIffCondition) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@cg_extra", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_iff");
  ASSERT_NE(cg, nullptr);
  const auto *cp = findCP(cg->getStmts(), "cp_guarded");
  ASSERT_NE(cp, nullptr) << "cp_guarded not found";
  EXPECT_NE(cp->getCondition(), nullptr) << "cp_guarded should have IFF condition";
}

TEST_F(CovergroupDeclTest, CxIffHasCondition) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@cg_extra", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_iff");
  ASSERT_NE(cg, nullptr);
  const auto *cx = findCX(cg->getStmts(), "cx_iff");
  ASSERT_NE(cx, nullptr) << "cx_iff not found";
  EXPECT_NE(cx->getCondition(), nullptr) << "cx_iff should have IFF guard";
}

TEST_F(CovergroupDeclTest, CgIffHasTwoCrosses) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@cg_extra", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_iff");
  ASSERT_NE(cg, nullptr);
  EXPECT_EQ(countType<hldb::CoverCross>(cg->getStmts()), 2u) << "Expected cx_iff (labeled) and one unlabeled cross";
}

TEST_F(CovergroupDeclTest, CgIffHasUnlabeledCross) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@cg_extra", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_iff");
  ASSERT_NE(cg, nullptr);
  EXPECT_NE(findUnlabeledCX(cg->getStmts()), nullptr) << "Expected an unlabeled cover_cross in cg_iff";
}

TEST_F(CovergroupDeclTest, CgIffHasUnlabeledCoverPoint) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@cg_extra", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_iff");
  ASSERT_NE(cg, nullptr);
  EXPECT_NE(findUnlabeledCP(cg->getStmts()), nullptr) << "Expected an unlabeled coverpoint in cg_iff";
}

// cg_bins: advanced bin forms (transition, array, default, default sequence,
//          WITH filter, per-bin IFF, coverage_option in body)
TEST_F(CovergroupDeclTest, CpTransHasFirstBin) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@cg_extra", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_bins");
  ASSERT_NE(cg, nullptr);
  const auto *cp = findCP(cg->getStmts(), "cp_trans");
  ASSERT_NE(cp, nullptr) << "cp_trans not found";
  EXPECT_NE(firstBin(cp), nullptr) << "cp_trans should have a transition bin (rd_to_wr)";
}

TEST_F(CovergroupDeclTest, CpArrHasFirstBin) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@cg_extra", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_bins");
  ASSERT_NE(cg, nullptr);
  const auto *cp = findCP(cg->getStmts(), "cp_arr");
  ASSERT_NE(cp, nullptr) << "cp_arr not found";
  EXPECT_NE(firstBin(cp), nullptr) << "cp_arr should have an array bin (quarters[4])";
}

TEST_F(CovergroupDeclTest, CpDefHasFirstBin) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@cg_extra", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_bins");
  ASSERT_NE(cg, nullptr);
  const auto *cp = findCP(cg->getStmts(), "cp_def");
  ASSERT_NE(cp, nullptr) << "cp_def not found";
  EXPECT_NE(firstBin(cp), nullptr) << "cp_def should have a default bin";
}

TEST_F(CovergroupDeclTest, CpOptBodyHasCoverageOption) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@cg_extra", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_bins");
  ASSERT_NE(cg, nullptr);
  const auto *cp = findCP(cg->getStmts(), "cp_opt_body");
  ASSERT_NE(cp, nullptr) << "cp_opt_body not found";
  EXPECT_NE(firstOption(cp), nullptr) << "cp_opt_body should have coverage_option (option.auto_bin_max)";
}

// cg_sampled: WITH FUNCTION SAMPLE coverage_event
TEST_F(CovergroupDeclTest, CgSampledHasSamplingEvent) {
  const hldb::Module *mod = hldb::findByName<hldb::Module>("work@cg_extra", m_design->getAllModules());
  ASSERT_NE(mod, nullptr);
  const auto *cg = findCG(mod->getCoverGroups(), "cg_sampled");
  ASSERT_NE(cg, nullptr) << "cg_sampled not found";
  EXPECT_NE(cg->getCoverageEvent(), nullptr) << "cg_sampled should have 'with function sample(...)' event";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
