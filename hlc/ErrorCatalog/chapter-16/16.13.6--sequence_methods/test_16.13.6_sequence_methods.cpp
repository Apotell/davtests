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

// IEEE 1800-2023 Sec 16.13.6 sequence methods: triggered and matched.
//
// Nothing under tests/Google calls either method on a sequence, so this file is the
// whole coverage for them rather than a duplicate of something Google already tests.
//
// Neither method is declared in a user scope -- Sec 16.13.6 gives them no nameable
// owning type -- so both resolve to the compiler-synthesized SequenceDecl class, which
// is what these tests assert rather than merely that the call resolved.
//
// The two are not interchangeable and the fixture uses each only where it is legal:
// triggered in Boolean and wait contexts outside any sequence, matched only inside a
// multiclocked sequence expression. Using matched in a plain Boolean context is illegal
// and HLC correctly rejects it, so the fixture does not do that.
//
// Both are written without parentheses, which is the syntax Sec 16.13.6 shows
// ("sequence_instance.sequence_method"), so each arrives as a plain RefObj rather than
// a call node and binds through its actual.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/sequence_decl.h>
#include <hldb/task_func.h>

#include <string_view>
#include <vector>

namespace hlc {

class SequenceMethodsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16.13.6--sequence_methods.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Both call sites are the trailing element of a RefObj whose own name is the source
  // text of the call, so they are addressed by that name regardless of the scope they
  // sit in -- one is inside a sequence, one in an always block, one in an initial.
  std::vector<const hldb::RefObj *> findPaths(std::string_view name) const {
    std::vector<const hldb::RefObj *> found;
    for (const hldb::Any *const any : m_session->getDatabase().getObjects()) {
      const hldb::RefObj *const ro = any_cast<hldb::RefObj>(any);
      if ((ro == nullptr) || (ro->getName() != name)) continue;
      if ((ro->getPathElems() != nullptr) && !ro->getPathElems()->empty()) found.emplace_back(ro);
    }
    return found;
  }

  // The class the resolved method is declared in. Sentinels are returned rather than
  // asserted so a failure names the stage that broke instead of just "not equal".
  std::string_view ownerOf(const hldb::RefObj *path) const {
    if (path == nullptr) return "<no path>";

    const hldb::RefObj *const leaf = any_cast<hldb::RefObj>(path->getPathElems()->back());
    if (leaf == nullptr) return "<trailing element is not a RefObj>";

    const hldb::TaskFunc *const tf = leaf->getActual<hldb::TaskFunc>();
    if (tf == nullptr) return "<unresolved>";

    const hldb::ClassDefn *const owner = any_cast<hldb::ClassDefn>(tf->getParent());
    return (owner == nullptr) ? "<no owner>" : owner->getName();
  }
};

TEST_F(SequenceMethodsTest, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

TEST_F(SequenceMethodsTest, BothSequencesExist) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getSequenceDecls(), nullptr);
  EXPECT_EQ(top->getSequenceDecls()->size(), 2u) << "s1 and s2";
}

// ---------------------------------------------------------------------------
// matched -- only legal inside a sequence expression
// ---------------------------------------------------------------------------
TEST_F(SequenceMethodsTest, MatchedBindsToSequenceDecl) {
  const std::vector<const hldb::RefObj *> paths = findPaths("s1.matched");
  ASSERT_EQ(paths.size(), 1u) << "the fixture uses matched exactly once";
  EXPECT_EQ(ownerOf(paths.front()), "SequenceDecl")
      << "sequence.matched must bind (IEEE 1800-2023 Sec 16.13.6)";
}

// ---------------------------------------------------------------------------
// triggered -- legal in Boolean and wait contexts outside a sequence
// ---------------------------------------------------------------------------
TEST_F(SequenceMethodsTest, TriggeredBindsToSequenceDeclAtEverySite) {
  const std::vector<const hldb::RefObj *> paths = findPaths("s1.triggered");
  ASSERT_EQ(paths.size(), 2u) << "the fixture uses triggered in a Boolean and in a wait";
  for (const hldb::RefObj *const path : paths) {
    EXPECT_EQ(ownerOf(path), "SequenceDecl")
        << "sequence.triggered must bind (IEEE 1800-2023 Sec 16.13.6)";
  }
}

// The receiver is the sequence declaration itself, not a variable with a typespec, so
// this is the check that the binder recognizes that shape.
TEST_F(SequenceMethodsTest, ReceiverIsTheSequenceDecl) {
  const std::vector<const hldb::RefObj *> paths = findPaths("s1.triggered");
  ASSERT_FALSE(paths.empty());

  const hldb::RefObj *const receiver = any_cast<hldb::RefObj>(paths.front()->getPathElems()->at(0));
  ASSERT_NE(receiver, nullptr);
  EXPECT_EQ(receiver->getName(), "s1");
  EXPECT_NE(receiver->getActual<hldb::SequenceDecl>(), nullptr)
      << "receiver 's1' should resolve to the SequenceDecl";
}

// triggered is also a named-event method (Sec 15.5.3). The two share a name and are told
// apart only by the receiver, so this pins down that a sequence receiver reaches the
// sequence host rather than the event one.
TEST_F(SequenceMethodsTest, TriggeredOnASequenceIsNotTheNamedEventMethod) {
  const std::vector<const hldb::RefObj *> paths = findPaths("s1.triggered");
  ASSERT_FALSE(paths.empty());
  EXPECT_NE(ownerOf(paths.front()), "EventTypespec")
      << "a sequence receiver must not reach the Sec 15.5.3 named-event method";
}

TEST_F(SequenceMethodsTest, CompilerReportsNoErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(findError(ErrorDefinition::LINT_NULL_ACTUAL), nullptr)
      << "every use in this file is legal per Sec 16.13.6 and must bind";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
