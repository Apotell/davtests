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

// Spec-based validation of IEEE 1800-2023 sec. 16.12 property declarations.
// SV: tests/Google/chapter-16/16.12--property.sv
//
//   logic clk;
//   logic a;
//   assert property ( @(posedge clk) (a == 1));
//   assert property ( not @(posedge clk) (a == 1));
//
// -- sec. 16.12 constructs under test ----
//
// Assert[0]: anonymous inline concurrent assertion -- clocking event and
//   boolean expression written directly inside 'assert property'.
//
// Assert[1]: sec. 16.12 'not' property negation applied to the same inline
//   clocked property. Per Annex A property_expr grammar:
//     property_expr ::= ... | NOT property_expr | clocking_event property_expr | ...
//   so 'not @(posedge clk) (a == 1)' parses as NOT applied to the inner
//   'clocking_event property_expr' -- i.e. NOT wraps a clocked sequence.
//
// -- Declarations 'logic clk;' / 'logic a;' ----
// Neither declaration uses a net-type keyword (wire/tri/etc.), is
// 'interconnect', or is a user-defined nettype -- per sec. 6.7/6.8 both are
// plain variables, not nets, regardless of the module's default nettype.
//
// -- UHDM tree for Assert[0] ----
//
//   Assert[0]
//   +-- PropertySpec
//       +-- clockingEvent  Operation { posedgeOp(39) }
//       |   +-- operands[0]  RefObj("clk")
//       +-- propertyExpr   Operation { vpiEqOp(14) }
//           +-- operands[0]  RefObj("a")
//           +-- operands[1]  Constant("1", uIntConst=9)
//
// -- UHDM tree for Assert[1] ----
//
//   Assert[1]
//   +-- PropertySpec  (no clockingEvent at this level -- see below)
//       +-- propertyExpr  Operation { vpiNotOp(3) }
//           +-- operands[0]  ClockedSeq
//               +-- clockingEvent  Operation { posedgeOp(39) }
//               |   +-- operands[0]  RefObj("clk")
//               +-- sequenceExpr   Operation { vpiEqOp(14) }
//                   +-- operands[0]  RefObj("a")
//                   +-- operands[1]  Constant("1", uIntConst=9)
//
// For Assert[1] the clocking event is NOT a direct child of PropertySpec
// (unlike Assert[0]); it lives inside the ClockedSeq that is the single
// operand of the outer 'not' Operation. This is the correct per-grammar
// shape: 'not' takes a property_expr operand, and the operand here is
// itself 'clocking_event property_expr', which is modeled as ClockedSeq.
//
// -- VPI constants ----
//   vpiPosedgeOp   = 39
//   vpiEqOp        = 14
//   vpiNotOp       =  3
//   vpiUIntConst   =  9

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
#include <hldb/variable.h>

#include <string>

namespace hlc {

class PropertyTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16.12--property.hlc"}); }
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

// The (a == 1) Operation from Assert[0]
static const hldb::Operation *getEqOp(const hldb::Design *d) {
  const auto *ps = getPropSpec(d, 0);
  if (!ps) return nullptr;
  return ps->getPropertyExpr<hldb::Operation>();
}

// The outer 'not' Operation from Assert[1].
static const hldb::Operation *getNotOp(const hldb::Design *d) {
  const auto *ps = getPropSpec(d, 1);
  if (!ps) return nullptr;
  return ps->getPropertyExpr<hldb::Operation>();
}

// The ClockedSeq operand of the 'not' Operation from Assert[1].
static const hldb::ClockedSeq *getNotClockedSeq(const hldb::Design *d) {
  const auto *op = getNotOp(d);
  if (!op) return nullptr;
  const auto *operands = op->getOperands();
  if (!operands) return nullptr;
  if (operands->size() != 1) return nullptr;
  return any_cast<hldb::ClockedSeq>(operands->front());
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(PropertyTest, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'top' not found"; }

// ===========================================================================
// Variables -- logic clk, a (sec. 6.7/6.8: no net-type keyword -> Variable)
// ===========================================================================

TEST_F(PropertyTest, Variable_clk_HasLogicTypespec) {
  const auto *var = getVariable(m_design, "clk");
  ASSERT_NE(var, nullptr) << "variable 'clk' not found";
  ASSERT_NE(var->getTypespec(), nullptr);
  EXPECT_NE(var->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr)
      << "'logic clk' must produce a LogicTypespec";
}

TEST_F(PropertyTest, Variable_a_HasLogicTypespec) {
  const auto *var = getVariable(m_design, "a");
  ASSERT_NE(var, nullptr) << "variable 'a' not found";
  ASSERT_NE(var->getTypespec(), nullptr);
  EXPECT_NE(var->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr)
      << "'logic a' must produce a LogicTypespec";
}

TEST_F(PropertyTest, Variable_clk_NotAlsoInNets) {
  // sec. 6.7/6.8: 'logic clk' with no net-type keyword is a variable, never
  // a net -- it must not also appear in the module's net collection.
  const auto *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  if (m->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("clk", m->getNets()), nullptr)
        << "'clk' is a variable (no net-type keyword) and must not also appear in getNets()";
  }
}

TEST_F(PropertyTest, Variable_a_NotAlsoInNets) {
  const auto *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  if (m->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("a", m->getNets()), nullptr)
        << "'a' is a variable (no net-type keyword) and must not also appear in getNets()";
  }
}

// ===========================================================================
// Module structure -- no named sequence or property declarations
// ===========================================================================

TEST_F(PropertyTest, Module_HasNoSequenceDecls) {
  const auto *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getSequenceDecls(), nullptr) << "anonymous inline asserts have no sequence declarations";
}

TEST_F(PropertyTest, Module_HasNoPropertyDecls) {
  const auto *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_EQ(m->getPropertyDecls(), nullptr) << "anonymous inline asserts have no property declarations";
}

// ===========================================================================
// ConcurrentAssertions -- two asserts
// ===========================================================================

TEST_F(PropertyTest, ConcAssert_Collection_HasTwoEntries) {
  const auto *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getConcurrentAssertions(), nullptr);
  EXPECT_EQ(m->getConcurrentAssertions()->size(), 2u) << "two 'assert property' statements expected";
}

// ===========================================================================
// assert property (@(posedge clk) (a == 1)) -- Assert[0]
// ===========================================================================

TEST_F(PropertyTest, ConcAssert0_HasNoActionBlock) {
  const auto *ca = getAssertAt(m_design, 0);
  ASSERT_NE(ca, nullptr);
  EXPECT_EQ(ca->getStmt(), nullptr) << "assert has no action block -- getStmt() must be null";
}

TEST_F(PropertyTest, ConcAssert0_Property_IsPropertySpec) {
  const auto *ca = getAssertAt(m_design, 0);
  ASSERT_NE(ca, nullptr);
  EXPECT_NE(ca->getProperty<hldb::PropertySpec>(), nullptr) << "assert property must produce a PropertySpec node";
}

TEST_F(PropertyTest, ConcAssert0_ClockingEvent_IsPosedge) {
  const auto *ps = getPropSpec(m_design, 0);
  ASSERT_NE(ps, nullptr);
  const auto *op = ps->getClockingEvent<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "PropertySpec must have a clocking event";
  EXPECT_EQ(op->getOpType(), vpiPosedgeOp) << "'@(posedge clk)' must use vpiPosedgeOp (39)";
}

TEST_F(PropertyTest, ConcAssert0_ClockingEvent_HasOneOperand) {
  const auto *ps = getPropSpec(m_design, 0);
  ASSERT_NE(ps, nullptr);
  const auto *op = ps->getClockingEvent<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 1u) << "posedge event has exactly one operand (the clock signal)";
}

TEST_F(PropertyTest, ConcAssert0_ClockingEvent_OperandIsClk) {
  const auto *ps = getPropSpec(m_design, 0);
  ASSERT_NE(ps, nullptr);
  const auto *op = ps->getClockingEvent<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *ref = any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "clk");
}

TEST_F(PropertyTest, ConcAssert0_PropertyExpr_IsOperation) {
  // Inline boolean 'a == 1' must produce an Operation node.
  const auto *op = getEqOp(m_design);
  EXPECT_NE(op, nullptr) << "sec. 16.12: inline boolean property expr must be an Operation";
}

TEST_F(PropertyTest, ConcAssert0_PropertyExpr_IsEqualOp) {
  const auto *op = getEqOp(m_design);
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiEqOp) << "'a == 1' must use vpiEqOp (14)";
}

TEST_F(PropertyTest, ConcAssert0_PropertyExpr_HasTwoOperands) {
  const auto *op = getEqOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u) << "'a == 1' binary operator must have 2 operands";
}

TEST_F(PropertyTest, ConcAssert0_PropertyExpr_LeftOperand_IsRefObjA) {
  const auto *op = getEqOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *ref = any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "a") << "'a == 1' left operand must be signal 'a'";
}

TEST_F(PropertyTest, ConcAssert0_PropertyExpr_RightOperand_IsConstantOne) {
  const auto *op = getEqOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const auto *c = any_cast<const hldb::Constant *>((*op->getOperands())[1]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getValue()), "1") << "'a == 1' right operand must be the constant 1";
}

TEST_F(PropertyTest, ConcAssert0_PropertyExpr_RightOperand_IsUnsignedInt) {
  const auto *op = getEqOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const auto *c = any_cast<const hldb::Constant *>((*op->getOperands())[1]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), vpiUIntConst) << "literal '1' in property expression must be vpiUIntConst (9)";
}

// ===========================================================================
// assert property (not @(posedge clk) (a == 1)) -- Assert[1]
// ===========================================================================
//
// sec. 16.12 / Annex A property_expr grammar: 'not' is a unary property
// operator that negates the property_expr operand. Here the operand is
// itself 'clocking_event property_expr' ('@(posedge clk) (a == 1)'), which
// is modeled as a ClockedSeq. So the outer propertyExpr is
// Operation{vpiNotOp} with a single operand: the ClockedSeq.
//
// Key structural difference from Assert[0]:
//   Assert[0]: PropertySpec.clockingEvent exists (at PropertySpec level)
//   Assert[1]: PropertySpec has NO clockingEvent; the clocking event is
//              inside the ClockedSeq that is the operand of the 'not' op.

TEST_F(PropertyTest, ConcAssert1_HasNoActionBlock) {
  const auto *ca = getAssertAt(m_design, 1);
  ASSERT_NE(ca, nullptr);
  EXPECT_EQ(ca->getStmt(), nullptr) << "'assert property (not ...);' has no action block";
}

TEST_F(PropertyTest, ConcAssert1_Property_IsPropertySpec) {
  const auto *ca = getAssertAt(m_design, 1);
  ASSERT_NE(ca, nullptr);
  EXPECT_NE(ca->getProperty<hldb::PropertySpec>(), nullptr) << "assert property must produce a PropertySpec node";
}

TEST_F(PropertyTest, ConcAssert1_PropertyExpr_IsOperation) {
  // sec. 16.12: 'not property_expr' must produce Operation{vpiNotOp} as the
  // outer property expr.
  const auto *ps = getPropSpec(m_design, 1);
  ASSERT_NE(ps, nullptr);
  EXPECT_NE(ps->getPropertyExpr<hldb::Operation>(), nullptr) << "sec. 16.12: 'not property_expr' must produce "
                                                                 "an Operation node";
}

TEST_F(PropertyTest, ConcAssert1_PropertyExpr_IsNotOp) {
  const auto *op = getNotOp(m_design);
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiNotOp) << "'not property_expr' must use vpiNotOp (3)";
}

TEST_F(PropertyTest, ConcAssert1_PropertyExpr_HasOneOperand) {
  // 'not' is a unary property operator -- exactly 1 operand.
  const auto *op = getNotOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 1u) << "unary 'not' must have exactly 1 operand";
}

TEST_F(PropertyTest, ConcAssert1_PropertySpec_HasNoClockingEvent) {
  // The clocking event is inside the ClockedSeq (the 'not' operand), not on
  // the outer PropertySpec. Unlike Assert[0], PropertySpec.clockingEvent is
  // null for Assert[1].
  const auto *ps = getPropSpec(m_design, 1);
  ASSERT_NE(ps, nullptr);
  EXPECT_EQ(ps->getClockingEvent(), nullptr) << "Assert[1] PropertySpec has no clockingEvent at its own level -- "
                                                 "it is inside the ClockedSeq operand of the 'not' Operation";
}

TEST_F(PropertyTest, ConcAssert1_PropertyExpr_OperandIsClockedSeq) {
  // The single operand of the outer 'not' Operation must be a ClockedSeq,
  // since the negated property_expr is itself 'clocking_event property_expr'.
  const auto *cs = getNotClockedSeq(m_design);
  EXPECT_NE(cs, nullptr) << "operand of 'not' must be a ClockedSeq for '@(posedge clk) (a == 1)'";
}

TEST_F(PropertyTest, ConcAssert1_ClockedSeq_ClockingEvent_IsPosedge) {
  // The @(posedge clk) from the inner 'not @(posedge clk) (a == 1)'.
  const auto *cs = getNotClockedSeq(m_design);
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getClockingEvent<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "ClockedSeq must have a clocking event";
  EXPECT_EQ(op->getOpType(), vpiPosedgeOp) << "'@(posedge clk)' inside 'not' must use vpiPosedgeOp (39)";
}

TEST_F(PropertyTest, ConcAssert1_ClockedSeq_ClockingEvent_HasOneOperand) {
  const auto *cs = getNotClockedSeq(m_design);
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getClockingEvent<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 1u) << "posedge event has exactly one operand";
}

TEST_F(PropertyTest, ConcAssert1_ClockedSeq_ClockingEvent_OperandIsClk) {
  const auto *cs = getNotClockedSeq(m_design);
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getClockingEvent<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *ref = any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "clk");
}

TEST_F(PropertyTest, ConcAssert1_ClockedSeq_SequenceExpr_IsEqOp) {
  // The (a == 1) expression inside 'not @(posedge clk) (a == 1)'.
  const auto *cs = getNotClockedSeq(m_design);
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getSequenceExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "ClockedSeq sequenceExpr must be an Operation";
  EXPECT_EQ(op->getOpType(), vpiEqOp) << "'a == 1' inside ClockedSeq must use vpiEqOp (14)";
}

TEST_F(PropertyTest, ConcAssert1_ClockedSeq_SequenceExpr_HasTwoOperands) {
  const auto *cs = getNotClockedSeq(m_design);
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getSequenceExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u) << "'a == 1' binary operator must have 2 operands";
}

TEST_F(PropertyTest, ConcAssert1_ClockedSeq_SequenceExpr_LeftIsRefObjA) {
  const auto *cs = getNotClockedSeq(m_design);
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getSequenceExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *ref = any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "a") << "'a == 1' left operand must be signal 'a'";
}

TEST_F(PropertyTest, ConcAssert1_ClockedSeq_SequenceExpr_RightIsConstantOne) {
  const auto *cs = getNotClockedSeq(m_design);
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getSequenceExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const auto *c = any_cast<const hldb::Constant *>((*op->getOperands())[1]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getValue()), "1") << "'a == 1' right operand must be the constant 1";
}

TEST_F(PropertyTest, ConcAssert1_ClockedSeq_SequenceExpr_RightIsUnsignedInt) {
  const auto *cs = getNotClockedSeq(m_design);
  ASSERT_NE(cs, nullptr);
  const auto *op = cs->getSequenceExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const auto *c = any_cast<const hldb::Constant *>((*op->getOperands())[1]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), vpiUIntConst) << "literal '1' inside ClockedSeq must be vpiUIntConst (9)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
