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

// Spec-based validation of IEEE 1800-2017 §16.12 property declarations.
// SV: tests/Google/chapter-16/16.12--property.sv
//
//   logic clk;
//   logic a;
//   assert property ( @(posedge clk) (a == 1));
//   assert property ( not @(posedge clk) (a == 1));
//
// ── §16.12 constructs under test ───────────────────────────────────────────
//
// Assert[0]: anonymous inline concurrent assertion — clocking event and
//   boolean expression written directly inside 'assert property'.
//
// Assert[1]: §16.12 'not' property negation applied to the same inline
//   clocked property. 'not' is a unary property operator that negates the
//   property it wraps.
//
// ── UHDM tree for Assert[0] (from log) ────────────────────────────────────
//
//   Assert[0]
//   └── PropertySpec
//       ├── clockingEvent  Operation { posedgeOp(39) }
//       │   └── operands[0]  RefObj("clk")
//       └── propertyExpr   Operation { vpiEqOp(14) }
//           ├── operands[0]  RefObj("a")
//           └── operands[1]  Constant("1", uIntConst=9)
//
// ── UHDM tree for Assert[1] (from log) ────────────────────────────────────
//
//   Assert[1]
//   └── PropertySpec  (no clockingEvent at this level)
//       └── propertyExpr  ClockedSeq
//           ├── clockingEvent  Operation { posedgeOp(39) }
//           │   └── operands[0]  RefObj("clk")
//           └── sequenceExpr   Operation { vpiEqOp(14) }
//               ├── operands[0]  RefObj("a")
//               └── operands[1]  Constant("1", uIntConst=9)
//
// ── Surelog bug — 'not' silently dropped ──────────────────────────────────
//
// Surelog silently drops the §16.12 'not' property operator from
// 'assert property (not @(posedge clk) (a == 1))'. The inner clocked
// property is preserved as a ClockedSeq node, but the 'not' negation wrapper
// is completely absent from the UHDM output. No EL0535 error is reported —
// the drop is silent.
//
// This differs from Assert[0] where the clocking event lives directly on
// PropertySpec; for Assert[1] Surelog places it inside a ClockedSeq.
//
// Test ConcAssert1_PropertyExpr_ShouldBeOperation FAILS intentionally —
// it documents the dropped 'not'.
//
// ── VPI constants ──────────────────────────────────────────────────────────
//   vpiPosedgeOp   = 39
//   vpiEqOp        = 14
//   vpiNotOp       =  4
//   vpiUIntConst   =  9

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
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>

#include <string>

namespace SURELOG {

class PropertyTest : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "16.12--property.hlc"});

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

// The (a == 1) Operation from Assert[0]
static const uhdm::Operation *getEqOp(const uhdm::Design *d) {
  const auto *ps = getPropSpec(d, 0);
  if (!ps) return nullptr;
  return ps->getPropertyExpr<uhdm::Operation>();
}

// The ClockedSeq from Assert[1] (Surelog drops 'not', wraps inner property)
static const uhdm::ClockedSeq *getNotClockedSeq(const uhdm::Design *d) {
  const auto *ps = getPropSpec(d, 1);
  if (!ps) return nullptr;
  return ps->getPropertyExpr<uhdm::ClockedSeq>();
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(PropertyTest, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found";
}

// ===========================================================================
// Nets — logic clk, a
// ===========================================================================

TEST_F(PropertyTest, Net_clk_HasLogicTypespec) {
  const auto *net = getNet(m_design, "clk");
  ASSERT_NE(net, nullptr) << "net 'clk' not found";
  ASSERT_NE(net->getTypespec(), nullptr);
  EXPECT_NE(net->getTypespec()->getActual<uhdm::LogicTypespec>(), nullptr)
      << "'logic clk' must produce a LogicTypespec";
}

TEST_F(PropertyTest, Net_a_HasLogicTypespec) {
  const auto *net = getNet(m_design, "a");
  ASSERT_NE(net, nullptr) << "net 'a' not found";
  ASSERT_NE(net->getTypespec(), nullptr);
  EXPECT_NE(net->getTypespec()->getActual<uhdm::LogicTypespec>(), nullptr)
      << "'logic a' must produce a LogicTypespec";
}

// ===========================================================================
// Module structure — no named sequence or property declarations
// ===========================================================================

TEST_F(PropertyTest, Module_HasNoSequenceDecls) {
  const auto *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getSequenceDecls(), nullptr)
      << "anonymous inline asserts have no sequence declarations";
}

TEST_F(PropertyTest, Module_HasNoPropertyDecls) {
  const auto *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getPropertyDecls(), nullptr)
      << "anonymous inline asserts have no property declarations";
}

// ===========================================================================
// ConcurrentAssertions — two asserts
// ===========================================================================

TEST_F(PropertyTest, ConcAssert_Collection_HasTwoEntries) {
  const auto *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getConcurrentAssertions(), nullptr);
  EXPECT_EQ(m->getConcurrentAssertions()->size(), 2u)
      << "two 'assert property' statements expected";
}

// ===========================================================================
// assert property (@(posedge clk) (a == 1)) — Assert[0]
// ===========================================================================

TEST_F(PropertyTest, ConcAssert0_HasNoActionBlock) {
  const auto *ca = getAssertAt(m_design, 0);
  ASSERT_NE(ca, nullptr);
  EXPECT_EQ(ca->getStmt(), nullptr)
      << "assert has no action block — getStmt() must be null";
}

TEST_F(PropertyTest, ConcAssert0_Property_IsPropertySpec) {
  const auto *ca = getAssertAt(m_design, 0);
  ASSERT_NE(ca, nullptr);
  EXPECT_NE(ca->getProperty<uhdm::PropertySpec>(), nullptr)
      << "assert property must produce a PropertySpec node";
}

TEST_F(PropertyTest, ConcAssert0_ClockingEvent_IsPosedge) {
  const auto *ps = getPropSpec(m_design, 0);
  ASSERT_NE(ps, nullptr);
  const auto *op = ps->getClockingEvent<uhdm::Operation>();
  ASSERT_NE(op, nullptr) << "PropertySpec must have a clocking event";
  EXPECT_EQ(op->getOpType(), vpiPosedgeOp)
      << "'@(posedge clk)' must use vpiPosedgeOp (39)";
}

TEST_F(PropertyTest, ConcAssert0_ClockingEvent_HasOneOperand) {
  const auto *ps = getPropSpec(m_design, 0);
  ASSERT_NE(ps, nullptr);
  const auto *op = ps->getClockingEvent<uhdm::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 1u)
      << "posedge event has exactly one operand (the clock signal)";
}

TEST_F(PropertyTest, ConcAssert0_ClockingEvent_OperandIsClk) {
  const auto *ps = getPropSpec(m_design, 0);
  ASSERT_NE(ps, nullptr);
  const auto *op = ps->getClockingEvent<uhdm::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *ref =
      any_cast<const uhdm::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "clk");
}

TEST_F(PropertyTest, ConcAssert0_PropertyExpr_IsOperation) {
  // Inline boolean 'a == 1' must produce an Operation node. No EL0535 here,
  // so getPropertyExpr<Operation>() must succeed.
  const auto *op = getEqOp(m_design);
  EXPECT_NE(op, nullptr)
      << "§16.12: inline boolean property expr must be an Operation";
}

TEST_F(PropertyTest, ConcAssert0_PropertyExpr_IsEqualOp) {
  const auto *op = getEqOp(m_design);
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiEqOp)
      << "'a == 1' must use vpiEqOp (14)";
}

TEST_F(PropertyTest, ConcAssert0_PropertyExpr_HasTwoOperands) {
  const auto *op = getEqOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u)
      << "'a == 1' binary operator must have 2 operands";
}

TEST_F(PropertyTest, ConcAssert0_PropertyExpr_LeftOperand_IsRefObjA) {
  const auto *op = getEqOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *ref =
      any_cast<const uhdm::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "a")
      << "'a == 1' left operand must be signal 'a'";
}

TEST_F(PropertyTest, ConcAssert0_PropertyExpr_RightOperand_IsConstantOne) {
  const auto *op = getEqOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const auto *c =
      any_cast<const uhdm::Constant *>((*op->getOperands())[1]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getValue()), "1")
      << "'a == 1' right operand must be the constant 1";
}

TEST_F(PropertyTest, ConcAssert0_PropertyExpr_RightOperand_IsUnsignedInt) {
  const auto *op = getEqOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const auto *c =
      any_cast<const uhdm::Constant *>((*op->getOperands())[1]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), vpiUIntConst)
      << "literal '1' in property expression must be vpiUIntConst (9)";
}

// ===========================================================================
// assert property (not @(posedge clk) (a == 1)) — Assert[1]
// ===========================================================================
//
// §16.12: 'not' is a unary property operator that negates the property.
// Surelog silently drops the 'not' operator. The inner clocked property
// '@(posedge clk) (a == 1)' is preserved as a ClockedSeq node and becomes
// the propertyExpr of the outer PropertySpec directly.
//
// Key structural difference from Assert[0]:
//   Assert[0]: PropertySpec.clockingEvent exists (at PropertySpec level)
//   Assert[1]: PropertySpec has NO clockingEvent; the clocking event is
//              inside the ClockedSeq that IS the propertyExpr.

TEST_F(PropertyTest, ConcAssert1_HasNoActionBlock) {
  const auto *ca = getAssertAt(m_design, 1);
  ASSERT_NE(ca, nullptr);
  EXPECT_EQ(ca->getStmt(), nullptr)
      << "'assert property (not …);' has no action block";
}

TEST_F(PropertyTest, ConcAssert1_Property_IsPropertySpec) {
  const auto *ca = getAssertAt(m_design, 1);
  ASSERT_NE(ca, nullptr);
  EXPECT_NE(ca->getProperty<uhdm::PropertySpec>(), nullptr)
      << "assert property must produce a PropertySpec node";
}

TEST_F(PropertyTest, ConcAssert1_PropertyExpr_ShouldBeOperation) {
  // §16.12: 'not @(posedge clk) (a == 1)' must produce Operation{vpiNotOp}
  // as the outer property expr.
  // Surelog bug: 'not' is silently dropped — propertyExpr is ClockedSeq.
  // This test FAILS intentionally — documents the dropped 'not'.
  const auto *ps = getPropSpec(m_design, 1);
  ASSERT_NE(ps, nullptr);
  EXPECT_NE(ps->getPropertyExpr<uhdm::Operation>(), nullptr)
      << "§16.12: 'not property_expr' must produce Operation{vpiNotOp}; "
         "Surelog silently drops 'not' — ClockedSeq returned instead";
}

TEST_F(PropertyTest, ConcAssert1_PropertySpec_HasNoClockingEvent) {
  // Surelog places the clocking event inside the ClockedSeq (propertyExpr),
  // not on the outer PropertySpec. Unlike Assert[0], PropertySpec.clockingEvent
  // is null for Assert[1].
  const auto *ps = getPropSpec(m_design, 1);
  ASSERT_NE(ps, nullptr);
  EXPECT_EQ(ps->getClockingEvent(), nullptr)
      << "Assert[1] PropertySpec has no clockingEvent at its own level — "
         "it is inside the ClockedSeq propertyExpr";
}

TEST_F(PropertyTest, ConcAssert1_PropertyExpr_IsClockedSeq) {
  // Documents actual Surelog output: propertyExpr is ClockedSeq because
  // Surelog drops 'not' and preserves only the inner clocked property.
  const auto *cs = getNotClockedSeq(m_design);
  EXPECT_NE(cs, nullptr)
      << "Surelog drops 'not' — propertyExpr is ClockedSeq";
}

TEST_F(PropertyTest, ConcAssert1_ClockedSeq_ClockingEvent_IsPosedge) {
  // The @(posedge clk) from the inner 'not @(posedge clk) (a == 1)'.
  const auto *cs = getNotClockedSeq(m_design);
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getClockingEvent<uhdm::Operation>();
  ASSERT_NE(op, nullptr) << "ClockedSeq must have a clocking event";
  EXPECT_EQ(op->getOpType(), vpiPosedgeOp)
      << "'@(posedge clk)' inside 'not' must use vpiPosedgeOp (39)";
}

TEST_F(PropertyTest, ConcAssert1_ClockedSeq_ClockingEvent_HasOneOperand) {
  const auto *cs = getNotClockedSeq(m_design);
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getClockingEvent<uhdm::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 1u)
      << "posedge event has exactly one operand";
}

TEST_F(PropertyTest, ConcAssert1_ClockedSeq_ClockingEvent_OperandIsClk) {
  const auto *cs = getNotClockedSeq(m_design);
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

TEST_F(PropertyTest, ConcAssert1_ClockedSeq_SequenceExpr_IsEqOp) {
  // The (a == 1) expression inside 'not @(posedge clk) (a == 1)'.
  const auto *cs = getNotClockedSeq(m_design);
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getSequenceExpr<uhdm::Operation>();
  ASSERT_NE(op, nullptr) << "ClockedSeq sequenceExpr must be an Operation";
  EXPECT_EQ(op->getOpType(), vpiEqOp)
      << "'a == 1' inside ClockedSeq must use vpiEqOp (14)";
}

TEST_F(PropertyTest, ConcAssert1_ClockedSeq_SequenceExpr_HasTwoOperands) {
  const auto *cs = getNotClockedSeq(m_design);
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getSequenceExpr<uhdm::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u)
      << "'a == 1' binary operator must have 2 operands";
}

TEST_F(PropertyTest, ConcAssert1_ClockedSeq_SequenceExpr_LeftIsRefObjA) {
  const auto *cs = getNotClockedSeq(m_design);
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getSequenceExpr<uhdm::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *ref =
      any_cast<const uhdm::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "a")
      << "'a == 1' left operand must be signal 'a'";
}

TEST_F(PropertyTest, ConcAssert1_ClockedSeq_SequenceExpr_RightIsConstantOne) {
  const auto *cs = getNotClockedSeq(m_design);
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getSequenceExpr<uhdm::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const auto *c =
      any_cast<const uhdm::Constant *>((*op->getOperands())[1]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getValue()), "1")
      << "'a == 1' right operand must be the constant 1";
}

TEST_F(PropertyTest, ConcAssert1_ClockedSeq_SequenceExpr_RightIsUnsignedInt) {
  const auto *cs = getNotClockedSeq(m_design);
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getSequenceExpr<uhdm::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const auto *c =
      any_cast<const uhdm::Constant *>((*op->getOperands())[1]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), vpiUIntConst)
      << "literal '1' inside ClockedSeq must be vpiUIntConst (9)";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
