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

// Validates that a module-level sequence (seq2: a ##1 b ##2 c) and an inline
// concurrent assert property with clocking event are captured in the HLDB graph.
// Unlike sequence1, there is no separate property declaration — the clocking
// event is embedded directly in the assert property statement.

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

class Sequence2 : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "sequence2.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(Sequence2, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("tb", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Sequence declaration
// ---------------------------------------------------------------------------
TEST_F(Sequence2, SequenceDeclaration) {
  const hldb::Module *const tb = hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr) << "tb has no sequence declarations";

  const hldb::SequenceDecl *seq2 = nullptr;
  for (const hldb::SequenceDecl *const s : *tb->getSequenceDecls()) {
    if (s->getName() == "seq2") {
      seq2 = s;
      break;
    }
  }
  ASSERT_NE(seq2, nullptr) << "sequence 'seq2' not found in tb";
}

TEST_F(Sequence2, SequenceHasExpression) {
  const hldb::Module *const tb = hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr);

  const hldb::SequenceDecl *seq2 = nullptr;
  for (const hldb::SequenceDecl *const s : *tb->getSequenceDecls()) {
    if (s->getName() == "seq2") {
      seq2 = s;
      break;
    }
  }
  ASSERT_NE(seq2, nullptr) << "sequence 'seq2' not found";
  EXPECT_NE(seq2->getExpr(), nullptr) << "seq2 has no expression (expected a ##1 b ##2 c)";
}

TEST_F(Sequence2, SequenceExprIsUnaryCycleDelay) {
  const hldb::Module *const tb = hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr);

  const hldb::SequenceDecl *seq2 = nullptr;
  for (const hldb::SequenceDecl *const s : *tb->getSequenceDecls()) {
    if (s->getName() == "seq2") {
      seq2 = s;
      break;
    }
  }
  ASSERT_NE(seq2, nullptr);

  const hldb::Operation *const expr = seq2->getExpr<hldb::Operation>();
  ASSERT_NE(expr, nullptr) << "seq2 expression is not an Operation";
  EXPECT_EQ(expr->getOpType(), vpiCycleDelayOp) << "seq2 top-level op is not vpiUnaryCycleDelayOp";
}

TEST_F(Sequence2, SequenceExprHasThreeOperands) {
  const hldb::Module *const tb = hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr);

  const hldb::SequenceDecl *seq2 = nullptr;
  for (const hldb::SequenceDecl *const s : *tb->getSequenceDecls()) {
    if (s->getName() == "seq2") {
      seq2 = s;
      break;
    }
  }
  ASSERT_NE(seq2, nullptr);

  const hldb::Operation *const expr = seq2->getExpr<hldb::Operation>();
  ASSERT_NE(expr, nullptr) << "seq2 expression is not an Operation";
  ASSERT_NE(expr->getOperands(), nullptr) << "seq2 Operation has no operands";
  // a ##1 b ##2 c encodes as: UnaryCycleDelay(delay=1, lhs=a, rhs=UnaryCycleDelay(...))
  EXPECT_EQ(expr->getOperands()->size(), 3u) << "expected 3 operands (delay constant, 'a', nested b##2c op)";
}

// ---------------------------------------------------------------------------
// No separate property declaration (clocking event is inline in the assert)
// ---------------------------------------------------------------------------
TEST_F(Sequence2, NoSeparatePropertyDecl) {
  const hldb::Module *const tb = hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);

  const bool hasPropertyDecls = tb->getPropertyDecls() != nullptr && !tb->getPropertyDecls()->empty();
  EXPECT_FALSE(hasPropertyDecls) << "tb should have no named property declaration — the assert property is inline";
}

// ---------------------------------------------------------------------------
// Concurrent assertion — assert property(@(posedge clk) seq2)
// ---------------------------------------------------------------------------
TEST_F(Sequence2, ConcurrentAssertion) {
  const hldb::Module *const tb = hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
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

TEST_F(Sequence2, AssertHasInlineClockingEvent) {
  const hldb::Module *const tb = hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
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

TEST_F(Sequence2, AssertPropertyExprReferencesSeq2) {
  const hldb::Module *const tb = hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
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
  EXPECT_EQ(propExpr->getName(), "seq2") << "inline assert property expression does not reference 'seq2'";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
