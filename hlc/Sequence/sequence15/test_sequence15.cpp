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

// Tests ss.16.7 (sequence expressions) for the dist operator inside a sequence
// passed to an assert property statement.
// Source: sequence15.sv
//   sequence seq15;
//     (a dist {1:=3, 2:=1});
//   endsequence
//   assert property(@(posedge clk) seq15);
//
// IEEE 1800-2017 ss.16.7: When a 'dist' expression appears inside a sequence
// that is used in an assertion context, it is treated as an 'inside' operator.
// The weight information (':=3', ':=1') is discarded; only the value set
// {1, 2} is used for membership testing. The resulting HLDB node must be an
// Operation with vpiInsideOp (94), NOT a raw Distribution node.
//
// Expected HLDB tree for seq15 body (per LRM):
//   SequenceDecl("seq15")
//     -> getExpr<Operation>() with getOpType() == vpiInsideOp (94)
//          operand[0]: RefObj("a")  -- the signal being tested
//          operand[1..]: Constant("1"), Constant("2")  -- the value set
//                        (weights stripped; ':=3' and ':=1' become '1' and '2')
//
// Compile-stage bugs exposed:
//   Compiler_NoErrors                   -- EL0535: Surelog treats 'seq15' in
//                                          assert property(@(posedge clk) seq15)
//                                          as an implicit net instead of
//                                          resolving to the SequenceDecl.
//   Assert_PropertyExpr_ResolvedToSeq15Decl -- RefObj in the PropertySpec does
//                                          not link back to the SequenceDecl
//                                          (getActual<SequenceDecl>() is null).
//
// Elaboration-stage checks (SKIPPED -- elaboration not yet implemented):
//   Seq15_Expr_IsInsideOp               -- dist must become InsideOp(94) when
//                                          used in assert context (ss.16.7).
//   Seq15_Expr_IsNotDistribution        -- Distribution node must be gone after
//                                          elaboration converts dist to inside.
//   Seq15_InsideOp_Has/FirstOperand/ValueSet* -- InsideOp subtree structure.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assert_stmt.h>
#include <hldb/concurrent_assertions.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/dist_item.h>
#include <hldb/distribution.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/property_spec.h>
#include <hldb/ref_obj.h>
#include <hldb/sequence_decl.h>
#include <hldb/sv_vpi_user.h>

namespace hlc {

class Sequence15Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "sequence15.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::SequenceDecl *getSeqDecl(const hldb::Module *mod,
                                               std::string_view name) {
    if (!mod || !mod->getSequenceDecls()) return nullptr;
    for (const hldb::SequenceDecl *const s : *mod->getSequenceDecls()) {
      if (s->getName() == name) return s;
    }
    return nullptr;
  }

  static const hldb::Distribution *getDistribution(const hldb::Module *mod) {
    const hldb::SequenceDecl *s15 = getSeqDecl(mod, "seq15");
    if (!s15) return nullptr;
    return s15->getExpr<hldb::Distribution>();
  }
};

// ---------------------------------------------------------------------------
// Module
// ---------------------------------------------------------------------------
TEST_F(Sequence15Test, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("tb", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Sequence declaration
// ---------------------------------------------------------------------------
TEST_F(Sequence15Test, Seq15DeclarationExists) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr) << "tb has no sequence declarations";

  EXPECT_NE(getSeqDecl(tb, "seq15"), nullptr)
      << "sequence 'seq15' not found in tb";
}

TEST_F(Sequence15Test, Seq15HasExpression) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);

  const hldb::SequenceDecl *seq15 = getSeqDecl(tb, "seq15");
  ASSERT_NE(seq15, nullptr) << "sequence 'seq15' not found in tb";
  EXPECT_NE(seq15->getExpr(), nullptr)
      << "seq15 has no expression (expected Distribution for 'a dist {1:=3, 2:=1}')";
}

// ---------------------------------------------------------------------------
// Compile-stage: parser correctly reads 'a dist {1:=3, 2:=1}' and builds
// a Distribution node with subject RefObj("a") and two DistItems.
// These tests verify the compiler output without elaboration.
// ---------------------------------------------------------------------------
TEST_F(Sequence15Test, Seq15_Dist_IsDistribution) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);

  const hldb::SequenceDecl *seq15 = getSeqDecl(tb, "seq15");
  ASSERT_NE(seq15, nullptr);

  const hldb::Distribution *const dist = seq15->getExpr<hldb::Distribution>();
  EXPECT_NE(dist, nullptr)
      << "seq15 expr is not a Distribution -- parser did not read "
         "'a dist {1:=3, 2:=1}' as a Distribution node";
}

TEST_F(Sequence15Test, Seq15_Dist_IsNotSoft) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);

  const hldb::Distribution *const dist = getDistribution(tb);
  ASSERT_NE(dist, nullptr) << "Distribution node not found for seq15";
  EXPECT_FALSE(dist->getSoft())
      << "seq15 Distribution has soft=true -- no 'soft' keyword in source";
}

TEST_F(Sequence15Test, Seq15_Dist_ExprIsRefToA) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);

  const hldb::Distribution *const dist = getDistribution(tb);
  ASSERT_NE(dist, nullptr) << "Distribution node not found for seq15";

  const hldb::RefObj *const ref = dist->getExpr<hldb::RefObj>();
  ASSERT_NE(ref, nullptr)
      << "Distribution subject is not a RefObj (expected reference to 'a')";
  EXPECT_EQ(ref->getName(), "a") << "Distribution subject is not 'a'";
}

TEST_F(Sequence15Test, Seq15_Dist_HasTwoItems) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);

  const hldb::Distribution *const dist = getDistribution(tb);
  ASSERT_NE(dist, nullptr) << "Distribution node not found for seq15";
  ASSERT_NE(dist->getDistItems(), nullptr)
      << "Distribution has no DistItemCollection (expected {1:=3, 2:=1})";
  EXPECT_EQ(dist->getDistItems()->size(), 2u)
      << "expected 2 dist items for {1:=3, 2:=1}, got "
      << dist->getDistItems()->size();
}

// DistItem[0]: 1 := 3
TEST_F(Sequence15Test, Seq15_Dist_Item0_TypeIsEqualDist) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);

  const hldb::Distribution *const dist = getDistribution(tb);
  ASSERT_NE(dist, nullptr) << "Distribution node not found for seq15";
  ASSERT_NE(dist->getDistItems(), nullptr);
  ASSERT_GE(dist->getDistItems()->size(), 1u);

  const hldb::DistItem *const item0 = (*dist->getDistItems())[0];
  ASSERT_NE(item0, nullptr) << "DistItem[0] is null";
  EXPECT_EQ(item0->getDistType(), vpiEqualDist)
      << "DistItem[0]: expected vpiEqualDist (1) for ':=' operator, got "
      << item0->getDistType();
}

TEST_F(Sequence15Test, Seq15_Dist_Item0_ValueRangeIsOne) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);

  const hldb::Distribution *const dist = getDistribution(tb);
  ASSERT_NE(dist, nullptr) << "Distribution node not found for seq15";
  ASSERT_NE(dist->getDistItems(), nullptr);
  ASSERT_GE(dist->getDistItems()->size(), 1u);

  const hldb::DistItem *const item0 = (*dist->getDistItems())[0];
  ASSERT_NE(item0, nullptr);
  const hldb::Constant *const val =
      any_cast<hldb::Constant>(item0->getValueRange());
  ASSERT_NE(val, nullptr)
      << "DistItem[0] value range is not a Constant (expected '1')";
  EXPECT_EQ(val->getDecompile(), "1")
      << "DistItem[0] value: expected '1', got '" << val->getDecompile() << "'";
}

TEST_F(Sequence15Test, Seq15_Dist_Item0_WeightIsThree) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);

  const hldb::Distribution *const dist = getDistribution(tb);
  ASSERT_NE(dist, nullptr) << "Distribution node not found for seq15";
  ASSERT_NE(dist->getDistItems(), nullptr);
  ASSERT_GE(dist->getDistItems()->size(), 1u);

  const hldb::DistItem *const item0 = (*dist->getDistItems())[0];
  ASSERT_NE(item0, nullptr);
  const hldb::Constant *const weight =
      any_cast<hldb::Constant>(item0->getWeight());
  ASSERT_NE(weight, nullptr)
      << "DistItem[0] weight is not a Constant (expected '3')";
  EXPECT_EQ(weight->getDecompile(), "3")
      << "DistItem[0] weight: expected '3', got '" << weight->getDecompile() << "'";
}

// DistItem[1]: 2 := 1
TEST_F(Sequence15Test, Seq15_Dist_Item1_TypeIsEqualDist) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);

  const hldb::Distribution *const dist = getDistribution(tb);
  ASSERT_NE(dist, nullptr) << "Distribution node not found for seq15";
  ASSERT_NE(dist->getDistItems(), nullptr);
  ASSERT_GE(dist->getDistItems()->size(), 2u);

  const hldb::DistItem *const item1 = (*dist->getDistItems())[1];
  ASSERT_NE(item1, nullptr) << "DistItem[1] is null";
  EXPECT_EQ(item1->getDistType(), vpiEqualDist)
      << "DistItem[1]: expected vpiEqualDist (1) for ':=' operator, got "
      << item1->getDistType();
}

TEST_F(Sequence15Test, Seq15_Dist_Item1_ValueRangeIsTwo) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);

  const hldb::Distribution *const dist = getDistribution(tb);
  ASSERT_NE(dist, nullptr) << "Distribution node not found for seq15";
  ASSERT_NE(dist->getDistItems(), nullptr);
  ASSERT_GE(dist->getDistItems()->size(), 2u);

  const hldb::DistItem *const item1 = (*dist->getDistItems())[1];
  ASSERT_NE(item1, nullptr);
  const hldb::Constant *const val =
      any_cast<hldb::Constant>(item1->getValueRange());
  ASSERT_NE(val, nullptr)
      << "DistItem[1] value range is not a Constant (expected '2')";
  EXPECT_EQ(val->getDecompile(), "2")
      << "DistItem[1] value: expected '2', got '" << val->getDecompile() << "'";
}

TEST_F(Sequence15Test, Seq15_Dist_Item1_WeightIsOne) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);

  const hldb::Distribution *const dist = getDistribution(tb);
  ASSERT_NE(dist, nullptr) << "Distribution node not found for seq15";
  ASSERT_NE(dist->getDistItems(), nullptr);
  ASSERT_GE(dist->getDistItems()->size(), 2u);

  const hldb::DistItem *const item1 = (*dist->getDistItems())[1];
  ASSERT_NE(item1, nullptr);
  const hldb::Constant *const weight =
      any_cast<hldb::Constant>(item1->getWeight());
  ASSERT_NE(weight, nullptr)
      << "DistItem[1] weight is not a Constant (expected '1')";
  EXPECT_EQ(weight->getDecompile(), "1")
      << "DistItem[1] weight: expected '1', got '" << weight->getDecompile() << "'";
}

// ---------------------------------------------------------------------------
// Elaboration-stage checks: dist -> inside conversion (ss.16.7).
// The conversion of 'dist' to 'inside' in assertion context is performed
// during elaboration, not compilation. These tests are skipped until
// elaboration is implemented.
// ---------------------------------------------------------------------------
TEST_F(Sequence15Test, Seq15_Expr_IsInsideOp) {
  GTEST_SKIP() << "Elaboration not yet implemented; "
                  "dist->inside conversion (ss.16.7) is an elaboration-stage "
                  "transform.";
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);

  const hldb::SequenceDecl *seq15 = getSeqDecl(tb, "seq15");
  ASSERT_NE(seq15, nullptr);

  const hldb::Operation *const op = seq15->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr)
      << "seq15 expr is not an Operation(vpiInsideOp) -- dist was not "
         "converted to inside in assertion context";
  EXPECT_EQ(op->getOpType(), vpiInsideOp)
      << "seq15 expr op type is " << op->getOpType()
      << ", expected vpiInsideOp (94) -- 'a dist {1:=3, 2:=1}' in assertion "
         "context must be treated as 'a inside {1, 2}'";
}

TEST_F(Sequence15Test, Seq15_Expr_IsNotDistribution) {
  GTEST_SKIP() << "Elaboration not yet implemented; "
                  "at compile stage Distribution is the correct raw node; "
                  "conversion to InsideOp happens during elaboration.";
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);

  const hldb::SequenceDecl *seq15 = getSeqDecl(tb, "seq15");
  ASSERT_NE(seq15, nullptr);

  const hldb::Distribution *const dist = seq15->getExpr<hldb::Distribution>();
  EXPECT_EQ(dist, nullptr)
      << "seq15 expr is a raw Distribution node -- after elaboration 'dist' "
         "in assertion context must be converted to InsideOp (LRM ss.16.7)";
}

TEST_F(Sequence15Test, Seq15_InsideOp_HasOperands) {
  GTEST_SKIP() << "Elaboration not yet implemented; "
                  "InsideOp structure checks require dist->inside conversion.";
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);

  const hldb::SequenceDecl *seq15 = getSeqDecl(tb, "seq15");
  ASSERT_NE(seq15, nullptr);

  const hldb::Operation *const op = seq15->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "seq15 expr is not an Operation";
  ASSERT_EQ(op->getOpType(), vpiInsideOp)
      << "expected vpiInsideOp (94), got " << op->getOpType();

  ASSERT_NE(op->getOperands(), nullptr) << "InsideOp has no operands";
  // 'a inside {1, 2}': RefObj(a) + 2 value operands (weights stripped)
  EXPECT_GE(op->getOperands()->size(), 3u)
      << "InsideOp should have at least 3 operands: "
         "RefObj(a), Constant(1), Constant(2); got "
      << op->getOperands()->size();
}

TEST_F(Sequence15Test, Seq15_InsideOp_FirstOperandIsRefToA) {
  GTEST_SKIP() << "Elaboration not yet implemented; "
                  "InsideOp structure checks require dist->inside conversion.";
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);

  const hldb::SequenceDecl *seq15 = getSeqDecl(tb, "seq15");
  ASSERT_NE(seq15, nullptr);

  const hldb::Operation *const op = seq15->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "seq15 expr is not an Operation";
  ASSERT_EQ(op->getOpType(), vpiInsideOp)
      << "expected vpiInsideOp (94), got " << op->getOpType();
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);

  const hldb::RefObj *const ref =
      any_cast<hldb::RefObj>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr)
      << "InsideOp operand[0] is not a RefObj (expected reference to 'a')";
  EXPECT_EQ(ref->getName(), "a")
      << "InsideOp operand[0] does not reference 'a'";
}

TEST_F(Sequence15Test, Seq15_InsideOp_ValueSetContainsOne) {
  GTEST_SKIP() << "Elaboration not yet implemented; "
                  "InsideOp structure checks require dist->inside conversion.";
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);

  const hldb::SequenceDecl *seq15 = getSeqDecl(tb, "seq15");
  ASSERT_NE(seq15, nullptr);

  const hldb::Operation *const op = seq15->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "seq15 expr is not an Operation";
  ASSERT_EQ(op->getOpType(), vpiInsideOp)
      << "expected vpiInsideOp (94), got " << op->getOpType();
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);

  // Weights stripped; value set operands begin at index 1.
  const hldb::Constant *const val1 =
      any_cast<hldb::Constant>((*op->getOperands())[1]);
  ASSERT_NE(val1, nullptr)
      << "InsideOp operand[1] is not a Constant (expected '1' from '1:=3')";
  EXPECT_EQ(val1->getDecompile(), "1")
      << "InsideOp operand[1]: expected '1', got '" << val1->getDecompile() << "'";
}

TEST_F(Sequence15Test, Seq15_InsideOp_ValueSetContainsTwo) {
  GTEST_SKIP() << "Elaboration not yet implemented; "
                  "InsideOp structure checks require dist->inside conversion.";
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);

  const hldb::SequenceDecl *seq15 = getSeqDecl(tb, "seq15");
  ASSERT_NE(seq15, nullptr);

  const hldb::Operation *const op = seq15->getExpr<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "seq15 expr is not an Operation";
  ASSERT_EQ(op->getOpType(), vpiInsideOp)
      << "expected vpiInsideOp (94), got " << op->getOpType();
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 3u);

  const hldb::Constant *const val2 =
      any_cast<hldb::Constant>((*op->getOperands())[2]);
  ASSERT_NE(val2, nullptr)
      << "InsideOp operand[2] is not a Constant (expected '2' from '2:=1')";
  EXPECT_EQ(val2->getDecompile(), "2")
      << "InsideOp operand[2]: expected '2', got '" << val2->getDecompile() << "'";
}

// ---------------------------------------------------------------------------
// Compiler -- EL0535 bug: Surelog emits an error for 'seq15' in
// assert property(@(posedge clk) seq15) because it treats the sequence name
// as an undeclared implicit net instead of resolving it to the SequenceDecl.
// This test FAILS with the current compiler and PASSES when fixed.
// ---------------------------------------------------------------------------
TEST_F(Sequence15Test, Compiler_NoErrors) {
  EXPECT_EQ(m_compiler->getErrorStats().nbError, 0)
      << "Surelog emitted " << m_compiler->getErrorStats().nbError
      << " error(s) -- expected 0. Likely EL0535: 'seq15' in "
         "assert property(@(posedge clk) seq15) treated as an implicit net "
         "instead of the declared SequenceDecl.";
}

// ---------------------------------------------------------------------------
// Concurrent assertion -- assert property(@(posedge clk) seq15)
// ---------------------------------------------------------------------------
TEST_F(Sequence15Test, Assert_ConcurrentAssertionExists) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr)
      << "tb has no concurrent assertions";
  ASSERT_FALSE(tb->getConcurrentAssertions()->empty())
      << "tb concurrent assertions list is empty";

  const hldb::Assert *found = nullptr;
  for (const hldb::ConcurrentAssertions *const ca :
       *tb->getConcurrentAssertions()) {
    if (const hldb::Assert *const a = any_cast<hldb::Assert>(ca)) {
      found = a;
      break;
    }
  }
  EXPECT_NE(found, nullptr) << "No Assert node found in tb concurrent assertions";
}

TEST_F(Sequence15Test, Assert_HasInlineClockingEvent) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr);

  const hldb::Assert *found = nullptr;
  for (const hldb::ConcurrentAssertions *const ca :
       *tb->getConcurrentAssertions()) {
    if (const hldb::Assert *const a = any_cast<hldb::Assert>(ca)) {
      found = a;
      break;
    }
  }
  ASSERT_NE(found, nullptr);

  const hldb::PropertySpec *const spec = found->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr) << "Assert has no inline PropertySpec";
  EXPECT_NE(spec->getClockingEvent(), nullptr)
      << "inline assert property has no clocking event "
         "(expected @(posedge clk))";
}

// EL0535 bug: seq15 in 'assert property(@(posedge clk) seq15)' is not
// resolved to its SequenceDecl -- getActual<SequenceDecl>() returns null.
// This test FAILS with the current compiler and PASSES when fixed.
TEST_F(Sequence15Test, Assert_PropertyExpr_ResolvedToSeq15Decl) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr);

  const hldb::Assert *found = nullptr;
  for (const hldb::ConcurrentAssertions *const ca :
       *tb->getConcurrentAssertions()) {
    if (const hldb::Assert *const a = any_cast<hldb::Assert>(ca)) {
      found = a;
      break;
    }
  }
  ASSERT_NE(found, nullptr);

  const hldb::PropertySpec *const spec = found->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr) << "Assert has no inline PropertySpec";

  const hldb::RefObj *const propExpr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(propExpr, nullptr)
      << "inline assert property expression is not a RefObj "
         "(expected reference to 'seq15')";
  EXPECT_EQ(propExpr->getName(), "seq15")
      << "property expression does not reference 'seq15'";

  // EL0535: this returns null because Surelog does not resolve the sequence
  // name to the SequenceDecl node.
  EXPECT_NE(propExpr->getActual<hldb::SequenceDecl>(), nullptr)
      << "EL0535: 'seq15' in assert property is not resolved to its "
         "SequenceDecl -- getActual<SequenceDecl>() returns null";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
