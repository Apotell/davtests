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
// and concurrent assert property are captured correctly in the UHDM graph.

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/assert_stmt.h>
#include <uhdm/concurrent_assertions.h>
#include <uhdm/design.h>
#include <uhdm/module.h>
#include <uhdm/property_decl.h>
#include <uhdm/property_spec.h>
#include <uhdm/sequence_decl.h>

namespace SURELOG {

class Sequence1 : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "sequence1.hlc"});

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

TEST_F(Sequence1, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@tb", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Sequence declaration
// ---------------------------------------------------------------------------
TEST_F(Sequence1, SequenceDeclaration) {
  const uhdm::Module *const tb =
      uhdm::findByName<uhdm::Module>("work@tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr) << "tb has no sequence declarations";

  const uhdm::SequenceDecl *seq1 = nullptr;
  for (const uhdm::SequenceDecl *const s : *tb->getSequenceDecls()) {
    if (s->getName() == "seq1") { seq1 = s; break; }
  }
  ASSERT_NE(seq1, nullptr) << "sequence 'seq1' not found in tb";
}

TEST_F(Sequence1, SequenceHasExpression) {
  const uhdm::Module *const tb =
      uhdm::findByName<uhdm::Module>("work@tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getSequenceDecls(), nullptr);

  const uhdm::SequenceDecl *seq1 = nullptr;
  for (const uhdm::SequenceDecl *const s : *tb->getSequenceDecls()) {
    if (s->getName() == "seq1") { seq1 = s; break; }
  }
  ASSERT_NE(seq1, nullptr) << "sequence 'seq1' not found";
  EXPECT_NE(seq1->getExpr(), nullptr) << "seq1 has no expression (expected ##1 a ##2 b)";
}

// ---------------------------------------------------------------------------
// Property declaration
// ---------------------------------------------------------------------------
TEST_F(Sequence1, PropertyDeclaration) {
  const uhdm::Module *const tb =
      uhdm::findByName<uhdm::Module>("work@tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getPropertyDecls(), nullptr) << "tb has no property declarations";

  const uhdm::PropertyDecl *prop_p = nullptr;
  for (const uhdm::PropertyDecl *const p : *tb->getPropertyDecls()) {
    if (p->getName() == "p") { prop_p = p; break; }
  }
  ASSERT_NE(prop_p, nullptr) << "property 'p' not found in tb";
}

TEST_F(Sequence1, PropertyHasClockingEvent) {
  const uhdm::Module *const tb =
      uhdm::findByName<uhdm::Module>("work@tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getPropertyDecls(), nullptr);

  const uhdm::PropertyDecl *prop_p = nullptr;
  for (const uhdm::PropertyDecl *const p : *tb->getPropertyDecls()) {
    if (p->getName() == "p") { prop_p = p; break; }
  }
  ASSERT_NE(prop_p, nullptr) << "property 'p' not found";

  const uhdm::PropertySpec *const spec = prop_p->getPropertySpec();
  ASSERT_NE(spec, nullptr) << "property 'p' has no PropertySpec";
  EXPECT_NE(spec->getClockingEvent(), nullptr)
      << "property 'p' has no clocking event (expected @(posedge clk))";
}

TEST_F(Sequence1, PropertyHasExpression) {
  const uhdm::Module *const tb =
      uhdm::findByName<uhdm::Module>("work@tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getPropertyDecls(), nullptr);

  const uhdm::PropertyDecl *prop_p = nullptr;
  for (const uhdm::PropertyDecl *const p : *tb->getPropertyDecls()) {
    if (p->getName() == "p") { prop_p = p; break; }
  }
  ASSERT_NE(prop_p, nullptr) << "property 'p' not found";

  const uhdm::PropertySpec *const spec = prop_p->getPropertySpec();
  ASSERT_NE(spec, nullptr) << "property 'p' has no PropertySpec";
  EXPECT_NE(spec->getPropertyExpr(), nullptr)
      << "property 'p' has no property expression (expected seq1 reference)";
}

// ---------------------------------------------------------------------------
// Concurrent assertion (assert property(p))
// ---------------------------------------------------------------------------
TEST_F(Sequence1, ConcurrentAssertion) {
  const uhdm::Module *const tb =
      uhdm::findByName<uhdm::Module>("work@tb", m_design->getAllModules());
  ASSERT_NE(tb, nullptr);
  ASSERT_NE(tb->getConcurrentAssertions(), nullptr) << "tb has no concurrent assertions";
  ASSERT_FALSE(tb->getConcurrentAssertions()->empty()) << "tb concurrent assertions list is empty";

  const uhdm::Assert *found = nullptr;
  for (const uhdm::ConcurrentAssertions *const ca : *tb->getConcurrentAssertions()) {
    if (const uhdm::Assert *const a = any_cast<uhdm::Assert>(ca)) {
      found = a;
      break;
    }
  }
  EXPECT_NE(found, nullptr) << "No Assert node in tb concurrent assertions";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
