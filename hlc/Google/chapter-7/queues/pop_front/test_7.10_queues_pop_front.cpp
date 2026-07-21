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

// Tests for pop_front.sv (tags: 7.10.2.4 7.10.2)
//   module top ();
//     int q[$];
//     int r;
//     initial begin
//       q.push_back(2);
//       q.push_back(3);
//       q.push_back(4);
//       r = q.pop_front;
//       $display(":assert: (%d == 2)", q.size);
//       $display(":assert: (%d == 2)", r);
//     end
//   endmodule
//
// IEEE 1800-2017 7.10.2.4 "pop_front()": removes and returns the first
// element of a queue. 7.24.4 permits calling a no-argument built-in method
// with or without parentheses, so "r = q.pop_front;" is a legal use of
// pop_front() as an expression, equivalent to "r = q.pop_front();".
//
// Checked:
//   - design has module work@top with exactly 2 nets: "q" (unbounded
//     queue of int) and "r" (plain int)
//   - net "q": ArrayTypespec vpiArrayType=queue(4), unpacked, ElemTypespec
//     -> IntTypespec (signed); range left bound Constant "$"
//     (vpiConstType=unbounded)
//   - net "r": typespec resolves directly to a signed IntTypespec
//   - the 3 "q.push_back(N)" calls are each parsed as a HierPath with a
//     RefObj "q" (resolved to Net "q") and a MethodFuncCall "push_back"
//     carrying 1 Constant argument (2, 3, 4)
//   - "r = q.pop_front" must resolve like "r = q.pop_front()" would:
//     Assignment (blocking) whose lhs is RefObj "r" (resolved) and whose
//     rhs is a HierPath with RefObj "q" (resolved) and a MethodFuncCall
//     named "pop_front" taking no arguments -- see the KNOWN BUG note
//     below
//   - "q.size" must likewise resolve like "q.size()" would (RefObj "q"
//     resolved + MethodFuncCall "size" taking no arguments) -- same gap
//   - "r" in the final $display resolves to the declared Net "r"
//   - the initial process' Begin block has exactly 6 statements in source
//     order
//   - design-level typespecs (3): ModuleTypespec, IntTypespec, StringTypespec
//
// KNOWN COMPILER BUG (not a defect in pop_front.sv):
//   IEEE 1800-2017 7.24.4 permits any no-argument built-in method to be
//   called with or without parentheses. This HLC build never resolves the
//   parenthesis-less form: instead of a MethodFuncCall, both "pop_front"
//   in "q.pop_front" and "size" in "q.size" are left as unresolved RefObj
//   path elements, and a spurious ELAB_ILLEGAL_IMPLICIT_NET ("Illegal
//   implicit net") is raised for each (same gap tracked across
//   chapter-7/queues/bounded/bounded.sv, chapter-7/queues/delete/delete.sv,
//   chapter-7/queues/delete_assign/delete_assign.sv, chapter-7/queues/
//   insert_assign/insert_assign.sv, chapter-7/queues/insert/insert.sv,
//   chapter-7/queues/max-size/max-size.sv, chapter-7/queues/
//   persistence/persistence.sv, chapter-7/queues/
//   pop_back_assing/pop_back_assing.sv, chapter-7/queues/pop_back/pop_back.sv
//   and chapter-7/queues/pop_front_assign/pop_front_assign.sv). That the
//   parenthesized form works for other no-argument built-in methods is
//   independently verified by chapter-5/5.13-builtin-methods-arrays.sv
//   ("array.size()") and chapter-7/queues/persistence/persistence.sv
//   ("q.delete()"); no test in this repo directly exercises
//   "q.pop_front()" with explicit parens, but the bug is in how the
//   parser handles omitted parens generally, not in any particular method
//   name, so the same fix is expected to cover it (confirmed already
//   generalizing to "pop_back" in chapter-7/queues/pop_back/pop_back.sv).
//   FourthStmtAssignmentRhsMustBePopFrontMethodCall,
//   FifthStmtDisplayAssertsSizeTwo, and the two error-count tests below
//   assert the IEEE-mandated behavior and will FAIL until the parser is
//   fixed.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/ErrorReporting/Location.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/hier_path.h>
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
#include <hldb/vpi_user.h>

namespace hlc {

class QueuesPopFrontTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "pop_front.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()); }

  static const hldb::Net *getNetQ() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Net>("q", top->getNets());
  }

  static const hldb::Net *getNetR() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Net>("r", top->getNets());
  }

  static const hldb::ArrayTypespec *getQArrayTypespec() {
    const hldb::Net *const q = getNetQ();
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

  // Verifies stmt[index] is "q.push_back(value)": HierPath -> RefObj "q"
  // (resolved to Net) + MethodFuncCall "push_back" with 1 Constant arg.
  static void ExpectPushBack(size_t index, std::string_view value) {
    const hldb::Begin *const begin = getInitialBegin();
    ASSERT_NE(begin, nullptr);
    ASSERT_NE(begin->getStmts(), nullptr);
    ASSERT_GT(begin->getStmts()->size(), index);
    const hldb::HierPath *const hp = any_cast<hldb::HierPath>(begin->getStmts()->at(index));
    ASSERT_NE(hp, nullptr) << "stmt[" << index << "] should be a HierPath (q.push_back(...))";
    ASSERT_NE(hp->getPathElems(), nullptr);
    ASSERT_EQ(hp->getPathElems()->size(), 2u);

    const hldb::RefObj *const qRef = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
    ASSERT_NE(qRef, nullptr);
    EXPECT_EQ(qRef->getName(), "q");
    EXPECT_NE(qRef->getActual<hldb::Net>(), nullptr);

    const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->getName(), "push_back");
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 1u);
    const hldb::Constant *const arg = any_cast<hldb::Constant>(call->getArguments()->at(0));
    ASSERT_NE(arg, nullptr);
    EXPECT_EQ(arg->getDecompile(), value);
  }
};

// --- module / nets -----------------------------------------------------------

TEST_F(QueuesPopFrontTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(QueuesPopFrontTest, ModuleHasTwoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 2u);
}

TEST_F(QueuesPopFrontTest, NetQExists) { EXPECT_NE(getNetQ(), nullptr); }

TEST_F(QueuesPopFrontTest, NetRExists) { EXPECT_NE(getNetR(), nullptr); }

// --- net "q": unbounded queue "int q[$]" ------------------------------------

TEST_F(QueuesPopFrontTest, NetQArrayTypeIsQueue) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiQueueArray) << "7.10: 'int q[$]' must be modeled as a queue array";
}

TEST_F(QueuesPopFrontTest, NetQArrayIsNotPacked) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  EXPECT_FALSE(at->getPacked()) << "a queue dimension is an unpacked dimension";
}

TEST_F(QueuesPopFrontTest, NetQRangeLeftIsUnboundedDollar) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr);
  const hldb::Constant *const dollar = at->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(dollar, nullptr);
  EXPECT_EQ(dollar->getDecompile(), "$");
  EXPECT_EQ(dollar->getConstType(), vpiUnboundedConst);
}

TEST_F(QueuesPopFrontTest, NetQElemTypespecIsSignedIntTypespec) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::IntTypespec *const elem = at->getElemTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "element type of 'int q[$]' should resolve to IntTypespec";
  EXPECT_TRUE(elem->getSigned());
}

TEST_F(QueuesPopFrontTest, NetQHasNoInitialValue) {
  const hldb::Net *const q = getNetQ();
  ASSERT_NE(q, nullptr);
  EXPECT_EQ(q->getValue(), nullptr);
}

// --- net "r": plain "int r;" -------------------------------------------------

TEST_F(QueuesPopFrontTest, NetRTypespecIsSignedIntTypespec) {
  const hldb::Net *const r = getNetR();
  ASSERT_NE(r, nullptr);
  ASSERT_NE(r->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = r->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(QueuesPopFrontTest, NetRHasNoInitialValue) {
  const hldb::Net *const r = getNetR();
  ASSERT_NE(r, nullptr);
  EXPECT_EQ(r->getValue(), nullptr);
}

// --- initial process structure ----------------------------------------------

TEST_F(QueuesPopFrontTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(QueuesPopFrontTest, InitialBeginHasSixStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 6u);
}

// --- q.push_back(2/3/4) ------------------------------------------------------

TEST_F(QueuesPopFrontTest, FirstPushBackHasArgTwo) { ExpectPushBack(0, "2"); }
TEST_F(QueuesPopFrontTest, SecondPushBackHasArgThree) { ExpectPushBack(1, "3"); }
TEST_F(QueuesPopFrontTest, ThirdPushBackHasArgFour) { ExpectPushBack(2, "4"); }

// --- r = q.pop_front; must resolve like r = q.pop_front(); -----------------

TEST_F(QueuesPopFrontTest, FourthStmtAssignmentIsBlockingWithLhsR) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_GT(begin->getStmts()->size(), 3u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(3));
  ASSERT_NE(assign, nullptr) << "'r = q.pop_front' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "r");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr);
}

TEST_F(QueuesPopFrontTest, FourthStmtAssignmentRhsMustBePopFrontMethodCall) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(3));
  ASSERT_NE(assign, nullptr);
  const hldb::HierPath *const rhs = assign->getRhs<hldb::HierPath>();
  ASSERT_NE(rhs, nullptr) << "'q.pop_front' should be a HierPath";
  ASSERT_NE(rhs->getPathElems(), nullptr);
  ASSERT_EQ(rhs->getPathElems()->size(), 2u);

  const hldb::RefObj *const qRef = any_cast<hldb::RefObj>(rhs->getPathElems()->at(0));
  ASSERT_NE(qRef, nullptr);
  EXPECT_EQ(qRef->getName(), "q");
  EXPECT_NE(qRef->getActual<hldb::Net>(), nullptr);

  // IEEE 1800-2017 7.10.2.4/7.24.4: "q.pop_front" without parens must
  // resolve exactly like "q.pop_front()" does -- a MethodFuncCall named
  // "pop_front" taking no arguments. KNOWN BUG: this build currently
  // parses "pop_front" here as an unresolved RefObj instead, so this
  // assertion FAILS until fixed. See the file-level comment above.
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(rhs->getPathElems()->at(1));
  ASSERT_NE(call, nullptr) << "'pop_front' without parens should resolve to a MethodFuncCall, not a plain RefObj";
  EXPECT_EQ(call->getName(), "pop_front");
  EXPECT_EQ(call->getArguments(), nullptr) << "pop_front() takes no arguments";
}

// --- $display(":assert: (%d == 2)", q.size) ---------------------------------

TEST_F(QueuesPopFrontTest, FifthStmtDisplayAssertsSizeTwo) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(4));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 2u);

  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 2)");

  const hldb::HierPath *const size = any_cast<hldb::HierPath>(disp->getArguments()->at(1));
  ASSERT_NE(size, nullptr);
  EXPECT_EQ(size->getName(), "q.size");
  ASSERT_NE(size->getPathElems(), nullptr);
  ASSERT_EQ(size->getPathElems()->size(), 2u);

  const hldb::RefObj *const qRef = any_cast<hldb::RefObj>(size->getPathElems()->at(0));
  ASSERT_NE(qRef, nullptr);
  EXPECT_EQ(qRef->getName(), "q");
  EXPECT_NE(qRef->getActual<hldb::Net>(), nullptr);

  // Same parenthesis-less-builtin-method gap as "pop_front" above.
  const hldb::MethodFuncCall *const sizeCall = any_cast<hldb::MethodFuncCall>(size->getPathElems()->at(1));
  ASSERT_NE(sizeCall, nullptr) << "'size' without parens should resolve to a MethodFuncCall, not a plain RefObj";
  EXPECT_EQ(sizeCall->getName(), "size");
  EXPECT_EQ(sizeCall->getArguments(), nullptr) << "size() takes no arguments";
}

// --- $display(":assert: (%d == 2)", r) --------------------------------------

TEST_F(QueuesPopFrontTest, SixthStmtDisplayAssertsREqualsTwo) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(5));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 2u);

  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 2)");

  const hldb::RefObj *const rRef = any_cast<hldb::RefObj>(disp->getArguments()->at(1));
  ASSERT_NE(rRef, nullptr);
  EXPECT_EQ(rRef->getName(), "r");
  EXPECT_NE(rRef->getActual<hldb::Net>(), nullptr);
}

// --- structural completeness / design-level typespecs -----------------------

TEST_F(QueuesPopFrontTest, ModuleHasNoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

TEST_F(QueuesPopFrontTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(QueuesPopFrontTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "work@top");
}

TEST_F(QueuesPopFrontTest, DesignHasIntTypespecSigned) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(QueuesPopFrontTest, DesignHasStringTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  ASSERT_GT(m_design->getTypespecs()->size(), 2u);
  EXPECT_NE(any_cast<hldb::StringTypespec>(m_design->getTypespecs()->at(2)), nullptr);
}

// --- compiler diagnostics: KNOWN BUG, pop_front/size wrongly flagged ------

TEST_F(QueuesPopFrontTest, CompilerReportsNoErrors) {
  // pop_front.sv is valid SystemVerilog; a correct compiler reports zero
  // errors. KNOWN BUG: this build raises 2 spurious
  // ELAB_ILLEGAL_IMPLICIT_NET errors (one for "q.pop_front", one for
  // "q.size"), so this currently FAILS. See the file-level comment above.
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(QueuesPopFrontTest, NoIllegalImplicitNetErrorsForPopFrontOrSize) {
  // KNOWN BUG: currently raises 2 ELAB_ILLEGAL_IMPLICIT_NET errors (line
  // 25, column 8 for "q.pop_front"; line 26, column 35 for "q.size").
  // This assertion encodes the spec-correct expectation (zero such
  // errors) and FAILS until the parser recognizes parenthesis-less no-arg
  // built-in method calls.
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const std::vector<Error> &errors = m_session->getErrorContainer()->getErrors();
  std::vector<Error> implicitNetErrors;
  for (const Error &err : errors) {
    if (err.getType() == ErrorDefinition::ELAB_ILLEGAL_IMPLICIT_NET) {
      implicitNetErrors.push_back(err);
    }
  }
  EXPECT_EQ(implicitNetErrors.size(), 0u);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
