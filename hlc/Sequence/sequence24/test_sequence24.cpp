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

// Tests IEEE 1800-2017 ss.16.7 open-ended cycle delay range for sequence24.sv:
//
//   sequence seq_open;
//     a ##[2:$] b;  // delay >= 2
//   endsequence
//
//   assert property(@(posedge clk) seq_open);
//
// ss.16.7 rules:
//   'a ##[m:$] b' denotes a cycle delay of at least m cycles between a and b.
//   The HLDB must represent this as an Operation with opType vpiCycleDelayOp
//   (71) and three operands: RefObj(a), Range(2,$), RefObj(b). The lower
//   bound must be a finite vpiUIntConst (9) constant and the upper bound must
//   be a vpiUnboundedConst (11) constant that decompiles as "$".
//
// Compile-stage: 'seq_open' in 'assert property(...)' resolves correctly
// to SequenceDecl seq_open.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bit_typespec.h>
#include <hldb/concurrent_assertions.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/variable.h>
#include <hldb/operation.h>
#include <hldb/property_spec.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sequence_decl.h>
#include <hldb/sv_vpi_user.h>

#include <string>

namespace hlc {

class Sequence24Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "sequence24.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTb(const hldb::Design *d) {
    return hldb::findByName<hldb::Module>("tb", d->getAllModules());
  }

  static const hldb::SequenceDecl *getSeqDecl(const hldb::Module *mod,
                                               std::string_view name) {
    if (!mod || !mod->getSequenceDecls()) return nullptr;
    for (const hldb::SequenceDecl *const s : *mod->getSequenceDecls()) {
      if (s->getName() == name) return s;
    }
    return nullptr;
  }
};

// ===========================================================================
// Module
// ===========================================================================

TEST_F(Sequence24Test, ModuleExists) {
  ASSERT_NE(getTb(m_design), nullptr) << "module 'tb' not found";
}

// ===========================================================================
// Variables (IEEE 1800-2023 Sec 6.7/6.8: no net-type keyword means
// Variable, not Net, regardless of default_nettype)
// ===========================================================================

TEST_F(Sequence24Test, Variable_clk_HasBitTypespec) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getVariables(), nullptr);
  const hldb::Variable *const clk =
      hldb::findByName<hldb::Variable>("clk", tb->getVariables());
  ASSERT_NE(clk, nullptr) << "variable 'clk' not found";
  ASSERT_NE(clk->getTypespec(), nullptr);
  EXPECT_NE(clk->getTypespec()->getActual<hldb::BitTypespec>(), nullptr)
      << "'bit clk' must produce a BitTypespec";
  EXPECT_EQ(hldb::findByName<hldb::Net>("clk", tb->getNets()), nullptr)
      << "'bit clk' has no net-type keyword -- must not also appear in vpiNet";
}

TEST_F(Sequence24Test, Variable_a_HasBitTypespec) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getVariables(), nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", tb->getVariables());
  ASSERT_NE(a, nullptr) << "variable 'a' not found";
  ASSERT_NE(a->getTypespec(), nullptr);
  EXPECT_NE(a->getTypespec()->getActual<hldb::BitTypespec>(), nullptr)
      << "'bit a' must produce a BitTypespec";
  EXPECT_EQ(hldb::findByName<hldb::Net>("a", tb->getNets()), nullptr)
      << "'bit a' has no net-type keyword -- must not also appear in vpiNet";
}

TEST_F(Sequence24Test, Variable_b_HasBitTypespec) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getVariables(), nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", tb->getVariables());
  ASSERT_NE(b, nullptr) << "variable 'b' not found";
  ASSERT_NE(b->getTypespec(), nullptr);
  EXPECT_NE(b->getTypespec()->getActual<hldb::BitTypespec>(), nullptr)
      << "'bit b' must produce a BitTypespec";
  EXPECT_EQ(hldb::findByName<hldb::Net>("b", tb->getNets()), nullptr)
      << "'bit b' has no net-type keyword -- must not also appear in vpiNet";
}

// ===========================================================================
// SequenceDecl collection
// ===========================================================================

TEST_F(Sequence24Test, SeqDeclCollection_HasOneEntry) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr);
  EXPECT_EQ(tb->getSequenceDecls()->size(), 1u)
      << "one sequence declaration expected (seq_open)";
}

TEST_F(Sequence24Test, SeqDecl_seq_open_Exists) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  EXPECT_NE(getSeqDecl(tb, "seq_open"), nullptr)
      << "SequenceDecl named 'seq_open' not found";
}

// ===========================================================================
// seq_open body: a ##[2:$] b  (ss.16.7 open-ended cycle delay)
// ===========================================================================

TEST_F(Sequence24Test, SeqOpen_ExprIsCycleDelayOp) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_open");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCycleDelayOp)
      << "ss.16.7: 'a ##[2:$] b' must use vpiCycleDelayOp (71)";
}

TEST_F(Sequence24Test, SeqOpen_Op_HasThreeOperands) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_open");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 3u)
      << "ss.16.7: 'a ##[2:$] b' must have 3 operands: "
         "[RefObj(a), Range(2,$), RefObj(b)]";
}

TEST_F(Sequence24Test, SeqOpen_Op_Operand0_IsRefObjA) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_open");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const hldb::RefObj *const ref =
      any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "a")
      << "ss.16.7: operands[0] of 'a ##[2:$] b' must be a RefObj named 'a'";
}

TEST_F(Sequence24Test, SeqOpen_Op_Operand0_ResolvesToNetA) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_open");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const hldb::RefObj *const ref =
      any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_NE(ref->getActual<hldb::Variable>(), nullptr)
      << "ss.16.7: 'a' in 'a ##[2:$] b' must resolve to Variable 'a'";
}

TEST_F(Sequence24Test, SeqOpen_Op_Operand1_IsRange) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_open");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const hldb::Range *const range =
      any_cast<const hldb::Range *>((*op->getOperands())[1]);
  EXPECT_NE(range, nullptr)
      << "ss.16.7: operands[1] of 'a ##[2:$] b' must be a Range(2,$)";
}

TEST_F(Sequence24Test, SeqOpen_Op_Range_LowerBound_IsConstant) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_open");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const hldb::Range *const range =
      any_cast<const hldb::Range *>((*op->getOperands())[1]);
  ASSERT_NE(range, nullptr);
  EXPECT_NE(range->getLeftExpr<hldb::Constant>(), nullptr)
      << "ss.16.7: lower bound of Range must be a Constant";
}

TEST_F(Sequence24Test, SeqOpen_Op_Range_LowerBound_ValueIsTwo) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_open");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const hldb::Range *const range =
      any_cast<const hldb::Range *>((*op->getOperands())[1]);
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const lb = range->getLeftExpr<hldb::Constant>();
  ASSERT_NE(lb, nullptr);
  EXPECT_EQ(std::string(lb->getDecompile()), "2")
      << "ss.16.7: lower bound of '##[2:$]' must decompile to \"2\"";
}

TEST_F(Sequence24Test, SeqOpen_Op_Range_LowerBound_ConstType_IsUInt) {
  // Finite delay bounds use vpiUIntConst (9).
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_open");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const hldb::Range *const range =
      any_cast<const hldb::Range *>((*op->getOperands())[1]);
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const lb = range->getLeftExpr<hldb::Constant>();
  ASSERT_NE(lb, nullptr);
  EXPECT_EQ(lb->getConstType(), vpiUIntConst)
      << "ss.16.7: finite lower bound must be vpiUIntConst (9)";
}

TEST_F(Sequence24Test, SeqOpen_Op_Range_UpperBound_IsConstant) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_open");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const hldb::Range *const range =
      any_cast<const hldb::Range *>((*op->getOperands())[1]);
  ASSERT_NE(range, nullptr);
  EXPECT_NE(range->getRightExpr<hldb::Constant>(), nullptr)
      << "ss.16.7: upper bound of Range must be a Constant";
}

TEST_F(Sequence24Test, SeqOpen_Op_Range_UpperBound_DecompilesAsDollar) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_open");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const hldb::Range *const range =
      any_cast<const hldb::Range *>((*op->getOperands())[1]);
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const ub = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(ub, nullptr);
  EXPECT_EQ(std::string(ub->getDecompile()), "$")
      << "ss.16.7: upper bound of '##[2:$]' must decompile to \"$\"";
}

TEST_F(Sequence24Test, SeqOpen_Op_Range_UpperBound_ConstType_IsUnbounded) {
  // '$' in a delay range is vpiUnboundedConst (11), not vpiUIntConst (9).
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_open");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const hldb::Range *const range =
      any_cast<const hldb::Range *>((*op->getOperands())[1]);
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const ub = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(ub, nullptr);
  EXPECT_EQ(ub->getConstType(), vpiUnboundedConst)
      << "ss.16.7: '$' upper bound must be vpiUnboundedConst (11)";
}

TEST_F(Sequence24Test, SeqOpen_Op_Operand2_IsRefObjB) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_open");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 3u);
  const hldb::RefObj *const ref =
      any_cast<const hldb::RefObj *>((*op->getOperands())[2]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "b")
      << "ss.16.7: operands[2] of 'a ##[2:$] b' must be a RefObj named 'b'";
}

TEST_F(Sequence24Test, SeqOpen_Op_Operand2_ResolvesToNetB) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_open");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 3u);
  const hldb::RefObj *const ref =
      any_cast<const hldb::RefObj *>((*op->getOperands())[2]);
  ASSERT_NE(ref, nullptr);
  EXPECT_NE(ref->getActual<hldb::Variable>(), nullptr)
      << "ss.16.7: 'b' in 'a ##[2:$] b' must resolve to Variable 'b'";
}

// ===========================================================================
// Concurrent assertion
// ===========================================================================

TEST_F(Sequence24Test, Assert_ConcurrentAssertionExists) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr);
  EXPECT_GE(tb->getConcurrentAssertions()->size(), 1u)
      << "at least one concurrent assertion expected";
}

TEST_F(Sequence24Test, Assert_HasInlineClockingEvent) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr);
  ASSERT_GE(tb->getConcurrentAssertions()->size(), 1u);
  const hldb::ConcurrentAssertions *const ca =
      (*tb->getConcurrentAssertions())[0];
  ASSERT_NE(ca, nullptr);
  const hldb::PropertySpec *const spec = ca->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  EXPECT_NE(spec->getClockingEvent<hldb::Operation>(), nullptr)
      << "assert property must have an inline clocking event";
}

TEST_F(Sequence24Test, Assert_ClockingEvent_IsPosedge) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr);
  ASSERT_GE(tb->getConcurrentAssertions()->size(), 1u);
  const hldb::ConcurrentAssertions *const ca =
      (*tb->getConcurrentAssertions())[0];
  ASSERT_NE(ca, nullptr);
  const hldb::PropertySpec *const spec = ca->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::Operation *const clkOp =
      spec->getClockingEvent<hldb::Operation>();
  ASSERT_NE(clkOp, nullptr);
  EXPECT_EQ(clkOp->getOpType(), vpiPosedgeOp)
      << "clocking event must be vpiPosedgeOp (39)";
}

TEST_F(Sequence24Test, Assert_ClockingEvent_OperandIsClk) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr);
  ASSERT_GE(tb->getConcurrentAssertions()->size(), 1u);
  const hldb::ConcurrentAssertions *const ca =
      (*tb->getConcurrentAssertions())[0];
  ASSERT_NE(ca, nullptr);
  const hldb::PropertySpec *const spec = ca->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::Operation *const clkOp =
      spec->getClockingEvent<hldb::Operation>();
  ASSERT_NE(clkOp, nullptr);
  ASSERT_NE(clkOp->getOperands(), nullptr);
  ASSERT_GE(clkOp->getOperands()->size(), 1u);
  const hldb::RefObj *const ref =
      any_cast<const hldb::RefObj *>((*clkOp->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "clk")
      << "clocking event operand must reference 'clk'";
}

TEST_F(Sequence24Test, Assert_PropertyExpr_IsRefObj) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr);
  ASSERT_GE(tb->getConcurrentAssertions()->size(), 1u);
  const hldb::ConcurrentAssertions *const ca =
      (*tb->getConcurrentAssertions())[0];
  ASSERT_NE(ca, nullptr);
  const hldb::PropertySpec *const spec = ca->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  EXPECT_NE(spec->getPropertyExpr<hldb::RefObj>(), nullptr)
      << "property expression must be a RefObj referencing seq_open";
}

TEST_F(Sequence24Test, Assert_PropertyExpr_NameIsSeqOpen) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr);
  ASSERT_GE(tb->getConcurrentAssertions()->size(), 1u);
  const hldb::ConcurrentAssertions *const ca =
      (*tb->getConcurrentAssertions())[0];
  ASSERT_NE(ca, nullptr);
  const hldb::PropertySpec *const spec = ca->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *const expr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getName(), "seq_open")
      << "assertion must reference 'seq_open'";
}

TEST_F(Sequence24Test, Assert_PropertyExpr_ResolvedToSeqOpenDecl) {
  // ss.16.7: 'seq_open' in 'assert property(...)' must resolve to
  // SequenceDecl seq_open.
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr);
  ASSERT_GE(tb->getConcurrentAssertions()->size(), 1u);
  const hldb::ConcurrentAssertions *const ca =
      (*tb->getConcurrentAssertions())[0];
  ASSERT_NE(ca, nullptr);
  const hldb::PropertySpec *const spec = ca->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *const expr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(expr, nullptr);
  EXPECT_NE(expr->getActual<hldb::SequenceDecl>(), nullptr)
      << "ss.16.7: 'seq_open' in assert property must resolve to SequenceDecl";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
