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

// Spec-based validation of IEEE 1800-2017 §16.7 named sequence and concurrent
// assertion.
//
// All expected values are derived from §16.7 of the spec and the SV source.
// No expected value is taken from the UHDM log. Failing tests document
// Surelog bugs.
//
// ── §16.7 rules under test ─────────────────────────────────────────────────
//
// §16.7 defines named sequences and their use in concurrent assertions:
//
//   sequence identifier;
//     [clocking_event] sequence_expr;
//   endsequence
//   assert property (sequence_or_property);
//
// Rule 1 — A named sequence is declared with 'sequence … endsequence'.
//   → Module::getSequenceDecls() must be non-null and contain one SequenceDecl.
//   → SequenceDecl::getName() must be "seq".
//
// Rule 2 — This sequence has no formal argument list (no port list).
//   → SequenceDecl::getSeqFormalDecls() must be null.
//
// Rule 3 — The sequence body '@(posedge clk) a ##1 b' is a clocked sequence
//   expression. UHDM represents this as a ClockedSeq node:
//     ClockedSeq::getClockingEvent() → Operation (vpiPosedgeOp) with RefObj 'clk'
//     ClockedSeq::getSequenceExpr()  → Operation (vpiUnaryCycleDelayOp)
//       operands: [Constant("1"), RefObj("a"), RefObj("b")]
//   The cycle delay '##1' uses op-type vpiUnaryCycleDelayOp (53) with three
//   operands: delay amount, left sequence expression, right sequence expression.
//
// Rule 4 — 'assert property (seq)' produces a ConcurrentAssertions node.
//   → Module::getConcurrentAssertions() must be non-null and contain one entry.
//   → ConcurrentAssertions::getProperty() returns a PropertySpec.
//   → PropertySpec::getClockingEvent() is null — the clock is inside the
//     named sequence body, not on the assert statement.
//   → ConcurrentAssertions::getStmt() is null — no explicit action block.
//
// Rule 5 — The property expression in 'assert property (seq)' is a reference
//   to the named sequence 'seq'. Per §16.7, UHDM should produce a SequenceInst
//   referencing the SequenceDecl.
//
// ── Surelog bug EL0535 ─────────────────────────────────────────────────────
//
// Surelog logs:
//   [ERR:EL0535] 16.7--sequence.sv:26:18: Illegal implicit net "id:27, name:seq"
//
// Surelog treats 'seq' in 'assert property (seq)' as an implicit net instead
// of recognising it as a sequence reference. As a result:
//   PropertySpec::getPropertyExpr<SequenceInst>() returns nullptr.
// The spec-correct test (ConcAssert_PropSpec_PropertyExpr_IsSequenceInst) FAILS
// to document this bug.
//
// ── SV source ──────────────────────────────────────────────────────────────
//   logic clk;
//   logic a;
//   logic b;
//   sequence seq;
//       @(posedge clk) a ##1 b;
//   endsequence
//   assert property (seq);
//
// ── Spec-correct UHDM ──────────────────────────────────────────────────────
//   Module 'work@top':
//     Nets: clk, a, b — all LogicTypespec
//     SequenceDecls[0]: SequenceDecl {              // Rule 1
//       name: "seq"
//       seqFormalDecls: null                        // Rule 2
//       expr: ClockedSeq {                          // Rule 3
//         clockingEvent: Operation {
//           opType: vpiPosedgeOp (39)
//           operands[0]: RefObj("clk")
//         }
//         sequenceExpr: Operation {
//           opType: vpiUnaryCycleDelayOp (53)       // '##1'
//           operands[0]: Constant("1")              // delay amount
//           operands[1]: RefObj("a")
//           operands[2]: RefObj("b")
//         }
//       }
//     }
//     ConcurrentAssertions[0]: ConcurrentAssertions { // Rule 4
//       label: ""
//       stmt: null
//       property: PropertySpec {
//         clockingEvent: null
//         disableCondition: null
//         propertyExpr: SequenceInst → seq          // Rule 5 ← spec-correct
//         // Surelog bug EL0535: produces RefObj("seq") instead
//       }
//     }

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
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sequence_decl.h>
#include <hldb/sequence_inst.h>

#include <string>

namespace hlc {

class NamedSequenceTest : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "16.7--sequence.hlc"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@top", d->getAllModules());
}

static const hldb::Net *getNet(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getNets()) return nullptr;
  return hldb::findByName<hldb::Net>(name, m->getNets());
}

static const hldb::SequenceDecl *getSeqDecl(const hldb::Design *d) {
  const hldb::Module *m = getTop(d);
  if (!m) return nullptr;
  const auto *decls = m->getSequenceDecls();
  if (!decls || decls->empty()) return nullptr;
  return (*decls)[0];
}

static const hldb::ClockedSeq *getClockedSeq(const hldb::Design *d) {
  const auto *sd = getSeqDecl(d);
  if (!sd) return nullptr;
  return sd->getExpr<hldb::ClockedSeq>();
}

static const hldb::Operation *getClockingEventOp(const hldb::Design *d) {
  const auto *cs = getClockedSeq(d);
  if (!cs) return nullptr;
  return cs->getClockingEvent<hldb::Operation>();
}

static const hldb::Operation *getSeqExprOp(const hldb::Design *d) {
  const auto *cs = getClockedSeq(d);
  if (!cs) return nullptr;
  return cs->getSequenceExpr<hldb::Operation>();
}

static const hldb::ConcurrentAssertions *getAssert(const hldb::Design *d) {
  const hldb::Module *m = getTop(d);
  if (!m) return nullptr;
  const auto *cas = m->getConcurrentAssertions();
  if (!cas || cas->empty()) return nullptr;
  return (*cas)[0];
}

static const hldb::PropertySpec *getPropSpec(const hldb::Design *d) {
  const auto *ca = getAssert(d);
  if (!ca) return nullptr;
  return ca->getProperty<hldb::PropertySpec>();
}

// ---------------------------------------------------------------------------
// Module and nets
// ---------------------------------------------------------------------------

TEST_F(NamedSequenceTest, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found";
}

TEST_F(NamedSequenceTest, NetClk_HasLogicTypespec) {
  // SV source: 'logic clk' — §6.3: 4-state 1-bit variable.
  const hldb::Net *const net = getNet(m_design, "clk");
  ASSERT_NE(net, nullptr) << "net 'clk' not found";
  ASSERT_NE(net->getTypespec(), nullptr) << "net 'clk' has no typespec";
  EXPECT_NE(net->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr)
      << "'logic clk' must produce a LogicTypespec";
}

TEST_F(NamedSequenceTest, NetA_HasLogicTypespec) {
  const hldb::Net *const net = getNet(m_design, "a");
  ASSERT_NE(net, nullptr) << "net 'a' not found";
  ASSERT_NE(net->getTypespec(), nullptr) << "net 'a' has no typespec";
  EXPECT_NE(net->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr)
      << "'logic a' must produce a LogicTypespec";
}

TEST_F(NamedSequenceTest, NetB_HasLogicTypespec) {
  const hldb::Net *const net = getNet(m_design, "b");
  ASSERT_NE(net, nullptr) << "net 'b' not found";
  ASSERT_NE(net->getTypespec(), nullptr) << "net 'b' has no typespec";
  EXPECT_NE(net->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr)
      << "'logic b' must produce a LogicTypespec";
}

// ---------------------------------------------------------------------------
// §16.7 Rule 1: named sequence declaration.
// ---------------------------------------------------------------------------

TEST_F(NamedSequenceTest, SeqDecl_Collection_NonNull) {
  // §16.7: a 'sequence … endsequence' declaration at module scope must
  // populate Module::getSequenceDecls().
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(m->getSequenceDecls(), nullptr)
      << "§16.7: 'sequence seq; … endsequence' must populate "
         "getSequenceDecls()";
}

TEST_F(NamedSequenceTest, SeqDecl_Collection_HasOneEntry) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getSequenceDecls(), nullptr);
  EXPECT_EQ(m->getSequenceDecls()->size(), 1u)
      << "exactly one sequence declaration 'seq' must be present";
}

TEST_F(NamedSequenceTest, SeqDecl_Name_IsSeq) {
  // §16.7: 'sequence seq; …' — the declared sequence name must be "seq".
  const auto *sd = getSeqDecl(m_design);
  ASSERT_NE(sd, nullptr);
  EXPECT_EQ(sd->getName(), "seq")
      << "§16.7: SequenceDecl::getName() must be \"seq\"";
}

TEST_F(NamedSequenceTest, SeqDecl_NoFormalArguments) {
  // §16.7: the sequence declaration has no formal argument list.
  // getSeqFormalDecls() must be null.
  const auto *sd = getSeqDecl(m_design);
  ASSERT_NE(sd, nullptr);
  EXPECT_EQ(sd->getSeqFormalDecls(), nullptr)
      << "§16.7: 'sequence seq;' with no port list must have null "
         "getSeqFormalDecls()";
}

TEST_F(NamedSequenceTest, SeqDecl_HasBodyExpression) {
  // §16.7: the sequence body '@(posedge clk) a ##1 b' must be non-null.
  const auto *sd = getSeqDecl(m_design);
  ASSERT_NE(sd, nullptr);
  EXPECT_NE(sd->getExpr(), nullptr)
      << "§16.7: sequence body must be non-null";
}

TEST_F(NamedSequenceTest, SeqDecl_BodyExpr_IsClockedSeq) {
  // §16.7: '@(posedge clk) a ##1 b' is a clocked sequence expression.
  // UHDM represents this as a ClockedSeq node.
  const auto *sd = getSeqDecl(m_design);
  ASSERT_NE(sd, nullptr);
  EXPECT_NE(sd->getExpr<hldb::ClockedSeq>(), nullptr)
      << "§16.7: clocked sequence body '@(posedge clk) …' must be a ClockedSeq";
}

// ---------------------------------------------------------------------------
// §16.7 Rule 3: clocking event @(posedge clk).
// ---------------------------------------------------------------------------

TEST_F(NamedSequenceTest, SeqDecl_ClockedSeq_HasClockingEvent) {
  // §16.7: '@(posedge clk)' defines the clocking event for the sequence.
  const auto *cs = getClockedSeq(m_design);
  ASSERT_NE(cs, nullptr);
  EXPECT_NE(cs->getClockingEvent(), nullptr)
      << "§16.7: '@(posedge clk)' must produce a non-null clocking event";
}

TEST_F(NamedSequenceTest, SeqDecl_ClockedSeq_ClockingEvent_IsOperation) {
  const auto *cs = getClockedSeq(m_design);
  ASSERT_NE(cs, nullptr);
  EXPECT_NE(cs->getClockingEvent<hldb::Operation>(), nullptr)
      << "§16.7: clocking event '@(posedge clk)' must be an Operation node";
}

TEST_F(NamedSequenceTest, SeqDecl_ClockedSeq_ClockingEvent_IsPosedge) {
  // §16.7: 'posedge' edge sensitivity → vpiPosedgeOp (39).
  const auto *op = getClockingEventOp(m_design);
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiPosedgeOp)
      << "§16.7: '@(posedge clk)' clocking event must have "
         "opType == vpiPosedgeOp";
}

TEST_F(NamedSequenceTest, SeqDecl_ClockedSeq_ClockingEvent_HasOneOperand) {
  // The posedge operation has one operand: the clock signal 'clk'.
  const auto *op = getClockingEventOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 1u)
      << "posedge clocking event must have exactly 1 operand (the clock signal)";
}

TEST_F(NamedSequenceTest, SeqDecl_ClockedSeq_ClockingEvent_OperandIsClk) {
  // The single operand of '@(posedge clk)' must be a RefObj named 'clk'.
  const auto *op = getClockingEventOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *ref = any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr)
      << "clocking event operand must be a RefObj";
  EXPECT_EQ(ref->getName(), "clk")
      << "clocking event operand must reference signal 'clk'";
}

// ---------------------------------------------------------------------------
// §16.7 Rule 3: sequence expression 'a ##1 b'.
// '##1' is the binary cycle delay — one clock cycle between 'a' and 'b'.
// UHDM: Operation with opType vpiCycleDelayOp(71), 3 operands:
//   [0] RefObj("a")   -- left sequence expression
//   [1] Constant("1") -- the delay amount
//   [2] RefObj("b")   -- right sequence expression
// ---------------------------------------------------------------------------

TEST_F(NamedSequenceTest, SeqDecl_ClockedSeq_HasSeqExpr) {
  // §16.7: 'a ##1 b' — the sequence expression must be non-null.
  const auto *cs = getClockedSeq(m_design);
  ASSERT_NE(cs, nullptr);
  EXPECT_NE(cs->getSequenceExpr(), nullptr)
      << "§16.7: sequence expression 'a ##1 b' must be non-null";
}

TEST_F(NamedSequenceTest, SeqDecl_ClockedSeq_SeqExpr_IsCycleDelayOp) {
  // §16.7: 'a ##1 b' has a left operand so '##1' is the binary form ->
  // vpiCycleDelayOp(71). vpiUnaryCycleDelayOp(70) is for '##n expr' only.
  const auto *op = getSeqExprOp(m_design);
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCycleDelayOp)
      << "§16.7: 'a ##1 b' must use vpiCycleDelayOp(71); "
         "binary '##' with a left operand uses vpiCycleDelayOp";
}

TEST_F(NamedSequenceTest, SeqDecl_ClockedSeq_SeqExpr_HasThreeOperands) {
  // '##1' with a delay amount produces 3 operands:
  // [0] left seq, [1] delay amount (Constant 1), [2] right seq.
  const auto *op = getSeqExprOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 3u)
      << "'a ##1 b' must have 3 operands: left seq, delay amount, right seq";
}

TEST_F(NamedSequenceTest, SeqDecl_ClockedSeq_SeqExpr_DelayAmount_IsConstant) {
  // operands[1] is the cycle delay amount -- integer literal 1 -> Constant.
  const auto *op = getSeqExprOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  EXPECT_NE(any_cast<const hldb::Constant *>((*op->getOperands())[1]), nullptr)
      << "'##1' delay amount must be a Constant node";
}

TEST_F(NamedSequenceTest, SeqDecl_ClockedSeq_SeqExpr_DelayAmount_IsOne) {
  // §16.7: '##1' -- the delay amount is the integer literal 1.
  const auto *op = getSeqExprOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const auto *c = any_cast<const hldb::Constant *>((*op->getOperands())[1]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getDecompile()), "1")
      << "'##1' cycle delay amount must be the constant 1";
}

TEST_F(NamedSequenceTest, SeqDecl_ClockedSeq_SeqExpr_LeftSignal_IsRefObjA) {
  // operands[0] is the left sequence expression: signal 'a'.
  const auto *op = getSeqExprOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *ref = any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr)
      << "'a ##1 b' left operand must be a RefObj";
  EXPECT_EQ(ref->getName(), "a")
      << "'a ##1 b' left operand must reference signal 'a'";
}

TEST_F(NamedSequenceTest, SeqDecl_ClockedSeq_SeqExpr_RightSignal_IsRefObjB) {
  // operands[2] is the right sequence expression: signal 'b'.
  const auto *op = getSeqExprOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 3u);
  const auto *ref = any_cast<const hldb::RefObj *>((*op->getOperands())[2]);
  ASSERT_NE(ref, nullptr)
      << "'a ##1 b' right operand must be a RefObj";
  EXPECT_EQ(ref->getName(), "b")
      << "'a ##1 b' right operand must reference signal 'b'";
}

// ---------------------------------------------------------------------------
// §16.7 Rule 4: concurrent assertion 'assert property (seq)'.
// ---------------------------------------------------------------------------

TEST_F(NamedSequenceTest, ConcAssert_Collection_NonNull) {
  // §16.7: 'assert property (seq)' must populate getConcurrentAssertions().
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(m->getConcurrentAssertions(), nullptr)
      << "§16.7: 'assert property (seq)' must populate "
         "getConcurrentAssertions()";
}

TEST_F(NamedSequenceTest, ConcAssert_Collection_HasOneEntry) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getConcurrentAssertions(), nullptr);
  EXPECT_EQ(m->getConcurrentAssertions()->size(), 1u)
      << "exactly one concurrent assertion must be present";
}

TEST_F(NamedSequenceTest, ConcAssert_HasNoLabel) {
  // SV source: no label prefix on 'assert property (seq)'.
  // §16.7: unlabeled concurrent assertions have an empty label.
  const auto *ca = getAssert(m_design);
  ASSERT_NE(ca, nullptr);
  EXPECT_TRUE(ca->getLabel().empty())
      << "§16.7: unlabeled 'assert property' must have empty label";
}

TEST_F(NamedSequenceTest, ConcAssert_HasNoActionBlock) {
  // SV source: 'assert property (seq);' — no pass or fail statement.
  const auto *ca = getAssert(m_design);
  ASSERT_NE(ca, nullptr);
  EXPECT_EQ(ca->getStmt(), nullptr)
      << "§16.7: 'assert property (seq);' has no action block — "
         "getStmt() must be null";
}

TEST_F(NamedSequenceTest, ConcAssert_HasProperty) {
  // §16.7: the concurrent assertion must have a property expression.
  const auto *ca = getAssert(m_design);
  ASSERT_NE(ca, nullptr);
  EXPECT_NE(ca->getProperty(), nullptr)
      << "§16.7: 'assert property (seq)' must have a non-null property";
}

TEST_F(NamedSequenceTest, ConcAssert_Property_IsPropertySpec) {
  // §16.7: the property of a concurrent assertion is wrapped in a PropertySpec.
  const auto *ca = getAssert(m_design);
  ASSERT_NE(ca, nullptr);
  EXPECT_NE(ca->getProperty<hldb::PropertySpec>(), nullptr)
      << "§16.7: concurrent assertion property must be a PropertySpec";
}

TEST_F(NamedSequenceTest, ConcAssert_PropSpec_HasNoClockingEvent) {
  // §16.7: the clocking event '@(posedge clk)' is declared inside the named
  // sequence body, not on the 'assert property' statement. Therefore the
  // PropertySpec's own clocking event must be null — the sequence provides it.
  const auto *ps = getPropSpec(m_design);
  ASSERT_NE(ps, nullptr);
  EXPECT_EQ(ps->getClockingEvent(), nullptr)
      << "§16.7: clock is declared inside the named sequence — "
         "PropertySpec::getClockingEvent() must be null";
}

TEST_F(NamedSequenceTest, ConcAssert_PropSpec_HasNoDisableCondition) {
  // SV source: no 'disable iff (…)' clause on the assertion.
  const auto *ps = getPropSpec(m_design);
  ASSERT_NE(ps, nullptr);
  EXPECT_EQ(ps->getDisableCondition(), nullptr)
      << "§16.7: 'assert property (seq);' has no disable_iff — "
         "getDisableCondition() must be null";
}

TEST_F(NamedSequenceTest, ConcAssert_PropSpec_HasPropertyExpr) {
  // §16.7: 'assert property (seq)' — the PropertySpec must have a
  // non-null property expression referencing the sequence 'seq'.
  const auto *ps = getPropSpec(m_design);
  ASSERT_NE(ps, nullptr);
  EXPECT_NE(ps->getPropertyExpr(), nullptr)
      << "§16.7: PropertySpec::getPropertyExpr() must be non-null";
}

TEST_F(NamedSequenceTest, ConcAssert_PropSpec_PropertyExpr_IsSequenceInst) {
  // §16.7: 'assert property (seq)' — the property expression must be a
  // SequenceInst referencing the named sequence 'seq'.
  //
  // Surelog bug EL0535: Surelog treats 'seq' as an implicit net instead of a
  // sequence reference, so getPropertyExpr<SequenceInst>() returns nullptr.
  // This test FAILS to document that bug.
  const auto *ps = getPropSpec(m_design);
  ASSERT_NE(ps, nullptr);
  EXPECT_NE(ps->getPropertyExpr<hldb::SequenceInst>(), nullptr)
      << "§16.7: 'assert property (seq)' property expression must be a "
         "SequenceInst; Surelog EL0535: 'seq' is treated as an implicit net "
         "→ RefObj returned instead of SequenceInst";
}

TEST_F(NamedSequenceTest, ConcAssert_PropSpec_PropertyExpr_NameIsSeq) {
  // Regardless of the concrete node type (SequenceInst per spec, RefObj per
  // Surelog bug EL0535), the expression must reference 'seq' by name.
  const auto *ps = getPropSpec(m_design);
  ASSERT_NE(ps, nullptr);
  const auto *expr = ps->getPropertyExpr();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getName(), "seq")
      << "§16.7: property expression must reference the named sequence 'seq'";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
