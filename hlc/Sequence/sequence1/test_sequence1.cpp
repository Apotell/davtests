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

// Validates that a module-level sequence declaration, property declaration,
// and concurrent assert property are captured correctly in the HLDB graph.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assert_stmt.h>
#include <hldb/concurrent_assertions.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/property_decl.h>
#include <hldb/property_spec.h>
#include <hldb/sequence_decl.h>

namespace hlc {

class Sequence1 : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "sequence1.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(Sequence1, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("work@tb", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Sequence declaration
// ---------------------------------------------------------------------------
TEST_F(Sequence1, SequenceDeclaration) {
  const hldb::Module *const tb = hldb::findByName<hldb::Module>("work@tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr) << "tb has no sequence declarations";

  const hldb::SequenceDecl *seq1 = nullptr;
  for (const hldb::SequenceDecl *const s : *tb->getSequenceDecls()) {
    if (s->getName() == "seq1") {
      seq1 = s;
      break;
    }
  }
  ASSERT_NE(seq1, nullptr) << "sequence 'seq1' not found in tb";
}

TEST_F(Sequence1, SequenceHasExpression) {
  const hldb::Module *const tb = hldb::findByName<hldb::Module>("work@tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr);

  const hldb::SequenceDecl *seq1 = nullptr;
  for (const hldb::SequenceDecl *const s : *tb->getSequenceDecls()) {
    if (s->getName() == "seq1") {
      seq1 = s;
      break;
    }
  }
  ASSERT_NE(seq1, nullptr) << "sequence 'seq1' not found";
  EXPECT_NE(seq1->getExpr(), nullptr) << "seq1 has no expression (expected ##1 a ##2 b)";
}

// ---------------------------------------------------------------------------
// Property declaration
// ---------------------------------------------------------------------------
TEST_F(Sequence1, PropertyDeclaration) {
  const hldb::Module *const tb = hldb::findByName<hldb::Module>("work@tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getPropertyDecls(), nullptr) << "tb has no property declarations";

  const hldb::PropertyDecl *prop_p = nullptr;
  for (const hldb::PropertyDecl *const p : *tb->getPropertyDecls()) {
    if (p->getName() == "p") {
      prop_p = p;
      break;
    }
  }
  ASSERT_NE(prop_p, nullptr) << "property 'p' not found in tb";
}

TEST_F(Sequence1, PropertyHasClockingEvent) {
  const hldb::Module *const tb = hldb::findByName<hldb::Module>("work@tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getPropertyDecls(), nullptr);

  const hldb::PropertyDecl *prop_p = nullptr;
  for (const hldb::PropertyDecl *const p : *tb->getPropertyDecls()) {
    if (p->getName() == "p") {
      prop_p = p;
      break;
    }
  }
  ASSERT_NE(prop_p, nullptr) << "property 'p' not found";

  const hldb::PropertySpec *const spec = prop_p->getPropertySpec();
  ASSERT_NE(spec, nullptr) << "property 'p' has no PropertySpec";
  EXPECT_NE(spec->getClockingEvent(), nullptr) << "property 'p' has no clocking event (expected @(posedge clk))";
}

TEST_F(Sequence1, PropertyHasExpression) {
  const hldb::Module *const tb = hldb::findByName<hldb::Module>("work@tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getPropertyDecls(), nullptr);

  const hldb::PropertyDecl *prop_p = nullptr;
  for (const hldb::PropertyDecl *const p : *tb->getPropertyDecls()) {
    if (p->getName() == "p") {
      prop_p = p;
      break;
    }
  }
  ASSERT_NE(prop_p, nullptr) << "property 'p' not found";

  const hldb::PropertySpec *const spec = prop_p->getPropertySpec();
  ASSERT_NE(spec, nullptr) << "property 'p' has no PropertySpec";
  EXPECT_NE(spec->getPropertyExpr(), nullptr) << "property 'p' has no property expression (expected seq1 reference)";
}

// ---------------------------------------------------------------------------
// Concurrent assertion (assert property(p))
// ---------------------------------------------------------------------------
TEST_F(Sequence1, ConcurrentAssertion) {
  const hldb::Module *const tb = hldb::findByName<hldb::Module>("work@tb", m_design->getAllModules());
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
  EXPECT_NE(found, nullptr) << "No Assert node in tb concurrent assertions";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
