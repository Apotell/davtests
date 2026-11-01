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

// Tests for push_front.sv (tags: 7.10.2.6 7.10.2)
//   module top ();
//     int q[$];
//     initial begin
//       q.push_front(2);
//       q.push_front(3);
//       q.push_front(4);
//       $display(":assert: (%d == 3)", q.size);
//       $display(":assert: (%d == 4)", q[0]);
//     end
//   endmodule
//
// IEEE 1800-2017 7.10.2.6 "push_front(item)": inserts "item" at the front
// (index 0) of the queue, growing it by one element and shifting existing
// elements to higher indices. Pushing 2, then 3, then 4 leaves the queue
// as {4, 3, 2} -- the LAST-pushed value "4" ends up at the front
// (index 0), the opposite ordering from push_back.
//
// Checked:
//   - design has module top with exactly 1 net: "q" (unbounded
//     queue of int)
//   - net "q": ArrayTypespec vpiArrayType=queue(4), unpacked, ElemTypespec
//     -> IntTypespec (signed); range left bound Constant "$"
//     (vpiConstType=unbounded)
//   - the 3 "q.push_front(N)" calls are each a MethodFuncCall with vpiPrefix
//     RefObj "q" (resolved to Variable "q"), named "push_front" carrying 1
//     Constant argument, in source order (2, 3, 4)
//   - "q.size" is a MethodFuncCall "size" (vpiIsProperty, no arguments) whose
//     vpiPrefix is RefObj "q" (resolved)
//   - "q[0]" is a BitSelect with prefix RefObj "q" (resolved) and index
//     Constant "0" -- confirms the LAST push_front call (value 4) becomes
//     the front element, the reverse of push_back's insertion order
//   - the initial process' Begin block has exactly 5 statements in source
//     order
//   - design-level typespecs (3): ModuleTypespec, IntTypespec, StringTypespec
//
// Parenthesis-less built-in method calls (IEEE 1800-2023 13.5.5, 7.12,
// 7.10.2): a no-argument method such as "q.size" may be called without
// parentheses. Such a call is modeled like the parenthesized form -- a
// MethodFuncCall named after the method, with vpiIsProperty set, no
// arguments, and the receiver (RefObj "q") as its vpiPrefix (37.42
// detail 2) -- and must not raise CompFailedToBind.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/variable.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class QueuesPushFrontTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "push_front.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Variable *getNetQ() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("q", top->getVariables());
  }

  static const hldb::ArrayTypespec *getQArrayTypespec() {
    const hldb::Variable *const q = getNetQ();
    if (q == nullptr || q->getTypespec() == nullptr) return nullptr;
    return q->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  }

  static const hldb::Begin *getInitialBegin() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (init == nullptr) return nullptr;
    return init->getStmt<hldb::Begin>();
  }

  // Verifies stmt[index] is "q.push_front(value)": MethodFuncCall "push_front" whose
  // vpiPrefix is RefObj "q" (resolved to the Variable), with 1 Constant arg.
  static void ExpectPushFront(size_t index, std::string_view value) {
    const hldb::Begin *const begin = getInitialBegin();
    ASSERT_NE(begin, nullptr);
    ASSERT_NE(begin->getStmts(), nullptr);
    ASSERT_GT(begin->getStmts()->size(), index);
    const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(begin->getStmts()->at(index));
    ASSERT_NE(call, nullptr) << "stmt[" << index << "] should be a MethodFuncCall (q.push_front(...))";
    const hldb::RefObj *const qRef = call->getPrefix<hldb::RefObj>();
    ASSERT_NE(qRef, nullptr);
    EXPECT_EQ(qRef->getName(), "q");
    EXPECT_NE(qRef->getActual<hldb::Variable>(), nullptr);

    EXPECT_EQ(call->getName(), std::string_view("push_front"));
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 1u);
    const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
    ASSERT_NE(arg0, nullptr);
    const hldb::Constant *const arg = arg0->getHighConn<hldb::Constant>();
    ASSERT_NE(arg, nullptr);
    EXPECT_EQ(arg->getDecompile(), value);
  }

  template<typename T, typename R>
  static T* findFirstByType(const std::vector<R *> *collection) {
    if (collection == nullptr) return nullptr;
    for (R *any : *collection) {
      if (T *const t = any_cast<T>(any)) {
        return t;
      }
    }
    return nullptr;
  }  
};

// --- module / net ----

TEST_F(QueuesPushFrontTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(QueuesPushFrontTest, ModuleHasOneNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u);
}

TEST_F(QueuesPushFrontTest, NetQExists) { EXPECT_NE(getNetQ(), nullptr); }

// --- net "q": unbounded queue "int q[$]" ----

TEST_F(QueuesPushFrontTest, NetQArrayTypeIsQueue) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiQueueArray) << "7.10: 'int q[$]' must be modeled as a queue array";
}

TEST_F(QueuesPushFrontTest, NetQArrayIsNotPacked) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  EXPECT_FALSE(at->getPacked()) << "a queue dimension is an unpacked dimension";
}

TEST_F(QueuesPushFrontTest, NetQRangeLeftIsUnboundedDollar) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr);
  const hldb::Constant *const dollar = at->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(dollar, nullptr);
  EXPECT_EQ(dollar->getDecompile(), "$");
  EXPECT_EQ(dollar->getConstType(), vpiUnboundedConst);
}

TEST_F(QueuesPushFrontTest, NetQElemTypespecIsSignedIntTypespec) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::IntTypespec *const elem = at->getElemTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "element type of 'int q[$]' should resolve to IntTypespec";
  EXPECT_TRUE(elem->getSigned());
}

TEST_F(QueuesPushFrontTest, NetQHasNoInitialValue) {
  const hldb::Variable *const q = getNetQ();
  ASSERT_NE(q, nullptr);
  EXPECT_EQ(q->getValue(), nullptr);
}

// --- initial process structure ----

TEST_F(QueuesPushFrontTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(QueuesPushFrontTest, InitialBeginHasFiveStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 5u);
}

// --- q.push_front(2/3/4) ----

TEST_F(QueuesPushFrontTest, FirstPushFrontHasArgTwo) { ExpectPushFront(0, "2"); }
TEST_F(QueuesPushFrontTest, SecondPushFrontHasArgThree) { ExpectPushFront(1, "3"); }
TEST_F(QueuesPushFrontTest, ThirdPushFrontHasArgFour) { ExpectPushFront(2, "4"); }

// --- $display(":assert: (%d == 3)", q.size) ----

TEST_F(QueuesPushFrontTest, FourthStmtDisplayAssertsSizeThree) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(3));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 2u);

  const hldb::NamedArgument *const arg0 = disp->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const fmt = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 3)");

  const hldb::NamedArgument *const arg1 = disp->getArguments()->at(1);
  ASSERT_NE(arg1, nullptr);
  const hldb::MethodFuncCall *const sizeCall = arg1->getHighConn<hldb::MethodFuncCall>();
  ASSERT_NE(sizeCall, nullptr) << "'size' without parens should resolve to a MethodFuncCall, not a plain RefObj";
  const hldb::RefObj *const qRef = sizeCall->getPrefix<hldb::RefObj>();
  ASSERT_NE(qRef, nullptr);
  EXPECT_EQ(qRef->getName(), "q");
  EXPECT_NE(qRef->getActual<hldb::Variable>(), nullptr);

  EXPECT_EQ(sizeCall->getName(), std::string_view("size"));
  EXPECT_TRUE(sizeCall->getIsProperty()) << "'size' is written without parentheses (13.5.5)";
  EXPECT_EQ(sizeCall->getArguments(), nullptr) << "size() takes no arguments";
}

// --- $display(":assert: (%d == 4)", q[0]) ----

TEST_F(QueuesPushFrontTest, FifthStmtDisplayAssertsQAtZeroEqualsFour) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(4));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 2u);

  const hldb::NamedArgument *const arg0 = disp->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const fmt = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 4)");

  const hldb::NamedArgument *const arg1 = disp->getArguments()->at(1);
  ASSERT_NE(arg1, nullptr);
  const hldb::BitSelect *const sel = arg1->getHighConn<hldb::BitSelect>();
  ASSERT_NE(sel, nullptr)
      << "'q[0]' should be a BitSelect -- confirms the LAST push_front call becomes the front element";
  EXPECT_EQ(sel->getName(), "q[0]");
  const hldb::RefObj *const prefix = sel->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->getName(), "q");
  EXPECT_NE(prefix->getActual<hldb::Variable>(), nullptr);
  const hldb::Constant *const index = sel->getIndex<hldb::Constant>();
  ASSERT_NE(index, nullptr);
  EXPECT_EQ(index->getDecompile(), "0");
}

// --- structural completeness / design-level typespecs ----

TEST_F(QueuesPushFrontTest, ModuleHasNoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}


TEST_F(QueuesPushFrontTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = findFirstByType<hldb::ModuleTypespec>(m_design->getTypespecs());
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(QueuesPushFrontTest, DesignHasIntTypespecSigned) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = findFirstByType<hldb::IntTypespec>(m_design->getTypespecs());
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(QueuesPushFrontTest, DesignHasStringTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  ASSERT_GT(m_design->getTypespecs()->size(), 2u);
  EXPECT_NE(findFirstByType<hldb::StringTypespec>(m_design->getTypespecs()), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
