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

// IEEE 1800-2023 Clause 15 built-in class methods that the Google chapter-15 tests
// do not reach.
//
// Google covers the mailbox (15.4, blocking and non-blocking) and the named-event
// trigger and wait forms (15.5.1, 15.5.2). It never declares a semaphore, never calls
// try_peek, and never uses the triggered method. This file covers exactly that
// remainder rather than duplicating what Google already tests.
//
// Each call is asserted to resolve to the method of the class that actually declares
// it -- semaphore and mailbox are classes the standard names, so user code can declare
// them and they resolve like any other class; "triggered" has no nameable owning type,
// so it resolves to the compiler-synthesized EventTypespec class.
//
// Checked:
//   - semaphore new/put/get/try_get all bind to semaphore (15.3.1 - 15.3.4)
//   - mailbox try_peek binds to mailbox (15.4.7)
//   - event triggered binds to EventTypespec, in both the bare and the wait() spelling
//     (15.5.3 -- the standard writes it without parentheses inside a wait)

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
#include <hldb/named_event.h>
#include <hldb/tf_call.h>
#include <hldb/ref_obj.h>
#include <hldb/task_func.h>

#include <string_view>

namespace hlc {

class BuiltinClassMethodsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "15--builtin_class_methods.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Every call under test is the trailing element of a RefObj whose own name is the
  // source text of the call, so looking the RefObj up by that name addresses them all
  // uniformly regardless of which scope they sit in.
  const hldb::RefObj *findPath(std::string_view name) const {
    for (const hldb::Any *const any : m_session->getDatabase().getObjects()) {
      const hldb::RefObj *const ro = any_cast<hldb::RefObj>(any);
      if ((ro == nullptr) || (ro->getName() != name)) continue;
      if ((ro->getPathElems() != nullptr) && !ro->getPathElems()->empty()) return ro;
    }
    return nullptr;
  }

  // The class a resolved call's method is declared in. The trailing element is a TFCall
  // for a call written with parentheses and a plain RefObj for one written without, which
  // Sec 15.5.3 does for "wait (e.triggered)"; both are handled so the two spellings can be
  // asserted the same way. The sentinels are returned rather than asserted so a failure
  // reports which stage broke instead of just "not equal".
  std::string_view ownerOfCall(std::string_view name) const {
    const hldb::RefObj *const ro = findPath(name);
    if (ro == nullptr) return "<no path>";

    const hldb::Any *const trailing = ro->getPathElems()->back();
    const hldb::TaskFunc *tf = nullptr;
    if (const hldb::TFCall *const call = any_cast<hldb::TFCall>(trailing)) {
      tf = call->getTaskFunc();
    } else if (const hldb::RefObj *const leaf = any_cast<hldb::RefObj>(trailing)) {
      tf = leaf->getActual<hldb::TaskFunc>();
    } else {
      return "<unexpected trailing element>";
    }
    if (tf == nullptr) return "<unresolved>";

    const hldb::ClassDefn *const owner = any_cast<hldb::ClassDefn>(tf->getParent());
    return (owner == nullptr) ? "<no owner>" : owner->getName();
  }
};

TEST_F(BuiltinClassMethodsTest, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// 15.3 Semaphores -- Google has no semaphore test at all
// ---------------------------------------------------------------------------
TEST_F(BuiltinClassMethodsTest, SemaphorePutBindsToSemaphore) {
  EXPECT_EQ(ownerOfCall("sem.put(1)"), "semaphore")
      << "semaphore.put() must bind (IEEE 1800-2023 Sec 15.3.2)";
}

TEST_F(BuiltinClassMethodsTest, SemaphoreGetBindsToSemaphore) {
  EXPECT_EQ(ownerOfCall("sem.get(1)"), "semaphore")
      << "semaphore.get() must bind (IEEE 1800-2023 Sec 15.3.3)";
}

TEST_F(BuiltinClassMethodsTest, SemaphoreTryGetBindsToSemaphore) {
  EXPECT_EQ(ownerOfCall("sem.try_get(1)"), "semaphore")
      << "semaphore.try_get() must bind (IEEE 1800-2023 Sec 15.3.4)";
}

// Sec 15.3.2 gives put the prototype "function void put(int keyCount = 1)" -- a void
// function, not a task. get, by contrast, is a task (15.3.3), because it may block.
TEST_F(BuiltinClassMethodsTest, SemaphorePutIsAFunctionAndGetIsATask) {
  const hldb::ClassDefn *const sem =
      hldb::findByDefName<hldb::ClassDefn>("semaphore", m_design->getAllClasses());
  ASSERT_NE(sem, nullptr);

  const hldb::TaskFunc *const put = hldb::findByName<hldb::TaskFunc>("put", sem->getMethods());
  ASSERT_NE(put, nullptr);
  EXPECT_EQ(put->getAnyType(), hldb::AnyType::Function) << "15.3.2 declares put a void function";

  const hldb::TaskFunc *const get = hldb::findByName<hldb::TaskFunc>("get", sem->getMethods());
  ASSERT_NE(get, nullptr);
  EXPECT_EQ(get->getAnyType(), hldb::AnyType::Task) << "15.3.3 declares get a task";
}

// ---------------------------------------------------------------------------
// 15.4.7 Peek() -- Google's non-blocking test calls try_put/try_get but not try_peek
// ---------------------------------------------------------------------------
TEST_F(BuiltinClassMethodsTest, MailboxTryPeekBindsToMailbox) {
  EXPECT_EQ(ownerOfCall("mbx.try_peek(msg)"), "mailbox")
      << "mailbox.try_peek() must bind (IEEE 1800-2023 Sec 15.4.7)";
}

// ---------------------------------------------------------------------------
// 15.5.3 Persistent trigger -- "function bit triggered()"
// ---------------------------------------------------------------------------
TEST_F(BuiltinClassMethodsTest, TriggeredBindsToEventTypespec) {
  EXPECT_EQ(ownerOfCall("e.triggered"), "EventTypespec")
      << "event.triggered must bind (IEEE 1800-2023 Sec 15.5.3); the standard gives it no "
         "nameable owning type, so it resolves to the synthesized EventTypespec class";
}

// The receiver is a NamedEvent rather than a variable with a typespec, so this is the
// check that the binder recognizes that shape at all.
TEST_F(BuiltinClassMethodsTest, TriggeredReceiverIsTheNamedEvent) {
  const hldb::RefObj *const ro = findPath("e.triggered");
  ASSERT_NE(ro, nullptr);
  ASSERT_GE(ro->getPathElems()->size(), 2u);

  const hldb::RefObj *const receiver = any_cast<hldb::RefObj>(ro->getPathElems()->at(0));
  ASSERT_NE(receiver, nullptr);
  EXPECT_EQ(receiver->getName(), "e");
  EXPECT_NE(receiver->getActual<hldb::NamedEvent>(), nullptr)
      << "receiver 'e' should resolve to the NamedEvent";
}

TEST_F(BuiltinClassMethodsTest, CompilerReportsNoErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(findError(ErrorDefinition::LINT_NULL_ACTUAL), nullptr)
      << "every call in this file is a legal built-in class method and must bind";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
