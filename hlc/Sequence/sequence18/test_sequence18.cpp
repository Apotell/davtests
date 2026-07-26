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

// Tests IEEE 1800-2017 ss.16.9.1 consecutive repetition shorthand 'a[+]'
// for sequence18.sv:
//
//   module tb;
//     bit clk; always #5 clk = ~clk;
//     bit a;
//     sequence seq_plus;
//       a[+];
//     endsequence
//     assert property(@(posedge clk) seq_plus);
//   endmodule
//
// ss.16.9.1 rule: 'a[+]' is syntactic sugar for 'a[*1:$]' -- consecutive
// repetition with lower bound 1 and upper bound '$' (unbounded). The compiler
// must desugar 'a[+]' to 'a[*1:$]' and store it as vpiConsecutiveRepeatOp
// (77) with a Range operand whose left bound is 1 and right bound is '$'
// (vpiUnboundedConst=11). This is distinct from a fixed count like 'a[*3]'
// which uses a Constant operand, and from 'a[*]' (zero-or-more) which
// desugars to 'a[*0:$]'.
//
// Expected HLDB tree for seq_plus body (a[+]):
//   SequenceDecl("seq_plus")
//     -> getExpr<Operation>() with getOpType() == vpiConsecutiveRepeatOp (77)
//          operands[0]: RefObj("a")   -- the repeated boolean expression
//          operands[1]: Range         -- bounds after desugaring [+] -> [*1:$]
//                         leftExpr  Constant("1") vpiIntConst(7)
//                         rightExpr Constant("$") vpiUnboundedConst(11)
//
// Compile-stage bugs exposed:
//   Compiler_NoErrors  -- nbError is 0 even though EL0535 appears in the log;
//                         Surelog does not place EL0535 in the nbError bucket.
//   Assert_PropertyExpr_ResolvedToSeqPlusDecl -- 'seq_plus' in
//                         assert property(@(posedge clk) seq_plus) is treated
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

class Sequence18Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "sequence18.hlc"}); }
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

  static const hldb::Operation *getConsecRepeatOp(const hldb::Module *mod) {
    const hldb::SequenceDecl *sd = getSeqDecl(mod, "seq_plus");
    if (!sd) return nullptr;
    return sd->getExpr<hldb::Operation>();
  }

  static const hldb::Range *getRepeatRange(const hldb::Module *mod) {
    const hldb::Operation *op = getConsecRepeatOp(mod);
    if (!op || !op->getOperands() || op->getOperands()->size() < 2)
      return nullptr;
    return any_cast<const hldb::Range *>((*op->getOperands())[1]);
  }
};

// ===========================================================================
// Module
// ===========================================================================

TEST_F(Sequence18Test, ModuleExists) {
  ASSERT_NE(getTb(m_design), nullptr) << "module 'tb' not found";
}

// ===========================================================================
// Nets -- bit clk, bit a
// ===========================================================================

TEST_F(Sequence18Test, Net_clk_HasBitTypespec) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getNets(), nullptr);
  const hldb::Net *const clk = hldb::findByName<hldb::Net>("clk", tb->getNets());
  ASSERT_NE(clk, nullptr) << "net 'clk' not found";
  ASSERT_NE(clk->getTypespec(), nullptr);
  EXPECT_NE(clk->getTypespec()->getActual<hldb::BitTypespec>(), nullptr)
      << "'bit clk' must produce a BitTypespec";
}

TEST_F(Sequence18Test, Net_a_HasBitTypespec) {
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
// SequenceDecl -- seq_plus
// ===========================================================================

TEST_F(Sequence18Test, SeqDeclCollection_HasOneEntry) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr);
  EXPECT_EQ(tb->getSequenceDecls()->size(), 1u)
      << "exactly one sequence declaration (seq_plus) expected";
}

TEST_F(Sequence18Test, SeqDecl_seq_plus_Exists) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  EXPECT_NE(getSeqDecl(tb, "seq_plus"), nullptr)
      << "SequenceDecl named 'seq_plus' not found";
}

TEST_F(Sequence18Test, SeqDecl_seq_plus_HasExpression) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const sd = getSeqDecl(tb, "seq_plus");
  ASSERT_NE(sd, nullptr);
  EXPECT_NE(sd->getExpr(), nullptr)
      << "seq_plus body expression must be non-null";
}

// ===========================================================================
// Consecutive repeat operation -- a[+] desugared to a[*1:$]
// ===========================================================================

TEST_F(Sequence18Test, SeqDecl_seq_plus_ExprIsOperation) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  EXPECT_NE(getConsecRepeatOp(tb), nullptr)
      << "seq_plus body must be an Operation node";
}

TEST_F(Sequence18Test, SeqDecl_seq_plus_ExprIsConsecutiveRepeatOp) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getConsecRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiConsecutiveRepeatOp)
      << "ss.16.9.1: 'a[+]' must desugar to vpiConsecutiveRepeatOp (77)";
}

TEST_F(Sequence18Test, SeqDecl_seq_plus_ExprIsNotGotoRepeatOp) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getConsecRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  EXPECT_NE(op->getOpType(), vpiGotoRepeatOp)
      << "ss.16.9.1: consecutive '[+]' must not be confused with "
         "goto '[->]' (vpiGotoRepeatOp=78)";
}

TEST_F(Sequence18Test, SeqDecl_seq_plus_ExprIsNotRepeatOp) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getConsecRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  EXPECT_NE(op->getOpType(), vpiRepeatOp)
      << "ss.16.9.1: consecutive '[+]' must not be confused with "
         "non-consecutive '[=]' (vpiRepeatOp=76)";
}

TEST_F(Sequence18Test, SeqDecl_seq_plus_Op_HasTwoOperands) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getConsecRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u)
      << "consecutive repeat must have 2 operands: "
         "the repeated expression and the bounds";
}

// ---------------------------------------------------------------------------
// operands[0] -- the repeated boolean expression: RefObj("a")
// ---------------------------------------------------------------------------

TEST_F(Sequence18Test, SeqDecl_seq_plus_Op_Operand0_IsRefObj) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getConsecRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  EXPECT_NE(any_cast<const hldb::RefObj *>((*op->getOperands())[0]), nullptr)
      << "ss.16.9.1: repeated expression operand must be a RefObj";
}

TEST_F(Sequence18Test, SeqDecl_seq_plus_Op_Operand0_NameIsA) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getConsecRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const hldb::RefObj *const ref =
      any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "a")
      << "ss.16.9.1: 'a[+]' repeated expression must reference signal 'a'";
}

TEST_F(Sequence18Test, SeqDecl_seq_plus_Op_Operand0_ResolvesToNet) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getConsecRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const hldb::RefObj *const ref =
      any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_NE(ref->getActual<hldb::Net>(), nullptr)
      << "RefObj 'a' in consecutive repeat must resolve to the Net declaration";
}

// ---------------------------------------------------------------------------
// operands[1] -- the bounds Range: [1:$] after desugaring a[+] -> a[*1:$]
// ---------------------------------------------------------------------------

TEST_F(Sequence18Test, SeqDecl_seq_plus_Op_Operand1_IsRange) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  EXPECT_NE(getRepeatRange(tb), nullptr)
      << "ss.16.9.1: 'a[+]' desugars to 'a[*1:$]'; bounds must be a Range "
         "node, not a Constant";
}

TEST_F(Sequence18Test, SeqDecl_seq_plus_Op_Operand1_IsNotConstant) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getConsecRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<const hldb::Constant *>((*op->getOperands())[1]), nullptr)
      << "ss.16.9.1: 'a[+]' has two bounds; operand must be a Range, "
         "not a Constant (Constant is only used for single-count like 'a[->3]')";
}

// ---------------------------------------------------------------------------
// Range lower bound -- Constant("1")
// ---------------------------------------------------------------------------

TEST_F(Sequence18Test, SeqDecl_seq_plus_Range_HasLeftExpr) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Range *const range = getRepeatRange(tb);
  ASSERT_NE(range, nullptr);
  EXPECT_NE(range->getLeftExpr(), nullptr)
      << "Range must have a left (lower bound) expression";
}

TEST_F(Sequence18Test, SeqDecl_seq_plus_Range_LowerBound_IsConstant) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Range *const range = getRepeatRange(tb);
  ASSERT_NE(range, nullptr);
  EXPECT_NE(range->getLeftExpr<hldb::Constant>(), nullptr)
      << "lower bound of 'a[*1:$]' must be a Constant node";
}

TEST_F(Sequence18Test, SeqDecl_seq_plus_Range_LowerBound_ValueIsOne) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Range *const range = getRepeatRange(tb);
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const lo = range->getLeftExpr<hldb::Constant>();
  ASSERT_NE(lo, nullptr);
  EXPECT_EQ(std::string(lo->getDecompile()), "1")
      << "ss.16.9.1: 'a[+]' lower bound must be 1 (one or more)";
}

// ---------------------------------------------------------------------------
// Range upper bound -- Constant("$") unbounded
// ---------------------------------------------------------------------------

TEST_F(Sequence18Test, SeqDecl_seq_plus_Range_HasRightExpr) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Range *const range = getRepeatRange(tb);
  ASSERT_NE(range, nullptr);
  EXPECT_NE(range->getRightExpr(), nullptr)
      << "Range must have a right (upper bound) expression";
}

TEST_F(Sequence18Test, SeqDecl_seq_plus_Range_UpperBound_IsConstant) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Range *const range = getRepeatRange(tb);
  ASSERT_NE(range, nullptr);
  EXPECT_NE(range->getRightExpr<hldb::Constant>(), nullptr)
      << "upper bound of 'a[*1:$]' must be a Constant node";
}

TEST_F(Sequence18Test, SeqDecl_seq_plus_Range_UpperBound_IsUnbounded) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Range *const range = getRepeatRange(tb);
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const hi = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(hi, nullptr);
  EXPECT_EQ(hi->getConstType(), vpiUnboundedConst)
      << "ss.16.9.1: 'a[+]' upper bound must be '$' (vpiUnboundedConst=11)";
}

TEST_F(Sequence18Test, SeqDecl_seq_plus_Range_UpperBound_DecompilesAsDollar) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Range *const range = getRepeatRange(tb);
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const hi = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(hi, nullptr);
  EXPECT_EQ(std::string(hi->getDecompile()), "$")
      << "ss.16.9.1: 'a[+]' upper bound must decompile to \"$\"";
}

// ===========================================================================
// Concurrent assertion -- assert property(@(posedge clk) seq_plus)
// ===========================================================================

TEST_F(Sequence18Test, Assert_ConcurrentAssertionExists) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr)
      << "module must have concurrent assertions";
  EXPECT_FALSE(tb->getConcurrentAssertions()->empty())
      << "at least one concurrent assertion expected";
}

TEST_F(Sequence18Test, Assert_HasInlineClockingEvent) {
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

TEST_F(Sequence18Test, Assert_ClockingEvent_IsPosedge) {
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

TEST_F(Sequence18Test, Assert_ClockingEvent_OperandIsClk) {
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

TEST_F(Sequence18Test, Assert_PropertyExpr_IsRefObj) {
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

TEST_F(Sequence18Test, Assert_PropertyExpr_NameIsSeqPlus) {
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
  EXPECT_EQ(propExpr->getName(), "seq_plus")
      << "property expression must reference 'seq_plus'";
}

TEST_F(Sequence18Test, Assert_PropertyExpr_ResolvedToSeqPlusDecl) {
  // ss.16.9.1: 'seq_plus' in the property expression must resolve to the
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
      << "EL0535: 'seq_plus' in assert property must resolve to the "
         "SequenceDecl; Surelog treats it as an implicit net instead";
}


}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
