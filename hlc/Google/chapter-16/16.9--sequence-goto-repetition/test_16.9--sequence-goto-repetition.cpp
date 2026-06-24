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

// Spec-based validation of IEEE 1800-2017 §16.9 goto repetition.
// SV: tests/Google/chapter-16/16.9--sequence-goto-repetition.sv
//
//   logic clk; logic a; logic b;
//   sequence seq;
//       @(posedge clk) b ##1 a [->2:10] ##1 b;
//   endsequence
//   a_seq: assert property (seq);
//
// ── §16.9 rules under test ─────────────────────────────────────────────────
//
// '[->n:m]' goto repetition: the boolean expression is true for n to m
// non-consecutive occurrences; intermediate false cycles are permitted;
// the sequence ends on the cycle of the final true occurrence.
//
// §16.9 constraint: the operand of '[->]' must be a boolean expression.
// A sequence expression (e.g. '(a ##1 b)') is NOT a valid operand.
//
// ── UHDM tree for seq: b ##1 a [->2:10] ##1 b ─────────────────────────────
// (confirmed from log)
//
//   ClockedSeq
//   ├── clockingEvent  Operation { posedgeOp(39) }
//   │   └── operands[0]  RefObj("clk")
//   └── sequenceExpr   Operation { unaryCycleDelayOp(53) }    ← outer ##1
//       ├── operands[0]  Constant("1", intConst=7)
//       ├── operands[1]  RefObj("b")                          ← first b
//       └── operands[2]  Operation { unaryCycleDelayOp(53) }  ← inner ##1
//           ├── operands[0]  Constant("1", intConst=7)
//           ├── operands[1]  Operation { gotoRepeatOp(61) }   ← a [->2:10]
//           │   ├── operands[0]  Range
//           │   │   ├── leftExpr   Constant("2",  uIntConst=9)
//           │   │   └── rightExpr  Constant("10", uIntConst=9)
//           │   └── operands[1]  RefObj("a")
//           └── operands[2]  RefObj("b")                      ← final b
//
// ── Surelog bug EL0535 ────────────────────────────────────────────────────
//
// 'assert property (seq)': Surelog treats the sequence name as an implicit
// net. PropertySpec::getPropertyExpr() returns a RefObj instead of a
// SequenceInst. Test ConcAssert_seq_PropertyExpr_IsSequenceInst FAILS
// intentionally to document this bug.

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

class GotoRepetitionTest : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "16.9--sequence-goto-repetition.hlc"});

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

static const uhdm::SequenceDecl *getSeqDecl(const uhdm::Design *d) {
  const uhdm::Module *m = getTop(d);
  if (!m || !m->getSequenceDecls()) return nullptr;
  return uhdm::findByName<uhdm::SequenceDecl>("seq", m->getSequenceDecls());
}

static const uhdm::ClockedSeq *getClockedSeq(const uhdm::Design *d) {
  const auto *sd = getSeqDecl(d);
  if (!sd) return nullptr;
  return sd->getExpr<uhdm::ClockedSeq>();
}

// outer ##1 — top-level sequenceExpr of the ClockedSeq
static const uhdm::Operation *outerSeqOp(const uhdm::Design *d) {
  const auto *cs = getClockedSeq(d);
  if (!cs) return nullptr;
  return cs->getSequenceExpr<uhdm::Operation>();
}

// inner ##1 — operands[2] of the outer ##1
static const uhdm::Operation *innerSeqOp(const uhdm::Design *d) {
  const auto *outer = outerSeqOp(d);
  if (!outer || !outer->getOperands() ||
      outer->getOperands()->size() < 3) return nullptr;
  return any_cast<const uhdm::Operation *>((*outer->getOperands())[2]);
}

// goto repeat op — operands[1] of the inner ##1
static const uhdm::Operation *repOp(const uhdm::Design *d) {
  const auto *inner = innerSeqOp(d);
  if (!inner || !inner->getOperands() ||
      inner->getOperands()->size() < 2) return nullptr;
  return any_cast<const uhdm::Operation *>((*inner->getOperands())[1]);
}

// Range from the repeat op — operands[0]
static const uhdm::Range *repRange(const uhdm::Design *d) {
  const auto *rep = repOp(d);
  if (!rep || !rep->getOperands() || rep->getOperands()->empty()) return nullptr;
  return any_cast<const uhdm::Range *>((*rep->getOperands())[0]);
}

static const uhdm::ConcurrentAssertions *getAssert(const uhdm::Design *d) {
  const uhdm::Module *m = getTop(d);
  if (!m || !m->getConcurrentAssertions() ||
      m->getConcurrentAssertions()->empty()) return nullptr;
  return (*m->getConcurrentAssertions())[0];
}

static const uhdm::PropertySpec *getPropSpec(const uhdm::Design *d) {
  const auto *ca = getAssert(d);
  if (!ca) return nullptr;
  return ca->getProperty<uhdm::PropertySpec>();
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(GotoRepetitionTest, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found";
}

// ===========================================================================
// Nets — logic clk, a, b
// ===========================================================================

TEST_F(GotoRepetitionTest, Net_clk_HasLogicTypespec) {
  const auto *net = getNet(m_design, "clk");
  ASSERT_NE(net, nullptr) << "net 'clk' not found";
  ASSERT_NE(net->getTypespec(), nullptr);
  EXPECT_NE(net->getTypespec()->getActual<uhdm::LogicTypespec>(), nullptr)
      << "'logic clk' must produce a LogicTypespec";
}

TEST_F(GotoRepetitionTest, Net_a_HasLogicTypespec) {
  const auto *net = getNet(m_design, "a");
  ASSERT_NE(net, nullptr) << "net 'a' not found";
  ASSERT_NE(net->getTypespec(), nullptr);
  EXPECT_NE(net->getTypespec()->getActual<uhdm::LogicTypespec>(), nullptr)
      << "'logic a' must produce a LogicTypespec";
}

TEST_F(GotoRepetitionTest, Net_b_HasLogicTypespec) {
  const auto *net = getNet(m_design, "b");
  ASSERT_NE(net, nullptr) << "net 'b' not found";
  ASSERT_NE(net->getTypespec(), nullptr);
  EXPECT_NE(net->getTypespec()->getActual<uhdm::LogicTypespec>(), nullptr)
      << "'logic b' must produce a LogicTypespec";
}

// ===========================================================================
// SequenceDecl
// ===========================================================================

TEST_F(GotoRepetitionTest, SeqDecl_Collection_HasOneEntry) {
  const auto *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getSequenceDecls(), nullptr);
  EXPECT_EQ(m->getSequenceDecls()->size(), 1u)
      << "exactly one sequence declaration (seq) expected";
}

TEST_F(GotoRepetitionTest, SeqDecl_seq_Exists) {
  EXPECT_NE(getSeqDecl(m_design), nullptr)
      << "SequenceDecl named 'seq' not found";
}

TEST_F(GotoRepetitionTest, SeqDecl_seq_IsClockedSeq) {
  const auto *sd = getSeqDecl(m_design);
  ASSERT_NE(sd, nullptr);
  EXPECT_NE(sd->getExpr<uhdm::ClockedSeq>(), nullptr)
      << "seq body must be represented as a ClockedSeq (has clocking event)";
}

// ===========================================================================
// Clocking event — @(posedge clk)
// ===========================================================================

TEST_F(GotoRepetitionTest, Seq_ClockingEvent_IsPosedge) {
  const auto *cs = getClockedSeq(m_design);
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getClockingEvent<uhdm::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiPosedgeOp)
      << "'@(posedge clk)' must use vpiPosedgeOp (39)";
}

TEST_F(GotoRepetitionTest, Seq_ClockingEvent_HasOneOperand) {
  const auto *cs = getClockedSeq(m_design);
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getClockingEvent<uhdm::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 1u)
      << "posedge has one operand (the clock signal)";
}

TEST_F(GotoRepetitionTest, Seq_ClockingEvent_OperandIsClk) {
  const auto *cs = getClockedSeq(m_design);
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getClockingEvent<uhdm::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *ref =
      any_cast<const uhdm::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "clk")
      << "clocking event operand must reference 'clk'";
}

// ===========================================================================
// Outer ##1 — b ##1 (...)
// ===========================================================================

TEST_F(GotoRepetitionTest, Seq_OuterSeqOp_IsCycleDelayOp) {
  const auto *op = outerSeqOp(m_design);
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiUnaryCycleDelayOp)
      << "outer '##1' must use vpiUnaryCycleDelayOp (53)";
}

TEST_F(GotoRepetitionTest, Seq_OuterSeqOp_HasThreeOperands) {
  const auto *op = outerSeqOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 3u)
      << "outer ##1 must have 3 operands: [delay, left-seq, right-seq]";
}

TEST_F(GotoRepetitionTest, Seq_OuterSeqOp_DelayIsOne) {
  const auto *op = outerSeqOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *c =
      any_cast<const uhdm::Constant *>((*op->getOperands())[0]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getValue()), "1")
      << "outer '##1' delay constant must be 1";
}

TEST_F(GotoRepetitionTest, Seq_OuterSeqOp_LeftSeq_IsRefObjB) {
  const auto *op = outerSeqOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const auto *ref =
      any_cast<const uhdm::RefObj *>((*op->getOperands())[1]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "b")
      << "outer ##1 left operand must be 'b' (first 'b' in source)";
}

// ===========================================================================
// Inner ##1 — order check: a[->2:10] is left of final b
// ===========================================================================

TEST_F(GotoRepetitionTest, Seq_InnerSeqOp_IsCycleDelayOp) {
  const auto *inner = innerSeqOp(m_design);
  ASSERT_NE(inner, nullptr);
  EXPECT_EQ(inner->getOpType(), vpiUnaryCycleDelayOp)
      << "inner '##1' must use vpiUnaryCycleDelayOp (53)";
}

TEST_F(GotoRepetitionTest, Seq_InnerSeqOp_HasThreeOperands) {
  const auto *inner = innerSeqOp(m_design);
  ASSERT_NE(inner, nullptr);
  ASSERT_NE(inner->getOperands(), nullptr);
  EXPECT_EQ(inner->getOperands()->size(), 3u)
      << "inner ##1 must have 3 operands: [delay, left-seq, right-seq]";
}

TEST_F(GotoRepetitionTest, Seq_InnerSeqOp_DelayIsOne) {
  const auto *inner = innerSeqOp(m_design);
  ASSERT_NE(inner, nullptr);
  ASSERT_NE(inner->getOperands(), nullptr);
  ASSERT_GE(inner->getOperands()->size(), 1u);
  const auto *c =
      any_cast<const uhdm::Constant *>((*inner->getOperands())[0]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getValue()), "1")
      << "inner '##1' delay constant must be 1";
}

TEST_F(GotoRepetitionTest, Seq_InnerSeqOp_LeftSeq_IsGotoRepeat) {
  // Verifies source order: 'a [->2:10]' is left of the second '##1'.
  const auto *inner = innerSeqOp(m_design);
  ASSERT_NE(inner, nullptr);
  ASSERT_NE(inner->getOperands(), nullptr);
  ASSERT_GE(inner->getOperands()->size(), 2u);
  const auto *rep =
      any_cast<const uhdm::Operation *>((*inner->getOperands())[1]);
  ASSERT_NE(rep, nullptr) << "inner ##1 operands[1] must be an Operation";
  EXPECT_EQ(rep->getOpType(), vpiGotoRepeatOp)
      << "'a [->2:10]' must appear as left operand (operands[1]) of inner ##1";
}

TEST_F(GotoRepetitionTest, Seq_InnerSeqOp_RightSeq_IsRefObjB) {
  const auto *inner = innerSeqOp(m_design);
  ASSERT_NE(inner, nullptr);
  ASSERT_NE(inner->getOperands(), nullptr);
  ASSERT_GE(inner->getOperands()->size(), 3u);
  const auto *ref =
      any_cast<const uhdm::RefObj *>((*inner->getOperands())[2]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "b")
      << "inner ##1 right operand must be 'b' (final 'b' in source)";
}

// ===========================================================================
// Goto repeat — a [->2:10]
// ===========================================================================

TEST_F(GotoRepetitionTest, Seq_RepOp_IsGotoRepeatOp) {
  const auto *rep = repOp(m_design);
  ASSERT_NE(rep, nullptr);
  EXPECT_EQ(rep->getOpType(), vpiGotoRepeatOp)
      << "§16.9: 'a [->2:10]' must use vpiGotoRepeatOp (61)";
}

TEST_F(GotoRepetitionTest, Seq_RepOp_IsNotConsecutiveRepeatOp) {
  const auto *rep = repOp(m_design);
  ASSERT_NE(rep, nullptr);
  EXPECT_NE(rep->getOpType(), vpiConsecutiveRepeatOp)
      << "§16.9: 'a [->2:10]' must NOT be vpiConsecutiveRepeatOp (60); "
         "parser must not confuse goto '[->]' (61) with consecutive '[*]' (60)";
}

TEST_F(GotoRepetitionTest, Seq_RepOp_HasTwoOperands) {
  const auto *rep = repOp(m_design);
  ASSERT_NE(rep, nullptr);
  ASSERT_NE(rep->getOperands(), nullptr);
  EXPECT_EQ(rep->getOperands()->size(), 2u)
      << "goto repeat must have 2 operands: range/count and repeated expr";
}

TEST_F(GotoRepetitionTest, Seq_RepOp_FirstOperand_IsRange) {
  const auto *range = repRange(m_design);
  ASSERT_NE(range, nullptr)
      << "'a [->2:10]' bounds operand must be a Range node";
}

TEST_F(GotoRepetitionTest, Seq_RepOp_Range_LowerBound_IsTwo) {
  const auto *range = repRange(m_design);
  ASSERT_NE(range, nullptr);
  const auto *lo = range->getLeftExpr<uhdm::Constant>();
  ASSERT_NE(lo, nullptr);
  EXPECT_EQ(std::string(lo->getValue()), "2")
      << "§16.9: '[->2:10]' lower bound must be 2";
}

TEST_F(GotoRepetitionTest, Seq_RepOp_Range_UpperBound_IsTen) {
  const auto *range = repRange(m_design);
  ASSERT_NE(range, nullptr);
  const auto *hi = range->getRightExpr<uhdm::Constant>();
  ASSERT_NE(hi, nullptr);
  EXPECT_EQ(std::string(hi->getValue()), "10")
      << "§16.9: '[->2:10]' upper bound must be 10";
}

TEST_F(GotoRepetitionTest, Seq_RepOp_Range_BoundsAreUnsignedInt) {
  const auto *range = repRange(m_design);
  ASSERT_NE(range, nullptr);
  const auto *lo = range->getLeftExpr<uhdm::Constant>();
  const auto *hi = range->getRightExpr<uhdm::Constant>();
  ASSERT_NE(lo, nullptr);
  ASSERT_NE(hi, nullptr);
  EXPECT_EQ(lo->getConstType(), vpiUIntConst)
      << "§16.9: '[->2:10]' lower bound must be vpiUIntConst (9)";
  EXPECT_EQ(hi->getConstType(), vpiUIntConst)
      << "§16.9: '[->2:10]' upper bound must be vpiUIntConst (9)";
}

TEST_F(GotoRepetitionTest, Seq_RepOp_RepeatedExpr_IsRefObjA) {
  const auto *rep = repOp(m_design);
  ASSERT_NE(rep, nullptr);
  ASSERT_NE(rep->getOperands(), nullptr);
  ASSERT_GE(rep->getOperands()->size(), 2u);
  const auto *ref =
      any_cast<const uhdm::RefObj *>((*rep->getOperands())[1]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "a")
      << "§16.9: 'a [->2:10]' repeated expression must be signal 'a'";
}

// ===========================================================================
// ConcurrentAssertions — a_seq: assert property (seq);
// ===========================================================================

TEST_F(GotoRepetitionTest, ConcAssert_Collection_HasOneEntry) {
  const auto *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getConcurrentAssertions(), nullptr);
  EXPECT_EQ(m->getConcurrentAssertions()->size(), 1u)
      << "one concurrent assertion (a_seq) expected";
}

TEST_F(GotoRepetitionTest, ConcAssert_seq_HasLabel) {
  const auto *ca = getAssert(m_design);
  ASSERT_NE(ca, nullptr);
  EXPECT_EQ(ca->getLabel(), "a_seq")
      << "§16.7: concurrent assertion label must be \"a_seq\"";
}

TEST_F(GotoRepetitionTest, ConcAssert_seq_HasNoActionBlock) {
  const auto *ca = getAssert(m_design);
  ASSERT_NE(ca, nullptr);
  EXPECT_EQ(ca->getStmt(), nullptr)
      << "§16.9: 'assert property (seq);' has no action block";
}

TEST_F(GotoRepetitionTest, ConcAssert_seq_Property_IsPropertySpec) {
  const auto *ca = getAssert(m_design);
  ASSERT_NE(ca, nullptr);
  EXPECT_NE(ca->getProperty<uhdm::PropertySpec>(), nullptr)
      << "assert property must produce a PropertySpec node";
}

TEST_F(GotoRepetitionTest, ConcAssert_seq_PropertyExpr_IsSequenceInst) {
  // §16.9: property expr must be a SequenceInst referencing 'seq'.
  // Surelog bug EL0535: treats sequence name as implicit net, returns RefObj.
  // This test FAILS intentionally to document the bug.
  const auto *ps = getPropSpec(m_design);
  ASSERT_NE(ps, nullptr);
  EXPECT_NE(ps->getPropertyExpr<uhdm::SequenceInst>(), nullptr)
      << "§16.9: property expr must be SequenceInst; "
         "Surelog EL0535: RefObj returned instead";
}

TEST_F(GotoRepetitionTest, ConcAssert_seq_PropertyExpr_NameIsSeq) {
  const auto *ps = getPropSpec(m_design);
  ASSERT_NE(ps, nullptr);
  const auto *expr = ps->getPropertyExpr();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getName(), "seq")
      << "property expression must reference 'seq'";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
