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

// Tests for delete.sv (tags: 7.10.2.3 7.10.2)
//   module top ();
//     int q[$];
//     int r;
//     initial begin
//       q.push_back(2);
//       q.push_back(3);
//       q.push_back(4);
//       $display(":assert: (%d == 3)", q.size);
//       q.delete(0);
//       $display(":assert: (%d == 2)", q.size);
//       q.delete;
//       $display(":assert: (%d == 0)", q.size);
//     end
//   endmodule
//
// IEEE 1800-2017 7.10.2.3 "Deleting queue elements": "q.delete(index)"
// removes the element at "index"; the built-in "delete()" method may also
// be called with no arguments and no parentheses ("q.delete;") to remove
// all elements (7.10.2, Table 7.1 lists "delete" as callable without an
// argument).
//
// Checked:
//   - design has module top with exactly 2 nets: "q" (unbounded
//     queue of int) and "r" (plain int, unused otherwise)
//   - net "q": ArrayTypespec vpiArrayType=queue(4), unpacked, ElemTypespec
//     -> IntTypespec (signed); range left bound Constant "$"
//     (vpiConstType=unbounded)
//   - net "r": typespec resolves directly to a signed IntTypespec
//   - the 3 "q.push_back(N)" calls are each a MethodFuncCall with vpiPrefix
//     RefObj "q" (resolved to Variable "q"), named "push_back" carrying 1
//     Constant argument (2, 3, 4)
//   - "q.delete(0)" is a MethodFuncCall named "delete" with vpiPrefix RefObj
//     "q" (resolved), carrying 1 Constant argument "0" -- this is the
//     parenthesized-call form working as intended
//   - "q.delete;" (no parens, no args) is a MethodFuncCall named "delete"
//     (vpiIsProperty, no arguments) whose vpiPrefix is RefObj "q" (resolved)
//   - the 3 "q.size" (no parens) accesses are each a MethodFuncCall "size"
//     (vpiIsProperty, no arguments) whose vpiPrefix is RefObj "q" (resolved)
//   - the initial process' Begin block has exactly 8 statements in source
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

class QueuesDeleteTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "delete.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Variable *getNetQ() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("q", top->getVariables());
  }

  static const hldb::Variable *getNetR() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("r", top->getVariables());
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

  // Verifies stmt[index] is "q.push_back(value)": MethodFuncCall "push_back" whose
  // vpiPrefix is RefObj "q" (resolved to the Variable), with 1 Constant arg.
  static void ExpectPushBack(size_t index, std::string_view value) {
    const hldb::Begin *const begin = getInitialBegin();
    ASSERT_NE(begin, nullptr);
    ASSERT_NE(begin->getStmts(), nullptr);
    ASSERT_GT(begin->getStmts()->size(), index);
    const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(begin->getStmts()->at(index));
    ASSERT_NE(call, nullptr) << "stmt[" << index << "] should be a MethodFuncCall (q.push_back(...))";
    const hldb::RefObj *const qRef = call->getPrefix<hldb::RefObj>();
    ASSERT_NE(qRef, nullptr);
    EXPECT_EQ(qRef->getName(), "q");
    EXPECT_NE(qRef->getActual<hldb::Variable>(), nullptr);

    EXPECT_EQ(call->getName(), std::string_view("push_back"));
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 1u);
    const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
    ASSERT_NE(arg0, nullptr);
    const hldb::Constant *const arg = arg0->getHighConn<hldb::Constant>();
    ASSERT_NE(arg, nullptr);
    EXPECT_EQ(arg->getDecompile(), value);
  }

  // Verifies stmt[index] is "$display(fmt, q.size)": SysFuncCall with a
  // Constant format-string arg and a "q.size" MethodFuncCall arg (vpiPrefix "q").
  static void ExpectDisplayWithQSize(size_t index, std::string_view fmt) {
    const hldb::Begin *const begin = getInitialBegin();
    ASSERT_NE(begin, nullptr);
    ASSERT_GT(begin->getStmts()->size(), index);
    const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(index));
    ASSERT_NE(disp, nullptr) << "stmt[" << index << "] should be a $display SysFuncCall";
    EXPECT_EQ(disp->getName(), "$display");
    ASSERT_NE(disp->getArguments(), nullptr);
    ASSERT_EQ(disp->getArguments()->size(), 2u);

    const hldb::NamedArgument *const arg0 = disp->getArguments()->at(0);
    ASSERT_NE(arg0, nullptr);
    const hldb::Constant *const fmtArg = arg0->getHighConn<hldb::Constant>();
    ASSERT_NE(fmtArg, nullptr);
    EXPECT_EQ(fmtArg->getValue(), fmt);

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

// --- module / nets ----

TEST_F(QueuesDeleteTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(QueuesDeleteTest, ModuleHasTwoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 2u);
}

TEST_F(QueuesDeleteTest, NetQExists) { EXPECT_NE(getNetQ(), nullptr); }

TEST_F(QueuesDeleteTest, NetRExists) { EXPECT_NE(getNetR(), nullptr); }

// --- net "q": unbounded queue "int q[$]" ----

TEST_F(QueuesDeleteTest, NetQArrayTypeIsQueue) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiQueueArray) << "7.10: 'int q[$]' must be modeled as a queue array";
}

TEST_F(QueuesDeleteTest, NetQArrayIsNotPacked) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  EXPECT_FALSE(at->getPacked()) << "a queue dimension is an unpacked dimension";
}

TEST_F(QueuesDeleteTest, NetQRangeLeftIsUnboundedDollar) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr);
  const hldb::Constant *const dollar = at->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(dollar, nullptr);
  EXPECT_EQ(dollar->getDecompile(), "$");
  EXPECT_EQ(dollar->getConstType(), vpiUnboundedConst);
}

TEST_F(QueuesDeleteTest, NetQElemTypespecIsSignedIntTypespec) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::IntTypespec *const elem = at->getElemTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "element type of 'int q[$]' should resolve to IntTypespec";
  EXPECT_TRUE(elem->getSigned());
}

TEST_F(QueuesDeleteTest, NetQHasNoInitialValue) {
  const hldb::Variable *const q = getNetQ();
  ASSERT_NE(q, nullptr);
  EXPECT_EQ(q->getValue(), nullptr);
}

// --- net "r": plain "int r;" ----

TEST_F(QueuesDeleteTest, NetRTypespecIsSignedIntTypespec) {
  const hldb::Variable *const r = getNetR();
  ASSERT_NE(r, nullptr);
  ASSERT_NE(r->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = r->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(QueuesDeleteTest, NetRHasNoInitialValue) {
  const hldb::Variable *const r = getNetR();
  ASSERT_NE(r, nullptr);
  EXPECT_EQ(r->getValue(), nullptr);
}

// --- initial process structure ----

TEST_F(QueuesDeleteTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(QueuesDeleteTest, InitialBeginHasEightStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 8u);
}

// --- q.push_back(2/3/4) ----

TEST_F(QueuesDeleteTest, FirstPushBackHasArgTwo) { ExpectPushBack(0, "2"); }
TEST_F(QueuesDeleteTest, SecondPushBackHasArgThree) { ExpectPushBack(1, "3"); }
TEST_F(QueuesDeleteTest, ThirdPushBackHasArgFour) { ExpectPushBack(2, "4"); }

// --- $display(":assert: (%d == 3)", q.size) ----

TEST_F(QueuesDeleteTest, FirstDisplayAssertsSizeThree) { ExpectDisplayWithQSize(3, ":assert: (%d == 3)"); }

// --- q.delete(0): parenthesized delete(index) IS correctly recognized ----

TEST_F(QueuesDeleteTest, DeleteWithIndexIsMethodFuncCall) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 4u);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(begin->getStmts()->at(4));
  ASSERT_NE(call, nullptr) << "'q.delete(0)' should be a MethodFuncCall";
  const hldb::RefObj *const qRef = call->getPrefix<hldb::RefObj>();
  ASSERT_NE(qRef, nullptr);
  EXPECT_EQ(qRef->getName(), "q");
  EXPECT_NE(qRef->getActual<hldb::Variable>(), nullptr);

  EXPECT_EQ(call->getName(), std::string_view("delete"));
  EXPECT_FALSE(call->getIsProperty()) << "'delete(...)' is written with parentheses";
}

TEST_F(QueuesDeleteTest, DeleteWithIndexArgumentIsConstantZero) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(begin->getStmts()->at(4));
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u) << "'delete(0)' should carry exactly the index argument";
  const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const index = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(index, nullptr);
  EXPECT_EQ(index->getDecompile(), "0");
  EXPECT_EQ(index->getConstType(), vpiIntConst);
}

// --- $display(":assert: (%d == 2)", q.size) ----

TEST_F(QueuesDeleteTest, SecondDisplayAssertsSizeTwo) { ExpectDisplayWithQSize(5, ":assert: (%d == 2)"); }

// --- q.delete; (no parens, no args): must resolve like q.delete() ----

TEST_F(QueuesDeleteTest, DeleteWithNoArgsIsMethodFuncCall) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 6u);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(begin->getStmts()->at(6));
  ASSERT_NE(call, nullptr) << "'q.delete;' should still be a MethodFuncCall";
  const hldb::RefObj *const qRef = call->getPrefix<hldb::RefObj>();
  ASSERT_NE(qRef, nullptr);
  EXPECT_EQ(qRef->getName(), "q");
  EXPECT_NE(qRef->getActual<hldb::Variable>(), nullptr);

  EXPECT_EQ(call->getName(), std::string_view("delete"));
  EXPECT_TRUE(call->getIsProperty()) << "'delete' is written without parentheses (13.5.5)";
  EXPECT_EQ(call->getArguments(), nullptr) << "delete-all takes no arguments";
}

// --- $display(":assert: (%d == 0)", q.size) ----

TEST_F(QueuesDeleteTest, ThirdDisplayAssertsSizeZero) { ExpectDisplayWithQSize(7, ":assert: (%d == 0)"); }

// --- structural completeness / design-level typespecs ----

TEST_F(QueuesDeleteTest, ModuleHasNoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

TEST_F(QueuesDeleteTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = findFirstByType<hldb::ModuleTypespec>(m_design->getTypespecs());
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(QueuesDeleteTest, DesignHasIntTypespecSigned) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = findFirstByType<hldb::IntTypespec>(m_design->getTypespecs());
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(QueuesDeleteTest, DesignHasStringTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  ASSERT_GT(m_design->getTypespecs()->size(), 2u);
  EXPECT_NE(findFirstByType<hldb::StringTypespec>(m_design->getTypespecs()), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
