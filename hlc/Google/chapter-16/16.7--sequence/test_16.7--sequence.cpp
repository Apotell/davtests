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

// Spec-based validation of IEEE 1800-2023 sec. 16.7 named sequence and
// concurrent assertion.
//
// All expected values are derived from sec. 16.7 of the spec and the SV
// source. No expected value is taken from the UHDM log.
//
// -- sec. 16.7 rules under test ----
//
// sec. 16.7 defines named sequences and their use in concurrent assertions:
//
//   sequence identifier;
//     [clocking_event] sequence_expr;
//   endsequence
//   assert property (sequence_or_property);
//
// Rule 1 -- A named sequence is declared with 'sequence ... endsequence'.
//   -> Module::getSequenceDecls() must be non-null and contain one SequenceDecl.
//   -> SequenceDecl::getName() must be "seq".
//
// Rule 2 -- This sequence has no formal argument list (no port list).
//   -> SequenceDecl::getSeqFormalDecls() must be null.
//
// Rule 3 -- The sequence body '@(posedge clk) a ##1 b' is a clocked sequence
//   expression. UHDM represents this as a ClockedSeq node:
//     ClockedSeq::getClockingEvent() -> Operation (vpiPosedgeOp) with RefObj 'clk'
//     ClockedSeq::getSequenceExpr()  -> Operation (vpiCycleDelayOp)
//       operands: [RefObj("a"), Constant("1"), RefObj("b")]
//   The cycle delay '##1' with a left-hand operand uses op-type
//   vpiCycleDelayOp (71) with three operands: left sequence expression,
//   delay amount, right sequence expression.
//
// Rule 4 -- 'assert property (seq)' produces a ConcurrentAssertions node.
//   -> Module::getConcurrentAssertions() must be non-null and contain one entry.
//   -> ConcurrentAssertions::getProperty() returns a PropertySpec.
//   -> PropertySpec::getClockingEvent() is null -- the clock is inside the
//      named sequence body, not on the assert statement.
//   -> ConcurrentAssertions::getStmt() is null -- no explicit action block.
//
// Rule 5 -- The property expression in 'assert property (seq)' is a
//   reference to the named sequence 'seq'. A bare sequence identifier with
//   no argument list is represented as a RefObj whose getActual() resolves
//   to the SequenceDecl (this tool's ObjectBinder pass resolves same-scope
//   simple names without requiring full hierarchical elaboration).
//
// -- Declarations 'logic clk;' / 'logic a;' / 'logic b;' ----
// None of these use a net-type keyword, 'interconnect', or a user-defined
// nettype, so per sec. 6.7/6.8 all three are Variables, not Nets.
//
// -- SV source ----
//   logic clk;
//   logic a;
//   logic b;
//   sequence seq;
//       @(posedge clk) a ##1 b;
//   endsequence
//   assert property (seq);
//
// -- Spec-correct UHDM ----
//   Module 'top':
//     Variables: clk, a, b -- all LogicTypespec
//     SequenceDecls[0]: SequenceDecl {              // Rule 1
//       name: "seq"
//       seqFormalDecls: null                        // Rule 2
//       expr: ClockedSeq {                          // Rule 3
//         clockingEvent: Operation {
//           opType: vpiPosedgeOp (39)
//           operands[0]: RefObj("clk")
//         }
//         sequenceExpr: Operation {
//           opType: vpiCycleDelayOp (71)            // '##1'
//           operands[0]: RefObj("a")
//           operands[1]: Constant("1")              // delay amount
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
//         propertyExpr: RefObj("seq")               // Rule 5
//           actual: SequenceDecl("seq")              // resolved by ObjectBinder
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
#include <hldb/variable.h>

#include <string>

namespace hlc {

class NamedSequenceTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16.7--sequence.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

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

// ----
// Module and variables
// ----

TEST_F(NamedSequenceTest, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'top' not found"; }

TEST_F(NamedSequenceTest, VariableClk_HasLogicTypespec) {
  // SV source: 'logic clk' -- sec. 6.3: 4-state 1-bit type; no net-type
  // keyword, so per sec. 6.7/6.8 this is a Variable.
  const hldb::Variable *const var = getVariable(m_design, "clk");
  ASSERT_NE(var, nullptr) << "variable 'clk' not found";
  ASSERT_NE(var->getTypespec(), nullptr) << "variable 'clk' has no typespec";
  EXPECT_NE(var->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr)
      << "'logic clk' must produce a LogicTypespec";
}

TEST_F(NamedSequenceTest, VariableA_HasLogicTypespec) {
  const hldb::Variable *const var = getVariable(m_design, "a");
  ASSERT_NE(var, nullptr) << "variable 'a' not found";
  ASSERT_NE(var->getTypespec(), nullptr) << "variable 'a' has no typespec";
  EXPECT_NE(var->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr) << "'logic a' must produce a LogicTypespec";
}

TEST_F(NamedSequenceTest, VariableB_HasLogicTypespec) {
  const hldb::Variable *const var = getVariable(m_design, "b");
  ASSERT_NE(var, nullptr) << "variable 'b' not found";
  ASSERT_NE(var->getTypespec(), nullptr) << "variable 'b' has no typespec";
  EXPECT_NE(var->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr) << "'logic b' must produce a LogicTypespec";
}

TEST_F(NamedSequenceTest, Variables_NotAlsoInNets) {
  // sec. 6.7/6.8: none of clk/a/b uses a net-type keyword, so none may also
  // appear in the module's net collection.
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  if (m->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("clk", m->getNets()), nullptr) << "'clk' must not also appear in getNets()";
    EXPECT_EQ(hldb::findByName<hldb::Net>("a", m->getNets()), nullptr) << "'a' must not also appear in getNets()";
    EXPECT_EQ(hldb::findByName<hldb::Net>("b", m->getNets()), nullptr) << "'b' must not also appear in getNets()";
  }
}

// ----
// sec. 16.7 Rule 1: named sequence declaration.
// ----

TEST_F(NamedSequenceTest, SeqDecl_Collection_NonNull) {
  // sec. 16.7: a 'sequence ... endsequence' declaration at module scope must
  // populate Module::getSequenceDecls().
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(m->getSequenceDecls(), nullptr) << "sec. 16.7: 'sequence seq; ... endsequence' must populate "
                                               "getSequenceDecls()";
}

TEST_F(NamedSequenceTest, SeqDecl_Collection_HasOneEntry) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getSequenceDecls(), nullptr);
  EXPECT_EQ(m->getSequenceDecls()->size(), 1u) << "exactly one sequence declaration 'seq' must be present";
}

TEST_F(NamedSequenceTest, SeqDecl_Name_IsSeq) {
  // sec. 16.7: 'sequence seq; ...' -- the declared sequence name must be "seq".
  const auto *sd = getSeqDecl(m_design);
  ASSERT_NE(sd, nullptr);
  EXPECT_EQ(sd->getName(), "seq") << "sec. 16.7: SequenceDecl::getName() must be \"seq\"";
}

TEST_F(NamedSequenceTest, SeqDecl_NoFormalArguments) {
  // sec. 16.7: the sequence declaration has no formal argument list.
  // getSeqFormalDecls() must be null.
  const auto *sd = getSeqDecl(m_design);
  ASSERT_NE(sd, nullptr);
  EXPECT_EQ(sd->getSeqFormalDecls(), nullptr) << "sec. 16.7: 'sequence seq;' with no port list must have null "
                                                 "getSeqFormalDecls()";
}

TEST_F(NamedSequenceTest, SeqDecl_HasBodyExpression) {
  // sec. 16.7: the sequence body '@(posedge clk) a ##1 b' must be non-null.
  const auto *sd = getSeqDecl(m_design);
  ASSERT_NE(sd, nullptr);
  EXPECT_NE(sd->getExpr(), nullptr) << "sec. 16.7: sequence body must be non-null";
}

TEST_F(NamedSequenceTest, SeqDecl_BodyExpr_IsClockedSeq) {
  // sec. 16.7: '@(posedge clk) a ##1 b' is a clocked sequence expression.
  // UHDM represents this as a ClockedSeq node.
  const auto *sd = getSeqDecl(m_design);
  ASSERT_NE(sd, nullptr);
  EXPECT_NE(sd->getExpr<hldb::ClockedSeq>(), nullptr)
      << "sec. 16.7: clocked sequence body '@(posedge clk) ...' must be a ClockedSeq";
}

// ----
// sec. 16.7 Rule 3: clocking event @(posedge clk).
// ----

TEST_F(NamedSequenceTest, SeqDecl_ClockedSeq_HasClockingEvent) {
  // sec. 16.7: '@(posedge clk)' defines the clocking event for the sequence.
  const auto *cs = getClockedSeq(m_design);
  ASSERT_NE(cs, nullptr);
  EXPECT_NE(cs->getClockingEvent(), nullptr) << "sec. 16.7: '@(posedge clk)' must produce a non-null clocking event";
}

TEST_F(NamedSequenceTest, SeqDecl_ClockedSeq_ClockingEvent_IsOperation) {
  const auto *cs = getClockedSeq(m_design);
  ASSERT_NE(cs, nullptr);
  EXPECT_NE(cs->getClockingEvent<hldb::Operation>(), nullptr)
      << "sec. 16.7: clocking event '@(posedge clk)' must be an Operation node";
}

TEST_F(NamedSequenceTest, SeqDecl_ClockedSeq_ClockingEvent_IsPosedge) {
  // sec. 16.7: 'posedge' edge sensitivity -> vpiPosedgeOp (39).
  const auto *op = getClockingEventOp(m_design);
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiPosedgeOp) << "sec. 16.7: '@(posedge clk)' clocking event must have "
                                              "opType == vpiPosedgeOp";
}

TEST_F(NamedSequenceTest, SeqDecl_ClockedSeq_ClockingEvent_HasOneOperand) {
  // The posedge operation has one operand: the clock signal 'clk'.
  const auto *op = getClockingEventOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 1u) << "posedge clocking event must have exactly 1 operand (the clock signal)";
}

TEST_F(NamedSequenceTest, SeqDecl_ClockedSeq_ClockingEvent_OperandIsClk) {
  // The single operand of '@(posedge clk)' must be a RefObj named 'clk'.
  const auto *op = getClockingEventOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *ref = any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr) << "clocking event operand must be a RefObj";
  EXPECT_EQ(ref->getName(), "clk") << "clocking event operand must reference signal 'clk'";
}

// ----
// sec. 16.7 Rule 3: sequence expression 'a ##1 b'.
// '##1' is the binary cycle delay -- one clock cycle between 'a' and 'b'.
// UHDM: Operation with opType vpiCycleDelayOp(71), 3 operands:
//   [0] RefObj("a")   -- left sequence expression
//   [1] Constant("1") -- the delay amount
//   [2] RefObj("b")   -- right sequence expression
// ----

TEST_F(NamedSequenceTest, SeqDecl_ClockedSeq_HasSeqExpr) {
  // sec. 16.7: 'a ##1 b' -- the sequence expression must be non-null.
  const auto *cs = getClockedSeq(m_design);
  ASSERT_NE(cs, nullptr);
  EXPECT_NE(cs->getSequenceExpr(), nullptr) << "sec. 16.7: sequence expression 'a ##1 b' must be non-null";
}

TEST_F(NamedSequenceTest, SeqDecl_ClockedSeq_SeqExpr_IsCycleDelayOp) {
  // sec. 16.7: 'a ##1 b' has a left operand so '##1' is the binary form ->
  // vpiCycleDelayOp(71). vpiUnaryCycleDelayOp is for '##n expr' only.
  const auto *op = getSeqExprOp(m_design);
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCycleDelayOp) << "sec. 16.7: 'a ##1 b' must use vpiCycleDelayOp(71); "
                                                 "binary '##' with a left operand uses vpiCycleDelayOp";
}

TEST_F(NamedSequenceTest, SeqDecl_ClockedSeq_SeqExpr_HasThreeOperands) {
  // '##1' with a delay amount produces 3 operands:
  // [0] left seq, [1] delay amount (Constant 1), [2] right seq.
  const auto *op = getSeqExprOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 3u) << "'a ##1 b' must have 3 operands: left seq, delay amount, right seq";
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
  // sec. 16.7: '##1' -- the delay amount is the integer literal 1.
  const auto *op = getSeqExprOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const auto *c = any_cast<const hldb::Constant *>((*op->getOperands())[1]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getDecompile()), "1") << "'##1' cycle delay amount must be the constant 1";
}

TEST_F(NamedSequenceTest, SeqDecl_ClockedSeq_SeqExpr_LeftSignal_IsRefObjA) {
  // operands[0] is the left sequence expression: signal 'a'.
  const auto *op = getSeqExprOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *ref = any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr) << "'a ##1 b' left operand must be a RefObj";
  EXPECT_EQ(ref->getName(), "a") << "'a ##1 b' left operand must reference signal 'a'";
}

TEST_F(NamedSequenceTest, SeqDecl_ClockedSeq_SeqExpr_RightSignal_IsRefObjB) {
  // operands[2] is the right sequence expression: signal 'b'.
  const auto *op = getSeqExprOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 3u);
  const auto *ref = any_cast<const hldb::RefObj *>((*op->getOperands())[2]);
  ASSERT_NE(ref, nullptr) << "'a ##1 b' right operand must be a RefObj";
  EXPECT_EQ(ref->getName(), "b") << "'a ##1 b' right operand must reference signal 'b'";
}

// ----
// sec. 16.7 Rule 4: concurrent assertion 'assert property (seq)'.
// ----

TEST_F(NamedSequenceTest, ConcAssert_Collection_NonNull) {
  // sec. 16.7: 'assert property (seq)' must populate getConcurrentAssertions().
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(m->getConcurrentAssertions(), nullptr) << "sec. 16.7: 'assert property (seq)' must populate "
                                                      "getConcurrentAssertions()";
}

TEST_F(NamedSequenceTest, ConcAssert_Collection_HasOneEntry) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getConcurrentAssertions(), nullptr);
  EXPECT_EQ(m->getConcurrentAssertions()->size(), 1u) << "exactly one concurrent assertion must be present";
}

TEST_F(NamedSequenceTest, ConcAssert_HasNoLabel) {
  // SV source: no label prefix on 'assert property (seq)'.
  // sec. 16.7: unlabeled concurrent assertions have an empty label.
  const auto *ca = getAssert(m_design);
  ASSERT_NE(ca, nullptr);
  EXPECT_TRUE(ca->getLabel().empty()) << "sec. 16.7: unlabeled 'assert property' must have empty label";
}

TEST_F(NamedSequenceTest, ConcAssert_HasNoActionBlock) {
  // SV source: 'assert property (seq);' -- no pass or fail statement.
  const auto *ca = getAssert(m_design);
  ASSERT_NE(ca, nullptr);
  EXPECT_EQ(ca->getStmt(), nullptr) << "sec. 16.7: 'assert property (seq);' has no action block -- "
                                       "getStmt() must be null";
}

TEST_F(NamedSequenceTest, ConcAssert_HasProperty) {
  // sec. 16.7: the concurrent assertion must have a property expression.
  const auto *ca = getAssert(m_design);
  ASSERT_NE(ca, nullptr);
  EXPECT_NE(ca->getProperty(), nullptr) << "sec. 16.7: 'assert property (seq)' must have a non-null property";
}

TEST_F(NamedSequenceTest, ConcAssert_Property_IsPropertySpec) {
  // sec. 16.7: the property of a concurrent assertion is wrapped in a PropertySpec.
  const auto *ca = getAssert(m_design);
  ASSERT_NE(ca, nullptr);
  EXPECT_NE(ca->getProperty<hldb::PropertySpec>(), nullptr)
      << "sec. 16.7: concurrent assertion property must be a PropertySpec";
}

TEST_F(NamedSequenceTest, ConcAssert_PropSpec_HasNoClockingEvent) {
  // sec. 16.7: the clocking event '@(posedge clk)' is declared inside the
  // named sequence body, not on the 'assert property' statement. Therefore
  // the PropertySpec's own clocking event must be null -- the sequence
  // provides it.
  const auto *ps = getPropSpec(m_design);
  ASSERT_NE(ps, nullptr);
  EXPECT_EQ(ps->getClockingEvent(), nullptr) << "sec. 16.7: clock is declared inside the named sequence -- "
                                                "PropertySpec::getClockingEvent() must be null";
}

TEST_F(NamedSequenceTest, ConcAssert_PropSpec_HasNoDisableCondition) {
  // SV source: no 'disable iff (...)' clause on the assertion.
  const auto *ps = getPropSpec(m_design);
  ASSERT_NE(ps, nullptr);
  EXPECT_EQ(ps->getDisableCondition(), nullptr) << "sec. 16.7: 'assert property (seq);' has no disable_iff -- "
                                                   "getDisableCondition() must be null";
}

TEST_F(NamedSequenceTest, ConcAssert_PropSpec_HasPropertyExpr) {
  // sec. 16.7: 'assert property (seq)' -- the PropertySpec must have a
  // non-null property expression referencing the sequence 'seq'.
  const auto *ps = getPropSpec(m_design);
  ASSERT_NE(ps, nullptr);
  EXPECT_NE(ps->getPropertyExpr(), nullptr) << "sec. 16.7: PropertySpec::getPropertyExpr() must be non-null";
}

TEST_F(NamedSequenceTest, ConcAssert_PropSpec_PropertyExpr_IsRefObj) {
  // sec. 16.7: 'assert property (seq)' with a bare sequence identifier (no
  // argument list) is represented as a RefObj referencing the named
  // sequence 'seq'.
  const auto *ps = getPropSpec(m_design);
  ASSERT_NE(ps, nullptr);
  EXPECT_NE(ps->getPropertyExpr<hldb::RefObj>(), nullptr)
      << "sec. 16.7: 'assert property (seq)' property expression must be a RefObj referencing 'seq'";
}

TEST_F(NamedSequenceTest, ConcAssert_PropSpec_PropertyExpr_NameIsSeq) {
  const auto *ps = getPropSpec(m_design);
  ASSERT_NE(ps, nullptr);
  const auto *expr = ps->getPropertyExpr();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getName(), "seq") << "sec. 16.7: property expression must reference the named sequence 'seq'";
}

TEST_F(NamedSequenceTest, ConcAssert_PropSpec_PropertyExpr_ResolvesToSeqDecl) {
  // Rule 5: the RefObj's actual must resolve to the SequenceDecl 'seq' --
  // this tool's ObjectBinder pass resolves same-scope simple names to their
  // declaration without requiring full hierarchical elaboration.
  const auto *ps = getPropSpec(m_design);
  ASSERT_NE(ps, nullptr);
  const auto *ref = ps->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(ref, nullptr);
  const auto *sd = getSeqDecl(m_design);
  ASSERT_NE(sd, nullptr);
  EXPECT_EQ(ref->getActual<hldb::SequenceDecl>(), sd)
      << "sec. 16.7: RefObj('seq')::getActual() must resolve to the SequenceDecl 'seq'";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
