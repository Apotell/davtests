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

// Spec-based validation of IEEE 1800-2023 sec. 16.9 consecutive repetition.
// SV: tests/Google/chapter-16/16.9--sequence-cons-repetition.sv
//
//   logic clk; logic a; logic b;
//   sequence seq;    @(posedge clk) b ##1 a [*2:10] ##1 b;
//   sequence seq_2;  @(negedge clk) b ##1 ((a ##[1:2] b) [*2]) ##1 b;
//   assert property (seq);
//   assert property (seq_2);
//
// -- sec. 16.9 rules under test ----
//
// '[*n:m]' consecutive repetition: the operand MAY be a sequence expression.
// So '(a ##[1:2] b) [*2]' is syntactically valid per sec. 16.9.
//
// '[*n]' single-count form: uses constant_expression (no ':').
// '##[n:m]' variable delay uses a Range node as the delay operand.
//
// None of 'logic clk', 'logic a', 'logic b' uses a net-type keyword, is
// 'interconnect', or is a user-defined nettype, so per sec. 6.7/6.8 all
// three are Variables, not Nets.
//
// -- UHDM tree for seq ----
//
//   ClockedSeq
//   +-- clockingEvent  Operation { posedgeOp(39) }
//   |   +-- operands[0]  RefObj("clk")
//   +-- sequenceExpr   Operation { vpiCycleDelayOp(71) }       <- outer ##1
//       +-- operands[0]  RefObj("b")
//       +-- operands[1]  Constant("1", uintConst=9)
//       +-- operands[2]  Operation { vpiCycleDelayOp(71) }     <- inner ##1
//           +-- operands[0]  Operation { vpiConsecutiveRepeatOp(77) }
//           |   +-- operands[0]  RefObj("a")
//           |   +-- operands[1]  Range { left: Const("2",uint=9),
//           |                             right: Const("10",uint=9) }
//           +-- operands[1]  Constant("1", uintConst=9)
//           +-- operands[2]  RefObj("b")
//
// -- UHDM tree for seq_2 ----
//
//   ClockedSeq
//   +-- clockingEvent  Operation { negedgeOp(40) }              <- negedge
//   |   +-- operands[0]  RefObj("clk")
//   +-- sequenceExpr   Operation { vpiCycleDelayOp(71) }        <- outer ##1
//       +-- operands[0]  RefObj("b")
//       +-- operands[1]  Constant("1", uintConst=9)
//       +-- operands[2]  Operation { vpiCycleDelayOp(71) }      <- inner ##1
//           +-- operands[0]  Operation { vpiConsecutiveRepeatOp(77) }
//           |   +-- operands[0]  Operation { vpiCycleDelayOp(71) }
//           |   |   +-- operands[0]  RefObj("a")
//           |   |   +-- operands[1]  Range { left: Const("1",uint=9),
//           |   |   |                        right: Const("2",uint=9) }
//           |   |   +-- operands[2]  RefObj("b")
//           |   +-- operands[1]  Constant("2", uintConst=9)
//           +-- operands[1]  Constant("1", uintConst=9)
//           +-- operands[2]  RefObj("b")
//
// -- Named sequence references in the asserts ----
//
// 'assert property (seq)' and 'assert property (seq_2)': a bare sequence
// identifier with no argument list is represented as a RefObj whose
// getActual() resolves to the corresponding SequenceDecl (this tool's
// ObjectBinder pass resolves same-scope simple names without requiring
// full hierarchical elaboration).
//
// NOTE: both '##1' and '##[n:m]' produce vpiCycleDelayOp(71).

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
#include <hldb/variable.h>

#include <string>

namespace hlc {

class ConsecutiveRepetitionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16.9--sequence-cons-repetition.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ----
// Helpers
// ----

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("top", d->getAllModules());
}

static const hldb::Variable *getVariable(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getVariables()) return nullptr;
  return hldb::findByName<hldb::Variable>(name, m->getVariables());
}

static const hldb::SequenceDecl *getSeqDecl(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getSequenceDecls()) return nullptr;
  return hldb::findByName<hldb::SequenceDecl>(name, m->getSequenceDecls());
}

static const hldb::ClockedSeq *getClockedSeq(const hldb::Design *d, std::string_view seqName) {
  const auto *sd = getSeqDecl(d, seqName);
  if (!sd) return nullptr;
  return sd->getExpr<hldb::ClockedSeq>();
}

// top-level sequenceExpr of the named sequence
static const hldb::Operation *outerSeqOp(const hldb::Design *d, std::string_view seqName) {
  const auto *cs = getClockedSeq(d, seqName);
  if (!cs) return nullptr;
  return cs->getSequenceExpr<hldb::Operation>();
}

// operands[2] of the outer ##1 (the second sub-expression)
static const hldb::Operation *innerSeqOp(const hldb::Design *d, std::string_view seqName) {
  const auto *outer = outerSeqOp(d, seqName);
  if (!outer || !outer->getOperands() || outer->getOperands()->size() < 3) return nullptr;
  return any_cast<const hldb::Operation *>((*outer->getOperands())[2]);
}

// operands[0] of inner ##1:
//   seq   -> vpiConsecutiveRepeatOp(77) for 'a [*2:10]'
//   seq_2 -> vpiConsecutiveRepeatOp(77) for '(a ##[1:2] b) [*2]'
static const hldb::Operation *innerLeftOp(const hldb::Design *d, std::string_view seqName) {
  const auto *inner = innerSeqOp(d, seqName);
  if (!inner || !inner->getOperands() || inner->getOperands()->size() < 1) return nullptr;
  return any_cast<const hldb::Operation *>((*inner->getOperands())[0]);
}

// helpers specific to seq's consecutive repeat op
static const hldb::Range *seqRepRange(const hldb::Design *d) {
  const auto *rep = innerLeftOp(d, "seq");
  if (!rep || !rep->getOperands() || rep->getOperands()->size() < 2) return nullptr;
  return any_cast<const hldb::Range *>((*rep->getOperands())[1]);
}

// operands[0] of seq_2's consecutive repeat: the 'a ##[1:2] b' sub-sequence
static const hldb::Operation *seq2CycleDelayOp(const hldb::Design *d) {
  const auto *rep = innerLeftOp(d, "seq_2");
  if (!rep || !rep->getOperands() || rep->getOperands()->size() < 1) return nullptr;
  return any_cast<const hldb::Operation *>((*rep->getOperands())[0]);
}

// ConcurrentAssertions by index (no labels in this SV)
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

TEST_F(ConsecutiveRepetitionTest, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'top' not found";
}

// ===========================================================================
// Variables -- logic clk, a, b (sec. 6.7/6.8: no net-type keyword -> Variable)
// ===========================================================================

TEST_F(ConsecutiveRepetitionTest, Variable_clk_HasLogicTypespec) {
  const auto *var = getVariable(m_design, "clk");
  ASSERT_NE(var, nullptr) << "variable 'clk' not found";
  ASSERT_NE(var->getTypespec(), nullptr);
  EXPECT_NE(var->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr)
      << "'logic clk' must produce a LogicTypespec";
}

TEST_F(ConsecutiveRepetitionTest, Variable_a_HasLogicTypespec) {
  const auto *var = getVariable(m_design, "a");
  ASSERT_NE(var, nullptr) << "variable 'a' not found";
  ASSERT_NE(var->getTypespec(), nullptr);
  EXPECT_NE(var->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr) << "'logic a' must produce a LogicTypespec";
}

TEST_F(ConsecutiveRepetitionTest, Variable_b_HasLogicTypespec) {
  const auto *var = getVariable(m_design, "b");
  ASSERT_NE(var, nullptr) << "variable 'b' not found";
  ASSERT_NE(var->getTypespec(), nullptr);
  EXPECT_NE(var->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr) << "'logic b' must produce a LogicTypespec";
}

TEST_F(ConsecutiveRepetitionTest, Variables_NotAlsoInNets) {
  // sec. 6.7/6.8: none of clk/a/b uses a net-type keyword, so none may also
  // appear in the module's net collection.
  const auto *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  if (m->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("clk", m->getNets()), nullptr) << "'clk' must not also appear in getNets()";
    EXPECT_EQ(hldb::findByName<hldb::Net>("a", m->getNets()), nullptr) << "'a' must not also appear in getNets()";
    EXPECT_EQ(hldb::findByName<hldb::Net>("b", m->getNets()), nullptr) << "'b' must not also appear in getNets()";
  }
}

// ===========================================================================
// SequenceDecl collection
// ===========================================================================

TEST_F(ConsecutiveRepetitionTest, SeqDecl_Collection_HasTwoEntries) {
  const auto *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getSequenceDecls(), nullptr);
  EXPECT_EQ(m->getSequenceDecls()->size(), 2u) << "two sequence declarations (seq, seq_2) expected";
}

TEST_F(ConsecutiveRepetitionTest, SeqDecl_seq_Exists) {
  EXPECT_NE(getSeqDecl(m_design, "seq"), nullptr) << "SequenceDecl named 'seq' not found";
}

TEST_F(ConsecutiveRepetitionTest, SeqDecl_seq2_Exists) {
  EXPECT_NE(getSeqDecl(m_design, "seq_2"), nullptr) << "SequenceDecl named 'seq_2' not found";
}

// ===========================================================================
// seq -- @(posedge clk) b ##1 a [*2:10] ##1 b
// ===========================================================================

TEST_F(ConsecutiveRepetitionTest, Seq_SeqDecl_HasNoFormalArgs) {
  const auto *sd = getSeqDecl(m_design, "seq");
  ASSERT_NE(sd, nullptr);
  EXPECT_EQ(sd->getSeqFormalDecls(), nullptr) << "seq: no port list -- getSeqFormalDecls() must be null";
}

TEST_F(ConsecutiveRepetitionTest, Seq_SeqDecl_IsClockedSeq) {
  const auto *sd = getSeqDecl(m_design, "seq");
  ASSERT_NE(sd, nullptr);
  EXPECT_NE(sd->getExpr<hldb::ClockedSeq>(), nullptr) << "seq: '@(posedge clk) ...' body must be a ClockedSeq";
}

// -- clocking event: @(posedge clk) ----

TEST_F(ConsecutiveRepetitionTest, Seq_ClockingEvent_IsPosedge) {
  const auto *cs = getClockedSeq(m_design, "seq");
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getClockingEvent<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiPosedgeOp) << "seq: '@(posedge clk)' must use vpiPosedgeOp (39)";
}

TEST_F(ConsecutiveRepetitionTest, Seq_ClockingEvent_OperandIsClk) {
  const auto *cs = getClockedSeq(m_design, "seq");
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getClockingEvent<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *ref = any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "clk");
}

// -- outer ##1 ----

TEST_F(ConsecutiveRepetitionTest, Seq_OuterSeqOp_IsCycleDelayOp) {
  const auto *op = outerSeqOp(m_design, "seq");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCycleDelayOp) << "seq: outer '##1' must use vpiCycleDelayOp (71) -- binary form";
}

TEST_F(ConsecutiveRepetitionTest, Seq_OuterSeqOp_HasThreeOperands) {
  const auto *op = outerSeqOp(m_design, "seq");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 3u) << "outer ##1: [left-seq, delay, right-seq]";
}

TEST_F(ConsecutiveRepetitionTest, Seq_OuterSeqOp_DelayIsOne) {
  const auto *op = outerSeqOp(m_design, "seq");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const auto *c = any_cast<const hldb::Constant *>((*op->getOperands())[1]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getDecompile()), "1");
}

TEST_F(ConsecutiveRepetitionTest, Seq_OuterSeqOp_LeftSeq_IsRefObjB) {
  const auto *op = outerSeqOp(m_design, "seq");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *ref = any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "b") << "seq: outer ##1 left operand must be 'b' (first 'b' in source)";
}

// -- inner ##1 -- order check ----

TEST_F(ConsecutiveRepetitionTest, Seq_InnerSeqOp_IsCycleDelayOp) {
  const auto *op = innerSeqOp(m_design, "seq");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCycleDelayOp) << "seq: inner '##1' must use vpiCycleDelayOp (71) -- binary form";
}

TEST_F(ConsecutiveRepetitionTest, Seq_InnerSeqOp_HasThreeOperands) {
  const auto *op = innerSeqOp(m_design, "seq");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 3u) << "inner ##1: [left-seq, delay, right-seq]";
}

TEST_F(ConsecutiveRepetitionTest, Seq_InnerSeqOp_DelayIsOne) {
  const auto *op = innerSeqOp(m_design, "seq");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const auto *c = any_cast<const hldb::Constant *>((*op->getOperands())[1]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getDecompile()), "1");
}

TEST_F(ConsecutiveRepetitionTest, Seq_InnerSeqOp_LeftSeq_IsConsecRepeat) {
  const auto *rep = innerLeftOp(m_design, "seq");
  ASSERT_NE(rep, nullptr) << "inner ##1 operands[0] must be an Operation";
  EXPECT_EQ(rep->getOpType(), vpiConsecutiveRepeatOp) << "seq: 'a [*2:10]' must be left operand of inner ##1";
}

TEST_F(ConsecutiveRepetitionTest, Seq_InnerSeqOp_RightSeq_IsRefObjB) {
  const auto *op = innerSeqOp(m_design, "seq");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 3u);
  const auto *ref = any_cast<const hldb::RefObj *>((*op->getOperands())[2]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "b") << "seq: inner ##1 right operand must be 'b' (final 'b' in source)";
}

// -- consecutive repeat 'a [*2:10]' ----

TEST_F(ConsecutiveRepetitionTest, Seq_RepOp_HasTwoOperands) {
  const auto *rep = innerLeftOp(m_design, "seq");
  ASSERT_NE(rep, nullptr);
  ASSERT_NE(rep->getOperands(), nullptr);
  EXPECT_EQ(rep->getOperands()->size(), 2u) << "consecutive repeat: [repeated-expr, range]";
}

TEST_F(ConsecutiveRepetitionTest, Seq_RepOp_SecondOperand_IsRange) {
  const auto *range = seqRepRange(m_design);
  ASSERT_NE(range, nullptr) << "'a [*2:10]' bounds must be a Range node";
}

TEST_F(ConsecutiveRepetitionTest, Seq_RepOp_Range_LowerBound_IsTwo) {
  const auto *range = seqRepRange(m_design);
  ASSERT_NE(range, nullptr);
  const auto *lo = range->getLeftExpr<hldb::Constant>();
  ASSERT_NE(lo, nullptr);
  EXPECT_EQ(std::string(lo->getDecompile()), "2") << "sec. 16.9: '[*2:10]' lower bound must be 2";
}

TEST_F(ConsecutiveRepetitionTest, Seq_RepOp_Range_UpperBound_IsTen) {
  const auto *range = seqRepRange(m_design);
  ASSERT_NE(range, nullptr);
  const auto *hi = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(hi, nullptr);
  EXPECT_EQ(std::string(hi->getDecompile()), "10") << "sec. 16.9: '[*2:10]' upper bound must be 10";
}

TEST_F(ConsecutiveRepetitionTest, Seq_RepOp_Range_BoundsAreUnsignedInt) {
  const auto *range = seqRepRange(m_design);
  ASSERT_NE(range, nullptr);
  const auto *lo = range->getLeftExpr<hldb::Constant>();
  const auto *hi = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(lo, nullptr);
  ASSERT_NE(hi, nullptr);
  EXPECT_EQ(lo->getConstType(), vpiUIntConst) << "sec. 16.9: '[*2:10]' lower bound must be vpiUIntConst (9)";
  EXPECT_EQ(hi->getConstType(), vpiUIntConst) << "sec. 16.9: '[*2:10]' upper bound must be vpiUIntConst (9)";
}

TEST_F(ConsecutiveRepetitionTest, Seq_RepOp_RepeatedExpr_IsRefObjA) {
  const auto *rep = innerLeftOp(m_design, "seq");
  ASSERT_NE(rep, nullptr);
  ASSERT_NE(rep->getOperands(), nullptr);
  ASSERT_GE(rep->getOperands()->size(), 1u);
  const auto *ref = any_cast<const hldb::RefObj *>((*rep->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "a") << "sec. 16.9: 'a [*2:10]' repeated expression must be signal 'a'";
}

// ===========================================================================
// seq_2 -- @(negedge clk) b ##1 ((a ##[1:2] b) [*2]) ##1 b
// ===========================================================================

TEST_F(ConsecutiveRepetitionTest, Seq2_SeqDecl_HasNoFormalArgs) {
  const auto *sd = getSeqDecl(m_design, "seq_2");
  ASSERT_NE(sd, nullptr);
  EXPECT_EQ(sd->getSeqFormalDecls(), nullptr) << "seq_2: no port list -- getSeqFormalDecls() must be null";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_SeqDecl_IsClockedSeq) {
  const auto *sd = getSeqDecl(m_design, "seq_2");
  ASSERT_NE(sd, nullptr);
  EXPECT_NE(sd->getExpr<hldb::ClockedSeq>(), nullptr) << "seq_2: '@(negedge clk) ...' body must be a ClockedSeq";
}

// -- clocking event: @(negedge clk) ----

TEST_F(ConsecutiveRepetitionTest, Seq2_ClockingEvent_IsNegedge) {
  const auto *cs = getClockedSeq(m_design, "seq_2");
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getClockingEvent<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiNegedgeOp) << "seq_2: '@(negedge clk)' must use vpiNegedgeOp (40)";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_ClockingEvent_IsNotPosedge) {
  const auto *cs = getClockedSeq(m_design, "seq_2");
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getClockingEvent<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_NE(op->getOpType(), vpiPosedgeOp) << "seq_2 uses negedge, not posedge";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_ClockingEvent_OperandIsClk) {
  const auto *cs = getClockedSeq(m_design, "seq_2");
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getClockingEvent<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *ref = any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "clk");
}

// -- outer ##1 ----

TEST_F(ConsecutiveRepetitionTest, Seq2_OuterSeqOp_IsCycleDelayOp) {
  const auto *op = outerSeqOp(m_design, "seq_2");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCycleDelayOp) << "seq_2: outer '##1' must use vpiCycleDelayOp (71)";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_OuterSeqOp_HasThreeOperands) {
  const auto *op = outerSeqOp(m_design, "seq_2");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 3u) << "outer ##1: [left-seq, delay, right-seq]";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_OuterSeqOp_LeftSeq_IsRefObjB) {
  const auto *op = outerSeqOp(m_design, "seq_2");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *ref = any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "b") << "seq_2: outer ##1 left operand must be 'b' (first 'b' in source)";
}

// -- inner ##1 ----

TEST_F(ConsecutiveRepetitionTest, Seq2_InnerSeqOp_IsCycleDelayOp) {
  const auto *op = innerSeqOp(m_design, "seq_2");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCycleDelayOp) << "seq_2: inner '##1' must use vpiCycleDelayOp (71)";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_InnerSeqOp_HasThreeOperands) {
  const auto *op = innerSeqOp(m_design, "seq_2");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 3u) << "inner ##1: [left-seq, delay, right-seq]";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_InnerSeqOp_LeftSeq_ShouldBeConsecRepeat) {
  // sec. 16.9: '(a ##[1:2] b) [*2]' wraps the sub-sequence in
  // vpiConsecutiveRepeatOp(77).
  const auto *op = innerLeftOp(m_design, "seq_2");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiConsecutiveRepeatOp)
      << "sec. 16.9: '(a ##[1:2] b) [*2]' must produce vpiConsecutiveRepeatOp(77)";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_InnerSeqOp_RightSeq_IsRefObjB) {
  const auto *op = innerSeqOp(m_design, "seq_2");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 3u);
  const auto *ref = any_cast<const hldb::RefObj *>((*op->getOperands())[2]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "b") << "seq_2: inner ##1 right operand must be 'b' (final 'b' in source)";
}

// -- sub-sequence 'a ##[1:2] b' inside seq_2's consecutive repeat ----
//
// vpiCycleDelayOp(71) at vpiConsecutiveRepeatOp(77) operands[0]:
//   operands[0]  RefObj("a")
//   operands[1]  Range { left: Const("1",uint=9), right: Const("2",uint=9) }
//   operands[2]  RefObj("b")

TEST_F(ConsecutiveRepetitionTest, Seq2_VarCycleDelay_HasThreeOperands) {
  const auto *sub = seq2CycleDelayOp(m_design);
  ASSERT_NE(sub, nullptr);
  ASSERT_NE(sub->getOperands(), nullptr);
  EXPECT_EQ(sub->getOperands()->size(), 3u) << "'a ##[1:2] b': [a, delay-range, b]";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_VarCycleDelay_DelayIsRange) {
  // '##[1:2]' variable delay -- operands[1] is a Range, not a Constant.
  // '##1' fixed delay would have Constant as operands[1].
  const auto *sub = seq2CycleDelayOp(m_design);
  ASSERT_NE(sub, nullptr);
  ASSERT_NE(sub->getOperands(), nullptr);
  ASSERT_GE(sub->getOperands()->size(), 2u);
  EXPECT_NE(any_cast<const hldb::Range *>((*sub->getOperands())[1]), nullptr)
      << "'##[1:2]' variable delay operand must be a Range, not a Constant";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_VarCycleDelay_DelayRange_LowerIsOne) {
  const auto *sub = seq2CycleDelayOp(m_design);
  ASSERT_NE(sub, nullptr);
  ASSERT_NE(sub->getOperands(), nullptr);
  ASSERT_GE(sub->getOperands()->size(), 2u);
  const auto *range = any_cast<const hldb::Range *>((*sub->getOperands())[1]);
  ASSERT_NE(range, nullptr);
  const auto *lo = range->getLeftExpr<hldb::Constant>();
  ASSERT_NE(lo, nullptr);
  EXPECT_EQ(std::string(lo->getDecompile()), "1") << "'##[1:2]' lower delay bound must be 1";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_VarCycleDelay_DelayRange_UpperIsTwo) {
  const auto *sub = seq2CycleDelayOp(m_design);
  ASSERT_NE(sub, nullptr);
  ASSERT_NE(sub->getOperands(), nullptr);
  ASSERT_GE(sub->getOperands()->size(), 2u);
  const auto *range = any_cast<const hldb::Range *>((*sub->getOperands())[1]);
  ASSERT_NE(range, nullptr);
  const auto *hi = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(hi, nullptr);
  EXPECT_EQ(std::string(hi->getDecompile()), "2") << "'##[1:2]' upper delay bound must be 2";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_VarCycleDelay_DelayRange_BoundsAreUnsignedInt) {
  const auto *sub = seq2CycleDelayOp(m_design);
  ASSERT_NE(sub, nullptr);
  ASSERT_NE(sub->getOperands(), nullptr);
  ASSERT_GE(sub->getOperands()->size(), 2u);
  const auto *range = any_cast<const hldb::Range *>((*sub->getOperands())[1]);
  ASSERT_NE(range, nullptr);
  const auto *lo = range->getLeftExpr<hldb::Constant>();
  const auto *hi = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(lo, nullptr);
  ASSERT_NE(hi, nullptr);
  EXPECT_EQ(lo->getConstType(), vpiUIntConst) << "'##[1:2]' lower delay bound must be vpiUIntConst (9)";
  EXPECT_EQ(hi->getConstType(), vpiUIntConst) << "'##[1:2]' upper delay bound must be vpiUIntConst (9)";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_VarCycleDelay_LeftIsRefObjA) {
  const auto *sub = seq2CycleDelayOp(m_design);
  ASSERT_NE(sub, nullptr);
  ASSERT_NE(sub->getOperands(), nullptr);
  ASSERT_GE(sub->getOperands()->size(), 1u);
  const auto *ref = any_cast<const hldb::RefObj *>((*sub->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "a") << "'(a ##[1:2] b)' left element must be signal 'a'";
}

TEST_F(ConsecutiveRepetitionTest, Seq2_VarCycleDelay_RightIsRefObjB) {
  const auto *sub = seq2CycleDelayOp(m_design);
  ASSERT_NE(sub, nullptr);
  ASSERT_NE(sub->getOperands(), nullptr);
  ASSERT_GE(sub->getOperands()->size(), 3u);
  const auto *ref = any_cast<const hldb::RefObj *>((*sub->getOperands())[2]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "b") << "'(a ##[1:2] b)' right element must be signal 'b'";
}

// ===========================================================================
// ConcurrentAssertions -- two unlabelled asserts; accessed by index
// ===========================================================================

TEST_F(ConsecutiveRepetitionTest, ConcAssert_Collection_HasTwoEntries) {
  const auto *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getConcurrentAssertions(), nullptr);
  EXPECT_EQ(m->getConcurrentAssertions()->size(), 2u) << "two concurrent assertions expected (seq, seq_2)";
}

// -- assert property (seq) -- index 0 ----

TEST_F(ConsecutiveRepetitionTest, ConcAssert_seq_HasNoActionBlock) {
  const auto *ca = getAssertAt(m_design, 0);
  ASSERT_NE(ca, nullptr);
  EXPECT_EQ(ca->getStmt(), nullptr) << "'assert property (seq);' has no action block";
}

TEST_F(ConsecutiveRepetitionTest, ConcAssert_seq_Property_IsPropertySpec) {
  const auto *ca = getAssertAt(m_design, 0);
  ASSERT_NE(ca, nullptr);
  EXPECT_NE(ca->getProperty<hldb::PropertySpec>(), nullptr) << "assert property must produce a PropertySpec node";
}

TEST_F(ConsecutiveRepetitionTest, ConcAssert_seq_PropertyExpr_IsRefObj) {
  // A bare sequence identifier with no argument list is represented as a
  // RefObj referencing the named sequence 'seq'.
  const auto *ps = getPropSpec(m_design, 0);
  ASSERT_NE(ps, nullptr);
  EXPECT_NE(ps->getPropertyExpr<hldb::RefObj>(), nullptr)
      << "sec. 16.9: 'assert property (seq)' property expression must be a RefObj referencing 'seq'";
}

TEST_F(ConsecutiveRepetitionTest, ConcAssert_seq_PropertyExpr_NameIsSeq) {
  const auto *ps = getPropSpec(m_design, 0);
  ASSERT_NE(ps, nullptr);
  const auto *expr = ps->getPropertyExpr();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getName(), "seq") << "first assertion property expression must reference 'seq'";
}

TEST_F(ConsecutiveRepetitionTest, ConcAssert_seq_PropertyExpr_ResolvesToSeqDecl) {
  // The RefObj's actual must resolve to the SequenceDecl 'seq'.
  const auto *ps = getPropSpec(m_design, 0);
  ASSERT_NE(ps, nullptr);
  const auto *ref = ps->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(ref, nullptr);
  const auto *sd = getSeqDecl(m_design, "seq");
  ASSERT_NE(sd, nullptr);
  EXPECT_EQ(ref->getActual<hldb::SequenceDecl>(), sd)
      << "sec. 16.9: RefObj('seq')::getActual() must resolve to the SequenceDecl 'seq'";
}

// -- assert property (seq_2) -- index 1 ----

TEST_F(ConsecutiveRepetitionTest, ConcAssert_seq2_HasNoActionBlock) {
  const auto *ca = getAssertAt(m_design, 1);
  ASSERT_NE(ca, nullptr);
  EXPECT_EQ(ca->getStmt(), nullptr) << "'assert property (seq_2);' has no action block";
}

TEST_F(ConsecutiveRepetitionTest, ConcAssert_seq2_Property_IsPropertySpec) {
  const auto *ca = getAssertAt(m_design, 1);
  ASSERT_NE(ca, nullptr);
  EXPECT_NE(ca->getProperty<hldb::PropertySpec>(), nullptr) << "assert property must produce a PropertySpec node";
}

TEST_F(ConsecutiveRepetitionTest, ConcAssert_seq2_PropertyExpr_IsRefObj) {
  // A bare sequence identifier with no argument list is represented as a
  // RefObj referencing the named sequence 'seq_2'.
  const auto *ps = getPropSpec(m_design, 1);
  ASSERT_NE(ps, nullptr);
  EXPECT_NE(ps->getPropertyExpr<hldb::RefObj>(), nullptr)
      << "sec. 16.9: 'assert property (seq_2)' property expression must be a RefObj referencing 'seq_2'";
}

TEST_F(ConsecutiveRepetitionTest, ConcAssert_seq2_PropertyExpr_NameIsSeq2) {
  const auto *ps = getPropSpec(m_design, 1);
  ASSERT_NE(ps, nullptr);
  const auto *expr = ps->getPropertyExpr();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getName(), "seq_2") << "second assertion property expression must reference 'seq_2'";
}

TEST_F(ConsecutiveRepetitionTest, ConcAssert_seq2_PropertyExpr_ResolvesToSeqDecl) {
  // The RefObj's actual must resolve to the SequenceDecl 'seq_2'.
  const auto *ps = getPropSpec(m_design, 1);
  ASSERT_NE(ps, nullptr);
  const auto *ref = ps->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(ref, nullptr);
  const auto *sd = getSeqDecl(m_design, "seq_2");
  ASSERT_NE(sd, nullptr);
  EXPECT_EQ(ref->getActual<hldb::SequenceDecl>(), sd)
      << "sec. 16.9: RefObj('seq_2')::getActual() must resolve to the SequenceDecl 'seq_2'";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
