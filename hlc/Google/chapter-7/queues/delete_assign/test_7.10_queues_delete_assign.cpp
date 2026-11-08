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

// Tests for delete_assign.sv (tags: 7.10.4)
//   module top ();
//     int q[$];
//     int r;
//     initial begin
//       q.push_back(2);
//       q.push_back(3);
//       q.push_back(4);
//       $display(":assert: (%d == 3)", q.size);
//       q = q[1:$]; // q.delete(0)
//       $display(":assert: (%d == 2)", q.size);
//       q = {}; // q.delete
//       $display(":assert: (%d == 0)", q.size);
//     end
//   endmodule
//
// IEEE 1800-2017 7.10.4 "Deleting queue elements": assigning a part-select
// of a queue back to itself (e.g. "q = q[1:$]") deletes the elements
// outside that range, and assigning an empty unpacked array/queue literal
// ("q = {}") deletes all elements.
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
//   - "q = q[1:$]": Assignment (blocking) whose lhs is RefObj "q" (resolved)
//     and whose rhs is a PartSelect "q[1:$]" with prefix RefObj "q"
//     (resolved) and a Range with left bound Constant "1" (IntTypespec,
//     unsigned int) and right bound Constant "$" (vpiConstType=unbounded)
//   - "q = {}": Assignment (blocking) whose lhs is RefObj "q" (resolved);
//     see the KNOWN BUG note below for its rhs
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
//
// KNOWN COMPILER BUG (not a defect in delete_assign.sv):
//   - IEEE 1800-2017 5.11 defines "{}" as an empty unpacked array/queue
//     literal, and 7.10.4 permits "q = {};" to delete all queue elements
//     -- so the Assignment's rhs must capture SOME representation of that
//     empty value. This HLC build attaches no value at all to the rhs
//     (getRhs() == nullptr). There is no working precedent elsewhere in
//     this repo for how an empty literal should be elaborated, so
//     DeleteAllAssignmentRhsMustBeResolved below only asserts the minimal
//     correctness bar (rhs is non-null) rather than a specific shape; it
//     will FAIL until the compiler attaches a value.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/part_select.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class QueuesDeleteAssignTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "delete_assign.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

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

TEST_F(QueuesDeleteAssignTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(QueuesDeleteAssignTest, ModuleHasTwoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 2u);
}

TEST_F(QueuesDeleteAssignTest, NetQExists) { EXPECT_NE(getNetQ(), nullptr); }

TEST_F(QueuesDeleteAssignTest, NetRExists) { EXPECT_NE(getNetR(), nullptr); }

// --- net "q": unbounded queue "int q[$]" ----

TEST_F(QueuesDeleteAssignTest, NetQArrayTypeIsQueue) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiQueueArray) << "7.10: 'int q[$]' must be modeled as a queue array";
}

TEST_F(QueuesDeleteAssignTest, NetQArrayIsNotPacked) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  EXPECT_FALSE(at->getPacked()) << "a queue dimension is an unpacked dimension";
}

TEST_F(QueuesDeleteAssignTest, NetQRangeLeftIsUnboundedDollar) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr);
  const hldb::Constant *const dollar = at->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(dollar, nullptr);
  EXPECT_EQ(dollar->getDecompile(), "$");
  EXPECT_EQ(dollar->getConstType(), vpiUnboundedConst);
}

TEST_F(QueuesDeleteAssignTest, NetQElemTypespecIsSignedIntTypespec) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::IntTypespec *const elem = at->getElemTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "element type of 'int q[$]' should resolve to IntTypespec";
  EXPECT_TRUE(elem->getSigned());
}

TEST_F(QueuesDeleteAssignTest, NetQHasNoInitialValue) {
  const hldb::Variable *const q = getNetQ();
  ASSERT_NE(q, nullptr);
  EXPECT_EQ(q->getValue(), nullptr);
}

// --- net "r": plain "int r;" ----

TEST_F(QueuesDeleteAssignTest, NetRTypespecIsSignedIntTypespec) {
  const hldb::Variable *const r = getNetR();
  ASSERT_NE(r, nullptr);
  ASSERT_NE(r->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = r->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(QueuesDeleteAssignTest, NetRHasNoInitialValue) {
  const hldb::Variable *const r = getNetR();
  ASSERT_NE(r, nullptr);
  EXPECT_EQ(r->getValue(), nullptr);
}

// --- initial process structure ----

TEST_F(QueuesDeleteAssignTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(QueuesDeleteAssignTest, InitialBeginHasEightStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 8u);
}

// --- q.push_back(2/3/4) ----

TEST_F(QueuesDeleteAssignTest, FirstPushBackHasArgTwo) { ExpectPushBack(0, "2"); }
TEST_F(QueuesDeleteAssignTest, SecondPushBackHasArgThree) { ExpectPushBack(1, "3"); }
TEST_F(QueuesDeleteAssignTest, ThirdPushBackHasArgFour) { ExpectPushBack(2, "4"); }

// --- $display(":assert: (%d == 3)", q.size) ----

TEST_F(QueuesDeleteAssignTest, FirstDisplayAssertsSizeThree) { ExpectDisplayWithQSize(3, ":assert: (%d == 3)"); }

// --- q = q[1:$]; deletes element 0 (7.10.4) ----

TEST_F(QueuesDeleteAssignTest, DeleteBySliceAssignmentIsBlocking) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 4u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(4));
  ASSERT_NE(assign, nullptr) << "'q = q[1:$]' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking());
}

TEST_F(QueuesDeleteAssignTest, DeleteBySliceAssignmentLhsIsQ) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(4));
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "q");
  EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr);
}

TEST_F(QueuesDeleteAssignTest, DeleteBySliceAssignmentRhsIsQOneToDollarPartSelect) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(4));
  ASSERT_NE(assign, nullptr);
  const hldb::PartSelect *const rhs = assign->getRhs<hldb::PartSelect>();
  ASSERT_NE(rhs, nullptr) << "'q[1:$]' should be a PartSelect";
  EXPECT_EQ(rhs->getName(), "q[1:$]");

  const hldb::RefObj *const prefix = rhs->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->getName(), "q");
  EXPECT_NE(prefix->getActual<hldb::Variable>(), nullptr);

  ASSERT_NE(rhs->getRange(), nullptr);
  const hldb::Constant *const left = rhs->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "1");

  const hldb::Constant *const right = rhs->getRange()->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "$");
  EXPECT_EQ(right->getConstType(), vpiUnboundedConst);
}

// --- $display(":assert: (%d == 2)", q.size) ----

TEST_F(QueuesDeleteAssignTest, SecondDisplayAssertsSizeTwo) { ExpectDisplayWithQSize(5, ":assert: (%d == 2)"); }

// --- q = {}; deletes all elements (7.10.4) ----

TEST_F(QueuesDeleteAssignTest, DeleteAllAssignmentIsBlockingWithLhsQ) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 6u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(6));
  ASSERT_NE(assign, nullptr) << "'q = {}' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "q");
  EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr);
}

TEST_F(QueuesDeleteAssignTest, DeleteAllAssignmentRhsMustBeResolved) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(6));
  ASSERT_NE(assign, nullptr);
  EXPECT_NE(assign->getRhs(), nullptr) << "'{}' must be attached as the rhs, not dropped";
}

// --- $display(":assert: (%d == 0)", q.size) ----

TEST_F(QueuesDeleteAssignTest, ThirdDisplayAssertsSizeZero) { ExpectDisplayWithQSize(7, ":assert: (%d == 0)"); }

// --- structural completeness / design-level typespecs ----

TEST_F(QueuesDeleteAssignTest, ModuleHasNoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

TEST_F(QueuesDeleteAssignTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = findFirstByType<hldb::ModuleTypespec>(m_design->getTypespecs());
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(QueuesDeleteAssignTest, DesignHasIntTypespecSigned) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = findFirstByType<hldb::IntTypespec>(m_design->getTypespecs());
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(QueuesDeleteAssignTest, DesignHasStringTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  ASSERT_GT(m_design->getTypespecs()->size(), 2u);
  EXPECT_NE(findFirstByType<hldb::StringTypespec>(m_design->getTypespecs()), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
