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

// Tests for bounded.sv (tags: 7.10.5 7.10 7.10.2.7 7.10.2.1)
//   module top ();
//     int q[$:2]; // 3 elements
//     initial begin
//       q.push_back(1);
//       q.push_back(2);
//       q.push_back(3);
//       $display(":assert: ((%d == 1) and (%d == 2) and (%d == 3))",
//         q[0], q[1], q[2]);
//       $display(":re: BEGIN:QUEUE_FULL"); // expect warning
//       q.push_back(4);
//       $display(":re: END");
//       $display(":assert: (%d==3)", q.size);
//     end
//   endmodule
//
// IEEE 1800-2017 7.10.2 / 7.10.2.7: "int q[$:2]" declares a bounded queue
// with maximum size 3 (bound is the highest legal index, so a size of
// bound+1 elements) whose element type is a signed 32-bit int. 7.24.4.1
// push_back() appends one element to the end of a queue.
//
// `int q[$:2]` has no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8
// it is a variable_declaration, not a net_declaration.
//
// Checked:
//   - design has module top with exactly 1 variable: "q"
//   - variable "q": ArrayTypespec vpiArrayType=queue(4), unpacked,
//     ElemTypespec -> IntTypespec (signed); range left bound is Constant "$"
//     (vpiConstType=unbounded), right bound is Constant "2"
//     (vpiConstType=unsigned int) -- i.e. the parser keeps the "$:2" bound
//     on the queue's Range rather than discarding it
//   - the 4 "q.push_back(N)" calls are each a MethodFuncCall with vpiPrefix
//     RefObj "q" (resolved to Variable "q"), named "push_back" carrying 1
//     Constant argument (1, 2, 3, 4 respectively)
//   - "q[0]"/"q[1]"/"q[2]" are BitSelects with prefix RefObj "q" (resolved
//     to Variable "q") and Constant indices 0, 1, 2
//   - "q.size" (no parens) is a MethodFuncCall named "size" (vpiIsProperty, no
//     arguments) whose vpiPrefix is RefObj "q" (resolved)
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
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class QueuesBoundedTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "bounded.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Variable *getVariableQ() {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("q", top->getVariables());
  }

  static const hldb::ArrayTypespec *getQArrayTypespec() {
    const hldb::Variable *const q = getVariableQ();
    if (q == nullptr || q->getTypespec() == nullptr) return nullptr;
    return q->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  }

  static const hldb::Begin *getInitialBegin() {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (init == nullptr) return nullptr;
    return init->getStmt<hldb::Begin>();
  }
};

// --- module / variable ----

TEST_F(QueuesBoundedTest, ModuleExists) {
  EXPECT_NE(hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

TEST_F(QueuesBoundedTest, ModuleHasOneVariable) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u);
}

TEST_F(QueuesBoundedTest, VariableQExists) { EXPECT_NE(getVariableQ(), nullptr); }

// `q` has no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8 it must
// not also appear in the module's net collection.
TEST_F(QueuesBoundedTest, VariableQIsNotDuplicatedAsNet) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  if (top->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("q", top->getNets()), nullptr)
        << "'int q[$:2]' has no net-type keyword and must not also appear as a Net";
  }
}

// --- variable "q": bounded queue "int q[$:2]" ----

TEST_F(QueuesBoundedTest, VariableQTypespecIsArrayTypespec) { EXPECT_NE(getQArrayTypespec(), nullptr); }

TEST_F(QueuesBoundedTest, VariableQArrayTypeIsQueue) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiQueueArray) << "7.10: 'int q[$:2]' must be modeled as a queue array";
}

TEST_F(QueuesBoundedTest, VariableQArrayIsNotPacked) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  EXPECT_FALSE(at->getPacked()) << "a queue dimension is an unpacked dimension";
}

TEST_F(QueuesBoundedTest, VariableQRangeLeftIsUnboundedDollar) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr);
  const hldb::Constant *const dollar = at->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(dollar, nullptr) << "range left bound is not a Constant";
  EXPECT_EQ(dollar->getDecompile(), "$");
  EXPECT_EQ(dollar->getValue(), "$");
  EXPECT_EQ(dollar->getConstType(), vpiUnboundedConst);
}

TEST_F(QueuesBoundedTest, VariableQRangeRightIsBoundConstantTwo) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr);
  const hldb::Constant *const bound = at->getRange()->getRightExpr<hldb::Constant>();
  ASSERT_NE(bound, nullptr) << "7.10.2.7: bounded queue must keep its ':2' bound as the range right expr";
  EXPECT_EQ(bound->getDecompile(), "2");
  EXPECT_EQ(bound->getValue(), "2");
  EXPECT_EQ(bound->getConstType(), vpiIntConst);
}

TEST_F(QueuesBoundedTest, VariableQElemTypespecIsSignedIntTypespec) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr) << "queue ArrayTypespec has no elemTypespec";
  const hldb::IntTypespec *const elem = at->getElemTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "element type of 'int q[$:2]' should resolve to IntTypespec";
  EXPECT_TRUE(elem->getSigned()) << "'int' is a signed 32-bit type";
}

TEST_F(QueuesBoundedTest, VariableQHasNoInitialValue) {
  const hldb::Variable *const q = getVariableQ();
  ASSERT_NE(q, nullptr);
  EXPECT_EQ(q->getValue(), nullptr) << "'int q[$:2]' has no initializer";
}

// --- initial process structure ----

TEST_F(QueuesBoundedTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(QueuesBoundedTest, InitialBeginHasEightStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 8u);
}

// --- q.push_back(1/2/3/4): MethodFuncCall with vpiPrefix "q" ----

TEST_F(QueuesBoundedTest, FirstPushBackCallsPushBackWithArgOne) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(begin->getStmts()->at(0));
  ASSERT_NE(call, nullptr) << "'q.push_back(1)' should be a MethodFuncCall";
  const hldb::RefObj *const qRef = call->getPrefix<hldb::RefObj>();
  ASSERT_NE(qRef, nullptr);
  EXPECT_EQ(qRef->getName(), "q");
  EXPECT_NE(qRef->getActual<hldb::Variable>(), nullptr) << "'q' should resolve to the declared Variable";

  EXPECT_EQ(call->getName(), std::string_view("push_back"));
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const arg = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getDecompile(), "1");
}

TEST_F(QueuesBoundedTest, SecondPushBackCallsPushBackWithArgTwo) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(begin->getStmts()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), std::string_view("push_back"));
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const arg = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getDecompile(), "2");
}

TEST_F(QueuesBoundedTest, ThirdPushBackCallsPushBackWithArgThree) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(begin->getStmts()->at(2));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), std::string_view("push_back"));
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const arg = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getDecompile(), "3");
}

// --- $display(":assert: ((%d==1) and (%d==2) and (%d==3))", q[0], q[1], q[2]) ---

TEST_F(QueuesBoundedTest, FirstDisplayFormatStringIsThreeElemAssert) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(3));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 4u);
  const hldb::NamedArgument *const arg0 = disp->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const fmt = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: ((%d == 1) and (%d == 2) and (%d == 3))");
}

TEST_F(QueuesBoundedTest, FirstDisplayArgsAreQBitSelectsZeroOneTwo) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(3));
  ASSERT_NE(disp, nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 4u);

  for (uint32_t i = 0; i < 3u; ++i) {
    const hldb::NamedArgument *const selArg = disp->getArguments()->at(i + 1);
    ASSERT_NE(selArg, nullptr);
    const hldb::BitSelect *const sel = selArg->getHighConn<hldb::BitSelect>();
    ASSERT_NE(sel, nullptr) << "argument " << (i + 1) << " should be a BitSelect";
    const hldb::RefObj *const prefix = sel->getPrefix<hldb::RefObj>();
    ASSERT_NE(prefix, nullptr);
    EXPECT_EQ(prefix->getName(), "q");
    EXPECT_NE(prefix->getActual<hldb::Variable>(), nullptr);
    const hldb::Constant *const index = sel->getIndex<hldb::Constant>();
    ASSERT_NE(index, nullptr);
    EXPECT_EQ(index->getDecompile(), std::to_string(i));
  }
}

// --- $display(":re: BEGIN:QUEUE_FULL") / push_back(4) / $display(":re: END") ---

TEST_F(QueuesBoundedTest, SecondDisplayIsQueueFullMarker) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(4));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);
  const hldb::NamedArgument *const arg0 = disp->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const fmt = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":re: BEGIN:QUEUE_FULL");
}

TEST_F(QueuesBoundedTest, FourthPushBackCallsPushBackWithArgFour) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(begin->getStmts()->at(5));
  ASSERT_NE(call, nullptr) << "'q.push_back(4)' should be a MethodFuncCall even though the queue is already full";
  EXPECT_EQ(call->getName(), std::string_view("push_back"));
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::NamedArgument *const arg0 = call->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const arg = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getDecompile(), "4");
}

TEST_F(QueuesBoundedTest, ThirdDisplayIsEndMarker) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(6));
  ASSERT_NE(disp, nullptr);
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);
  const hldb::NamedArgument *const arg0 = disp->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const fmt = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":re: END");
}

// --- $display(":assert: (%d==3)", q.size) ----

TEST_F(QueuesBoundedTest, FourthDisplayFormatStringIsSizeAssert) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(7));
  ASSERT_NE(disp, nullptr);
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 2u);
  const hldb::NamedArgument *const arg0 = disp->getArguments()->at(0);
  ASSERT_NE(arg0, nullptr);
  const hldb::Constant *const fmt = arg0->getHighConn<hldb::Constant>();
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (%d==3)");
}

TEST_F(QueuesBoundedTest, FourthDisplaySecondArgIsQDotSize) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(7));
  ASSERT_NE(disp, nullptr);
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

// --- structural completeness / design-level typespecs ----

TEST_F(QueuesBoundedTest, ModuleHasNoContAssigns) {
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

TEST_F(QueuesBoundedTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = hldb::findByDefName<hldb::ModuleTypespec>("top", m_design->getTypespecs());
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(QueuesBoundedTest, DesignHasIntTypespecSigned) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *it = nullptr;
  for (hldb::Typespec *ts : *m_design->getTypespecs()) {
    if ((it = any_cast<hldb::IntTypespec>(ts))) {
      if (it->getSigned()) break;
    }
  }
  ASSERT_NE(it, nullptr);
}

TEST_F(QueuesBoundedTest, DesignHasStringTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  ASSERT_GT(m_design->getTypespecs()->size(), 2u);
  const hldb::StringTypespec *st = nullptr;
  for (hldb::Typespec *ts : *m_design->getTypespecs()) {
    if ((st = any_cast<hldb::StringTypespec>(ts))) {
      break;
    }
  }
  EXPECT_NE(st, nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
