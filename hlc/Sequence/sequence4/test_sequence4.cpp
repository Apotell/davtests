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

// Validates consecutive-repetition applied to a sequence instance (seq4: base_seq[*3])
// rather than a plain signal, plus an inline concurrent assert property with clocking event.
// Grammar: sequence_expr → sequence_instance boolean_abbrev (consecutive_repetition)
// maps to Operation with vpiConsecutiveRepeatOp (60), first operand is a RefObj to base_seq.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assert_stmt.h>
#include <hldb/concurrent_assertions.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/property_spec.h>
#include <hldb/ref_obj.h>
#include <hldb/sequence_decl.h>

namespace hlc {

class Sequence4 : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "sequence4.hlc"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

TEST_F(Sequence4, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("work@tb", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Sequence declarations — both base_seq and seq4 must be present
// ---------------------------------------------------------------------------
TEST_F(Sequence4, TwoSequenceDeclarations) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("work@tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr) << "tb has no sequence declarations";
  EXPECT_GE(tb->getSequenceDecls()->size(), 2u)
      << "expected at least 2 sequence declarations (base_seq, seq4)";
}

TEST_F(Sequence4, BaseSeqDeclaration) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("work@tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr);

  const hldb::SequenceDecl *baseSeq = nullptr;
  for (const hldb::SequenceDecl *const s : *tb->getSequenceDecls()) {
    if (s->getName() == "base_seq") { baseSeq = s; break; }
  }
  ASSERT_NE(baseSeq, nullptr) << "sequence 'base_seq' not found in tb";
  EXPECT_NE(baseSeq->getExpr(), nullptr) << "base_seq has no expression (expected 'a')";
}

TEST_F(Sequence4, BaseSeqExprIsRefToA) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("work@tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr);

  const hldb::SequenceDecl *baseSeq = nullptr;
  for (const hldb::SequenceDecl *const s : *tb->getSequenceDecls()) {
    if (s->getName() == "base_seq") { baseSeq = s; break; }
  }
  ASSERT_NE(baseSeq, nullptr) << "sequence 'base_seq' not found";

  const hldb::RefObj *const expr = baseSeq->getExpr<hldb::RefObj>();
  ASSERT_NE(expr, nullptr) << "base_seq expression is not a RefObj (expected reference to 'a')";
  EXPECT_EQ(expr->getName(), "a") << "base_seq expression does not reference signal 'a'";
}

TEST_F(Sequence4, Seq4Declaration) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("work@tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr);

  const hldb::SequenceDecl *seq4 = nullptr;
  for (const hldb::SequenceDecl *const s : *tb->getSequenceDecls()) {
    if (s->getName() == "seq4") { seq4 = s; break; }
  }
  ASSERT_NE(seq4, nullptr) << "sequence 'seq4' not found in tb";
}

TEST_F(Sequence4, Seq4HasExpression) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("work@tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr);

  const hldb::SequenceDecl *seq4 = nullptr;
  for (const hldb::SequenceDecl *const s : *tb->getSequenceDecls()) {
    if (s->getName() == "seq4") { seq4 = s; break; }
  }
  ASSERT_NE(seq4, nullptr) << "sequence 'seq4' not found";
  EXPECT_NE(seq4->getExpr(), nullptr) << "seq4 has no expression (expected base_seq[*3])";
}

TEST_F(Sequence4, Seq4ExprIsConsecutiveRepeat) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("work@tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr);

  const hldb::SequenceDecl *seq4 = nullptr;
  for (const hldb::SequenceDecl *const s : *tb->getSequenceDecls()) {
    if (s->getName() == "seq4") { seq4 = s; break; }
  }
  ASSERT_NE(seq4, nullptr);

  const hldb::Operation *const expr = seq4->getExpr<hldb::Operation>();
  ASSERT_NE(expr, nullptr) << "seq4 expression is not an Operation";
  EXPECT_EQ(expr->getOpType(), vpiConsecutiveRepeatOp)
      << "seq4 op is not vpiConsecutiveRepeatOp (expected base_seq[*3])";
}

TEST_F(Sequence4, Seq4ExprHasTwoOperands) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("work@tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr);

  const hldb::SequenceDecl *seq4 = nullptr;
  for (const hldb::SequenceDecl *const s : *tb->getSequenceDecls()) {
    if (s->getName() == "seq4") { seq4 = s; break; }
  }
  ASSERT_NE(seq4, nullptr);

  const hldb::Operation *const expr = seq4->getExpr<hldb::Operation>();
  ASSERT_NE(expr, nullptr) << "seq4 expression is not an Operation";
  ASSERT_NE(expr->getOperands(), nullptr) << "seq4 Operation has no operands";
  EXPECT_EQ(expr->getOperands()->size(), 2u)
      << "expected 2 operands (sequence instance 'base_seq' and repetition count 3)";
}

TEST_F(Sequence4, Seq4ExprSecondOperandReferencesBaseSeq) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("work@tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr);

  const hldb::SequenceDecl *seq4 = nullptr;
  for (const hldb::SequenceDecl *const s : *tb->getSequenceDecls()) {
    if (s->getName() == "seq4") { seq4 = s; break; }
  }
  ASSERT_NE(seq4, nullptr);

  const hldb::Operation *const expr = seq4->getExpr<hldb::Operation>();
  ASSERT_NE(expr, nullptr);
  ASSERT_NE(expr->getOperands(), nullptr);
  ASSERT_GE(expr->getOperands()->size(), 2u);

  // Operand layout for ConsecutiveRepeatOp: [0] = count, [1] = repeated expr
  const hldb::RefObj *const seqOp =
      any_cast<hldb::RefObj>((*expr->getOperands())[0]);
  ASSERT_NE(seqOp, nullptr)
      << "seq4 second operand is not a RefObj (expected reference to 'base_seq')";
  EXPECT_EQ(seqOp->getName(), "base_seq")
      << "seq4 second operand does not reference 'base_seq'";
}

// ---------------------------------------------------------------------------
// No separate property declaration (clocking event is inline in the assert)
// ---------------------------------------------------------------------------
TEST_F(Sequence4, NoSeparatePropertyDecl) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("work@tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);

  const bool hasPropertyDecls =
      tb->getPropertyDecls() != nullptr && !tb->getPropertyDecls()->empty();
  EXPECT_FALSE(hasPropertyDecls)
      << "tb should have no named property declaration — the assert property is inline";
}

// ---------------------------------------------------------------------------
// Concurrent assertion — assert property(@(posedge clk) seq4)
// ---------------------------------------------------------------------------
TEST_F(Sequence4, ConcurrentAssertion) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("work@tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr) << "tb has no concurrent assertions";
  ASSERT_FALSE(tb->getConcurrentAssertions()->empty()) << "tb concurrent assertions list is empty";

  const hldb::Assert *found = nullptr;
  for (const hldb::ConcurrentAssertions *const ca : *tb->getConcurrentAssertions()) {
    if (const hldb::Assert *const a = any_cast<hldb::Assert>(ca)) {
      found = a;
      break;
    }
  }
  ASSERT_NE(found, nullptr) << "No Assert node in tb concurrent assertions";
}

TEST_F(Sequence4, AssertHasInlineClockingEvent) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("work@tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr);

  const hldb::Assert *found = nullptr;
  for (const hldb::ConcurrentAssertions *const ca : *tb->getConcurrentAssertions()) {
    if (const hldb::Assert *const a = any_cast<hldb::Assert>(ca)) {
      found = a;
      break;
    }
  }
  ASSERT_NE(found, nullptr);

  const hldb::PropertySpec *const spec = found->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr) << "Assert has no inline PropertySpec";
  EXPECT_NE(spec->getClockingEvent(), nullptr)
      << "inline assert property has no clocking event (expected @(posedge clk))";
}

TEST_F(Sequence4, AssertPropertyExprReferencesSeq4) {
  const hldb::Module *const tb =
      hldb::findByName<hldb::Module>("work@tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr);

  const hldb::Assert *found = nullptr;
  for (const hldb::ConcurrentAssertions *const ca : *tb->getConcurrentAssertions()) {
    if (const hldb::Assert *const a = any_cast<hldb::Assert>(ca)) {
      found = a;
      break;
    }
  }
  ASSERT_NE(found, nullptr);

  const hldb::PropertySpec *const spec = found->getProperty<hldb::PropertySpec>();
  ASSERT_NE(spec, nullptr) << "Assert has no inline PropertySpec";

  const hldb::RefObj *const propExpr = spec->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(propExpr, nullptr) << "inline assert property expression is not a RefObj";
  EXPECT_EQ(propExpr->getName(), "seq4")
      << "inline assert property expression does not reference 'seq4'";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
