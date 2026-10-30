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

// Tests IEEE 1800-2017 ss.16.7 unbounded cycle-delay shorthands '##[*]' and
// '##[+]' for sequence20.sv:
//
//   sequence seq_delay_star;  a ##[*] b;  endsequence  // delay >= 0
//   sequence seq_delay_plus;  a ##[+] b;  endsequence  // delay >= 1
//   assert property(@(posedge clk) seq_delay_star);
//   assert property(@(posedge clk) seq_delay_plus);
//
// ss.16.7 rules:
//   'a ##[*] b' is shorthand for 'a ##[0:$] b' -- b occurs after zero or
//   more clock cycles following a. Operator is vpiCycleDelayOp (71).
//
//   'a ##[+] b' is shorthand for 'a ##[1:$] b' -- b occurs after one or
//   more clock cycles following a.
//
// ss.37.54 detail 3: vpiCycleDelayOp operands are the lhs sequence, the rhs
// sequence, the left range, then the right range (given because it differs
// from the left). So both forms have 4 operands:
//   a ##[*] b  ->  [RefObj(a), RefObj(b), Const(0), Const($)]
//   a ##[+] b  ->  [RefObj(a), RefObj(b), Const(1), Const($)]
//
// Compile-stage: both sequence names in 'assert property' resolve correctly
// to their SequenceDecl nodes.

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

class Sequence20Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "sequence20.hlc"}); }
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

  static const hldb::Operation *getCycleDelayOp(const hldb::Module *mod,
                                                 std::string_view seqName) {
    const hldb::SequenceDecl *sd = getSeqDecl(mod, seqName);
    if (!sd) return nullptr;
    return sd->getExpr<hldb::Operation>();
  }

  static const hldb::ConcurrentAssertions *getAssert(const hldb::Module *mod,
                                                      size_t idx) {
    if (!mod || !mod->getConcurrentAssertions()) return nullptr;
    if (mod->getConcurrentAssertions()->size() <= idx) return nullptr;
    return (*mod->getConcurrentAssertions())[idx];
  }
};

// ===========================================================================
// Module
// ===========================================================================

TEST_F(Sequence20Test, ModuleExists) {
  ASSERT_NE(getTb(m_design), nullptr) << "module 'tb' not found";
}

// ===========================================================================
// Variables -- bit clk, bit a, bit b (IEEE 1800-2023 Sec 6.7/6.8: no
// net-type keyword means Variable, not Net, regardless of default_nettype)
// ===========================================================================

TEST_F(Sequence20Test, Variable_a_HasBitTypespec) {
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

TEST_F(Sequence20Test, Variable_b_HasBitTypespec) {
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

TEST_F(Sequence20Test, SeqDeclCollection_HasTwoEntries) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr);
  EXPECT_EQ(tb->getSequenceDecls()->size(), 2u)
      << "exactly two sequence declarations (seq_delay_star, seq_delay_plus) "
         "expected";
}

TEST_F(Sequence20Test, SeqDecl_seq_delay_star_Exists) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  EXPECT_NE(getSeqDecl(tb, "seq_delay_star"), nullptr)
      << "SequenceDecl named 'seq_delay_star' not found";
}

TEST_F(Sequence20Test, SeqDecl_seq_delay_plus_Exists) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  EXPECT_NE(getSeqDecl(tb, "seq_delay_plus"), nullptr)
      << "SequenceDecl named 'seq_delay_plus' not found";
}

// ===========================================================================
// seq_delay_star: a ##[*] b  (should be a ##[0:$] b)
// ===========================================================================

TEST_F(Sequence20Test, SeqDelayStar_ExprIsCycleDelayOp) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getCycleDelayOp(tb, "seq_delay_star");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCycleDelayOp)
      << "ss.16.7: 'a ##[*] b' must use vpiCycleDelayOp (71)";
}

TEST_F(Sequence20Test, SeqDelayStar_Op_HasFourOperands) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getCycleDelayOp(tb, "seq_delay_star");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 4u)
      << "ss.37.54: 'a ##[*] b' must have 4 operands: "
         "[RefObj(a), RefObj(b), Const(0), Const($)]";
}

TEST_F(Sequence20Test, SeqDelayStar_Op_Operand0_IsRefObjA) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getCycleDelayOp(tb, "seq_delay_star");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const hldb::RefObj *const ref =
      any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "a")
      << "ss.16.7: 'a ##[*] b' left operand must reference signal 'a'";
}

TEST_F(Sequence20Test, SeqDelayStar_Op_Operand1_IsRefObjB) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getCycleDelayOp(tb, "seq_delay_star");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const hldb::RefObj *const ref =
      any_cast<const hldb::RefObj *>((*op->getOperands())[1]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "b")
      << "ss.37.54: 'a ##[*] b' rhs sequence (operands[1]) must reference signal 'b'";
}

TEST_F(Sequence20Test, SeqDelayStar_Op_LeftRange_IsZero) {
  // ss.16.7: '##[*]' = '##[0:$]' -- left range (operands[2]) must be 0.
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getCycleDelayOp(tb, "seq_delay_star");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 3u);
  const hldb::Constant *const lo =
      any_cast<const hldb::Constant *>((*op->getOperands())[2]);
  ASSERT_NE(lo, nullptr) << "left range must be a Constant";
  EXPECT_EQ(std::string(lo->getDecompile()), "0")
      << "ss.16.7: '##[*]' lower bound must be 0 (zero or more cycles)";
}

TEST_F(Sequence20Test, SeqDelayStar_Op_RightRange_IsUnbounded) {
  // ss.16.7: '##[*]' right range (operands[3]) must be '$' (unbounded).
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getCycleDelayOp(tb, "seq_delay_star");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 4u);
  const hldb::Constant *const hi =
      any_cast<const hldb::Constant *>((*op->getOperands())[3]);
  ASSERT_NE(hi, nullptr) << "right range must be a Constant";
  EXPECT_EQ(hi->getConstType(), vpiUnboundedConst)
      << "ss.16.7: '##[*]' upper bound must be '$' (vpiUnboundedConst=11)";
}

// ===========================================================================
// seq_delay_plus: a ##[+] b  (should be a ##[1:$] b)
// ===========================================================================

TEST_F(Sequence20Test, SeqDelayPlus_ExprIsCycleDelayOp) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getCycleDelayOp(tb, "seq_delay_plus");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCycleDelayOp)
      << "ss.16.7: 'a ##[+] b' must use vpiCycleDelayOp (71)";
}

TEST_F(Sequence20Test, SeqDelayPlus_Op_HasFourOperands) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getCycleDelayOp(tb, "seq_delay_plus");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 4u)
      << "ss.37.54: 'a ##[+] b' must have 4 operands: "
         "[RefObj(a), RefObj(b), Const(1), Const($)]";
}

TEST_F(Sequence20Test, SeqDelayPlus_Op_Operand0_IsRefObjA) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getCycleDelayOp(tb, "seq_delay_plus");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const hldb::RefObj *const ref =
      any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "a")
      << "ss.16.7: 'a ##[+] b' left operand must reference signal 'a'";
}

TEST_F(Sequence20Test, SeqDelayPlus_Op_Operand1_IsRefObjB) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getCycleDelayOp(tb, "seq_delay_plus");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const hldb::RefObj *const ref =
      any_cast<const hldb::RefObj *>((*op->getOperands())[1]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "b")
      << "ss.37.54: 'a ##[+] b' rhs sequence (operands[1]) must reference signal 'b'";
}

TEST_F(Sequence20Test, SeqDelayPlus_Op_LeftRange_IsOne) {
  // ss.16.7: '##[+]' = '##[1:$]' -- left range (operands[2]) must be 1.
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getCycleDelayOp(tb, "seq_delay_plus");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 3u);
  const hldb::Constant *const lo =
      any_cast<const hldb::Constant *>((*op->getOperands())[2]);
  ASSERT_NE(lo, nullptr) << "left range must be a Constant";
  EXPECT_EQ(std::string(lo->getDecompile()), "1")
      << "ss.16.7: '##[+]' lower bound must be 1 (one or more cycles)";
}

TEST_F(Sequence20Test, SeqDelayPlus_Op_RightRange_IsUnbounded) {
  // ss.16.7: '##[+]' right range (operands[3]) must be '$' (unbounded).
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::Operation *const op = getCycleDelayOp(tb, "seq_delay_plus");
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 4u);
  const hldb::Constant *const hi =
      any_cast<const hldb::Constant *>((*op->getOperands())[3]);
  ASSERT_NE(hi, nullptr) << "right range must be a Constant";
  EXPECT_EQ(hi->getConstType(), vpiUnboundedConst)
      << "ss.16.7: '##[+]' upper bound must be '$' (vpiUnboundedConst=11)";
}

// ===========================================================================
// Concurrent assertions
// ===========================================================================

TEST_F(Sequence20Test, Assert_Collection_HasTwoEntries) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr);
  EXPECT_EQ(tb->getConcurrentAssertions()->size(), 2u)
      << "two concurrent assertions expected (one per sequence)";
}

TEST_F(Sequence20Test, Assert0_PropertyExpr_NameIsSeqDelayStar) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::ConcurrentAssertions *const ca = getAssert(tb, 0);
  ASSERT_NE(ca, nullptr);
  const hldb::PropertySpec *const spec = ca->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::SequenceInst *const expr = spec->getPropertyExpr<hldb::SequenceInst>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getName(), "seq_delay_star")
      << "first assertion must reference 'seq_delay_star'";
}

TEST_F(Sequence20Test, Assert1_PropertyExpr_NameIsSeqDelayPlus) {
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::ConcurrentAssertions *const ca = getAssert(tb, 1);
  ASSERT_NE(ca, nullptr);
  const hldb::PropertySpec *const spec = ca->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::SequenceInst *const expr = spec->getPropertyExpr<hldb::SequenceInst>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getName(), "seq_delay_plus")
      << "second assertion must reference 'seq_delay_plus'";
}

TEST_F(Sequence20Test, Assert0_PropertyExpr_ResolvedToSeqDelayStarDecl) {
  // ss.16.7: 'seq_delay_star' in assert property must resolve to the
  // SequenceDecl.
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::ConcurrentAssertions *const ca = getAssert(tb, 0);
  ASSERT_NE(ca, nullptr);
  const hldb::PropertySpec *const spec = ca->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::SequenceInst *const expr = spec->getPropertyExpr<hldb::SequenceInst>();
  ASSERT_NE(expr, nullptr);
  EXPECT_NE(expr->getSequenceDecl(), nullptr)
      << "ss.16.7: 'seq_delay_star' in assert property must resolve to "
         "SequenceDecl";
}

TEST_F(Sequence20Test, Assert1_PropertyExpr_ResolvedToSeqDelayPlusDecl) {
  // ss.16.7: 'seq_delay_plus' in assert property must resolve to the
  // SequenceDecl.
  const hldb::Module *const tb = getTb(m_design);
  ASSERT_NE(tb, nullptr);
  const hldb::ConcurrentAssertions *const ca = getAssert(tb, 1);
  ASSERT_NE(ca, nullptr);
  const hldb::PropertySpec *const spec = ca->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr);
  const hldb::SequenceInst *const expr = spec->getPropertyExpr<hldb::SequenceInst>();
  ASSERT_NE(expr, nullptr);
  EXPECT_NE(expr->getSequenceDecl(), nullptr)
      << "ss.16.7: 'seq_delay_plus' in assert property must resolve to "
         "SequenceDecl";
}

TEST_F(Sequence20Test, Compiler_NoErrors) {
  EXPECT_EQ(m_compiler->getErrorStats().nbError, 0)
      << "Compiler must not emit any error for legal use of 'a ##[*] b' / "
         "'a ##[+] b'";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
