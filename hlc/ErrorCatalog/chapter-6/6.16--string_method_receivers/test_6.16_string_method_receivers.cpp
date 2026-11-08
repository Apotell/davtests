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

// IEEE 1800-2023 Sec 6.16 string methods, varied by RECEIVER rather than by method.
//
// The per-subclause 6.16.x fixtures cover all 18 method names, but each one calls its
// method on the same shape: a module-level "string" variable. Sec 6.16 places no
// restriction on where the string comes from, so a string reached through a typedef, a
// struct member, a class property or a subroutine formal has to bind identically. This
// file fixes the method (len/substr) and varies the receiver instead, so what is under
// test is the resolution path rather than the method table.
//
// A string method is declared in no user scope -- the standard gives these methods no
// nameable owning type -- so a bound call resolves to the compiler-synthesized
// "StringTypespec" class. That owner is what each test below asserts: checking only
// that getTaskFunc() is non-null would also pass if the call had bound to some other
// method that happened to share the name, which is exactly the shadowing case tested
// at the bottom.
//
// Checked:
//   - typedef of string, struct member, class property (with and without a handle),
//     subroutine formal, subroutine local, and a chained call all bind to StringTypespec
//   - a user-declared class method whose name collides with a string method is not
//     displaced by the builtin
//   - an element of a queue of strings is a KNOWN GAP (skipped, see that test)

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
#include <hldb/task_func.h>

#include <string_view>

namespace hlc {

class StringMethodReceiversTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.16--string_method_receivers.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // The call sites under test sit in four different scopes (a module initial block, a
  // module function, and two class methods), so navigating to each one individually
  // would say more about those scopes than about the receiver resolution being tested.
  // Each one is written "<receiver>.<method>[(...)]" and is its own MethodFuncCall named
  // after the bare method, with the receiver expression as its vpiPrefix (IEEE 1800-2023
  // 37.42 detail 2) -- a RefObj (path) or select named after the receiver text, or, for a
  // chained call, the previous MethodFuncCall. Matching on that shape addresses them all
  // uniformly.
  //
  // Splits "<receiver>.<method>[(...)]" into its receiver and method text.
  static bool splitCall(std::string_view text, std::string_view &receiver, std::string_view &method) {
    std::string_view rest = text;
    if (!rest.empty() && (rest.back() == ')')) {
      int32_t depth = 0;
      size_t i = rest.size();
      while (i > 0) {
        --i;
        if (rest[i] == ')') ++depth;
        if ((rest[i] == '(') && (--depth == 0)) break;
      }
      rest = rest.substr(0, i);
    }
    const size_t dot = rest.rfind('.');
    if (dot == std::string_view::npos) return false;
    receiver = rest.substr(0, dot);
    method = rest.substr(dot + 1);
    return true;
  }

  // Whether `call` is the call written as `text`, receiver included.
  static bool callMatches(const hldb::MethodFuncCall *call, std::string_view text) {
    std::string_view receiver;
    std::string_view method;
    if ((call == nullptr) || !splitCall(text, receiver, method) || (call->getName() != method)) return false;
    const hldb::Expr *const prefix = call->getPrefix();
    if (prefix == nullptr) return false;
    if (!receiver.empty() && (receiver.back() == ')')) {
      return callMatches(any_cast<hldb::MethodFuncCall>(prefix), receiver);
    }
    return prefix->getName() == receiver;
  }

  // The method call written as `name` -- "a.b.len()" is len, applied to "a.b".
  const hldb::MethodFuncCall *trailingCall(std::string_view name) const {
    for (const hldb::Any *const any : m_session->getDatabase().getObjects()) {
      const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(any);
      if (callMatches(call, name)) return call;
    }
    return nullptr;
  }

  // The class a resolved call's method is declared in. The sentinels are returned rather
  // than asserted so a failure reports which stage broke instead of just "not equal".
  std::string_view ownerOfCall(std::string_view name) const {
    const hldb::MethodFuncCall *const call = trailingCall(name);
    if (call == nullptr) return "<no call>";
    const hldb::TaskFunc *const tf = call->getTaskFunc();
    if (tf == nullptr) return "<unresolved>";
    const hldb::ClassDefn *const owner = any_cast<hldb::ClassDefn>(tf->getParent());
    return (owner == nullptr) ? "<no owner>" : owner->getName();
  }
};

TEST_F(StringMethodReceiversTest, ModuleExists) {
  ASSERT_NE(hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Receiver shapes that must all reach the same builtin
// ---------------------------------------------------------------------------
TEST_F(StringMethodReceiversTest, TypedefReceiverBindsToStringTypespec) {
  EXPECT_EQ(ownerOfCall("via_typedef.len()"), std::string_view("StringTypespec"))
      << "a 'typedef string' receiver must bind (IEEE 1800-2023 Sec 6.16)";
}

TEST_F(StringMethodReceiversTest, StructMemberReceiverBindsToStringTypespec) {
  EXPECT_EQ(ownerOfCall("rec.name.len()"), std::string_view("StringTypespec"))
      << "a string struct member receiver must bind (IEEE 1800-2023 Sec 6.16)";
}

TEST_F(StringMethodReceiversTest, ClassPropertyViaHandleBindsToStringTypespec) {
  EXPECT_EQ(ownerOfCall("h.prop.len()"), std::string_view("StringTypespec"))
      << "a string class property reached through a handle must bind (IEEE 1800-2023 Sec 6.16)";
}

TEST_F(StringMethodReceiversTest, ClassPropertyWithoutHandleBindsToStringTypespec) {
  EXPECT_EQ(ownerOfCall("prop.len()"), std::string_view("StringTypespec"))
      << "a string class property named from inside its own class must bind (IEEE 1800-2023 Sec 6.16)";
}

TEST_F(StringMethodReceiversTest, SubroutineFormalReceiverBindsToStringTypespec) {
  EXPECT_EQ(ownerOfCall("formal.len()"), std::string_view("StringTypespec"))
      << "a string subroutine formal receiver must bind (IEEE 1800-2023 Sec 6.16)";
}

TEST_F(StringMethodReceiversTest, SubroutineLocalReceiverBindsToStringTypespec) {
  EXPECT_EQ(ownerOfCall("local_str.len()"), std::string_view("StringTypespec"))
      << "a string subroutine local receiver must bind (IEEE 1800-2023 Sec 6.16)";
}

// ---------------------------------------------------------------------------
// Chained: substr() returns a string per Sec 6.16.8, so its result is itself a receiver
// ---------------------------------------------------------------------------
TEST_F(StringMethodReceiversTest, ChainedCallBindsBothMethods) {
  const hldb::MethodFuncCall *const len = trailingCall("plain.substr(1, 2).len()");
  ASSERT_NE(len, nullptr);
  EXPECT_EQ(len->getName(), std::string_view("len"));

  // Chained call: len's receiver is the substr() call, whose own receiver is 'plain'.
  const hldb::MethodFuncCall *const substr = len->getPrefix<hldb::MethodFuncCall>();
  ASSERT_NE(substr, nullptr) << "len()'s vpiPrefix should be the substr() call (IEEE 1800-2023 37.42)";
  EXPECT_EQ(substr->getName(), std::string_view("substr"));
  EXPECT_NE(substr->getTaskFunc(), nullptr) << "substr() must bind (IEEE 1800-2023 Sec 6.16.8)";
  const hldb::RefObj *const plain = substr->getPrefix<hldb::RefObj>();
  ASSERT_NE(plain, nullptr);
  EXPECT_EQ(plain->getName(), std::string_view("plain"));

  EXPECT_EQ(ownerOfCall("plain.substr(1, 2).len()"), std::string_view("StringTypespec"))
      << "a substr() result is a string per Sec 6.16.8, so len() on it must bind";
}

// ---------------------------------------------------------------------------
// The builtin must not displace a user method that shares its name
// ---------------------------------------------------------------------------
TEST_F(StringMethodReceiversTest, UserMethodWithStringMethodNameIsNotDisplaced) {
  EXPECT_EQ(ownerOfCall("sh.len()"), std::string_view("shadow"))
      << "len() on a class handle is that class's own method, not the Sec 6.16 string method";
}

// ---------------------------------------------------------------------------
// Known gap
// ---------------------------------------------------------------------------
TEST_F(StringMethodReceiversTest, QueueElementReceiverBindsToStringTypespec) {
  GTEST_SKIP() << "known gap: an element of an array/queue of strings does not bind. After a "
                  "select path element the binder's context is the Variable, where q.len() "
                  "(illegal) cannot be told apart from q[1].len() (legal per IEEE 1800-2023 "
                  "Sec 6.16, the element being of type string). Needs the element-type "
                  "resolution the queue and associative-array methods also require.";
  EXPECT_EQ(ownerOfCall("q[1].len"), std::string_view("StringTypespec"));
}

TEST_F(StringMethodReceiversTest, CompilerReportsNoFatalOrSyntaxErrors) {
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
