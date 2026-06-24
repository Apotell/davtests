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

// Spec-based validation of IEEE 1800-2017 §16.9 consecutive repetition.
// SV: tests/Google/chapter-16/16.9--sequence-cons-repetition.sv
//
//   logic clk; logic a; logic b;
//   sequence seq;    @(posedge clk) b ##1 a [*2:10] ##1 b;
//   sequence seq_2;  @(negedge clk) b ##1 ((a ##[1:2] b) [*2]) ##1 b;
//   assert property (seq);
//   assert property (seq_2);
//
// ── §16.9 rules under test ─────────────────────────────────────────────────
//
// '[*n:m]' consecutive repetition: the operand MAY be a sequence expression.
// So '(a ##[1:2] b) [*2]' is syntactically valid per §16.9.
//
// '[*n]' single-count form: uses constant_expression (no ':').
// '##[n:m]' variable delay uses a Range node as the delay operand.
//
// ── UHDM tree for seq (from log) ───────────────────────────────────────────
//
//   ClockedSeq
//   ├── clockingEvent  Operation { posedgeOp(39) }
//   │   └── operands[0]  RefObj("clk")
//   └── sequenceExpr   Operation { unaryCycleDelayOp(53) }   ← outer ##1
//       ├── operands[0]  Constant("1", intConst=7)
//       ├── operands[1]  RefObj("b")
//       └── operands[2]  Operation { unaryCycleDelayOp(53) }  ← inner ##1
//           ├── operands[0]  Constant("1", intConst=7)
//           ├── operands[1]  Operation { consecutiveRepeatOp(60) }
//           │   ├── operands[0]  Range { left: Const("2",uint=9),
//           │   │                        right: Const("10",uint=9) }
//           │   └── operands[1]  RefObj("a")
//           └── operands[2]  RefObj("b")
//
// ── UHDM tree for seq_2 (from log) ────────────────────────────────────────
//
//   ClockedSeq
//   ├── clockingEvent  Operation { negedgeOp(40) }             ← negedge ✓
//   │   └── operands[0]  RefObj("clk")
//   └── sequenceExpr   Operation { unaryCycleDelayOp(53) }    ← outer ##1
//       ├── operands[0]  Constant("1", intConst=7)
//       ├── operands[1]  RefObj("b")
//       └── operands[2]  Operation { unaryCycleDelayOp(53) }  ← inner ##1
//           ├── operands[0]  Constant("1", intConst=7)
//           ├── operands[1]  Operation { cycleDelayOp(54) }   ← a ##[1:2] b
//           │   │                          ^^^ SURELOG BUG — see below
//           │   ├── operands[0]  Range { left: Const("1",uint=9),
//           │   │                        right: Const("2",uint=9) }
//           │   ├── operands[1]  RefObj("a")
//           │   └── operands[2]  RefObj("b")
//           └── operands[2]  RefObj("b")
//
// ── Surelog bugs ──────────────────────────────────────────────────────────
//
// BUG 1 — EL0535 (both asserts):
//   'assert property (seq)' and 'assert property (seq_2)': sequence names
//   treated as implicit nets; getPropertyExpr() returns RefObj instead of
//   SequenceInst.  Tests *_PropertyExpr_IsSequenceInst FAIL intentionally.
//
// BUG 2 — Dropped '[*2]' on sub-sequence in seq_2:
//   The source '(a ##[1:2] b) [*2]' should produce a consecutiveRepeatOp(60)
//   wrapping the sub-sequence. Surelog silently drops '[*2]' and places the
//   inner cycleDelayOp(54) directly at inner-##1 operands[1].
//   §16.9 explicitly permits consecutive repetition of a sequence expression.
//   Test Seq2_InnerSeqOp_LeftSeq_ShouldBeConsecRepeat FAILS intentionally.
//
// NOTE: '##1' fixed delay → unaryCycleDelayOp(53).
//       '##[n:m]' ranged delay → cycleDelayOp(54) — different opType.

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/clocked_seq.h>
#include <uhdm/concurrent_assertions.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/logic_typespec.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/operation.h>
#include <uhdm/property_spec.h>
#include <uhdm/range.h>
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/sequence_decl.h>
#include <uhdm/sequence_inst.h>

#include <string>

namespace SURELOG {

class ConsecutiveRepetitionTest : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "16.9--sequence-cons-repetition.hlc"});

    ASSERT_NE(m_session,  nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design,   nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const uhdm::Module *getTop(const uhdm::Design *d) {
  return uhdm::findByName<uhdm::Module>("work@top", d->getAllModules());
}

static const uhdm::Net *getNet(const uhdm::Design *d, std::string_view name) {
  const uhdm::Module *m = getTop(d);
  if (!m || !m->getNets()) return nullptr;
  return uhdm::findByName<uhdm::Net>(name, m->getNets());
}

static const uhdm::SequenceDecl *getSeqDecl(const uhdm::Design *d,
                                              std::string_view name) {
  const uhdm::Module *m = getTop(d);
  if (!m || !m->getSequenceDecls()) return nullptr;
  return uhdm::findByName<uhdm::SequenceDecl>(name, m->getSequenceDecls());
}

static const uhdm::ClockedSeq *getClockedSeq(const uhdm::Design *d,
                                               std::string_view seqName) {
  const auto *sd = getSeqDecl(d, seqName);
  if (!sd) return nullptr;
  return sd->getExpr<uhdm::ClockedSeq>();
}

// top-level sequenceExpr of the named sequence
static const uhdm::Operation *outerSeqOp(const uhdm::Design *d,
                                          std::string_view seqName) {
  const auto *cs = getClockedSeq(d, seqName);
  if (!cs) return nullptr;
  return cs->getSequenceExpr<uhdm::Operation>();
}

// operands[2] of the outer ##1 (the second sub-expression)
static const uhdm::Operation *innerSeqOp(const uhdm::Design *d,
                                          std::string_view seqName) {
  const auto *outer = outerSeqOp(d, seqName);
  if (!outer || !outer->getOperands() ||
      outer->getOperands()->size() < 3) return nullptr;
  return any_cast<const uhdm::Operation *>((*outer->getOperands())[2]);
}

// operands[1] of inner ##1:
//   seq   → consecutiveRepeatOp(60)   for 'a [*2:10]'
//   seq_2 → cycleDelayOp(54)          [Surelog bug: '[*2]' dropped]
static const uhdm::Operation *innerLeftOp(const uhdm::Design *d,
                                           std::string_view seqName) {
  const auto *inner = innerSeqOp(d, seqName);
  if (!inner || !inner->getOperands() ||
      inner->getOperands()->size() < 2) return nullptr;
  return any_cast<const uhdm::Operation *>((*inner->getOperands())[1]);
}

// helpers specific to seq's consecutive repeat op
static const uhdm::Range *seqRepRange(const uhdm::Design *d) {
  const auto *rep = innerLeftOp(d, "seq");
  if (!rep || !rep->getOperands() || rep->getOperands()->empty()) return nullptr;
  return any_cast<const uhdm::Range *>((*rep->getOperands())[0]);
}

// ConcurrentAssertions by index (no labels in this SV)
static const uhdm::ConcurrentAssertions *getAssertAt(const uhdm::Design *d,
                                                       size_t idx) {
  const uhdm::Module *m = getTop(d);
  if (!m || !m->getConcurrentAssertions() ||
      m->getConcurrentAssertions()->size() <= idx) return nullptr;
  return (*m->getConcurrentAssertions())[idx];
}

static const uhdm::PropertySpec *getPropSpec(const uhdm::Design *d,
                                              size_t assertIdx) {
  const auto *ca = getAssertAt(d, assertIdx);
  if (!ca) return nullptr;
  return ca->getProperty<uhdm::PropertySpec>();
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(ConsecutiveRepetitionTest, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found";
}

// ===========================================================================
// Nets — logic clk, a, b
// ===========================================================================

TEST_F(ConsecutiveRepetitionTest, Net_clk_HasLogicTypespec) {
  const auto *net = getNet(m_design, "clk");
  ASSERT_NE(net, nullptr) << "net 'clk' not found";
  ASSERT_NE(net->getTypespec(), nullptr);
  EXPECT_NE(net->getTypespec()->getActual<uhdm::LogicTypespec>(), nullptr)
      << "'logic clk' must produce a LogicTypespec";
}

TEST_F(ConsecutiveRepetitionTest, Net_a_HasLogicTypespec) {
  const auto *net = getNet(m_design, "a");
  ASSERT_NE(net, nullptr) << "net 'a' not found";
  ASSERT_NE(net->getTypespec(), nullptr);
  EXPECT_NE(net->getTypespec()->getActual<uhdm::LogicTypespec>(), nullptr)
      << "'logic a' must produce a LogicTypespec";
}

TEST_F(ConsecutiveRepetitionTest, Net_b_HasLogicTypespec) {
  const auto *net = getNet(m_design, "b");
  ASSERT_NE(net, nullptr) << "net 'b' not found";
  ASSERT_NE(net->getTypespec(), nullptr);
  EXPECT_NE(net->getTypespec()->getActual<uhdm::LogicTypespec>(), nullptr)
      << "'logic b' must produce a LogicTypespec";
}

// ===========================================================================
// SequenceDecl collection
// ===========================================================================

TEST_F(ConsecutiveRepetitionTest, SeqDecl_Collection_HasTwoEntries) {
  const auto *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getSequenceDecls(), nullptr);
  EXPECT_EQ(m->getSequenceDecls()->size(), 2u)
      << "two sequence declarations (seq, seq_2) expected";
}

TEST_F(ConsecutiveRepetitionTest, SeqDecl_seq_Exists) {
  EXPECT_NE(getSeqDecl(m_design, "seq"), nullptr)
      << "SequenceDecl named 'seq' not found";
}

TEST_F(ConsecutiveRepetitionTest, SeqDecl_seq2_Exists) {
  EXPECT_NE(getSeqDecl(m_design, "seq_2"), nullptr)
      << "SequenceDecl named 'seq_2' not found";
}

// ===========================================================================
// seq — @(posedge clk) b ##1 a [*2:10] ##1 b
// ===========================================================================

TEST_F(ConsecutiveRepetitionTest, Seq_SeqDecl_HasNoFormalArgs) {
  const auto *sd = getSeqDecl(m_design, "seq");
  ASSERT_NE(sd, nullptr);
  EXPECT_EQ(sd->getSeqFormalDecls(), nullptr)
      << "seq: no port list — getSeqFormalDecls() must be null";
}

TEST_F(ConsecutiveRepetitionTest, Seq_SeqDecl_IsClockedSeq) {
  const auto *sd = getSeqDecl(m_design, "seq");
  ASSERT_NE(sd, nullptr);
  EXPECT_NE(sd->getExpr<uhdm::ClockedSeq>(), nullptr)
      << "seq: '@(posedge clk) …' body must be a ClockedSeq";
}

// ── clocking event: @(posedge clk) ────────────────────────────────────────

TEST_F(ConsecutiveRepetitionTest, Seq_ClockingEvent_IsPosedge) {
  const auto *cs = getClockedSeq(m_design, "seq");
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getClockingEvent<uhdm::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiPosedgeOp)
      << "seq: '@(posedge clk)' must use vpiPosedgeOp (39)";
}

TEST_F(ConsecutiveRepetitionTest, Seq_ClockingEvent_OperandIsClk) {
  const auto *cs = getClockedSeq(m_design, "seq");
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getClockingEvent<uhdm::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *ref =
      any_cast<const uhdm::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "clk");
}

// ── outer ##1 ─────────────────────────────────────────────────────────────

TEST_F(ConsecutiveRepetitionTest, Seq_OuterSeqOp_IsCycleDelayOp) {
  const auto *op = outerSeqOp(m_design, "seq");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiUnaryCycleDelayOp)
      << "seq: outer '##1' must use vpiUnaryCycleDelayOp (53)";
}

TEST_F(ConsecutiveRepetitionTest, Seq_OuterSeqOp_HasThreeOperands) {
  const auto *op = outerSeqOp(m_design, "seq");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 3u)
      << "outer ##1: [delay, left-seq, right-seq]";
}

TEST_F(ConsecutiveRepetitionTest, Seq_OuterSeqOp_DelayIsOne) {
  const auto *op = outerSeqOp(m_design, "seq");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *c =
      any_cast<const uhdm::Constant *>((*op->getOperands())[0]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getValue()), "1");
}

TEST_F(ConsecutiveRepetitionTest, Seq_OuterSeqOp_LeftSeq_IsRefObjB) {
  const auto *op = outerSeqOp(m_design, "seq");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const auto *ref =
      any_cast<const uhdm::RefObj *>((*op->getOperands())[1]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "b")
      << "seq: outer ##1 left operand must be 'b' (first 'b' in source)";
}

// ── inner ##1 — order check ───────────────────────────────────────────────

TEST_F(ConsecutiveRepetitionTest, Seq_InnerSeqOp_IsCycleDelayOp) {
  const auto *op = innerSeqOp(m_design, "seq");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiUnaryCycleDelayOp)
      << "seq: inner '##1' must use vpiUnaryCycleDelayOp (53)";
}

TEST_F(ConsecutiveRepetitionTest, Seq_InnerSeqOp_HasThreeOperands) {
  const auto *op = innerSeqOp(m_design, "seq");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 3u)
      << "inner ##1: [delay, left-seq, right-seq]";
}

TEST_F(ConsecutiveRepetitionTest, Seq_InnerSeqOp_DelayIsOne) {
  const auto *op = innerSeqOp(m_design, "seq");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *c =
      any_cast<const uhdm::Constant *>((*op->getOperands())[0]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getValue()), "1");
}

TEST_F(ConsecutiveRepetitionTest, Seq_InnerSeqOp_LeftSeq_IsConsecRepeat) {
  const auto *rep = innerLeftOp(m_design, "seq");
  ASSERT_NE(rep, nullptr) << "inner ##1 operands[1] must be an Operation";
  EXPECT_EQ(rep->getOpType(), vpiConsecutiveRepeatOp)
      << "seq: 'a [*2:10]' must be left operand of inner ##1";
}

TEST_F(ConsecutiveRepetitionTest, Seq_InnerSeqOp_RightSeq_IsRefObjB) {
  const auto *op = innerSeqOp(m_design, "seq");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 3u);
  const auto *ref =
      any_cast<const uhdm::RefObj *>((*op->getOperands())[2]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "b")
      << "seq: inner ##1 right operand must be 'b' (final 'b' in source)";
}

// ── consecutive repeat 'a [*2:10]' ────────────────────────────────────────

TEST_F(ConsecutiveRepetitionTest, Seq_RepOp_HasTwoOperands) {
  const auto *rep = innerLeftOp(m_design, "seq");
  ASSERT_NE(rep, nullptr);
  ASSERT_NE(rep->getOperands(), nullptr);
  EXPECT_EQ(rep->getOperands()->size(), 2u)
      << "consecutive repeat: [range, repeated-expr]";
}

TEST_F(ConsecutiveRepetitionTest, Seq_RepOp_FirstOperand_IsRange) {
  const auto *range = seqRepRange(m_design);
  ASSERT_NE(range, nullptr)
      << "'a [*2:10]' bounds must be a Range node";
}

TEST_F(ConsecutiveRepetitionTest, Seq_RepOp_Range_LowerBound_IsTwo) {
  const auto *range = seqRepRange(m_design);
  ASSERT_NE(range, nullptr);
  const auto *lo = range->getLeftExpr<uhdm::Constant>();
  ASSERT_NE(lo, nullptr);
  EXPECT_EQ(std::string(lo->getValue()), "2")
      << "§16.9: '[*2:10]' lower bound must be 2";
}

TEST_F(ConsecutiveRepetitionTest, Seq_RepOp_Range_UpperBound_IsTen) {
  const auto *range = seqRepRange(m_design);
  ASSERT_NE(range, nullptr);
  const auto *hi = range->getRightExpr<uhdm::Constant>();
  ASSERT_NE(hi, nullptr);
  EXPECT_EQ(std::string(hi->getValue()), "10")
      << "§16.9: '[*2:10]' upper bound must be 10";
}

TEST_F(ConsecutiveRepetitionTest, Seq_RepOp_Range_BoundsAreUnsignedInt) {
  const auto *range = seqRepRange(m_design);
  ASSERT_NE(range, nullptr);
  const auto *lo = range->getLeftExpr<uhdm::Constant>();
  const auto *hi = range->getRightExpr<uhdm::Constant>();
  ASSERT_NE(lo, nullptr);
  ASSERT_NE(hi, nullptr);
  EXPECT_EQ(lo->getConstType(), vpiUIntConst)
      << "§16.9: '[*2:10]' lower bound must be vpiUIntConst (9)";
  EXPECT_EQ(hi->getConstType(), vpiUIntConst)
      << "§16.9: '[*2:10]' upper bound must be vpiUIntConst (9)";
}

TEST_F(ConsecutiveRepetitionTest, Seq_RepOp_RepeatedExpr_IsRefObjA) {
  const auto *rep = innerLeftOp(m_design, "seq");
  ASSERT_NE(rep, nullptr);
  ASSERT_NE(rep->getOperands(), nullptr);
  ASSERT_GE(rep->getOperands()->size(), 2u);
  const auto *ref =
      any_cast<const uhdm::RefObj *>((*rep->getOperands())[1]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "a")
      << "§16.9: 'a [*2:10]' repeated expression must be signal 'a'";
}

// ===========================================================================
// seq_2 — @(negedge clk) b ##1 ((a ##[1:2] b) [*2]) ##1 b
// ===========================================================================

TEST_F(ConsecutiveRepetitionTest, Seq2_SeqDecl_HasNoFormalArgs) {
  const auto *sd = getSeqDecl(m_design, "seq_2");
  ASSERT_NE(sd, nullptr);
  EXPECT_EQ(sd->getSeqFormalDecls(), nullptr)
      << "seq_2: no port list — getSeqFormalDecls() must be null";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_SeqDecl_IsClockedSeq) {
  const auto *sd = getSeqDecl(m_design, "seq_2");
  ASSERT_NE(sd, nullptr);
  EXPECT_NE(sd->getExpr<uhdm::ClockedSeq>(), nullptr)
      << "seq_2: '@(negedge clk) …' body must be a ClockedSeq";
}

// ── clocking event: @(negedge clk) ────────────────────────────────────────

TEST_F(ConsecutiveRepetitionTest, Seq2_ClockingEvent_IsNegedge) {
  const auto *cs = getClockedSeq(m_design, "seq_2");
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getClockingEvent<uhdm::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiNegedgeOp)
      << "seq_2: '@(negedge clk)' must use vpiNegedgeOp (40)";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_ClockingEvent_IsNotPosedge) {
  const auto *cs = getClockedSeq(m_design, "seq_2");
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getClockingEvent<uhdm::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_NE(op->getOpType(), vpiPosedgeOp)
      << "seq_2 uses negedge, not posedge";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_ClockingEvent_OperandIsClk) {
  const auto *cs = getClockedSeq(m_design, "seq_2");
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getClockingEvent<uhdm::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *ref =
      any_cast<const uhdm::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "clk");
}

// ── outer ##1 ─────────────────────────────────────────────────────────────

TEST_F(ConsecutiveRepetitionTest, Seq2_OuterSeqOp_IsCycleDelayOp) {
  const auto *op = outerSeqOp(m_design, "seq_2");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiUnaryCycleDelayOp)
      << "seq_2: outer '##1' must use vpiUnaryCycleDelayOp (53)";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_OuterSeqOp_HasThreeOperands) {
  const auto *op = outerSeqOp(m_design, "seq_2");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 3u)
      << "outer ##1: [delay, left-seq, right-seq]";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_OuterSeqOp_LeftSeq_IsRefObjB) {
  const auto *op = outerSeqOp(m_design, "seq_2");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const auto *ref =
      any_cast<const uhdm::RefObj *>((*op->getOperands())[1]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "b")
      << "seq_2: outer ##1 left operand must be 'b' (first 'b' in source)";
}

// ── inner ##1 ─────────────────────────────────────────────────────────────

TEST_F(ConsecutiveRepetitionTest, Seq2_InnerSeqOp_IsCycleDelayOp) {
  const auto *op = innerSeqOp(m_design, "seq_2");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiUnaryCycleDelayOp)
      << "seq_2: inner '##1' must use vpiUnaryCycleDelayOp (53)";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_InnerSeqOp_HasThreeOperands) {
  const auto *op = innerSeqOp(m_design, "seq_2");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 3u)
      << "inner ##1: [delay, left-seq, right-seq]";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_InnerSeqOp_LeftSeq_ShouldBeConsecRepeat) {
  // §16.9: '(a ##[1:2] b) [*2]' is valid; UHDM should have
  // consecutiveRepeatOp(60) here wrapping the sub-sequence.
  // Surelog silently drops '[*2]', leaving cycleDelayOp(54) directly.
  // This test FAILS intentionally — Surelog bug.
  const auto *op = innerLeftOp(m_design, "seq_2");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiConsecutiveRepeatOp)
      << "§16.9: '(a ##[1:2] b) [*2]' must produce consecutiveRepeatOp(60); "
         "Surelog bug: '[*2]' dropped, cycleDelayOp(54) returned instead";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_InnerSeqOp_LeftSeq_IsVarCycleDelay) {
  // Documents what Surelog ACTUALLY returns after dropping '[*2]':
  // the bare sub-sequence 'a ##[1:2] b' with cycleDelayOp(54).
  // '##[n:m]' ranged delay uses opType 54; fixed '##n' uses opType 53.
  const auto *op = innerLeftOp(m_design, "seq_2");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), 54 /* vpiCycleDelayOp */)
      << "After Surelog drops '[*2]', inner left op must be cycleDelayOp(54)";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_InnerSeqOp_RightSeq_IsRefObjB) {
  const auto *op = innerSeqOp(m_design, "seq_2");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 3u);
  const auto *ref =
      any_cast<const uhdm::RefObj *>((*op->getOperands())[2]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "b")
      << "seq_2: inner ##1 right operand must be 'b' (final 'b' in source)";
}

// ── sub-sequence 'a ##[1:2] b' (as returned by Surelog after [*2] dropped) ──
//
// cycleDelayOp(54) node at inner-##1 operands[1]:
//   operands[0]  Range { left: Const("1",uint=9), right: Const("2",uint=9) }
//   operands[1]  RefObj("a")
//   operands[2]  RefObj("b")

TEST_F(ConsecutiveRepetitionTest, Seq2_VarCycleDelay_HasThreeOperands) {
  const auto *sub = innerLeftOp(m_design, "seq_2");
  ASSERT_NE(sub, nullptr);
  ASSERT_NE(sub->getOperands(), nullptr);
  EXPECT_EQ(sub->getOperands()->size(), 3u)
      << "'a ##[1:2] b': [delay-range, a, b]";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_VarCycleDelay_DelayIsRange) {
  // '##[1:2]' variable delay — operands[0] is a Range, not a Constant.
  // '##1' fixed delay would have Constant as operands[0].
  const auto *sub = innerLeftOp(m_design, "seq_2");
  ASSERT_NE(sub, nullptr);
  ASSERT_NE(sub->getOperands(), nullptr);
  ASSERT_GE(sub->getOperands()->size(), 1u);
  EXPECT_NE(any_cast<const uhdm::Range *>((*sub->getOperands())[0]), nullptr)
      << "'##[1:2]' variable delay operand must be a Range, not a Constant";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_VarCycleDelay_DelayRange_LowerIsOne) {
  const auto *sub = innerLeftOp(m_design, "seq_2");
  ASSERT_NE(sub, nullptr);
  ASSERT_NE(sub->getOperands(), nullptr);
  ASSERT_GE(sub->getOperands()->size(), 1u);
  const auto *range =
      any_cast<const uhdm::Range *>((*sub->getOperands())[0]);
  ASSERT_NE(range, nullptr);
  const auto *lo = range->getLeftExpr<uhdm::Constant>();
  ASSERT_NE(lo, nullptr);
  EXPECT_EQ(std::string(lo->getValue()), "1")
      << "'##[1:2]' lower delay bound must be 1";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_VarCycleDelay_DelayRange_UpperIsTwo) {
  const auto *sub = innerLeftOp(m_design, "seq_2");
  ASSERT_NE(sub, nullptr);
  ASSERT_NE(sub->getOperands(), nullptr);
  ASSERT_GE(sub->getOperands()->size(), 1u);
  const auto *range =
      any_cast<const uhdm::Range *>((*sub->getOperands())[0]);
  ASSERT_NE(range, nullptr);
  const auto *hi = range->getRightExpr<uhdm::Constant>();
  ASSERT_NE(hi, nullptr);
  EXPECT_EQ(std::string(hi->getValue()), "2")
      << "'##[1:2]' upper delay bound must be 2";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_VarCycleDelay_DelayRange_BoundsAreUnsignedInt) {
  const auto *sub = innerLeftOp(m_design, "seq_2");
  ASSERT_NE(sub, nullptr);
  ASSERT_NE(sub->getOperands(), nullptr);
  ASSERT_GE(sub->getOperands()->size(), 1u);
  const auto *range =
      any_cast<const uhdm::Range *>((*sub->getOperands())[0]);
  ASSERT_NE(range, nullptr);
  const auto *lo = range->getLeftExpr<uhdm::Constant>();
  const auto *hi = range->getRightExpr<uhdm::Constant>();
  ASSERT_NE(lo, nullptr);
  ASSERT_NE(hi, nullptr);
  EXPECT_EQ(lo->getConstType(), vpiUIntConst)
      << "'##[1:2]' lower delay bound must be vpiUIntConst (9)";
  EXPECT_EQ(hi->getConstType(), vpiUIntConst)
      << "'##[1:2]' upper delay bound must be vpiUIntConst (9)";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_VarCycleDelay_LeftIsRefObjA) {
  const auto *sub = innerLeftOp(m_design, "seq_2");
  ASSERT_NE(sub, nullptr);
  ASSERT_NE(sub->getOperands(), nullptr);
  ASSERT_GE(sub->getOperands()->size(), 2u);
  const auto *ref =
      any_cast<const uhdm::RefObj *>((*sub->getOperands())[1]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "a")
      << "'(a ##[1:2] b)' left element must be signal 'a'";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_VarCycleDelay_RightIsRefObjB) {
  const auto *sub = innerLeftOp(m_design, "seq_2");
  ASSERT_NE(sub, nullptr);
  ASSERT_NE(sub->getOperands(), nullptr);
  ASSERT_GE(sub->getOperands()->size(), 3u);
  const auto *ref =
      any_cast<const uhdm::RefObj *>((*sub->getOperands())[2]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "b")
      << "'(a ##[1:2] b)' right element must be signal 'b'";
}

// ===========================================================================
// ConcurrentAssertions — two unlabelled asserts; accessed by index
// ===========================================================================

TEST_F(ConsecutiveRepetitionTest, ConcAssert_Collection_HasTwoEntries) {
  const auto *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getConcurrentAssertions(), nullptr);
  EXPECT_EQ(m->getConcurrentAssertions()->size(), 2u)
      << "two concurrent assertions expected (seq, seq_2)";
}

// ── assert property (seq) — index 0 ───────────────────────────────────────

TEST_F(ConsecutiveRepetitionTest, ConcAssert_seq_HasNoActionBlock) {
  const auto *ca = getAssertAt(m_design, 0);
  ASSERT_NE(ca, nullptr);
  EXPECT_EQ(ca->getStmt(), nullptr)
      << "'assert property (seq);' has no action block";
}

TEST_F(ConsecutiveRepetitionTest, ConcAssert_seq_Property_IsPropertySpec) {
  const auto *ca = getAssertAt(m_design, 0);
  ASSERT_NE(ca, nullptr);
  EXPECT_NE(ca->getProperty<uhdm::PropertySpec>(), nullptr)
      << "assert property must produce a PropertySpec node";
}

TEST_F(ConsecutiveRepetitionTest, ConcAssert_seq_PropertyExpr_IsSequenceInst) {
  // §16.9: property expr must be SequenceInst referencing 'seq'.
  // Surelog bug EL0535: returns RefObj instead. This test FAILS intentionally.
  const auto *ps = getPropSpec(m_design, 0);
  ASSERT_NE(ps, nullptr);
  EXPECT_NE(ps->getPropertyExpr<uhdm::SequenceInst>(), nullptr)
      << "§16.9: property expr must be SequenceInst; "
         "Surelog EL0535: RefObj returned instead";
}

TEST_F(ConsecutiveRepetitionTest, ConcAssert_seq_PropertyExpr_NameIsSeq) {
  const auto *ps = getPropSpec(m_design, 0);
  ASSERT_NE(ps, nullptr);
  const auto *expr = ps->getPropertyExpr();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getName(), "seq")
      << "first assertion property expression must reference 'seq'";
}

// ── assert property (seq_2) — index 1 ─────────────────────────────────────

TEST_F(ConsecutiveRepetitionTest, ConcAssert_seq2_HasNoActionBlock) {
  const auto *ca = getAssertAt(m_design, 1);
  ASSERT_NE(ca, nullptr);
  EXPECT_EQ(ca->getStmt(), nullptr)
      << "'assert property (seq_2);' has no action block";
}

TEST_F(ConsecutiveRepetitionTest, ConcAssert_seq2_Property_IsPropertySpec) {
  const auto *ca = getAssertAt(m_design, 1);
  ASSERT_NE(ca, nullptr);
  EXPECT_NE(ca->getProperty<uhdm::PropertySpec>(), nullptr)
      << "assert property must produce a PropertySpec node";
}

TEST_F(ConsecutiveRepetitionTest, ConcAssert_seq2_PropertyExpr_IsSequenceInst) {
  // §16.9: property expr must be SequenceInst referencing 'seq_2'.
  // Surelog bug EL0535: returns RefObj instead. This test FAILS intentionally.
  const auto *ps = getPropSpec(m_design, 1);
  ASSERT_NE(ps, nullptr);
  EXPECT_NE(ps->getPropertyExpr<uhdm::SequenceInst>(), nullptr)
      << "§16.9: property expr must be SequenceInst; "
         "Surelog EL0535: RefObj returned instead";
}

TEST_F(ConsecutiveRepetitionTest, ConcAssert_seq2_PropertyExpr_NameIsSeq2) {
  const auto *ps = getPropSpec(m_design, 1);
  ASSERT_NE(ps, nullptr);
  const auto *expr = ps->getPropertyExpr();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getName(), "seq_2")
      << "second assertion property expression must reference 'seq_2'";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
