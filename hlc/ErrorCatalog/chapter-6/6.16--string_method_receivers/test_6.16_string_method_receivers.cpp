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
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
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
  // Each one is the trailing element of a RefObj whose own name is the source text of
  // the call, so looking the RefObj up by that name addresses them all uniformly.
  const hldb::RefObj *findPath(std::string_view name) const {
    for (const hldb::Any *const any : m_session->getDatabase().getObjects()) {
      const hldb::RefObj *const ro = any_cast<hldb::RefObj>(any);
      if ((ro == nullptr) || (ro->getName() != name)) continue;
      if ((ro->getPathElems() != nullptr) && !ro->getPathElems()->empty()) return ro;
    }
    return nullptr;
  }

  // The method call a path ends in -- "a.b.len" ends in len.
  const hldb::MethodFuncCall *trailingCall(std::string_view name) const {
    const hldb::RefObj *const ro = findPath(name);
    if (ro == nullptr) return nullptr;
    return any_cast<hldb::MethodFuncCall>(ro->getPathElems()->back());
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
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Receiver shapes that must all reach the same builtin
// ---------------------------------------------------------------------------
TEST_F(StringMethodReceiversTest, TypedefReceiverBindsToStringTypespec) {
  EXPECT_EQ(ownerOfCall("via_typedef.len"), "StringTypespec")
      << "a 'typedef string' receiver must bind (IEEE 1800-2023 Sec 6.16)";
}

TEST_F(StringMethodReceiversTest, StructMemberReceiverBindsToStringTypespec) {
  EXPECT_EQ(ownerOfCall("rec.name.len"), "StringTypespec")
      << "a string struct member receiver must bind (IEEE 1800-2023 Sec 6.16)";
}

TEST_F(StringMethodReceiversTest, ClassPropertyViaHandleBindsToStringTypespec) {
  EXPECT_EQ(ownerOfCall("h.prop.len"), "StringTypespec")
      << "a string class property reached through a handle must bind (IEEE 1800-2023 Sec 6.16)";
}

TEST_F(StringMethodReceiversTest, ClassPropertyWithoutHandleBindsToStringTypespec) {
  EXPECT_EQ(ownerOfCall("prop.len"), "StringTypespec")
      << "a string class property named from inside its own class must bind (IEEE 1800-2023 Sec 6.16)";
}

TEST_F(StringMethodReceiversTest, SubroutineFormalReceiverBindsToStringTypespec) {
  EXPECT_EQ(ownerOfCall("formal.len"), "StringTypespec")
      << "a string subroutine formal receiver must bind (IEEE 1800-2023 Sec 6.16)";
}

TEST_F(StringMethodReceiversTest, SubroutineLocalReceiverBindsToStringTypespec) {
  EXPECT_EQ(ownerOfCall("local_str.len"), "StringTypespec")
      << "a string subroutine local receiver must bind (IEEE 1800-2023 Sec 6.16)";
}

// ---------------------------------------------------------------------------
// Chained: substr() returns a string per Sec 6.16.8, so its result is itself a receiver
// ---------------------------------------------------------------------------
TEST_F(StringMethodReceiversTest, ChainedCallBindsBothMethods) {
  const hldb::RefObj *const ro = findPath("plain.substr(1, 2).len");
  ASSERT_NE(ro, nullptr);
  ASSERT_EQ(ro->getPathElems()->size(), 3u) << "expected receiver, substr, len";

  const hldb::MethodFuncCall *const substr = any_cast<hldb::MethodFuncCall>(ro->getPathElems()->at(1));
  ASSERT_NE(substr, nullptr);
  EXPECT_EQ(substr->getName(), "substr");
  EXPECT_NE(substr->getTaskFunc(), nullptr) << "substr() must bind (IEEE 1800-2023 Sec 6.16.8)";

  EXPECT_EQ(ownerOfCall("plain.substr(1, 2).len"), "StringTypespec")
      << "a substr() result is a string per Sec 6.16.8, so len() on it must bind";
}

// ---------------------------------------------------------------------------
// The builtin must not displace a user method that shares its name
// ---------------------------------------------------------------------------
TEST_F(StringMethodReceiversTest, UserMethodWithStringMethodNameIsNotDisplaced) {
  EXPECT_EQ(ownerOfCall("sh.len"), "shadow")
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
  EXPECT_EQ(ownerOfCall("q[1].len"), "StringTypespec");
  EXPECT_EQ(findError(ErrorDefinition::LINT_NULL_ACTUAL), nullptr);
}

TEST_F(StringMethodReceiversTest, CompilerReportsNoFatalOrSyntaxErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
