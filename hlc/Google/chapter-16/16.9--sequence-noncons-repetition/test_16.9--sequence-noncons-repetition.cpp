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

// Spec-based validation of IEEE 1800-2023 sec. 16.9 non-consecutive
// repetition and sec. 16.12 property 'not' operator.
// SV: tests/Google/chapter-16/16.9--sequence-noncons-repetition.sv
//
//   logic clk; logic a; logic b;
//   sequence seq;
//       @(posedge clk) b ##1 a [=2:10] ##1 b;
//   endsequence
//   assert property (seq);
//   assert property (not seq);
//
// -- sec. 16.9 operator under test ----
//
// '[=n:m]' non-consecutive repetition: signal 'a' holds true for at least n
// and at most m non-consecutive clock cycles within the overall sequence.
// The operand must be a boolean expression (same requirement as goto [->]).
//
// The three sec. 16.9 repetition operators map to distinct VPI opTypes:
//   [=n:m]  non-consecutive  -> vpiRepeatOp            (76)  <- this test
//   [*n:m]  consecutive      -> vpiConsecutiveRepeatOp  (77)
//   [->n:m] goto             -> vpiGotoRepeatOp         (78)
//
// None of 'logic clk', 'logic a', 'logic b' uses a net-type keyword, is
// 'interconnect', or is a user-defined nettype, so per sec. 6.7/6.8 all
// three are Variables, not Nets.
//
// -- sec. 16.12 'not' operator under test ----
//
// 'not property_expr' negates a property (sec. 16.12.1 grammar).
// 'assert property (not seq)' asserts that seq never holds.
//
// -- UHDM tree for seq ----
//
//   ClockedSeq
//   +-- clockingEvent  Operation { posedgeOp(39) }
//   |   +-- operands[0]  RefObj("clk")
//   +-- sequenceExpr   Operation { vpiCycleDelayOp(71) }   <- outer ##1
//       +-- operands[0]  RefObj("b")
//       +-- operands[1]  Constant("1", intConst=7)
//       +-- operands[2]  Operation { vpiCycleDelayOp(71) }  <- inner ##1
//           +-- operands[0]  Operation { vpiRepeatOp(76) }    <- a [=2:10]
//           |   +-- operands[0]  RefObj("a")
//           |   +-- operands[1]  Range { left: Const("2",uint=9),
//           |                             right: Const("10",uint=9) }
//           +-- operands[1]  Constant("1", intConst=7)
//           +-- operands[2]  RefObj("b")
//
// -- Named sequence references in the asserts ----
//
// 'assert property (seq)' [Assert index 0]: a bare sequence identifier with
// no argument list is represented as a RefObj whose getActual() resolves to
// the SequenceDecl 'seq' (this tool's ObjectBinder pass resolves same-scope
// simple names without requiring full hierarchical elaboration).
//
// 'assert property (not seq)' [Assert index 1]: per sec. 16.12.1, 'not
// property_expr' produces Operation{vpiNotOp} wrapping the negated
// property_expr. Here the operand is the same RefObj("seq") pattern as
// Assert[0], one level down inside the 'not' Operation.

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

class NonconsecutiveRepetitionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16.9--sequence-noncons-repetition.hlc"}); }
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

static const hldb::SequenceDecl *getSeqDecl(const hldb::Design *d) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getSequenceDecls()) return nullptr;
  return hldb::findByName<hldb::SequenceDecl>("seq", m->getSequenceDecls());
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
  ASSERT_NE(getTop(m_design), nullptr) << "module 'top' not found";
}

// ===========================================================================
// Variables -- logic clk, a, b (sec. 6.7/6.8: no net-type keyword -> Variable)
// ===========================================================================

TEST_F(NonconsecutiveRepetitionTest, Variable_clk_HasLogicTypespec) {
  const auto *var = getVariable(m_design, "clk");
  ASSERT_NE(var, nullptr) << "variable 'clk' not found";
  ASSERT_NE(var->getTypespec(), nullptr);
  EXPECT_NE(var->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr)
      << "'logic clk' must produce a LogicTypespec";
}

TEST_F(NonconsecutiveRepetitionTest, Variable_a_HasLogicTypespec) {
  const auto *var = getVariable(m_design, "a");
  ASSERT_NE(var, nullptr) << "variable 'a' not found";
  ASSERT_NE(var->getTypespec(), nullptr);
  EXPECT_NE(var->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr) << "'logic a' must produce a LogicTypespec";
}

TEST_F(NonconsecutiveRepetitionTest, Variable_b_HasLogicTypespec) {
  const auto *var = getVariable(m_design, "b");
  ASSERT_NE(var, nullptr) << "variable 'b' not found";
  ASSERT_NE(var->getTypespec(), nullptr);
  EXPECT_NE(var->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr) << "'logic b' must produce a LogicTypespec";
}

TEST_F(NonconsecutiveRepetitionTest, Variables_NotAlsoInNets) {
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
  EXPECT_EQ(sd->getSeqFormalDecls(), nullptr) << "seq: no port list -- getSeqFormalDecls() must be null";
}

TEST_F(NonconsecutiveRepetitionTest, SeqDecl_seq_IsClockedSeq) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  const auto *sd = hldb::findByName<hldb::SequenceDecl>("seq", m->getSequenceDecls());
  ASSERT_NE(sd, nullptr);
  EXPECT_NE(sd->getExpr<hldb::ClockedSeq>(), nullptr) << "seq: '@(posedge clk) ...' body must be a ClockedSeq";
}

// ===========================================================================
// Clocking event -- @(posedge clk)
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
// Inner ##1 -- order check
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
  // sec. 16.9: the three repetition operators:
  // [=] non-consecutive -> vpiRepeatOp(76)
  // [*] consecutive     -> vpiConsecutiveRepeatOp(77)
  // [->] goto           -> vpiGotoRepeatOp(78)
  const auto *rep = repOp(m_design);
  ASSERT_NE(rep, nullptr) << "inner ##1 operands[0] must be an Operation";
  EXPECT_EQ(rep->getOpType(), vpiRepeatOp) << "sec. 16.9: '[=2:10]' non-consecutive repetition must use vpiRepeatOp (76)";
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
  EXPECT_EQ(std::string(lo->getDecompile()), "2") << "sec. 16.9: '[=2:10]' lower bound must be 2";
}

TEST_F(NonconsecutiveRepetitionTest, Seq_RepOp_Range_UpperBound_IsTen) {
  const auto *range = repRange(m_design);
  ASSERT_NE(range, nullptr);
  const auto *hi = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(hi, nullptr);
  EXPECT_EQ(std::string(hi->getDecompile()), "10") << "sec. 16.9: '[=2:10]' upper bound must be 10";
}

TEST_F(NonconsecutiveRepetitionTest, Seq_RepOp_Range_BoundsAreUnsignedInt) {
  const auto *range = repRange(m_design);
  ASSERT_NE(range, nullptr);
  const auto *lo = range->getLeftExpr<hldb::Constant>();
  const auto *hi = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(lo, nullptr);
  ASSERT_NE(hi, nullptr);
  EXPECT_EQ(lo->getConstType(), vpiUIntConst) << "sec. 16.9: '[=2:10]' lower bound must be vpiUIntConst (9)";
  EXPECT_EQ(hi->getConstType(), vpiUIntConst) << "sec. 16.9: '[=2:10]' upper bound must be vpiUIntConst (9)";
}

TEST_F(NonconsecutiveRepetitionTest, Seq_RepOp_RepeatedExpr_IsRefObjA) {
  const auto *rep = repOp(m_design);
  ASSERT_NE(rep, nullptr);
  ASSERT_NE(rep->getOperands(), nullptr);
  ASSERT_GE(rep->getOperands()->size(), 1u);
  const auto *ref = any_cast<const hldb::RefObj *>((*rep->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "a") << "sec. 16.9: 'a [=2:10]' repeated expression must be signal 'a'";
}

// ===========================================================================
// ConcurrentAssertions -- two asserts; accessed by index
// ===========================================================================

TEST_F(NonconsecutiveRepetitionTest, ConcAssert_Collection_HasTwoEntries) {
  const auto *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getConcurrentAssertions(), nullptr);
  EXPECT_EQ(m->getConcurrentAssertions()->size(), 2u) << "two concurrent assertions expected: 'assert property (seq)' "
                                                         "and 'assert property (not seq)'";
}

// ===========================================================================
// assert property (seq) -- index 0
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

TEST_F(NonconsecutiveRepetitionTest, ConcAssert_seq_PropertyExpr_IsRefObj) {
  // A bare sequence identifier with no argument list is represented as a
  // RefObj referencing the named sequence 'seq'.
  const auto *ps = getPropSpec(m_design, 0);
  ASSERT_NE(ps, nullptr);
  EXPECT_NE(ps->getPropertyExpr<hldb::RefObj>(), nullptr)
      << "sec. 16.9: 'assert property (seq)' property expression must be a RefObj referencing 'seq'";
}

TEST_F(NonconsecutiveRepetitionTest, ConcAssert_seq_PropertyExpr_NameIsSeq) {
  const auto *ps = getPropSpec(m_design, 0);
  ASSERT_NE(ps, nullptr);
  const auto *expr = ps->getPropertyExpr();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getName(), "seq") << "first assertion property expression must reference 'seq'";
}

TEST_F(NonconsecutiveRepetitionTest, ConcAssert_seq_PropertyExpr_ResolvesToSeqDecl) {
  // The RefObj's actual must resolve to the SequenceDecl 'seq'.
  const auto *ps = getPropSpec(m_design, 0);
  ASSERT_NE(ps, nullptr);
  const auto *ref = ps->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(ref, nullptr);
  const auto *sd = getSeqDecl(m_design);
  ASSERT_NE(sd, nullptr);
  EXPECT_EQ(ref->getActual<hldb::SequenceDecl>(), sd)
      << "sec. 16.9: RefObj('seq')::getActual() must resolve to the SequenceDecl 'seq'";
}

// ===========================================================================
// assert property (not seq) -- index 1
// ===========================================================================
//
// sec. 16.12.1: 'not property_expr' negates a property. 'not seq' produces
// Operation{vpiNotOp} wrapping a RefObj referencing 'seq', asserting that
// the sequence seq never holds.

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

TEST_F(NonconsecutiveRepetitionTest, ConcAssert_not_seq_PropertyExpr_IsNotOperation) {
  // sec. 16.12: 'not seq' must produce Operation{vpiNotOp} as the property expr.
  const auto *ps = getPropSpec(m_design, 1);
  ASSERT_NE(ps, nullptr);
  const auto *op = ps->getPropertyExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "sec. 16.12: 'not seq' property expr must be an Operation";
  EXPECT_EQ(op->getOpType(), vpiNotOp) << "sec. 16.12: 'not property_expr' must use vpiNotOp (3)";
}

TEST_F(NonconsecutiveRepetitionTest, ConcAssert_not_seq_PropertyExpr_NameIsSeq) {
  // The single operand of the 'not' Operation must reference 'seq'.
  const auto *ps = getPropSpec(m_design, 1);
  ASSERT_NE(ps, nullptr);
  const auto *op = ps->getPropertyExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_EQ(op->getOpType(), vpiNotOp);
  const auto *operands = op->getOperands();
  ASSERT_NE(operands, nullptr);
  ASSERT_EQ(operands->size(), 1u);
  const auto *ref = any_cast<hldb::RefObj>(operands->front());
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "seq") << "'not seq' inner operand must reference sequence 'seq'";
}

TEST_F(NonconsecutiveRepetitionTest, ConcAssert_not_seq_PropertyExpr_OperandResolvesToSeqDecl) {
  // The RefObj operand's actual must resolve to the SequenceDecl 'seq'.
  const auto *ps = getPropSpec(m_design, 1);
  ASSERT_NE(ps, nullptr);
  const auto *op = ps->getPropertyExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  const auto *operands = op->getOperands();
  ASSERT_NE(operands, nullptr);
  ASSERT_EQ(operands->size(), 1u);
  const auto *ref = any_cast<hldb::RefObj>(operands->front());
  ASSERT_NE(ref, nullptr);
  const auto *sd = getSeqDecl(m_design);
  ASSERT_NE(sd, nullptr);
  EXPECT_EQ(ref->getActual<hldb::SequenceDecl>(), sd)
      << "sec. 16.12: RefObj('seq') inside 'not' operand must resolve to the SequenceDecl 'seq'";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
