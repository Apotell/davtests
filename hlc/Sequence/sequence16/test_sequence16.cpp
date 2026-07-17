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

// Tests IEEE 1800-2017 ss.16.9 goto repetition for sequence16.sv:
//
//   module tb;
//     bit clk; always #5 clk = ~clk;
//     bit a;
//     sequence seq_goto;
//       a[->3];
//     endsequence
//     assert property(@(posedge clk) seq_goto);
//   endmodule
//
// ss.16.9 rule: 'a[->n]' (goto repetition) requires signal 'a' to be true
// exactly n times, not necessarily on consecutive cycles. The sequence ends
// on the cycle of the n-th true occurrence. Unlike consecutive repetition
// 'a[*n]' (vpiConsecutiveRepeatOp=77), goto repetition uses vpiGotoRepeatOp
// (78). A single count '[->3]' (not a range) is stored as a Constant operand,
// not a Range node.
//
// Expected HLDB tree for seq_goto body (a[->3]):
//   SequenceDecl("seq_goto")
//     -> getExpr<Operation>() with getOpType() == vpiGotoRepeatOp (78)
//          operands[0]: RefObj("a")   -- the repeated boolean expression
//          operands[1]: Constant("3") -- exact count (vpiUIntConst=9)
//
// Compile-stage bugs exposed:
//   Compiler_NoErrors  -- nbError is 0 even though EL0535 appears in the log;
//                         Surelog does not place EL0535 in the nbError bucket.
//   Assert_PropertyExpr_ResolvedToSeqGotoDecl -- 'seq_goto' in
//                         assert property(@(posedge clk) seq_goto) is treated
//                         as an implicit net (EL0535) instead of resolving to
//                         the SequenceDecl; getActual<SequenceDecl>() is null.

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
#include <hldb/operation.h>
#include <hldb/property_spec.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sequence_decl.h>
#include <hldb/sv_vpi_user.h>

#include <string>

namespace hlc {

class Sequence16Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "sequence16.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTb(const hldb::Design *d) {
    return hldb::findByName<hldb::Module>("work@tb", d->getAllModules());
  }

  static const hldb::SequenceDecl *getSeqDecl(const hldb::Module *mod,
                                               std::string_view name) {
    if (!mod || !mod->getSequenceDecls()) return nullptr;
    for (const hldb::SequenceDecl *const s : *mod->getSequenceDecls()) {
      if (s->getName() == name) return s;
    }
    return nullptr;
  }

  static const hldb::Operation *getGotoRepeatOp(const hldb::Module *mod) {
    const hldb::SequenceDecl *sd = getSeqDecl(mod, "seq_goto");
    if (!sd) return nullptr;
    return sd->getExpr<hldb::Operation>();
  }
};

// ===========================================================================
// Module
// ===========================================================================

TEST_F(Sequence16Test, ModuleExists) {
  ASSERT_NE(getTb(m_design), nullptr) << "module 'work@tb' not found";
}

// ===========================================================================
// Nets -- bit clk, bit a
// ===========================================================================

TEST_F(Sequence16Test, Net_clk_HasBitTypespec) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getNets(), nullptr);
  const hldb::Net *const clk = hldb::findByName<hldb::Net>("clk", tb->getNets());
  ASSERT_NE(clk, nullptr) << "net 'clk' not found";
  ASSERT_NE(clk->getTypespec(), nullptr);
  EXPECT_NE(clk->getTypespec()->getActual<hldb::BitTypespec>(), nullptr)
      << "'bit clk' must produce a BitTypespec";
}

TEST_F(Sequence16Test, Net_a_HasBitTypespec) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getNets(), nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", tb->getNets());
  ASSERT_NE(a, nullptr) << "net 'a' not found";
  ASSERT_NE(a->getTypespec(), nullptr);
  EXPECT_NE(a->getTypespec()->getActual<hldb::BitTypespec>(), nullptr)
      << "'bit a' must produce a BitTypespec";
}

// ===========================================================================
// SequenceDecl -- seq_goto
// ===========================================================================

TEST_F(Sequence16Test, SeqDeclCollection_HasOneEntry) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr);
  EXPECT_EQ(tb->getSequenceDecls()->size(), 1u)
      << "exactly one sequence declaration (seq_goto) expected";
}

TEST_F(Sequence16Test, SeqDecl_seq_goto_Exists) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  EXPECT_NE(getSeqDecl(tb, "seq_goto"), nullptr)
      << "SequenceDecl named 'seq_goto' not found";
}

TEST_F(Sequence16Test, SeqDecl_seq_goto_HasExpression) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const sd = getSeqDecl(tb, "seq_goto");
  ASSERT_NE(sd, nullptr);
  EXPECT_NE(sd->getExpr(), nullptr)
      << "seq_goto body expression must be non-null";
}

// ===========================================================================
// Goto repeat operation -- a[->3]
// ===========================================================================

TEST_F(Sequence16Test, SeqDecl_seq_goto_ExprIsOperation) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  EXPECT_NE(getGotoRepeatOp(tb), nullptr)
      << "seq_goto body must be an Operation node";
}

TEST_F(Sequence16Test, SeqDecl_seq_goto_ExprIsGotoRepeatOp) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getGotoRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiGotoRepeatOp)
      << "ss.16.9: 'a[->3]' must use vpiGotoRepeatOp (78)";
}

TEST_F(Sequence16Test, SeqDecl_seq_goto_ExprIsNotConsecutiveRepeatOp) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getGotoRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  EXPECT_NE(op->getOpType(), vpiConsecutiveRepeatOp)
      << "ss.16.9: goto '[->]' (78) must not be confused with "
         "consecutive '[*]' (vpiConsecutiveRepeatOp=77)";
}

TEST_F(Sequence16Test, SeqDecl_seq_goto_Op_HasTwoOperands) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getGotoRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u)
      << "goto repeat must have 2 operands: the repeated expression and the count";
}

// ---------------------------------------------------------------------------
// operands[0] -- the repeated boolean expression: RefObj("a")
// ---------------------------------------------------------------------------

TEST_F(Sequence16Test, SeqDecl_seq_goto_Op_Operand0_IsRefObj) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getGotoRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  EXPECT_NE(any_cast<const hldb::RefObj *>((*op->getOperands())[0]), nullptr)
      << "ss.16.9: repeated expression operand must be a RefObj";
}

TEST_F(Sequence16Test, SeqDecl_seq_goto_Op_Operand0_NameIsA) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getGotoRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const hldb::RefObj *const ref =
      any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "a")
      << "ss.16.9: 'a[->3]' repeated expression must reference signal 'a'";
}

TEST_F(Sequence16Test, SeqDecl_seq_goto_Op_Operand0_ResolvesToNet) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getGotoRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const hldb::RefObj *const ref =
      any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_NE(ref->getActual<hldb::Net>(), nullptr)
      << "RefObj 'a' in goto repeat must resolve to the Net declaration";
}

// ---------------------------------------------------------------------------
// operands[1] -- the exact count: Constant("3"), not a Range
// ---------------------------------------------------------------------------

TEST_F(Sequence16Test, SeqDecl_seq_goto_Op_Operand1_IsConstant) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getGotoRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  EXPECT_NE(any_cast<const hldb::Constant *>((*op->getOperands())[1]), nullptr)
      << "ss.16.9: single count '[->3]' must be stored as a Constant, not a Range";
}

TEST_F(Sequence16Test, SeqDecl_seq_goto_Op_Operand1_IsNotRange) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getGotoRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<const hldb::Range *>((*op->getOperands())[1]), nullptr)
      << "ss.16.9: single count '[->3]' must not be a Range node "
         "(Range is only used for '[->n:m]' with distinct bounds)";
}

TEST_F(Sequence16Test, SeqDecl_seq_goto_Op_Operand1_IsUIntConst) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getGotoRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const hldb::Constant *const count =
      any_cast<const hldb::Constant *>((*op->getOperands())[1]);
  ASSERT_NE(count, nullptr);
  EXPECT_EQ(count->getConstType(), vpiUIntConst)
      << "ss.16.9: goto repetition count must be an unsigned integer constant "
         "(vpiUIntConst=9)";
}

TEST_F(Sequence16Test, SeqDecl_seq_goto_Op_Operand1_ValueIsThree) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getGotoRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const hldb::Constant *const count =
      any_cast<const hldb::Constant *>((*op->getOperands())[1]);
  ASSERT_NE(count, nullptr);
  EXPECT_EQ(std::string(count->getDecompile()), "3")
      << "ss.16.9: 'a[->3]' count constant must decompile to \"3\"";
}

// ===========================================================================
// Concurrent assertion -- assert property(@(posedge clk) seq_goto)
// ===========================================================================

TEST_F(Sequence16Test, Assert_ConcurrentAssertionExists) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr)
      << "module must have concurrent assertions";
  EXPECT_FALSE(tb->getConcurrentAssertions()->empty())
      << "at least one concurrent assertion expected";
}

TEST_F(Sequence16Test, Assert_HasInlineClockingEvent) {
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
      << "assert property must have an inline clocking event "
         "(@(posedge clk))";
}

TEST_F(Sequence16Test, Assert_ClockingEvent_IsPosedge) {
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

TEST_F(Sequence16Test, Assert_ClockingEvent_HasOneOperand) {
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

TEST_F(Sequence16Test, Assert_ClockingEvent_OperandIsClk) {
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

TEST_F(Sequence16Test, Assert_PropertyExpr_IsRefObj) {
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

TEST_F(Sequence16Test, Assert_PropertyExpr_NameIsSeqGoto) {
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
  EXPECT_EQ(propExpr->getName(), "seq_goto")
      << "property expression must reference 'seq_goto'";
}

TEST_F(Sequence16Test, Assert_PropertyExpr_ResolvedToSeqGotoDecl) {
  // ss.16.9: 'seq_goto' in the property expression must resolve to the
  // SequenceDecl. Surelog bug EL0535 treats it as an implicit net instead;
  // getActual<SequenceDecl>() returns null. This test FAILS intentionally
  // to document the bug.
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
      << "EL0535: 'seq_goto' in assert property must resolve to the "
         "SequenceDecl; Surelog treats it as an implicit net instead";
}


}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
