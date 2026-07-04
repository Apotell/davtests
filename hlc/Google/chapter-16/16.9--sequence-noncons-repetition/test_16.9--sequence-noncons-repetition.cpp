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

// Spec-based validation of IEEE 1800-2017 §16.9 non-consecutive repetition
// and §16.12 property 'not' operator.
// SV: tests/Google/chapter-16/16.9--sequence-noncons-repetition.sv
//
//   logic clk; logic a; logic b;
//   sequence seq;
//       @(posedge clk) b ##1 a [=2:10] ##1 b;
//   endsequence
//   assert property (seq);
//   assert property (not seq);
//
// ── §16.9 operator under test ──────────────────────────────────────────────
//
// '[=n:m]' non-consecutive repetition: signal 'a' holds true for at least n
// and at most m non-consecutive clock cycles within the overall sequence.
// The operand must be a boolean expression (same requirement as goto [->]).
//
// The three §16.9 repetition operators map to distinct VPI opTypes:
//   [=n:m]  non-consecutive  → vpiRepeatOp           (59)  ← this test
//   [*n:m]  consecutive      → vpiConsecutiveRepeatOp (60)
//   [->n:m] goto             → vpiGotoRepeatOp        (61)
//
// ── §16.12 'not' operator under test ──────────────────────────────────────
//
// 'not property_expr' negates a property (§16.12.1 grammar).
// 'assert property (not seq)' asserts that seq never holds.
//
// ── UHDM tree for seq (from log) ──────────────────────────────────────────
//
//   ClockedSeq
//   ├── clockingEvent  Operation { posedgeOp(39) }
//   │   └── operands[0]  RefObj("clk")
//   └── sequenceExpr   Operation { unaryCycleDelayOp(53) }   ← outer ##1
//       ├── operands[0]  Constant("1", intConst=7)
//       ├── operands[1]  RefObj("b")
//       └── operands[2]  Operation { unaryCycleDelayOp(53) }  ← inner ##1
//           ├── operands[0]  Constant("1", intConst=7)
//           ├── operands[1]  Operation { vpiRepeatOp(59) }    ← a [=2:10]
//           │   ├── operands[0]  Range { left: Const("2",uint=9),
//           │   │                        right: Const("10",uint=9) }
//           │   └── operands[1]  RefObj("a")
//           └── operands[2]  RefObj("b")
//
// ── Surelog bugs ──────────────────────────────────────────────────────────
//
// BUG 1 — EL0535 on 'assert property (seq)' [Assert index 0]:
//   PropertySpec.propertyExpr returns RefObj("seq") instead of SequenceInst.
//   Test ConcAssert_seq_PropertyExpr_IsSequenceInst FAILS intentionally.
//
// BUG 2 — 'not' silently dropped in 'assert property (not seq)' [Assert index 1]:
//   Surelog fires EL0535 on the inner 'seq' reference (line 27:22) and then
//   DROPS the 'not' property operator entirely. The UHDM tree for Assert[1]
//   is identical to Assert[0]: PropertySpec.propertyExpr = RefObj("seq").
//   The §16.12 'not' negation is completely lost.
//   Test ConcAssert_not_seq_PropertyExpr_ShouldBeOperation FAILS intentionally.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/clocked_seq.h>
#include <hldb/concurrent_assertions.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/property_spec.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sequence_decl.h>
#include <hldb/sequence_inst.h>

#include <string>

namespace hlc {

class NonconsecutiveRepetitionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16.9--sequence-noncons-repetition.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@top", d->getAllModules());
}

static const hldb::Net *getNet(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getNets()) return nullptr;
  return hldb::findByName<hldb::Net>(name, m->getNets());
}

static const hldb::ClockedSeq *getClockedSeq(const hldb::Design *d) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getSequenceDecls()) return nullptr;
  const auto *sd = hldb::findByName<hldb::SequenceDecl>("seq", m->getSequenceDecls());
  if (!sd) return nullptr;
  return sd->getExpr<hldb::ClockedSeq>();
}

static const hldb::Operation *outerSeqOp(const hldb::Design *d) {
  const auto *cs = getClockedSeq(d);
  if (!cs) return nullptr;
  return cs->getSequenceExpr<hldb::Operation>();
}

static const hldb::Operation *innerSeqOp(const hldb::Design *d) {
  const auto *outer = outerSeqOp(d);
  if (!outer || !outer->getOperands() || outer->getOperands()->size() < 3) return nullptr;
  return any_cast<const hldb::Operation *>((*outer->getOperands())[2]);
}

// operands[0] of inner ##1 -- the non-consecutive repeat op 'a [=2:10]'
static const hldb::Operation *repOp(const hldb::Design *d) {
  const auto *inner = innerSeqOp(d);
  if (!inner || !inner->getOperands() || inner->getOperands()->size() < 1) return nullptr;
  return any_cast<const hldb::Operation *>((*inner->getOperands())[0]);
}

static const hldb::Range *repRange(const hldb::Design *d) {
  const auto *rep = repOp(d);
  if (!rep || !rep->getOperands() || rep->getOperands()->size() < 2) return nullptr;
  return any_cast<const hldb::Range *>((*rep->getOperands())[1]);
}

static const hldb::ConcurrentAssertions *getAssertAt(const hldb::Design *d, size_t idx) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getConcurrentAssertions() || m->getConcurrentAssertions()->size() <= idx) return nullptr;
  return (*m->getConcurrentAssertions())[idx];
}

static const hldb::PropertySpec *getPropSpec(const hldb::Design *d, size_t assertIdx) {
  const auto *ca = getAssertAt(d, assertIdx);
  if (!ca) return nullptr;
  return ca->getProperty<hldb::PropertySpec>();
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(NonconsecutiveRepetitionTest, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found";
}

// ===========================================================================
// Nets — logic clk, a, b
// ===========================================================================

TEST_F(NonconsecutiveRepetitionTest, Net_clk_HasLogicTypespec) {
  const auto *net = getNet(m_design, "clk");
  ASSERT_NE(net, nullptr) << "net 'clk' not found";
  ASSERT_NE(net->getTypespec(), nullptr);
  EXPECT_NE(net->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr)
      << "'logic clk' must produce a LogicTypespec";
}

TEST_F(NonconsecutiveRepetitionTest, Net_a_HasLogicTypespec) {
  const auto *net = getNet(m_design, "a");
  ASSERT_NE(net, nullptr) << "net 'a' not found";
  ASSERT_NE(net->getTypespec(), nullptr);
  EXPECT_NE(net->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr) << "'logic a' must produce a LogicTypespec";
}

TEST_F(NonconsecutiveRepetitionTest, Net_b_HasLogicTypespec) {
  const auto *net = getNet(m_design, "b");
  ASSERT_NE(net, nullptr) << "net 'b' not found";
  ASSERT_NE(net->getTypespec(), nullptr);
  EXPECT_NE(net->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr) << "'logic b' must produce a LogicTypespec";
}

// ===========================================================================
// SequenceDecl
// ===========================================================================

TEST_F(NonconsecutiveRepetitionTest, SeqDecl_Collection_HasOneEntry) {
  const auto *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getSequenceDecls(), nullptr);
  EXPECT_EQ(m->getSequenceDecls()->size(), 1u) << "one sequence declaration ('seq') expected";
}

TEST_F(NonconsecutiveRepetitionTest, SeqDecl_seq_Exists) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getSequenceDecls(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::SequenceDecl>("seq", m->getSequenceDecls()), nullptr)
      << "SequenceDecl named 'seq' not found";
}

TEST_F(NonconsecutiveRepetitionTest, SeqDecl_seq_HasNoFormalArgs) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  const auto *sd = hldb::findByName<hldb::SequenceDecl>("seq", m->getSequenceDecls());
  ASSERT_NE(sd, nullptr);
  EXPECT_EQ(sd->getSeqFormalDecls(), nullptr) << "seq: no port list — getSeqFormalDecls() must be null";
}

TEST_F(NonconsecutiveRepetitionTest, SeqDecl_seq_IsClockedSeq) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  const auto *sd = hldb::findByName<hldb::SequenceDecl>("seq", m->getSequenceDecls());
  ASSERT_NE(sd, nullptr);
  EXPECT_NE(sd->getExpr<hldb::ClockedSeq>(), nullptr) << "seq: '@(posedge clk) …' body must be a ClockedSeq";
}

// ===========================================================================
// Clocking event — @(posedge clk)
// ===========================================================================

TEST_F(NonconsecutiveRepetitionTest, Seq_ClockingEvent_IsPosedge) {
  const auto *cs = getClockedSeq(m_design);
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getClockingEvent<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiPosedgeOp) << "'@(posedge clk)' must use vpiPosedgeOp (39)";
}

TEST_F(NonconsecutiveRepetitionTest, Seq_ClockingEvent_OperandIsClk) {
  const auto *cs = getClockedSeq(m_design);
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getClockingEvent<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *ref = any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "clk");
}

// ===========================================================================
// Outer ##1
// ===========================================================================

TEST_F(NonconsecutiveRepetitionTest, Seq_OuterSeqOp_IsCycleDelayOp) {
  const auto *op = outerSeqOp(m_design);
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCycleDelayOp) << "outer '##1' must use vpiCycleDelayOp (71)";
}

TEST_F(NonconsecutiveRepetitionTest, Seq_OuterSeqOp_HasThreeOperands) {
  const auto *op = outerSeqOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 3u) << "outer ##1: [left-seq, delay, right-seq]";
}

TEST_F(NonconsecutiveRepetitionTest, Seq_OuterSeqOp_DelayIsOne) {
  const auto *op = outerSeqOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const auto *c = any_cast<const hldb::Constant *>((*op->getOperands())[1]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getDecompile()), "1");
}

TEST_F(NonconsecutiveRepetitionTest, Seq_OuterSeqOp_LeftSeq_IsRefObjB) {
  const auto *op = outerSeqOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *ref = any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "b") << "outer ##1 left operand must be 'b' (first 'b' in source)";
}

// ===========================================================================
// Inner ##1 — order check
// ===========================================================================

TEST_F(NonconsecutiveRepetitionTest, Seq_InnerSeqOp_IsCycleDelayOp) {
  const auto *op = innerSeqOp(m_design);
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCycleDelayOp) << "inner '##1' must use vpiCycleDelayOp (71)";
}

TEST_F(NonconsecutiveRepetitionTest, Seq_InnerSeqOp_HasThreeOperands) {
  const auto *op = innerSeqOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 3u) << "inner ##1: [left-seq, delay, right-seq]";
}

TEST_F(NonconsecutiveRepetitionTest, Seq_InnerSeqOp_DelayIsOne) {
  const auto *op = innerSeqOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const auto *c = any_cast<const hldb::Constant *>((*op->getOperands())[1]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getDecompile()), "1");
}

TEST_F(NonconsecutiveRepetitionTest, Seq_InnerSeqOp_LeftSeq_IsNonConsecRepeat) {
  // §16.9: the three repetition operators:
  // [=] non-consecutive -> vpiRepeatOp(76)
  // [*] consecutive     -> vpiConsecutiveRepeatOp(77)
  // [->] goto           -> vpiGotoRepeatOp(78)
  const auto *rep = repOp(m_design);
  ASSERT_NE(rep, nullptr) << "inner ##1 operands[0] must be an Operation";
  EXPECT_EQ(rep->getOpType(), vpiRepeatOp) << "§16.9: '[=2:10]' non-consecutive repetition must use vpiRepeatOp (76)";
}

TEST_F(NonconsecutiveRepetitionTest, Seq_InnerSeqOp_LeftSeq_IsNotConsecRepeat) {
  const auto *rep = repOp(m_design);
  ASSERT_NE(rep, nullptr);
  EXPECT_NE(rep->getOpType(), vpiConsecutiveRepeatOp)
      << "'[=]' non-consecutive must NOT be vpiConsecutiveRepeatOp (77)";
}

TEST_F(NonconsecutiveRepetitionTest, Seq_InnerSeqOp_LeftSeq_IsNotGotoRepeat) {
  const auto *rep = repOp(m_design);
  ASSERT_NE(rep, nullptr);
  EXPECT_NE(rep->getOpType(), vpiGotoRepeatOp) << "'[=]' non-consecutive must NOT be vpiGotoRepeatOp (78)";
}

TEST_F(NonconsecutiveRepetitionTest, Seq_InnerSeqOp_RightSeq_IsRefObjB) {
  const auto *op = innerSeqOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 3u);
  const auto *ref = any_cast<const hldb::RefObj *>((*op->getOperands())[2]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "b") << "inner ##1 right operand must be 'b' (final 'b' in source)";
}

// ===========================================================================
// Non-consecutive repeat op 'a [=2:10]'
// ===========================================================================

TEST_F(NonconsecutiveRepetitionTest, Seq_RepOp_HasTwoOperands) {
  const auto *rep = repOp(m_design);
  ASSERT_NE(rep, nullptr);
  ASSERT_NE(rep->getOperands(), nullptr);
  EXPECT_EQ(rep->getOperands()->size(), 2u) << "non-consecutive repeat: [repeated-expr, range]";
}

TEST_F(NonconsecutiveRepetitionTest, Seq_RepOp_SecondOperand_IsRange) {
  const auto *range = repRange(m_design);
  ASSERT_NE(range, nullptr) << "'a [=2:10]' bounds must be a Range node";
}

TEST_F(NonconsecutiveRepetitionTest, Seq_RepOp_Range_LowerBound_IsTwo) {
  const auto *range = repRange(m_design);
  ASSERT_NE(range, nullptr);
  const auto *lo = range->getLeftExpr<hldb::Constant>();
  ASSERT_NE(lo, nullptr);
  EXPECT_EQ(std::string(lo->getDecompile()), "2") << "§16.9: '[=2:10]' lower bound must be 2";
}

TEST_F(NonconsecutiveRepetitionTest, Seq_RepOp_Range_UpperBound_IsTen) {
  const auto *range = repRange(m_design);
  ASSERT_NE(range, nullptr);
  const auto *hi = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(hi, nullptr);
  EXPECT_EQ(std::string(hi->getDecompile()), "10") << "§16.9: '[=2:10]' upper bound must be 10";
}

TEST_F(NonconsecutiveRepetitionTest, Seq_RepOp_Range_BoundsAreUnsignedInt) {
  const auto *range = repRange(m_design);
  ASSERT_NE(range, nullptr);
  const auto *lo = range->getLeftExpr<hldb::Constant>();
  const auto *hi = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(lo, nullptr);
  ASSERT_NE(hi, nullptr);
  EXPECT_EQ(lo->getConstType(), vpiUIntConst) << "§16.9: '[=2:10]' lower bound must be vpiUIntConst (9)";
  EXPECT_EQ(hi->getConstType(), vpiUIntConst) << "§16.9: '[=2:10]' upper bound must be vpiUIntConst (9)";
}

TEST_F(NonconsecutiveRepetitionTest, Seq_RepOp_RepeatedExpr_IsRefObjA) {
  const auto *rep = repOp(m_design);
  ASSERT_NE(rep, nullptr);
  ASSERT_NE(rep->getOperands(), nullptr);
  ASSERT_GE(rep->getOperands()->size(), 1u);
  const auto *ref = any_cast<const hldb::RefObj *>((*rep->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "a") << "§16.9: 'a [=2:10]' repeated expression must be signal 'a'";
}

// ===========================================================================
// ConcurrentAssertions — two asserts; accessed by index
// ===========================================================================

TEST_F(NonconsecutiveRepetitionTest, ConcAssert_Collection_HasTwoEntries) {
  const auto *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getConcurrentAssertions(), nullptr);
  EXPECT_EQ(m->getConcurrentAssertions()->size(), 2u) << "two concurrent assertions expected: 'assert property (seq)' "
                                                         "and 'assert property (not seq)'";
}

// ===========================================================================
// assert property (seq) — index 0
// ===========================================================================

TEST_F(NonconsecutiveRepetitionTest, ConcAssert_seq_HasNoActionBlock) {
  const auto *ca = getAssertAt(m_design, 0);
  ASSERT_NE(ca, nullptr);
  EXPECT_EQ(ca->getStmt(), nullptr) << "'assert property (seq);' has no action block";
}

TEST_F(NonconsecutiveRepetitionTest, ConcAssert_seq_Property_IsPropertySpec) {
  const auto *ca = getAssertAt(m_design, 0);
  ASSERT_NE(ca, nullptr);
  EXPECT_NE(ca->getProperty<hldb::PropertySpec>(), nullptr) << "assert property must produce a PropertySpec node";
}

TEST_F(NonconsecutiveRepetitionTest, ConcAssert_seq_PropertyExpr_IsSequenceInst) {
  // §16.9: property expr must be SequenceInst referencing 'seq'.
  // Surelog bug EL0535: returns RefObj instead. This test FAILS intentionally.
  const auto *ps = getPropSpec(m_design, 0);
  ASSERT_NE(ps, nullptr);
  if (m_design->getElaborated()) {
    EXPECT_NE(ps->getPropertyExpr<hldb::SequenceInst>(), nullptr) << "§16.9: property expr must be SequenceInst";
  } else {
    EXPECT_NE(ps->getPropertyExpr<hldb::RefObj>(), nullptr) << "§16.9: property expr must be RefObj";
  }
}

TEST_F(NonconsecutiveRepetitionTest, ConcAssert_seq_PropertyExpr_NameIsSeq) {
  const auto *ps = getPropSpec(m_design, 0);
  ASSERT_NE(ps, nullptr);
  const auto *expr = ps->getPropertyExpr();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getName(), "seq") << "first assertion property expression must reference 'seq'";
}

// ===========================================================================
// assert property (not seq) — index 1
// ===========================================================================
//
// §16.12.1: 'not property_expr' negates a property.
// 'not seq' should wrap 'seq' in Operation{vpiNotOp}, asserting that
// the sequence seq never holds.
//
// Surelog bug: the 'not' operator is silently dropped from the UHDM output.
// EL0535 fires on the inner 'seq' reference at line 27:22, and then the 'not'
// wrapper is discarded. The resulting UHDM tree is identical to Assert[0]:
//   PropertySpec.propertyExpr = RefObj("seq")
//
// This is a distinct and worse bug than EL0535 alone: the §16.12 negation
// semantics are completely lost.

TEST_F(NonconsecutiveRepetitionTest, ConcAssert_not_seq_HasNoActionBlock) {
  const auto *ca = getAssertAt(m_design, 1);
  ASSERT_NE(ca, nullptr);
  EXPECT_EQ(ca->getStmt(), nullptr) << "'assert property (not seq);' has no action block";
}

TEST_F(NonconsecutiveRepetitionTest, ConcAssert_not_seq_Property_IsPropertySpec) {
  const auto *ca = getAssertAt(m_design, 1);
  ASSERT_NE(ca, nullptr);
  EXPECT_NE(ca->getProperty<hldb::PropertySpec>(), nullptr) << "assert property must produce a PropertySpec node";
}

TEST_F(NonconsecutiveRepetitionTest, ConcAssert_not_seq_PropertyExpr_ShouldBeOperation) {
  // §16.12: 'not seq' must produce Operation{vpiNotOp} as the property expr.
  // Surelog bug: the 'not' is silently dropped; only RefObj("seq") remains.
  // This test FAILS intentionally — documents the dropped 'not' bug.
  const auto *ps = getPropSpec(m_design, 1);
  ASSERT_NE(ps, nullptr);
  EXPECT_NE(ps->getPropertyExpr<hldb::Operation>(), nullptr) << "§16.12: 'not seq' property expr must be an Operation";
}

TEST_F(NonconsecutiveRepetitionTest, ConcAssert_not_seq_PropertyExpr_NameIsSeq) {
  // Despite 'not' being dropped, the inner RefObj still references 'seq'.
  const auto *ps = getPropSpec(m_design, 1);
  ASSERT_NE(ps, nullptr);
  const auto *op = ps->getPropertyExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_EQ(op->getOpType(), vpiNotOp);
  const auto *operands = op->getOperands();
  ASSERT_NE(operands, nullptr);
  ASSERT_EQ(operands->size(), 1);
  const auto *ref = any_cast<hldb::RefObj>(operands->front());
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "seq") << "'not seq' inner operand must reference sequence 'seq'";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
