/*
 Copyright 2026 Alain Dargelas

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

// IEEE 1800-2023 Sec 16.9.1 Table 16-1: sequence operator precedence and associativity.
// SV: tests/SequencePrecedence/dut.sv
//
//   [* ] [= ] [-> ]  >  ## (left)  >  throughout (right)  >  within (left)
//                    >  intersect (left)  >  and (left)  >  or (left)
//
// Each test checks the grouping the table requires, through the operator tree:
//   vpiCycleDelayOp    operands: lhs, rhs, delay          (Sec 37.54 detail 3)
//   vpiUnaryCycleDelayOp operands: sequence, delay
//   and / or / intersect / within / throughout operands: lhs, rhs
//   (sequence 'and' / 'or' are vpiCompAndOp / vpiCompOrOp, Sec 37.54 detail 2)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/sequence_decl.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

#include <gtest/gtest.h>

#include <string>

namespace hlc {

class SequencePrecedenceTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "SequencePrecedence.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::Operation *seqOp(std::string_view name) const {
    const hldb::Module *mod = hldb::findByName<hldb::Module>("sequence_precedence", m_design->getAllModules());
    if (mod == nullptr) return nullptr;
    const hldb::SequenceDecl *sd = hldb::findByName<hldb::SequenceDecl>(name, mod->getSequenceDecls());
    if (sd == nullptr) return nullptr;
    return any_cast<hldb::Operation>(sd->getExpr());
  }

  static const hldb::Any *operandAt(const hldb::Operation *op, size_t index) {
    if ((op == nullptr) || (op->getOperands() == nullptr) || (op->getOperands()->size() <= index)) return nullptr;
    return (*op->getOperands())[index];
  }

  // The operand at 'index' as an Operation of the given type, or nullptr.
  static const hldb::Operation *subOp(const hldb::Operation *op, size_t index, int32_t opType) {
    const hldb::Operation *sub = any_cast<hldb::Operation>(operandAt(op, index));
    return ((sub != nullptr) && (sub->getOpType() == opType)) ? sub : nullptr;
  }

  // Whether the operand at 'index' is a reference to the named signal.
  static bool isRef(const hldb::Operation *op, size_t index, std::string_view name) {
    const hldb::RefObj *ref = any_cast<hldb::RefObj>(operandAt(op, index));
    return (ref != nullptr) && (ref->getName() == name);
  }
};

// ----------------------------------------------------------------------------
// ## binds tighter than and / or
// ----------------------------------------------------------------------------
TEST_F(SequencePrecedenceTest, DelayAnd_IsAndOfDelay) {
  // a ##1 b and c  ->  (a ##1 b) and c
  const hldb::Operation *op = seqOp("delay_and");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCompAndOp) << "Table 16-1: ## binds tighter than and";
  const hldb::Operation *lhs = subOp(op, 0, vpiCycleDelayOp);
  ASSERT_NE(lhs, nullptr) << "lhs of 'and' must be 'a ##1 b'";
  EXPECT_TRUE(isRef(lhs, 0, "a"));
  EXPECT_TRUE(isRef(lhs, 1, "b"));
  EXPECT_TRUE(isRef(op, 1, "c"));
}

TEST_F(SequencePrecedenceTest, DelayOr_IsOrOfTwoDelays) {
  // a ##1 b or c ##1 d  ->  (a ##1 b) or (c ##1 d)
  const hldb::Operation *op = seqOp("delay_or");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCompOrOp) << "Table 16-1: ## binds tighter than or";
  const hldb::Operation *lhs = subOp(op, 0, vpiCycleDelayOp);
  const hldb::Operation *rhs = subOp(op, 1, vpiCycleDelayOp);
  ASSERT_NE(lhs, nullptr) << "lhs of 'or' must be 'a ##1 b'";
  ASSERT_NE(rhs, nullptr) << "rhs of 'or' must be 'c ##1 d'";
  EXPECT_TRUE(isRef(lhs, 0, "a"));
  EXPECT_TRUE(isRef(lhs, 1, "b"));
  EXPECT_TRUE(isRef(rhs, 0, "c"));
  EXPECT_TRUE(isRef(rhs, 1, "d"));
}

TEST_F(SequencePrecedenceTest, AndDelay_IsAndWithDelayRhs) {
  // a and b ##1 c  ->  a and (b ##1 c)
  const hldb::Operation *op = seqOp("and_delay");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCompAndOp);
  EXPECT_TRUE(isRef(op, 0, "a"));
  const hldb::Operation *rhs = subOp(op, 1, vpiCycleDelayOp);
  ASSERT_NE(rhs, nullptr) << "rhs of 'and' must be 'b ##1 c'";
  EXPECT_TRUE(isRef(rhs, 0, "b"));
  EXPECT_TRUE(isRef(rhs, 1, "c"));
}

// ----------------------------------------------------------------------------
// ## is left associative
// ----------------------------------------------------------------------------
TEST_F(SequencePrecedenceTest, DelayChain_IsLeftAssociative) {
  // a ##1 b ##2 c  ->  (a ##1 b) ##2 c
  const hldb::Operation *op = seqOp("delay_chain");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCycleDelayOp);
  const hldb::Operation *lhs = subOp(op, 0, vpiCycleDelayOp);
  ASSERT_NE(lhs, nullptr) << "Table 16-1: ## is left associative; lhs must be 'a ##1 b'";
  EXPECT_TRUE(isRef(lhs, 0, "a"));
  EXPECT_TRUE(isRef(lhs, 1, "b"));
  const hldb::Constant *innerDelay = any_cast<hldb::Constant>(operandAt(lhs, 2));
  ASSERT_NE(innerDelay, nullptr);
  EXPECT_EQ(innerDelay->getValue(), "1");
  EXPECT_TRUE(isRef(op, 1, "c"));
  const hldb::Constant *outerDelay = any_cast<hldb::Constant>(operandAt(op, 2));
  ASSERT_NE(outerDelay, nullptr);
  EXPECT_EQ(outerDelay->getValue(), "2");
}

TEST_F(SequencePrecedenceTest, UnaryChain_IsLeftAssociative) {
  // ##1 a ##2 b  ->  (##1 a) ##2 b
  const hldb::Operation *op = seqOp("unary_chain");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCycleDelayOp) << "the outer ## is the binary ##2";
  const hldb::Operation *lhs = subOp(op, 0, vpiUnaryCycleDelayOp);
  ASSERT_NE(lhs, nullptr) << "lhs must be the unary '##1 a'";
  EXPECT_TRUE(isRef(lhs, 0, "a"));
  EXPECT_TRUE(isRef(op, 1, "b"));
}

TEST_F(SequencePrecedenceTest, DelayParen_KeepsExplicitGrouping) {
  // a ##1 (b ##2 c)  ->  a ##1 (b ##2 c)
  const hldb::Operation *op = seqOp("delay_paren");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCycleDelayOp);
  EXPECT_TRUE(isRef(op, 0, "a"));
  const hldb::Operation *rhs = subOp(op, 1, vpiCycleDelayOp);
  ASSERT_NE(rhs, nullptr) << "explicit parentheses must keep 'b ##2 c' as the rhs";
  EXPECT_TRUE(isRef(rhs, 0, "b"));
  EXPECT_TRUE(isRef(rhs, 1, "c"));
}

// ----------------------------------------------------------------------------
// intersect > and > or, all left associative
// ----------------------------------------------------------------------------
TEST_F(SequencePrecedenceTest, AndIntersect_IntersectBindsTighter) {
  // a and b intersect c  ->  a and (b intersect c)
  const hldb::Operation *op = seqOp("and_intersect");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCompAndOp) << "Table 16-1: intersect binds tighter than and";
  EXPECT_TRUE(isRef(op, 0, "a"));
  const hldb::Operation *rhs = subOp(op, 1, vpiIntersectOp);
  ASSERT_NE(rhs, nullptr) << "rhs of 'and' must be 'b intersect c'";
}

TEST_F(SequencePrecedenceTest, OrAnd_AndBindsTighter) {
  // a or b and c  ->  a or (b and c)
  const hldb::Operation *op = seqOp("or_and");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCompOrOp) << "Table 16-1: and binds tighter than or";
  EXPECT_TRUE(isRef(op, 0, "a"));
  EXPECT_NE(subOp(op, 1, vpiCompAndOp), nullptr) << "rhs of 'or' must be 'b and c'";
}

TEST_F(SequencePrecedenceTest, AndChain_IsLeftAssociative) {
  // a and b and c  ->  (a and b) and c
  const hldb::Operation *op = seqOp("and_chain");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCompAndOp);
  EXPECT_NE(subOp(op, 0, vpiCompAndOp), nullptr) << "Table 16-1: and is left associative";
  EXPECT_TRUE(isRef(op, 1, "c"));
}

// ----------------------------------------------------------------------------
// within > intersect
// ----------------------------------------------------------------------------
TEST_F(SequencePrecedenceTest, IntersectWithin_WithinBindsTighter) {
  // a intersect b within c  ->  a intersect (b within c)
  const hldb::Operation *op = seqOp("intersect_within");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiIntersectOp) << "Table 16-1: within binds tighter than intersect";
  EXPECT_TRUE(isRef(op, 0, "a"));
  EXPECT_NE(subOp(op, 1, vpiWithinOp), nullptr) << "rhs of 'intersect' must be 'b within c'";
}

// ----------------------------------------------------------------------------
// ## > throughout > within; throughout is right associative
// ----------------------------------------------------------------------------
TEST_F(SequencePrecedenceTest, ThroughoutWithin_ThroughoutBindsTighter) {
  // a throughout b within c  ->  (a throughout b) within c
  const hldb::Operation *op = seqOp("throughout_within");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiWithinOp) << "Table 16-1: throughout binds tighter than within";
  const hldb::Operation *lhs = subOp(op, 0, vpiThroughoutOp);
  ASSERT_NE(lhs, nullptr) << "lhs of 'within' must be 'a throughout b'";
  EXPECT_TRUE(isRef(lhs, 0, "a"));
  EXPECT_TRUE(isRef(lhs, 1, "b"));
  EXPECT_TRUE(isRef(op, 1, "c"));
}

TEST_F(SequencePrecedenceTest, ThroughoutDelay_DelayBindsTighter) {
  // a throughout b ##1 c  ->  a throughout (b ##1 c)
  const hldb::Operation *op = seqOp("throughout_delay");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiThroughoutOp) << "Table 16-1: ## binds tighter than throughout";
  EXPECT_TRUE(isRef(op, 0, "a"));
  const hldb::Operation *rhs = subOp(op, 1, vpiCycleDelayOp);
  ASSERT_NE(rhs, nullptr) << "rhs of 'throughout' must be 'b ##1 c'";
  EXPECT_TRUE(isRef(rhs, 0, "b"));
  EXPECT_TRUE(isRef(rhs, 1, "c"));
}

TEST_F(SequencePrecedenceTest, ThroughoutChain_IsRightAssociative) {
  // a throughout b throughout c  ->  a throughout (b throughout c)
  const hldb::Operation *op = seqOp("throughout_chain");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiThroughoutOp);
  EXPECT_TRUE(isRef(op, 0, "a"));
  const hldb::Operation *rhs = subOp(op, 1, vpiThroughoutOp);
  ASSERT_NE(rhs, nullptr) << "Table 16-1: throughout is right associative";
  EXPECT_TRUE(isRef(rhs, 0, "b"));
  EXPECT_TRUE(isRef(rhs, 1, "c"));
}

// ----------------------------------------------------------------------------
// repetition > ##
// ----------------------------------------------------------------------------
TEST_F(SequencePrecedenceTest, RepDelay_RepetitionBindsTighter) {
  // a[*2] ##1 b  ->  (a[*2]) ##1 b
  const hldb::Operation *op = seqOp("rep_delay");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiCycleDelayOp);
  const hldb::Operation *lhs = subOp(op, 0, vpiConsecutiveRepeatOp);
  ASSERT_NE(lhs, nullptr) << "Table 16-1: [*] binds tighter than ##; lhs must be 'a[*2]'";
  EXPECT_TRUE(isRef(lhs, 0, "a"));
  EXPECT_TRUE(isRef(op, 1, "b"));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
