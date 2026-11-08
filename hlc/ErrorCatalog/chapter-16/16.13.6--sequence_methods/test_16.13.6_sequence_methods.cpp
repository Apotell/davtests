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
// ("sequence_instance.sequence_method"). Per Sec 13.5.5 a method named without
// parentheses is still a call: each is a MethodFuncCall with getIsProperty() set, and the
// receiver is its vpiPrefix (Sec 37.42 detail 2).

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/design.h>
#include <hldb/method_func_call.h>
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
  // Every call site is written "<receiver>.<method>" (no parentheses, 13.5.5), so each is a
  // MethodFuncCall named after the bare method with getIsProperty() set. A call applied to
  // an object carries the receiver as its vpiPrefix (IEEE 1800-2023 37.42 detail 2); this
  // finds those by method and receiver name, regardless of the scope they sit in -- one is
  // inside a sequence, one in an always block, one in an initial. Inside a sequence
  // expression the receiver is written as a sequence instance, elsewhere as a plain name;
  // either way it is the same object, so both methods carry the same receiver shape.
  std::vector<const hldb::MethodFuncCall *> findCalls(std::string_view receiver, std::string_view method) const {
    std::vector<const hldb::MethodFuncCall *> found;
    for (const hldb::Any *const any : m_session->getDatabase().getObjects()) {
      const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(any);
      if ((call == nullptr) || (call->getName() != method) || (call->getParent() == nullptr)) continue;
      const hldb::RefObj *const prefix = call->getPrefix<hldb::RefObj>();
      if ((prefix != nullptr) && (prefix->getName() == receiver)) found.emplace_back(call);
    }
    return found;
  }

  // The class the resolved method is declared in. Sentinels are returned rather than
  // asserted so a failure names the stage that broke instead of just "not equal".
  std::string_view ownerOf(const hldb::MethodFuncCall *call) const {
    if (call == nullptr) return "<no call>";

    const hldb::TaskFunc *const tf = call->getTaskFunc();
    if (tf == nullptr) return "<unresolved>";

    const hldb::ClassDefn *const owner = any_cast<hldb::ClassDefn>(tf->getParent());
    return (owner == nullptr) ? "<no owner>" : owner->getName();
  }
};

TEST_F(SequenceMethodsTest, ModuleExists) {
  ASSERT_NE(hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

TEST_F(SequenceMethodsTest, BothSequencesExist) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getSequenceDecls(), nullptr);
  EXPECT_EQ(top->getSequenceDecls()->size(), 2u) << "s1 and s2";
}

// ---------------------------------------------------------------------------
// matched -- only legal inside a sequence expression
// ---------------------------------------------------------------------------
TEST_F(SequenceMethodsTest, MatchedBindsToSequenceDecl) {
  const std::vector<const hldb::MethodFuncCall *> calls = findCalls("s1", "matched");
  ASSERT_EQ(calls.size(), 1u) << "the fixture uses matched exactly once";
  EXPECT_TRUE(calls.front()->getIsProperty()) << "'s1.matched' is written without parentheses";
  EXPECT_EQ(ownerOf(calls.front()), std::string_view("SequenceDecl"))
      << "sequence.matched must bind (IEEE 1800-2023 Sec 16.13.6)";
}

// The same receiver shape as triggered: the sequence is the call's vpiPrefix (37.42 detail 2),
// not the head of a path the call still sits in.
TEST_F(SequenceMethodsTest, MatchedReceiverIsTheSequenceDecl) {
  const std::vector<const hldb::MethodFuncCall *> calls = findCalls("s1", "matched");
  ASSERT_EQ(calls.size(), 1u);

  const hldb::RefObj *const receiver = calls.front()->getPrefix<hldb::RefObj>();
  ASSERT_NE(receiver, nullptr) << "the receiver should be the call's vpiPrefix (IEEE 1800-2023 37.42)";
  EXPECT_EQ(receiver->getName(), std::string_view("s1"));
  EXPECT_NE(receiver->getActual<hldb::SequenceDecl>(), nullptr)
      << "receiver 's1' should resolve to the SequenceDecl";
  EXPECT_EQ(calls.front()->getParent<hldb::RefObj>(), nullptr) << "'s1.matched' should not remain a path";
}

// ---------------------------------------------------------------------------
// triggered -- legal in Boolean and wait contexts outside a sequence
// ---------------------------------------------------------------------------
TEST_F(SequenceMethodsTest, TriggeredBindsToSequenceDeclAtEverySite) {
  const std::vector<const hldb::MethodFuncCall *> calls = findCalls("s1", "triggered");
  ASSERT_EQ(calls.size(), 2u) << "the fixture uses triggered in a Boolean and in a wait";
  for (const hldb::MethodFuncCall *const call : calls) {
    EXPECT_TRUE(call->getIsProperty()) << "'s1.triggered' is written without parentheses";
    EXPECT_EQ(ownerOf(call), std::string_view("SequenceDecl"))
        << "sequence.triggered must bind (IEEE 1800-2023 Sec 16.13.6)";
  }
}

// The receiver is the sequence declaration itself, not a variable with a typespec, so
// this is the check that the binder recognizes that shape.
TEST_F(SequenceMethodsTest, ReceiverIsTheSequenceDecl) {
  const std::vector<const hldb::MethodFuncCall *> calls = findCalls("s1", "triggered");
  ASSERT_FALSE(calls.empty());

  const hldb::RefObj *const receiver = calls.front()->getPrefix<hldb::RefObj>();
  ASSERT_NE(receiver, nullptr) << "the receiver should be the call's vpiPrefix (IEEE 1800-2023 37.42)";
  EXPECT_EQ(receiver->getName(), std::string_view("s1"));
  EXPECT_NE(receiver->getActual<hldb::SequenceDecl>(), nullptr)
      << "receiver 's1' should resolve to the SequenceDecl";
}

// triggered is also a named-event method (Sec 15.5.3). The two share a name and are told
// apart only by the receiver, so this pins down that a sequence receiver reaches the
// sequence host rather than the event one.
TEST_F(SequenceMethodsTest, TriggeredOnASequenceIsNotTheNamedEventMethod) {
  const std::vector<const hldb::MethodFuncCall *> calls = findCalls("s1", "triggered");
  ASSERT_FALSE(calls.empty());
  EXPECT_NE(ownerOf(calls.front()), std::string_view("EventTypespec"))
      << "a sequence receiver must not reach the Sec 15.5.3 named-event method";
}

TEST_F(SequenceMethodsTest, CompilerReportsNoErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
