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

// Tests IEEE 1800-2017 ss.16.9.2 non-consecutive repetition for sequence17.sv:
//
//   module tb;
//     bit clk; always #5 clk = ~clk;
//     bit a;
//     sequence seq_nonconsec;
//       a[=2];
//     endsequence
//     assert property(@(posedge clk) seq_nonconsec);
//   endmodule
//
// ss.16.9.2 rule: 'a[=n]' (non-consecutive repetition) requires signal 'a'
// to be true exactly n times, not necessarily on consecutive cycles. Unlike
// goto repetition 'a[->n]' which ends on the cycle of the n-th occurrence,
// non-consecutive repetition ends on the cycle AFTER the last occurrence
// (the sequence window closes one clock after the final match). The VPI
// constant is vpiRepeatOp (76), distinct from vpiGotoRepeatOp (78) and
// vpiConsecutiveRepeatOp (77).
//
// Expected HLDB tree for seq_nonconsec body (a[=2]):
//   SequenceDecl("seq_nonconsec")
//     -> getExpr<Operation>() with getOpType() == vpiRepeatOp (76)
//          operands[0]: RefObj("a")   -- the repeated boolean expression
//          operands[1]: Constant("2") -- exact count (vpiUIntConst=9)
//
// Compile-stage: 'seq_nonconsec' in assert property(@(posedge clk)
// seq_nonconsec) resolves correctly to its SequenceDecl and the compile
// produces zero errors.

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

class Sequence17Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "sequence17.hlc"}); }
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

  static const hldb::Operation *getNonConsecRepeatOp(const hldb::Module *mod) {
    const hldb::SequenceDecl *sd = getSeqDecl(mod, "seq_nonconsec");
    if (!sd) return nullptr;
    return sd->getExpr<hldb::Operation>();
  }
};

// ===========================================================================
// Module
// ===========================================================================

TEST_F(Sequence17Test, ModuleExists) {
  ASSERT_NE(getTb(m_design), nullptr) << "module 'tb' not found";
}

// ===========================================================================
// Variables -- bit clk, bit a (IEEE 1800-2023 Sec 6.7/6.8: no net-type
// keyword means Variable, not Net, regardless of default_nettype)
// ===========================================================================

TEST_F(Sequence17Test, Variable_clk_HasBitTypespec) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getVariables(), nullptr);
  const hldb::Variable *const clk = hldb::findByName<hldb::Variable>("clk", tb->getVariables());
  ASSERT_NE(clk, nullptr) << "variable 'clk' not found";
  ASSERT_NE(clk->getTypespec(), nullptr);
  EXPECT_NE(clk->getTypespec()->getActual<hldb::BitTypespec>(), nullptr)
      << "'bit clk' must produce a BitTypespec";
  EXPECT_EQ(hldb::findByName<hldb::Net>("clk", tb->getNets()), nullptr)
      << "'bit clk' has no net-type keyword -- must not also appear in vpiNet";
}

TEST_F(Sequence17Test, Variable_a_HasBitTypespec) {
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
// SequenceDecl -- seq_nonconsec
// ===========================================================================

TEST_F(Sequence17Test, SeqDeclCollection_HasOneEntry) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr);
  EXPECT_EQ(tb->getSequenceDecls()->size(), 1u)
      << "exactly one sequence declaration (seq_nonconsec) expected";
}

TEST_F(Sequence17Test, SeqDecl_seq_nonconsec_Exists) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  EXPECT_NE(getSeqDecl(tb, "seq_nonconsec"), nullptr)
      << "SequenceDecl named 'seq_nonconsec' not found";
}

TEST_F(Sequence17Test, SeqDecl_seq_nonconsec_HasExpression) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const sd = getSeqDecl(tb, "seq_nonconsec");
  ASSERT_NE(sd, nullptr);
  EXPECT_NE(sd->getExpr(), nullptr)
      << "seq_nonconsec body expression must be non-null";
}

// ===========================================================================
// Non-consecutive repeat operation -- a[=2]
// ===========================================================================

TEST_F(Sequence17Test, SeqDecl_seq_nonconsec_ExprIsOperation) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  EXPECT_NE(getNonConsecRepeatOp(tb), nullptr)
      << "seq_nonconsec body must be an Operation node";
}

TEST_F(Sequence17Test, SeqDecl_seq_nonconsec_ExprIsRepeatOp) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getNonConsecRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiRepeatOp)
      << "ss.16.9.2: 'a[=2]' must use vpiRepeatOp (76)";
}

TEST_F(Sequence17Test, SeqDecl_seq_nonconsec_ExprIsNotGotoRepeatOp) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getNonConsecRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  EXPECT_NE(op->getOpType(), vpiGotoRepeatOp)
      << "ss.16.9.2: non-consecutive '[=]' (76) must not be confused with "
         "goto '[->]' (vpiGotoRepeatOp=78)";
}

TEST_F(Sequence17Test, SeqDecl_seq_nonconsec_ExprIsNotConsecutiveRepeatOp) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getNonConsecRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  EXPECT_NE(op->getOpType(), vpiConsecutiveRepeatOp)
      << "ss.16.9.2: non-consecutive '[=]' (76) must not be confused with "
         "consecutive '[*]' (vpiConsecutiveRepeatOp=77)";
}

TEST_F(Sequence17Test, SeqDecl_seq_nonconsec_Op_HasTwoOperands) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getNonConsecRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u)
      << "non-consecutive repeat must have 2 operands: "
         "the repeated expression and the count";
}

// ----
// operands[0] -- the repeated boolean expression: RefObj("a")
// ----

TEST_F(Sequence17Test, SeqDecl_seq_nonconsec_Op_Operand0_IsRefObj) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getNonConsecRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  EXPECT_NE(any_cast<const hldb::RefObj *>((*op->getOperands())[0]), nullptr)
      << "ss.16.9.2: repeated expression operand must be a RefObj";
}

TEST_F(Sequence17Test, SeqDecl_seq_nonconsec_Op_Operand0_NameIsA) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getNonConsecRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const hldb::RefObj *const ref =
      any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "a")
      << "ss.16.9.2: 'a[=2]' repeated expression must reference signal 'a'";
}

TEST_F(Sequence17Test, SeqDecl_seq_nonconsec_Op_Operand0_ResolvesToNet) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getNonConsecRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const hldb::RefObj *const ref =
      any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_NE(ref->getActual<hldb::Variable>(), nullptr)
      << "RefObj 'a' in non-consecutive repeat must resolve to the Variable "
         "declaration";
}

// ----
// operands[1] -- the exact count: Constant("2"), not a Range
// ----

TEST_F(Sequence17Test, SeqDecl_seq_nonconsec_Op_Operand1_IsConstant) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getNonConsecRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  EXPECT_NE(any_cast<const hldb::Constant *>((*op->getOperands())[1]), nullptr)
      << "ss.16.9.2: single count '[=2]' must be stored as a Constant, "
         "not a Range";
}

TEST_F(Sequence17Test, SeqDecl_seq_nonconsec_Op_Operand1_IsNotRange) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getNonConsecRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<const hldb::Range *>((*op->getOperands())[1]), nullptr)
      << "ss.16.9.2: single count '[=2]' must not be a Range node "
         "(Range is only used for '[=n:m]' with distinct bounds)";
}

TEST_F(Sequence17Test, SeqDecl_seq_nonconsec_Op_Operand1_IsUIntConst) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getNonConsecRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const hldb::Constant *const count =
      any_cast<const hldb::Constant *>((*op->getOperands())[1]);
  ASSERT_NE(count, nullptr);
  EXPECT_EQ(count->getConstType(), vpiUIntConst)
      << "ss.16.9.2: non-consecutive repetition count must be an unsigned "
         "integer constant (vpiUIntConst=9)";
}

TEST_F(Sequence17Test, SeqDecl_seq_nonconsec_Op_Operand1_ValueIsTwo) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getNonConsecRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const hldb::Constant *const count =
      any_cast<const hldb::Constant *>((*op->getOperands())[1]);
  ASSERT_NE(count, nullptr);
  EXPECT_EQ(std::string(count->getDecompile()), "2")
      << "ss.16.9.2: 'a[=2]' count constant must decompile to \"2\"";
}

// ===========================================================================
// Concurrent assertion -- assert property(@(posedge clk) seq_nonconsec)
// ===========================================================================

TEST_F(Sequence17Test, Assert_ConcurrentAssertionExists) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr)
      << "module must have concurrent assertions";
  EXPECT_FALSE(tb->getConcurrentAssertions()->empty())
      << "at least one concurrent assertion expected";
}

TEST_F(Sequence17Test, Assert_HasInlineClockingEvent) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr);
  ASSERT_FALSE(tb->getConcurrentAssertions()->empty());
  const hldb::ConcurrentAssertions *const ca =
      (*tb->getConcurrentAssertions())[0];
  ASSERT_NE(ca, nullptr);
  const hldb::PropertySpec *const spec = ca->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr) << "assertion must have a PropertySpec";
  EXPECT_NE(spec->getClockingEvent(), nullptr)
      << "assert property must have an inline clocking event (@(posedge clk))";
}

TEST_F(Sequence17Test, Assert_ClockingEvent_IsPosedge) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr);
  ASSERT_FALSE(tb->getConcurrentAssertions()->empty());
  const hldb::ConcurrentAssertions *const ca =
      (*tb->getConcurrentAssertions())[0];
  ASSERT_NE(ca, nullptr);
  const hldb::PropertySpec *const spec = ca->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::Operation *const clkOp =
      spec->getClockingEvent<hldb::Operation>();
  ASSERT_NE(clkOp, nullptr);
  EXPECT_EQ(clkOp->getOpType(), vpiPosedgeOp)
      << "@(posedge clk) must use vpiPosedgeOp (39)";
}

TEST_F(Sequence17Test, Assert_ClockingEvent_HasOneOperand) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr);
  ASSERT_FALSE(tb->getConcurrentAssertions()->empty());
  const hldb::ConcurrentAssertions *const ca =
      (*tb->getConcurrentAssertions())[0];
  ASSERT_NE(ca, nullptr);
  const hldb::PropertySpec *const spec = ca->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::Operation *const clkOp =
      spec->getClockingEvent<hldb::Operation>();
  ASSERT_NE(clkOp, nullptr);
  ASSERT_NE(clkOp->getOperands(), nullptr);
  EXPECT_EQ(clkOp->getOperands()->size(), 1u)
      << "posedge operation must have exactly one operand (the clock signal)";
}

TEST_F(Sequence17Test, Assert_ClockingEvent_OperandIsClk) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr);
  ASSERT_FALSE(tb->getConcurrentAssertions()->empty());
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
  const hldb::RefObj *const clkRef =
      any_cast<const hldb::RefObj *>((*clkOp->getOperands())[0]);
  ASSERT_NE(clkRef, nullptr);
  EXPECT_EQ(clkRef->getName(), "clk")
      << "clocking event operand must reference 'clk'";
}

TEST_F(Sequence17Test, Assert_PropertyExpr_IsRefObj) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr);
  ASSERT_FALSE(tb->getConcurrentAssertions()->empty());
  const hldb::ConcurrentAssertions *const ca =
      (*tb->getConcurrentAssertions())[0];
  ASSERT_NE(ca, nullptr);
  const hldb::PropertySpec *const spec = ca->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  EXPECT_NE(spec->getPropertyExpr<hldb::RefObj>(), nullptr)
      << "property expression must be a RefObj";
}

TEST_F(Sequence17Test, Assert_PropertyExpr_NameIsSeqNonconsec) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr);
  ASSERT_FALSE(tb->getConcurrentAssertions()->empty());
  const hldb::ConcurrentAssertions *const ca =
      (*tb->getConcurrentAssertions())[0];
  ASSERT_NE(ca, nullptr);
  const hldb::PropertySpec *const spec = ca->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *const propExpr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(propExpr, nullptr);
  EXPECT_EQ(propExpr->getName(), "seq_nonconsec")
      << "property expression must reference 'seq_nonconsec'";
}

TEST_F(Sequence17Test, Assert_PropertyExpr_ResolvedToSeqNonconsecDecl) {
  // ss.16.9.2: 'seq_nonconsec' in the property expression must resolve to
  // the SequenceDecl.
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr);
  ASSERT_FALSE(tb->getConcurrentAssertions()->empty());
  const hldb::ConcurrentAssertions *const ca =
      (*tb->getConcurrentAssertions())[0];
  ASSERT_NE(ca, nullptr);
  const hldb::PropertySpec *const spec = ca->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::RefObj *const propExpr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(propExpr, nullptr);
  EXPECT_NE(propExpr->getActual<hldb::SequenceDecl>(), nullptr)
      << "ss.16.9.2: 'seq_nonconsec' in assert property must resolve to the "
         "SequenceDecl";
}

TEST_F(Sequence17Test, Compiler_NoErrors) {
  EXPECT_EQ(m_compiler->getErrorStats().nbError, 0)
      << "Compiler must not emit any error for legal use of 'seq_nonconsec' "
         "in assert property(@(posedge clk) seq_nonconsec)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
