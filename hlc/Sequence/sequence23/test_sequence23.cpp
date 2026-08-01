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

// Tests IEEE 1800-2017 ss.16.9.1 bounded consecutive repetition for
// sequence23.sv:
//
//   sequence seq_range;
//     a[*2:4];  // between 2 and 4 consecutive matches
//   endsequence
//
//   assert property(@(posedge clk) seq_range);
//
// ss.16.9.1 rules:
//   'expr[*m:n]' denotes consecutive repetition of expr between m and n
//   times. The HLDB must represent this as an Operation with opType
//   vpiConsecutiveRepeatOp (77), two operands: the base expression and a
//   Range node whose left bound is m and right bound is n. Both bounds are
//   finite unsigned integer constants (vpiUIntConst=9).
//
// Compile-stage: 'seq_range' in 'assert property(...)' resolves correctly
// to SequenceDecl seq_range.

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

class Sequence23Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "sequence23.hlc"}); }
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

TEST_F(Sequence23Test, ModuleExists) {
  ASSERT_NE(getTb(m_design), nullptr) << "module 'tb' not found";
}

// ===========================================================================
// Variables (IEEE 1800-2023 Sec 6.7/6.8: no net-type keyword means
// Variable, not Net, regardless of default_nettype)
// ===========================================================================

TEST_F(Sequence23Test, Variable_clk_HasBitTypespec) {
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

TEST_F(Sequence23Test, Variable_a_HasBitTypespec) {
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

// ===========================================================================
// SequenceDecl collection
// ===========================================================================

TEST_F(Sequence23Test, SeqDeclCollection_HasOneEntry) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr);
  EXPECT_EQ(tb->getSequenceDecls()->size(), 1u)
      << "one sequence declaration expected (seq_range)";
}

TEST_F(Sequence23Test, SeqDecl_seq_range_Exists) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  EXPECT_NE(getSeqDecl(tb, "seq_range"), nullptr)
      << "SequenceDecl named 'seq_range' not found";
}

// ===========================================================================
// seq_range body: a[*2:4]  (ss.16.9.1 bounded consecutive repetition)
// ===========================================================================

TEST_F(Sequence23Test, SeqRange_ExprIsOperation) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_range");
  ASSERT_NE(seq, nullptr);
  EXPECT_NE(seq->getExpr<hldb::Operation>(), nullptr)
      << "ss.16.9.1: 'a[*2:4]' body must be an Operation node";
}

TEST_F(Sequence23Test, SeqRange_ExprIsConsecutiveRepeatOp) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_range");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiConsecutiveRepeatOp)
      << "ss.16.9.1: 'a[*2:4]' must use vpiConsecutiveRepeatOp (77)";
}

TEST_F(Sequence23Test, SeqRange_Op_HasTwoOperands) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_range");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u)
      << "ss.16.9.1: 'a[*2:4]' must have 2 operands: [RefObj(a), Range(2,4)]";
}

TEST_F(Sequence23Test, SeqRange_Op_Operand0_IsRefObjA) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_range");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const hldb::RefObj *const ref =
      any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "a")
      << "ss.16.9.1: operands[0] of 'a[*2:4]' must be a RefObj named 'a'";
}

TEST_F(Sequence23Test, SeqRange_Op_Operand0_ResolvesToNetA) {
  // ss.16.9.1: 'a' is a module-level variable ('bit a', no net-type
  // keyword); it must resolve to Variable 'a'.
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_range");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const hldb::RefObj *const ref =
      any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_NE(ref->getActual<hldb::Variable>(), nullptr)
      << "ss.16.9.1: 'a' in 'a[*2:4]' must resolve to Variable 'a'";
}

TEST_F(Sequence23Test, SeqRange_Op_Operand1_IsRange) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_range");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const hldb::Range *const range =
      any_cast<const hldb::Range *>((*op->getOperands())[1]);
  EXPECT_NE(range, nullptr)
      << "ss.16.9.1: operands[1] of 'a[*2:4]' must be a Range(2,4)";
}

TEST_F(Sequence23Test, SeqRange_Op_Range_LowerBound_IsConstant) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_range");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const hldb::Range *const range =
      any_cast<const hldb::Range *>((*op->getOperands())[1]);
  ASSERT_NE(range, nullptr);
  EXPECT_NE(range->getLeftExpr<hldb::Constant>(), nullptr)
      << "ss.16.9.1: lower bound of Range must be a Constant";
}

TEST_F(Sequence23Test, SeqRange_Op_Range_LowerBound_ValueIsTwo) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_range");
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
      << "ss.16.9.1: lower bound of 'a[*2:4]' must decompile to \"2\"";
}

TEST_F(Sequence23Test, SeqRange_Op_Range_LowerBound_ConstType_IsUInt) {
  // Repetition bounds are vpiUIntConst (9), not vpiIntConst (7).
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_range");
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
      << "ss.16.9.1: repetition lower bound must be vpiUIntConst (9)";
}

TEST_F(Sequence23Test, SeqRange_Op_Range_UpperBound_IsConstant) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_range");
  ASSERT_NE(seq, nullptr);
  const hldb::Operation *const op = seq->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const hldb::Range *const range =
      any_cast<const hldb::Range *>((*op->getOperands())[1]);
  ASSERT_NE(range, nullptr);
  EXPECT_NE(range->getRightExpr<hldb::Constant>(), nullptr)
      << "ss.16.9.1: upper bound of Range must be a Constant";
}

TEST_F(Sequence23Test, SeqRange_Op_Range_UpperBound_ValueIsFour) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_range");
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
  EXPECT_EQ(std::string(ub->getDecompile()), "4")
      << "ss.16.9.1: upper bound of 'a[*2:4]' must decompile to \"4\"";
}

TEST_F(Sequence23Test, SeqRange_Op_Range_UpperBound_ConstType_IsUInt) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const seq = getSeqDecl(tb, "seq_range");
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
  EXPECT_EQ(ub->getConstType(), vpiUIntConst)
      << "ss.16.9.1: repetition upper bound must be vpiUIntConst (9)";
}

// ===========================================================================
// Concurrent assertion
// ===========================================================================

TEST_F(Sequence23Test, Assert_ConcurrentAssertionExists) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr);
  EXPECT_GE(tb->getConcurrentAssertions()->size(), 1u)
      << "at least one concurrent assertion expected";
}

TEST_F(Sequence23Test, Assert_HasInlineClockingEvent) {
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

TEST_F(Sequence23Test, Assert_ClockingEvent_IsPosedge) {
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

TEST_F(Sequence23Test, Assert_ClockingEvent_OperandIsClk) {
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

TEST_F(Sequence23Test, Assert_PropertyExpr_IsRefObj) {
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
      << "property expression must be a RefObj referencing seq_range";
}

TEST_F(Sequence23Test, Assert_PropertyExpr_NameIsSeqRange) {
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
  EXPECT_EQ(expr->getName(), "seq_range")
      << "assertion must reference 'seq_range'";
}

TEST_F(Sequence23Test, Assert_PropertyExpr_ResolvedToSeqRangeDecl) {
  // ss.16.9.1: 'seq_range' in 'assert property(...)' must resolve to
  // SequenceDecl seq_range.
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
      << "ss.16.9.1: 'seq_range' in assert property must resolve to SequenceDecl";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
