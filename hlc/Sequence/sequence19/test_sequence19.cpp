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

// Tests IEEE 1800-2017 ss.16.9.1 consecutive repetition shorthand 'a[*]'
// for sequence19.sv:
//
//   module tb;
//     bit clk; always #5 clk = ~clk;
//     bit a;
//     sequence seq_star;
//       a[*];
//     endsequence
//     assert property(@(posedge clk) seq_star);
//   endmodule
//
// ss.16.9.1 rule: 'a[*]' is syntactic sugar for 'a[*0:$]' -- consecutive
// repetition with lower bound 0 (zero or more occurrences) and upper bound
// '$' (unbounded). This is distinct from 'a[+]' (sequence18) which desugars
// to 'a[*1:$]' (one or more). The compiler must store the lower bound as 0,
// not 1 -- conflating '[*]' with '[+]' would be a correctness bug.
//
// Expected HLDB tree for seq_star body (a[*]):
//   SequenceDecl("seq_star")
//     -> getExpr<Operation>() with getOpType() == vpiConsecutiveRepeatOp (77)
//          operands[0]: RefObj("a")   -- the repeated boolean expression
//          operands[1]: Range         -- bounds after desugaring [*] -> [*0:$]
//                         leftExpr  Constant("0") vpiIntConst(7)
//                         rightExpr Constant("$") vpiUnboundedConst(11)
//
// Compile-stage bugs exposed:
//   Compiler_NoErrors  -- nbError is 0 even though EL0535 appears in the log;
//                         Surelog does not place EL0535 in the nbError bucket.
//   Assert_PropertyExpr_ResolvedToSeqStarDecl -- 'seq_star' in
//                         assert property(@(posedge clk) seq_star) is treated
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

class Sequence19Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "sequence19.hlc"}); }
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
    const hldb::SequenceDecl *sd = getSeqDecl(mod, "seq_star");
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

TEST_F(Sequence19Test, ModuleExists) {
  ASSERT_NE(getTb(m_design), nullptr) << "module 'tb' not found";
}

// ===========================================================================
// Nets -- bit clk, bit a
// ===========================================================================

TEST_F(Sequence19Test, Net_clk_HasBitTypespec) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getNets(), nullptr);
  const hldb::Net *const clk = hldb::findByName<hldb::Net>("clk", tb->getNets());
  ASSERT_NE(clk, nullptr) << "net 'clk' not found";
  ASSERT_NE(clk->getTypespec(), nullptr);
  EXPECT_NE(clk->getTypespec()->getActual<hldb::BitTypespec>(), nullptr)
      << "'bit clk' must produce a BitTypespec";
}

TEST_F(Sequence19Test, Net_a_HasBitTypespec) {
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
// SequenceDecl -- seq_star
// ===========================================================================

TEST_F(Sequence19Test, SeqDeclCollection_HasOneEntry) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr);
  EXPECT_EQ(tb->getSequenceDecls()->size(), 1u)
      << "exactly one sequence declaration (seq_star) expected";
}

TEST_F(Sequence19Test, SeqDecl_seq_star_Exists) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  EXPECT_NE(getSeqDecl(tb, "seq_star"), nullptr)
      << "SequenceDecl named 'seq_star' not found";
}

TEST_F(Sequence19Test, SeqDecl_seq_star_HasExpression) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::SequenceDecl *const sd = getSeqDecl(tb, "seq_star");
  ASSERT_NE(sd, nullptr);
  EXPECT_NE(sd->getExpr(), nullptr)
      << "seq_star body expression must be non-null";
}

// ===========================================================================
// Consecutive repeat operation -- a[*] desugared to a[*0:$]
// ===========================================================================

TEST_F(Sequence19Test, SeqDecl_seq_star_ExprIsOperation) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  EXPECT_NE(getConsecRepeatOp(tb), nullptr)
      << "seq_star body must be an Operation node";
}

TEST_F(Sequence19Test, SeqDecl_seq_star_ExprIsConsecutiveRepeatOp) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getConsecRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiConsecutiveRepeatOp)
      << "ss.16.9.1: 'a[*]' must desugar to vpiConsecutiveRepeatOp (77)";
}

TEST_F(Sequence19Test, SeqDecl_seq_star_Op_HasTwoOperands) {
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

TEST_F(Sequence19Test, SeqDecl_seq_star_Op_Operand0_IsRefObj) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getConsecRepeatOp(tb);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  EXPECT_NE(any_cast<const hldb::RefObj *>((*op->getOperands())[0]), nullptr)
      << "ss.16.9.1: repeated expression operand must be a RefObj";
}

TEST_F(Sequence19Test, SeqDecl_seq_star_Op_Operand0_NameIsA) {
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
      << "ss.16.9.1: 'a[*]' repeated expression must reference signal 'a'";
}

TEST_F(Sequence19Test, SeqDecl_seq_star_Op_Operand0_ResolvesToNet) {
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
// operands[1] -- the bounds Range: [0:$] after desugaring a[*] -> a[*0:$]
// ---------------------------------------------------------------------------

TEST_F(Sequence19Test, SeqDecl_seq_star_Op_Operand1_IsRange) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  EXPECT_NE(getRepeatRange(tb), nullptr)
      << "ss.16.9.1: 'a[*]' desugars to 'a[*0:$]'; bounds must be a Range "
         "node, not a Constant";
}

// ---------------------------------------------------------------------------
// Range lower bound -- Constant("0"): zero or more, distinct from a[+] -> 1
// ---------------------------------------------------------------------------

TEST_F(Sequence19Test, SeqDecl_seq_star_Range_LowerBound_IsConstant) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Range *const range = getRepeatRange(tb);
  ASSERT_NE(range, nullptr);
  EXPECT_NE(range->getLeftExpr<hldb::Constant>(), nullptr)
      << "lower bound of 'a[*0:$]' must be a Constant node";
}

TEST_F(Sequence19Test, SeqDecl_seq_star_Range_LowerBound_ValueIsZero) {
  // ss.16.9.1: 'a[*]' means zero or more -- lower bound must be 0.
  // If the compiler stores 1 here, it has conflated '[*]' with '[+]'.
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Range *const range = getRepeatRange(tb);
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const lo = range->getLeftExpr<hldb::Constant>();
  ASSERT_NE(lo, nullptr);
  EXPECT_EQ(std::string(lo->getDecompile()), "0")
      << "ss.16.9.1: 'a[*]' lower bound must be 0 (zero or more); "
         "'a[+]' has lower bound 1 (one or more)";
}

// ---------------------------------------------------------------------------
// Range upper bound -- Constant("$") unbounded
// ---------------------------------------------------------------------------

TEST_F(Sequence19Test, SeqDecl_seq_star_Range_UpperBound_IsUnbounded) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Range *const range = getRepeatRange(tb);
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const hi = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(hi, nullptr);
  EXPECT_EQ(hi->getConstType(), vpiUnboundedConst)
      << "ss.16.9.1: 'a[*]' upper bound must be '$' (vpiUnboundedConst=11)";
}

TEST_F(Sequence19Test, SeqDecl_seq_star_Range_UpperBound_DecompilesAsDollar) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Range *const range = getRepeatRange(tb);
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const hi = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(hi, nullptr);
  EXPECT_EQ(std::string(hi->getDecompile()), "$")
      << "ss.16.9.1: 'a[*]' upper bound must decompile to \"$\"";
}

// ===========================================================================
// Concurrent assertion -- assert property(@(posedge clk) seq_star)
// ===========================================================================

TEST_F(Sequence19Test, Assert_ConcurrentAssertionExists) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr)
      << "module must have concurrent assertions";
  EXPECT_FALSE(tb->getConcurrentAssertions()->empty())
      << "at least one concurrent assertion expected";
}

TEST_F(Sequence19Test, Assert_HasInlineClockingEvent) {
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

TEST_F(Sequence19Test, Assert_ClockingEvent_IsPosedge) {
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

TEST_F(Sequence19Test, Assert_ClockingEvent_OperandIsClk) {
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

TEST_F(Sequence19Test, Assert_PropertyExpr_IsRefObj) {
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

TEST_F(Sequence19Test, Assert_PropertyExpr_NameIsSeqStar) {
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
  EXPECT_EQ(propExpr->getName(), "seq_star")
      << "property expression must reference 'seq_star'";
}

TEST_F(Sequence19Test, Assert_PropertyExpr_ResolvedToSeqStarDecl) {
  // ss.16.9.1: 'seq_star' in the property expression must resolve to the
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
      << "EL0535: 'seq_star' in assert property must resolve to the "
         "SequenceDecl; Surelog treats it as an implicit net instead";
}


}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
