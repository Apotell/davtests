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

// Validates that a consecutive-repetition sequence (seq3: a[*2]) and an inline
// concurrent assert property with clocking event are captured in the HLDB graph.
// Grammar: sequence_expr -> expression_or_dist boolean_abbrev (consecutive_repetition)
// maps to Operation with vpiConsecutiveRepeatOp (77).

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

class Sequence3 : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "sequence3.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(Sequence3, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("tb", m_design->getAllModules()), nullptr);
}

// ----
// Sequence declaration
// ----
TEST_F(Sequence3, SequenceDeclaration) {
  const hldb::Module *const tb = hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr) << "tb has no sequence declarations";

  const hldb::SequenceDecl *seq3 = nullptr;
  for (const hldb::SequenceDecl *const s : *tb->getSequenceDecls()) {
    if (s->getName() == "seq3") {
      seq3 = s;
      break;
    }
  }
  ASSERT_NE(seq3, nullptr) << "sequence 'seq3' not found in tb";
}

TEST_F(Sequence3, SequenceHasExpression) {
  const hldb::Module *const tb = hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr);

  const hldb::SequenceDecl *seq3 = nullptr;
  for (const hldb::SequenceDecl *const s : *tb->getSequenceDecls()) {
    if (s->getName() == "seq3") {
      seq3 = s;
      break;
    }
  }
  ASSERT_NE(seq3, nullptr) << "sequence 'seq3' not found";
  EXPECT_NE(seq3->getExpr(), nullptr) << "seq3 has no expression (expected a[*2])";
}

TEST_F(Sequence3, SequenceExprIsConsecutiveRepeat) {
  const hldb::Module *const tb = hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr);

  const hldb::SequenceDecl *seq3 = nullptr;
  for (const hldb::SequenceDecl *const s : *tb->getSequenceDecls()) {
    if (s->getName() == "seq3") {
      seq3 = s;
      break;
    }
  }
  ASSERT_NE(seq3, nullptr);

  const hldb::Operation *const expr = seq3->getExpr<hldb::Operation>();
  ASSERT_NE(expr, nullptr) << "seq3 expression is not an Operation";
  EXPECT_EQ(expr->getOpType(), vpiConsecutiveRepeatOp) << "seq3 op is not vpiConsecutiveRepeatOp (expected a[*2])";
}

TEST_F(Sequence3, SequenceExprHasTwoOperands) {
  const hldb::Module *const tb = hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr);

  const hldb::SequenceDecl *seq3 = nullptr;
  for (const hldb::SequenceDecl *const s : *tb->getSequenceDecls()) {
    if (s->getName() == "seq3") {
      seq3 = s;
      break;
    }
  }
  ASSERT_NE(seq3, nullptr);

  const hldb::Operation *const expr = seq3->getExpr<hldb::Operation>();
  ASSERT_NE(expr, nullptr) << "seq3 expression is not an Operation";
  ASSERT_NE(expr->getOperands(), nullptr) << "seq3 Operation has no operands";
  // a[*2] -> ConsecutiveRepeatOp(a, 2): the repeated expression and the count
  EXPECT_EQ(expr->getOperands()->size(), 2u) << "expected 2 operands (signal 'a' and repetition count 2)";
}

// ----
// No separate property declaration (clocking event is inline in the assert)
// ----
TEST_F(Sequence3, NoSeparatePropertyDecl) {
  const hldb::Module *const tb = hldb::findByName<hldb::Module>("tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);

  const bool hasPropertyDecls = tb->getPropertyDecls() != nullptr && !tb->getPropertyDecls()->empty();
  EXPECT_FALSE(hasPropertyDecls) << "tb should have no named property declaration -- the assert property is inline";
}

// ----
// Concurrent assertion -- assert property(@(posedge clk) seq3)
// ----
TEST_F(Sequence3, ConcurrentAssertion) {
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

TEST_F(Sequence3, AssertHasInlineClockingEvent) {
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

TEST_F(Sequence3, AssertPropertyExprReferencesSeq3) {
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
  EXPECT_EQ(propExpr->getName(), "seq3") << "inline assert property expression does not reference 'seq3'";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
