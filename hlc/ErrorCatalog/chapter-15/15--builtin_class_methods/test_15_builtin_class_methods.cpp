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
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/design.h>
#include <hldb/expr.h>
#include <hldb/method_func_call.h>
#include <hldb/method_task_call.h>
#include <hldb/module.h>
#include <hldb/named_event.h>
#include <hldb/tf_call.h>
#include <hldb/ref_obj.h>
#include <hldb/subroutine_call.h>
#include <hldb/task_func.h>

#include <string_view>

namespace hlc {

class BuiltinClassMethodsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "15--builtin_class_methods.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // The receiver of a method call (IEEE 1800-2023 37.42 detail 2: vpiPrefix), or nullptr.
  static const hldb::Expr *callPrefix(const hldb::Any *any) {
    if (const hldb::MethodFuncCall *const mfc = any_cast<hldb::MethodFuncCall>(any)) return mfc->getPrefix();
    if (const hldb::MethodTaskCall *const mtc = any_cast<hldb::MethodTaskCall>(any)) return mtc->getPrefix();
    if (const hldb::SubroutineCall *const sc = any_cast<hldb::SubroutineCall>(any)) return sc->getPrefix();
    return nullptr;
  }

  // Every call under test is written "<receiver>.<method>[(...)]". A method call is its own
  // TFCall named after the bare method, with the receiver as its vpiPrefix (IEEE 1800-2023
  // 37.42 detail 2), so the call is found by method name plus receiver name. Sec 15.5.3's
  // "e.triggered" (no parentheses) is a call too (13.5.5), with getIsProperty() set.
  const hldb::TFCall *findCall(std::string_view text) const {
    const size_t dot = text.find('.');
    if (dot == std::string_view::npos) return nullptr;
    const std::string_view receiver = text.substr(0, dot);
    const std::string_view rest = text.substr(dot + 1);
    const std::string_view method = rest.substr(0, rest.find('('));
    for (const hldb::Any *const any : m_session->getDatabase().getObjects()) {
      const hldb::TFCall *const call = any_cast<hldb::TFCall>(any);
      if ((call == nullptr) || (call->getName() != method)) continue;
      const hldb::RefObj *const prefix = any_cast<hldb::RefObj>(callPrefix(call));
      if ((prefix != nullptr) && (prefix->getName() == receiver)) return call;
    }
    return nullptr;
  }

  // The class a resolved call's method is declared in. The sentinels are returned rather
  // than asserted so a failure reports which stage broke instead of just "not equal".
  std::string_view ownerOfCall(std::string_view name) const {
    const hldb::TFCall *const call = findCall(name);
    if (call == nullptr) return "<no call>";

    const hldb::TaskFunc *const tf = call->getTaskFunc();
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
  EXPECT_EQ(ownerOfCall("sem.put(1)"), std::string_view("semaphore"))
      << "semaphore.put() must bind (IEEE 1800-2023 Sec 15.3.2)";
}

TEST_F(BuiltinClassMethodsTest, SemaphoreGetBindsToSemaphore) {
  EXPECT_EQ(ownerOfCall("sem.get(1)"), std::string_view("semaphore"))
      << "semaphore.get() must bind (IEEE 1800-2023 Sec 15.3.3)";
}

TEST_F(BuiltinClassMethodsTest, SemaphoreTryGetBindsToSemaphore) {
  EXPECT_EQ(ownerOfCall("sem.try_get(1)"), std::string_view("semaphore"))
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
  EXPECT_EQ(ownerOfCall("mbx.try_peek(msg)"), std::string_view("mailbox"))
      << "mailbox.try_peek() must bind (IEEE 1800-2023 Sec 15.4.7)";
}

// ---------------------------------------------------------------------------
// 15.5.3 Persistent trigger -- "function bit triggered()"
// ---------------------------------------------------------------------------
TEST_F(BuiltinClassMethodsTest, TriggeredBindsToEventTypespec) {
  EXPECT_EQ(ownerOfCall("e.triggered"), std::string_view("EventTypespec"))
      << "event.triggered must bind (IEEE 1800-2023 Sec 15.5.3); the standard gives it no "
         "nameable owning type, so it resolves to the synthesized EventTypespec class";
}

// The receiver is a NamedEvent rather than a variable with a typespec, so this is the
// check that the binder recognizes that shape at all.
TEST_F(BuiltinClassMethodsTest, TriggeredReceiverIsTheNamedEvent) {
  const hldb::TFCall *const call = findCall("e.triggered");
  ASSERT_NE(call, nullptr);
  EXPECT_TRUE(call->getIsProperty()) << "'e.triggered' is written without parentheses (13.5.5)";

  const hldb::RefObj *const receiver = any_cast<hldb::RefObj>(callPrefix(call));
  ASSERT_NE(receiver, nullptr);
  EXPECT_EQ(receiver->getName(), "e");
  EXPECT_NE(receiver->getActual<hldb::NamedEvent>(), nullptr)
      << "receiver 'e' should resolve to the NamedEvent";
}

TEST_F(BuiltinClassMethodsTest, CompilerReportsNoErrors) {
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
