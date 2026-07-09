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

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/any.h>
#include <hldb/assignment.h>
#include <hldb/clocked_seq.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/sequence_decl.h>
#include <hldb/sequence_inst.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

#include <gtest/gtest.h>

#include <filesystem>

namespace hlc {

// ============================================================================
// Test fixture — compiles tests/SequenceExpr/dut.sv once for all test cases.
// ============================================================================
class SequenceExprTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "SequenceExpr.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::Module *getModule() const {
    return getByName<hldb::Module>("work@sequence_expr_coverage", m_design->getAllModules());
  }

  // Find a named sequence_decl inside the module.
  const hldb::SequenceDecl *findSeq(std::string_view name) const {
    const hldb::Module *mod = getModule();
    if (mod == nullptr) return nullptr;
    return getByName<hldb::SequenceDecl>(name, mod->getSequenceDecls());
  }

  // Return the expr of a named sequence, cast to Operation.
  // Returns nullptr if the sequence is missing or expr is not an Operation.
  const hldb::Operation *seqOp(std::string_view name) const {
    const hldb::SequenceDecl *sd = findSeq(name);
    if (sd == nullptr) return nullptr;
    return any_cast<hldb::Operation>(sd->getExpr());
  }
};

// ============================================================================
// Module and sequence count sanity
// ============================================================================
TEST_F(SequenceExprTest, ModuleExists) { ASSERT_NE(getModule(), nullptr); }

TEST_F(SequenceExprTest, SequenceDeclsPresent) {
  const hldb::Module *mod = getModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getSequenceDecls(), nullptr);
  // dut.sv declares named sequences: 4 helpers + alt1(a-f) + alt2(a-g) +
  // alt3(a-m) + alt4(a-k) + alt5(a-f) + alt6(a-g) + alt7(a-d) + alt8(a-c) +
  // alt9(a-d) + alt10(a-d) + alt11(a-e) + alt12(a-d) + alt13(a-g) + combos
  EXPECT_GT(mod->getSequenceDecls()->size(), 40u);
}

// ============================================================================
// Alt 1 — leading cycle_delay_range  (vpiUnaryCycleDelayOp)
// ============================================================================
TEST_F(SequenceExprTest, Alt1_Fixed_OpType) {
  // ##2 a  — single leading delay
  const auto *op = seqOp("alt1_fixed");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiUnaryCycleDelayOp);
}

TEST_F(SequenceExprTest, Alt1_Fixed_TwoOperands) {
  // ##2 a  → [Const(2), RefObj(a)]
  const auto *op = seqOp("alt1_fixed");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u);
}

TEST_F(SequenceExprTest, Alt1_Fixed_FirstOperandIsConstant) {
  // First operand is the delay value (##2 → Constant)
  const auto *op = seqOp("alt1_fixed");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Constant>((*op->getOperands())[0]), nullptr);
}

// ============================================================================
// Alt 2 — binary cycle_delay_range  (vpiCycleDelayOp)
// ============================================================================
TEST_F(SequenceExprTest, Alt2_Fixed_OpType) {
  // a ##1 b
  const auto *op = seqOp("alt2_fixed");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCycleDelayOp);
}

TEST_F(SequenceExprTest, Alt2_Fixed_ThreeOperands) {
  // a ##1 b  → [RefObj(a), Const(1), RefObj(b)]
  const auto *op = seqOp("alt2_fixed");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 3u);
}

TEST_F(SequenceExprTest, Alt2_Range_OpType) {
  // a ##[1:4] b
  const auto *op = seqOp("alt2_range");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCycleDelayOp);
}

TEST_F(SequenceExprTest, Alt2_Chain3_ThreeTerms) {
  // a ##1 b ##1 c — right-nested; outer has 3 direct items
  const auto *op = seqOp("alt2_chain3");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCycleDelayOp);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 3u);
}

// ============================================================================
// Alt 3 — consecutive_repetition  [* …]
// ============================================================================
TEST_F(SequenceExprTest, Alt3_Consec_Exact_OpType) {
  // a[*3]
  const auto *op = seqOp("alt3_consec_exact");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiConsecutiveRepeatOp);
}

TEST_F(SequenceExprTest, Alt3_Consec_Exact_TwoOperands) {
  // a[*3]  → [RefObj(a), Const(3)]
  const auto *op = seqOp("alt3_consec_exact");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u);
}

TEST_F(SequenceExprTest, Alt3_Consec_Exact_SubjectIsRefObj) {
  const auto *op = seqOp("alt3_consec_exact");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *subj = any_cast<hldb::RefObj>((*op->getOperands())[0]);
  ASSERT_NE(subj, nullptr);
  EXPECT_EQ(subj->getName(), "a");
}

TEST_F(SequenceExprTest, Alt3_Consec_Exact_CountIsConstant) {
  const auto *op = seqOp("alt3_consec_exact");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const auto *cnt = any_cast<hldb::Constant>((*op->getOperands())[1]);
  ASSERT_NE(cnt, nullptr);
  EXPECT_EQ(cnt->getValue(), "3");
}

TEST_F(SequenceExprTest, Alt3_Consec_Range_OpType) {
  // a[*2:5]
  const auto *op = seqOp("alt3_consec_range");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiConsecutiveRepeatOp);
}

TEST_F(SequenceExprTest, Alt3_Consec_Range_SecondOperandIsRange) {
  // a[*2:5]  → [RefObj(a), Range(2,5)]
  const auto *op = seqOp("alt3_consec_range");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  EXPECT_NE(any_cast<hldb::Range>((*op->getOperands())[1]), nullptr);
}

TEST_F(SequenceExprTest, Alt3_Consec_Star_OpType) {
  // a[*]  — zero-or-more
  const auto *op = seqOp("alt3_consec_star");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiConsecutiveRepeatOp);
}

TEST_F(SequenceExprTest, Alt3_Consec_Star_RangeLeft_IsZero) {
  // a[*]  → Range(0,$)
  const auto *op = seqOp("alt3_consec_star");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const auto *rng = any_cast<hldb::Range>((*op->getOperands())[1]);
  ASSERT_NE(rng, nullptr);
  const auto *lc = any_cast<hldb::Constant>(rng->getLeftExpr());
  ASSERT_NE(lc, nullptr);
  EXPECT_EQ(lc->getValue(), "0");
}

TEST_F(SequenceExprTest, Alt3_Consec_Plus_OpType) {
  // a[+]  — one-or-more
  const auto *op = seqOp("alt3_consec_plus");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiConsecutiveRepeatOp);
}

TEST_F(SequenceExprTest, Alt3_Consec_Plus_RangeLeft_IsOne) {
  // a[+]  → Range(1,$)
  const auto *op = seqOp("alt3_consec_plus");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const auto *rng = any_cast<hldb::Range>((*op->getOperands())[1]);
  ASSERT_NE(rng, nullptr);
  const auto *lc = any_cast<hldb::Constant>(rng->getLeftExpr());
  ASSERT_NE(lc, nullptr);
  EXPECT_EQ(lc->getValue(), "1");
}

TEST_F(SequenceExprTest, Alt3_Consec_Unbounded_RightIsDollar) {
  // a[*1:$]  → Range(1,$)
  const auto *op = seqOp("alt3_consec_unbounded");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const auto *rng = any_cast<hldb::Range>((*op->getOperands())[1]);
  ASSERT_NE(rng, nullptr);
  const auto *rc = any_cast<hldb::Constant>(rng->getRightExpr());
  ASSERT_NE(rc, nullptr);
  EXPECT_EQ(rc->getValue(), "$");
}

// ============================================================================
// Alt 3 — non_consecutive_repetition  [= …]  (vpiRepeatOp)
// ============================================================================
TEST_F(SequenceExprTest, Alt3_NonConsec_Exact_OpType) {
  // a[=2]
  const auto *op = seqOp("alt3_nonconsec_exact");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiRepeatOp);
}

TEST_F(SequenceExprTest, Alt3_NonConsec_Exact_TwoOperands) {
  // a[=2]  → [RefObj(a), Const(2)]
  const auto *op = seqOp("alt3_nonconsec_exact");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u);
}

TEST_F(SequenceExprTest, Alt3_NonConsec_Exact_SubjectIsRefObj) {
  const auto *op = seqOp("alt3_nonconsec_exact");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *subj = any_cast<hldb::RefObj>((*op->getOperands())[0]);
  ASSERT_NE(subj, nullptr);
  EXPECT_EQ(subj->getName(), "a");
}

TEST_F(SequenceExprTest, Alt3_NonConsec_Range_OpType) {
  // a[=1:4]
  const auto *op = seqOp("alt3_nonconsec_range");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiRepeatOp);
}

TEST_F(SequenceExprTest, Alt3_NonConsec_Range_SecondIsRange) {
  const auto *op = seqOp("alt3_nonconsec_range");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  EXPECT_NE(any_cast<hldb::Range>((*op->getOperands())[1]), nullptr);
}

TEST_F(SequenceExprTest, Alt3_NonConsec_Unbounded_RightIsDollar) {
  // a[=1:$]
  const auto *op = seqOp("alt3_nonconsec_unbounded");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const auto *rng = any_cast<hldb::Range>((*op->getOperands())[1]);
  ASSERT_NE(rng, nullptr);
  const auto *rc = any_cast<hldb::Constant>(rng->getRightExpr());
  ASSERT_NE(rc, nullptr);
  EXPECT_EQ(rc->getValue(), "$");
}

// ============================================================================
// Alt 3 — goto_repetition  [-> …]  (vpiGotoRepeatOp)
// ============================================================================
TEST_F(SequenceExprTest, Alt3_Goto_Exact_OpType) {
  // a[->1]
  const auto *op = seqOp("alt3_goto_exact");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiGotoRepeatOp);
}

TEST_F(SequenceExprTest, Alt3_Goto_Exact_TwoOperands) {
  // a[->1]  → [RefObj(a), Const(1)]
  const auto *op = seqOp("alt3_goto_exact");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u);
}

TEST_F(SequenceExprTest, Alt3_Goto_Exact_SubjectIsRefObj) {
  const auto *op = seqOp("alt3_goto_exact");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *subj = any_cast<hldb::RefObj>((*op->getOperands())[0]);
  ASSERT_NE(subj, nullptr);
  EXPECT_EQ(subj->getName(), "a");
}

TEST_F(SequenceExprTest, Alt3_Goto_Range_OpType) {
  // a[->2:4]
  const auto *op = seqOp("alt3_goto_range");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiGotoRepeatOp);
}

TEST_F(SequenceExprTest, Alt3_Goto_Range_SecondIsRange) {
  const auto *op = seqOp("alt3_goto_range");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  EXPECT_NE(any_cast<hldb::Range>((*op->getOperands())[1]), nullptr);
}

TEST_F(SequenceExprTest, Alt3_Goto_Unbounded_RightIsDollar) {
  // a[->1:$]
  const auto *op = seqOp("alt3_goto_unbounded");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const auto *rng = any_cast<hldb::Range>((*op->getOperands())[1]);
  ASSERT_NE(rng, nullptr);
  const auto *rc = any_cast<hldb::Constant>(rng->getRightExpr());
  ASSERT_NE(rc, nullptr);
  EXPECT_EQ(rc->getValue(), "$");
}

// ============================================================================
// Alt 4 — ( expression_or_dist (, match_items)* ) boolean_abbrev?
// ============================================================================
TEST_F(SequenceExprTest, Alt4_Consec_Abbrev_OpType) {
  // (a, cnt += 1)[*3]
  const auto *op = seqOp("alt4_consec_abbrev");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiConsecutiveRepeatOp);
}

TEST_F(SequenceExprTest, Alt4_Consec_Abbrev_ThreeOperands) {
  const auto *op = seqOp("alt4_consec_abbrev");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 3u);
}

TEST_F(SequenceExprTest, Alt4_Consec_Abbrev_OperandOrder) {
  // Verify: (a, cnt += 1)[*3] → [0]=RefObj, [1]=Const("3"), [2]=Assignment(cnt+=1)
  const auto *op = seqOp("alt4_consec_abbrev");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 3u);
  EXPECT_NE(any_cast<hldb::RefObj>((*op->getOperands())[0]), nullptr);      // [0] subject
  EXPECT_NE(any_cast<hldb::Constant>((*op->getOperands())[1]), nullptr);    // [1] count
  EXPECT_NE(any_cast<hldb::Assignment>((*op->getOperands())[2]), nullptr);  // [2] match item
}

TEST_F(SequenceExprTest, Alt4_Consec_RangeAbbrev_OpType) {
  // (a, cnt += 1)[*1:4]
  const auto *op = seqOp("alt4_consec_range_abbrev");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiConsecutiveRepeatOp);
}

TEST_F(SequenceExprTest, Alt4_NonConsec_Abbrev_OpType) {
  // (a, b = 1)[=2]
  const auto *op = seqOp("alt4_nonconsec_abbrev");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiRepeatOp);
}

TEST_F(SequenceExprTest, Alt4_Goto_Abbrev_OpType) {
  // (a, b = 1)[->1]
  const auto *op = seqOp("alt4_goto_abbrev");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiGotoRepeatOp);
}

TEST_F(SequenceExprTest, Alt4_Goto_Abbrev_ThreeOperands) {
  // (a, b = 1)[->1]  → [RefObj(a), match_item, Const(1)]
  const auto *op = seqOp("alt4_goto_abbrev");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 3u);
}

// ============================================================================
// Alt 5 — sequence_instance consecutive_repetition?
// ============================================================================
TEST_F(SequenceExprTest, Alt5_Consec_Exact_OpType) {
  // seq_ab[*3]
  const auto *op = seqOp("alt5_consec_exact");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiConsecutiveRepeatOp);
}

TEST_F(SequenceExprTest, Alt5_Consec_Exact_TwoOperands) {
  // seq_ab[*3]  → [RefObj(seq_ab), Const(3)]
  const auto *op = seqOp("alt5_consec_exact");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u);
}

TEST_F(SequenceExprTest, Alt5_Consec_Exact_SubjectName) {
  // First operand names the sequence being repeated
  const auto *op = seqOp("alt5_consec_exact");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *subj = any_cast<hldb::RefObj>((*op->getOperands())[0]);
  ASSERT_NE(subj, nullptr);
  EXPECT_EQ(subj->getName(), "seq_ab");
}

TEST_F(SequenceExprTest, Alt5_Star_RangeLeft_IsZero) {
  // seq_ab[*]  → Range(0,$)
  const auto *op = seqOp("alt5_star");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const auto *rng = any_cast<hldb::Range>((*op->getOperands())[1]);
  ASSERT_NE(rng, nullptr);
  const auto *lc = any_cast<hldb::Constant>(rng->getLeftExpr());
  ASSERT_NE(lc, nullptr);
  EXPECT_EQ(lc->getValue(), "0");
}

TEST_F(SequenceExprTest, Alt5_Plus_RangeLeft_IsOne) {
  // seq_ab[+]  → Range(1,$)
  const auto *op = seqOp("alt5_plus");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const auto *rng = any_cast<hldb::Range>((*op->getOperands())[1]);
  ASSERT_NE(rng, nullptr);
  const auto *lc = any_cast<hldb::Constant>(rng->getLeftExpr());
  ASSERT_NE(lc, nullptr);
  EXPECT_EQ(lc->getValue(), "1");
}

// ============================================================================
// Alt 6 — ( sequence_expr (, match_items)* ) consecutive_repetition?
// ============================================================================
TEST_F(SequenceExprTest, Alt6_MatchConsec_Exact_OpType) {
  // (a ##1 b, cnt += 1)[*2]
  const auto *op = seqOp("alt6_match_consec_exact");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiConsecutiveRepeatOp);
}

TEST_F(SequenceExprTest, Alt6_MatchConsec_Exact_ThreeOperands) {
  // (a ##1 b, cnt += 1)[*2]  → [Op(vpiCycleDelayOp,...), match_item, Const(2)]
  const auto *op = seqOp("alt6_match_consec_exact");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 3u);
}

TEST_F(SequenceExprTest, Alt6_MatchConsec_Exact_SubjectIsCycleDelay) {
  // First operand is the inner (a ##1 b) sequence
  const auto *op = seqOp("alt6_match_consec_exact");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *inner = any_cast<hldb::Operation>((*op->getOperands())[0]);
  ASSERT_NE(inner, nullptr);
  EXPECT_EQ(inner->getOpType(), vpiCycleDelayOp);
}

TEST_F(SequenceExprTest, Alt6_Star_OpType) {
  // (a ##1 b)[*]
  const auto *op = seqOp("alt6_star");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiConsecutiveRepeatOp);
}

TEST_F(SequenceExprTest, Alt6_Plus_OpType) {
  // (a ##1 b)[+]
  const auto *op = seqOp("alt6_plus");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiConsecutiveRepeatOp);
}

// ============================================================================
// Alt 7 — sequence_expr AND sequence_expr  (vpiCompAndOp)
// ============================================================================
TEST_F(SequenceExprTest, Alt7_Simple_OpType) {
  // a and b
  const auto *op = seqOp("alt7_simple");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCompAndOp);
}

TEST_F(SequenceExprTest, Alt7_Simple_TwoOperands) {
  const auto *op = seqOp("alt7_simple");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u);
}

TEST_F(SequenceExprTest, Alt7_Chained_OpType) {
  // a and b and c  — left-associative, outer is also vpiLogAndOp
  const auto *op = seqOp("alt7_chained");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCompAndOp);
}

// ============================================================================
// Alt 8 — sequence_expr INTERSECT sequence_expr  (vpiIntersectOp)
// ============================================================================
TEST_F(SequenceExprTest, Alt8_Simple_OpType) {
  // a intersect b
  const auto *op = seqOp("alt8_simple");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiIntersectOp);
}

TEST_F(SequenceExprTest, Alt8_Simple_TwoOperands) {
  const auto *op = seqOp("alt8_simple");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u);
}

TEST_F(SequenceExprTest, Alt8_WithRep_OpType) {
  // a[*3] intersect (b ##1 c ##1 d)
  const auto *op = seqOp("alt8_with_rep");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiIntersectOp);
}

TEST_F(SequenceExprTest, Alt8_WithRep_LeftIsConsecRepeat) {
  // Left operand of intersect is a[*3] (consecutive repeat)
  const auto *op = seqOp("alt8_with_rep");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *left = any_cast<hldb::Operation>((*op->getOperands())[0]);
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getOpType(), vpiConsecutiveRepeatOp);
}

// ============================================================================
// Alt 9 — sequence_expr OR sequence_expr  (vpiLogOrOp)
// ============================================================================
TEST_F(SequenceExprTest, Alt9_Simple_OpType) {
  // a or b
  const auto *op = seqOp("alt9_simple");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCompOrOp);
}

TEST_F(SequenceExprTest, Alt9_Simple_TwoOperands) {
  const auto *op = seqOp("alt9_simple");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u);
}

// ============================================================================
// Alt 10 — first_match ( sequence_expr (, match_items)* )  (vpiFirstMatchOp)
// ============================================================================
TEST_F(SequenceExprTest, Alt10_Simple_OpType) {
  // first_match(a ##1 b)
  const auto *op = seqOp("alt10_simple");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiFirstMatchOp);
}

TEST_F(SequenceExprTest, Alt10_Simple_OneOperand) {
  const auto *op = seqOp("alt10_simple");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 1u);
}

TEST_F(SequenceExprTest, Alt10_WithMatch_OperandCount) {
  // first_match(a ##[1:3] b, cnt = 0)  → 2 operands (seq + match_item)
  const auto *op = seqOp("alt10_one_match");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u);
}

// ============================================================================
// Alt 11 — expression_or_dist THROUGHOUT sequence_expr  (vpiThroughoutOp)
// ============================================================================
TEST_F(SequenceExprTest, Alt11_Simple_OpType) {
  // a throughout (b ##1 c)
  const auto *op = seqOp("alt11_simple");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiThroughoutOp);
}

TEST_F(SequenceExprTest, Alt11_Simple_TwoOperands) {
  const auto *op = seqOp("alt11_simple");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u);
}

// ============================================================================
// Alt 12 — sequence_expr WITHIN sequence_expr  (vpiWithinOp)
// ============================================================================
TEST_F(SequenceExprTest, Alt12_Simple_OpType) {
  // a within (b ##[0:5] c)
  const auto *op = seqOp("alt12_simple");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiWithinOp);
}

TEST_F(SequenceExprTest, Alt12_Simple_TwoOperands) {
  const auto *op = seqOp("alt12_simple");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u);
}

TEST_F(SequenceExprTest, Alt12_Sequenced_BothOperandsAreCycleDelay) {
  // (a ##1 b) within (c ##[1:10] d)
  const auto *op = seqOp("alt12_sequenced");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const auto *lhs = any_cast<hldb::Operation>((*op->getOperands())[0]);
  const auto *rhs = any_cast<hldb::Operation>((*op->getOperands())[1]);
  ASSERT_NE(lhs, nullptr);
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(lhs->getOpType(), vpiCycleDelayOp);
  EXPECT_EQ(rhs->getOpType(), vpiCycleDelayOp);
}

// ============================================================================
// Alt 13 — clocking_event sequence_expr  (ClockedSeq)
// ============================================================================
TEST_F(SequenceExprTest, Alt13_Posedge_IsClockedSeq) {
  // @(posedge clk) a
  const hldb::SequenceDecl *sd = findSeq("alt13_posedge");
  ASSERT_NE(sd, nullptr);
  EXPECT_NE(any_cast<hldb::ClockedSeq>(sd->getExpr()), nullptr);
}

TEST_F(SequenceExprTest, Alt13_Posedge_HasSequenceExpr) {
  const hldb::SequenceDecl *sd = findSeq("alt13_posedge");
  ASSERT_NE(sd, nullptr);
  const auto *cs = any_cast<hldb::ClockedSeq>(sd->getExpr());
  ASSERT_NE(cs, nullptr);
  EXPECT_NE(cs->getSequenceExpr(), nullptr);
}

TEST_F(SequenceExprTest, Alt13_Sequenced_IsClockedSeq) {
  // @(posedge clk) (a ##1 b)
  const hldb::SequenceDecl *sd = findSeq("alt13_sequenced");
  ASSERT_NE(sd, nullptr);
  EXPECT_NE(any_cast<hldb::ClockedSeq>(sd->getExpr()), nullptr);
}

TEST_F(SequenceExprTest, Alt13_Sequenced_InnerIsCycleDelay) {
  const hldb::SequenceDecl *sd = findSeq("alt13_sequenced");
  ASSERT_NE(sd, nullptr);
  const auto *cs = any_cast<hldb::ClockedSeq>(sd->getExpr());
  ASSERT_NE(cs, nullptr);
  const auto *inner = any_cast<hldb::Operation>(cs->getSequenceExpr());
  ASSERT_NE(inner, nullptr);
  EXPECT_EQ(inner->getOpType(), vpiCycleDelayOp);
}

// ============================================================================
// Combined / nested permutations
// ============================================================================
TEST_F(SequenceExprTest, Combo_AndOr_OuterIsOr) {
  // (a and b) or (c and d)
  const auto *op = seqOp("combo_and_or");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCompOrOp);
}

TEST_F(SequenceExprTest, Combo_AndOr_InnerOperandsAreAnd) {
  // (a and b) or (c and d)  — each branch is vpiLogAndOp
  const auto *op = seqOp("combo_and_or");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const auto *lhs = any_cast<hldb::Operation>((*op->getOperands())[0]);
  const auto *rhs = any_cast<hldb::Operation>((*op->getOperands())[1]);
  ASSERT_NE(lhs, nullptr);
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(lhs->getOpType(), vpiCompAndOp);
  EXPECT_EQ(rhs->getOpType(), vpiCompAndOp);
}

TEST_F(SequenceExprTest, Combo_ThroughoutWithin_OuterIsThroughout) {
  // a throughout (b within (c ##[0:5] d))
  const auto *op = seqOp("combo_throughout_within");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiThroughoutOp);
}

TEST_F(SequenceExprTest, Combo_ThroughoutWithin_InnerIsWithin) {
  const auto *op = seqOp("combo_throughout_within");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const auto *rhs = any_cast<hldb::Operation>((*op->getOperands())[1]);
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getOpType(), vpiWithinOp);
}

TEST_F(SequenceExprTest, Combo_LeadingInstRep_IsUnaryCycleDelay) {
  // ##1 seq_a[*2] ##1 b
  const auto *op = seqOp("combo_leading_inst_rep");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiUnaryCycleDelayOp);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
