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
// Checked:
//   - design has module work@top with exactly 1 net: "q"
//   - net "q": ArrayTypespec vpiArrayType=queue(4), unpacked, ElemTypespec
//     -> IntTypespec (signed); range left bound is Constant "$"
//     (vpiConstType=unbounded), right bound is Constant "2"
//     (vpiConstType=unsigned int) -- i.e. the parser keeps the "$:2" bound
//     on the queue's Range rather than discarding it
//   - the 4 "q.push_back(N)" calls are each parsed as a HierPath with 2
//     path elements: RefObj "q" (resolved to Net "q") and a MethodFuncCall
//     named "push_back" carrying 1 Constant argument (1, 2, 3, 4
//     respectively)
//   - "q[0]"/"q[1]"/"q[2]" are BitSelects with prefix RefObj "q" (resolved
//     to Net "q") and Constant indices 0, 1, 2
//   - "q.size" (no parens) must be parsed as a HierPath with 2 path
//     elements: RefObj "q" (resolved) and a MethodFuncCall named "size"
//     taking no arguments -- see the KNOWN BUG note below
//   - the initial process' Begin block has exactly 8 statements in source
//     order
//   - design-level typespecs (3): ModuleTypespec, IntTypespec, StringTypespec
//
// KNOWN COMPILER BUG (not a defect in bounded.sv):
//   IEEE 1800-2017 7.24.4 explicitly permits the built-in ".size" method to
//   be called with or without parentheses when it takes no arguments. This
//   HLC build never resolves the parenthesis-less form: instead of a
//   MethodFuncCall, "size" in "q.size" is left as an unresolved RefObj, and
//   the compiler raises a spurious ELAB_ILLEGAL_IMPLICIT_NET ("Illegal
//   implicit net") error for it. That the parenthesized form works
//   correctly is independently verified by
//   chapter-5/5.13-builtin-methods-arrays.sv ("array.size()") and
//   chapter-7/queues/persistence/persistence.sv ("q.delete()"): both
//   resolve to a MethodFuncCall with getArguments() == nullptr, which is
//   exactly the shape the tests below require for the parenthesis-less
//   form. The FourthDisplaySecondArgIsQDotSize test and the two error-count
//   tests below assert this IEEE-mandated behavior and will FAIL until the
//   parser is fixed to recognize parenthesis-less no-arg built-in method
//   calls; they are intentionally red, not intentionally tolerant, of this
//   bug (same underlying gap tracked for chapter-7/arrays/associative/
//   locator-methods/find/find.sv).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/ErrorReporting/Location.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
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

class QueuesBoundedTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "bounded.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Net *getNetQ() {
    const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Net>("q", top->getNets());
  }

  static const hldb::ArrayTypespec *getQArrayTypespec() {
    const hldb::Net *const q = getNetQ();
    if (q == nullptr || q->getTypespec() == nullptr) return nullptr;
    return q->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  }

  static const hldb::Begin *getInitialBegin() {
    const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (init == nullptr) return nullptr;
    return init->getStmt<hldb::Begin>();
  }
};

// --- module / net --------------------------------------------------------

TEST_F(QueuesBoundedTest, ModuleExists) {
  EXPECT_NE(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr);
}

TEST_F(QueuesBoundedTest, ModuleHasOneNet) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(QueuesBoundedTest, NetQExists) { EXPECT_NE(getNetQ(), nullptr); }

// --- net "q": bounded queue "int q[$:2]" ----------------------------------

TEST_F(QueuesBoundedTest, NetQTypespecIsArrayTypespec) { EXPECT_NE(getQArrayTypespec(), nullptr); }

TEST_F(QueuesBoundedTest, NetQArrayTypeIsQueue) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiQueueArray) << "7.10: 'int q[$:2]' must be modeled as a queue array";
}

TEST_F(QueuesBoundedTest, NetQArrayIsNotPacked) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  EXPECT_FALSE(at->getPacked()) << "a queue dimension is an unpacked dimension";
}

TEST_F(QueuesBoundedTest, NetQRangeLeftIsUnboundedDollar) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr);
  const hldb::Constant *const dollar = at->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(dollar, nullptr) << "range left bound is not a Constant";
  EXPECT_EQ(dollar->getDecompile(), "$");
  EXPECT_EQ(dollar->getValue(), "$");
  EXPECT_EQ(dollar->getConstType(), vpiUnboundedConst);
}

TEST_F(QueuesBoundedTest, NetQRangeRightIsBoundConstantTwo) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr);
  const hldb::Constant *const bound = at->getRange()->getRightExpr<hldb::Constant>();
  ASSERT_NE(bound, nullptr) << "7.10.2.7: bounded queue must keep its ':2' bound as the range right expr";
  EXPECT_EQ(bound->getDecompile(), "2");
  EXPECT_EQ(bound->getValue(), "2");
  EXPECT_EQ(bound->getConstType(), vpiUIntConst);
}

TEST_F(QueuesBoundedTest, NetQElemTypespecIsSignedIntTypespec) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr) << "queue ArrayTypespec has no elemTypespec";
  const hldb::IntTypespec *const elem = at->getElemTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "element type of 'int q[$:2]' should resolve to IntTypespec";
  EXPECT_TRUE(elem->getSigned()) << "'int' is a signed 32-bit type";
}

TEST_F(QueuesBoundedTest, NetQHasNoInitialValue) {
  const hldb::Net *const q = getNetQ();
  ASSERT_NE(q, nullptr);
  EXPECT_EQ(q->getValue(), nullptr) << "'int q[$:2]' has no initializer";
}

// --- initial process structure --------------------------------------------

TEST_F(QueuesBoundedTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
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

// --- q.push_back(1/2/3/4) parsed as HierPath + MethodFuncCall -------------

TEST_F(QueuesBoundedTest, FirstPushBackCallsPushBackWithArgOne) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>(begin->getStmts()->at(0));
  ASSERT_NE(hp, nullptr) << "'q.push_back(1)' should be a HierPath";
  ASSERT_NE(hp->getPathElems(), nullptr);
  ASSERT_EQ(hp->getPathElems()->size(), 2u);

  const hldb::RefObj *const qRef = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(qRef, nullptr);
  EXPECT_EQ(qRef->getName(), "q");
  EXPECT_NE(qRef->getActual<hldb::Net>(), nullptr) << "'q' should resolve to the declared Net";

  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr) << "'push_back' should be a MethodFuncCall";
  EXPECT_EQ(call->getName(), "push_back");
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::Constant *const arg = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getDecompile(), "1");
}

TEST_F(QueuesBoundedTest, SecondPushBackCallsPushBackWithArgTwo) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>(begin->getStmts()->at(1));
  ASSERT_NE(hp, nullptr);
  ASSERT_EQ(hp->getPathElems()->size(), 2u);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "push_back");
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::Constant *const arg = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getDecompile(), "2");
}

TEST_F(QueuesBoundedTest, ThirdPushBackCallsPushBackWithArgThree) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>(begin->getStmts()->at(2));
  ASSERT_NE(hp, nullptr);
  ASSERT_EQ(hp->getPathElems()->size(), 2u);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "push_back");
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::Constant *const arg = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getDecompile(), "3");
}

// --- $display(":assert: ((%d==1) and (%d==2) and (%d==3))", q[0], q[1], q[2]) ---

TEST_F(QueuesBoundedTest, FirstDisplayFormatStringIsThreeElemAssert) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(begin->getStmts()->at(3));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 4u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: ((%d == 1) and (%d == 2) and (%d == 3))");
}

TEST_F(QueuesBoundedTest, FirstDisplayArgsAreQBitSelectsZeroOneTwo) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(begin->getStmts()->at(3));
  ASSERT_NE(disp, nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 4u);

  for (uint32_t i = 0; i < 3u; ++i) {
    const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(disp->getArguments()->at(i + 1));
    ASSERT_NE(sel, nullptr) << "argument " << (i + 1) << " should be a BitSelect";
    const hldb::RefObj *const prefix = sel->getPrefix<hldb::RefObj>();
    ASSERT_NE(prefix, nullptr);
    EXPECT_EQ(prefix->getName(), "q");
    EXPECT_NE(prefix->getActual<hldb::Net>(), nullptr);
    const hldb::Constant *const index = sel->getIndex<hldb::Constant>();
    ASSERT_NE(index, nullptr);
    EXPECT_EQ(index->getDecompile(), std::to_string(i));
  }
}

// --- $display(":re: BEGIN:QUEUE_FULL") / push_back(4) / $display(":re: END") ---

TEST_F(QueuesBoundedTest, SecondDisplayIsQueueFullMarker) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(begin->getStmts()->at(4));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":re: BEGIN:QUEUE_FULL");
}

TEST_F(QueuesBoundedTest, FourthPushBackCallsPushBackWithArgFour) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::HierPath *const hp = any_cast<hldb::HierPath>(begin->getStmts()->at(5));
  ASSERT_NE(hp, nullptr) << "'q.push_back(4)' should be a HierPath even though the queue is already full";
  ASSERT_EQ(hp->getPathElems()->size(), 2u);
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "push_back");
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::Constant *const arg = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getDecompile(), "4");
}

TEST_F(QueuesBoundedTest, ThirdDisplayIsEndMarker) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(begin->getStmts()->at(6));
  ASSERT_NE(disp, nullptr);
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 1u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":re: END");
}

// --- $display(":assert: (%d==3)", q.size) ---------------------------------

TEST_F(QueuesBoundedTest, FourthDisplayFormatStringIsSizeAssert) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(begin->getStmts()->at(7));
  ASSERT_NE(disp, nullptr);
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 2u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (%d==3)");
}

TEST_F(QueuesBoundedTest, FourthDisplaySecondArgIsQDotSize) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(begin->getStmts()->at(7));
  ASSERT_NE(disp, nullptr);
  const hldb::HierPath *const size = any_cast<hldb::HierPath>(disp->getArguments()->at(1));
  ASSERT_NE(size, nullptr);
  EXPECT_EQ(size->getName(), "q.size()");
  ASSERT_NE(size->getPathElems(), nullptr);
  ASSERT_EQ(size->getPathElems()->size(), 2u);

  const hldb::RefObj *const qRef = any_cast<hldb::RefObj>(size->getPathElems()->at(0));
  ASSERT_NE(qRef, nullptr);
  EXPECT_EQ(qRef->getName(), "q");
  EXPECT_NE(qRef->getActual<hldb::Net>(), nullptr);

  // IEEE 1800-2017 7.24.4: the parenthesis-less built-in call "q.size"
  // must resolve exactly like "q.size()" does (verified working via
  // chapter-5/5.13-builtin-methods-arrays.sv) -- a MethodFuncCall named
  // "size" taking no arguments. KNOWN BUG: this HLC build currently parses
  // "size" here as an unresolved RefObj instead, so this assertion FAILS
  // until the parser is fixed. See the file-level comment above.
  const hldb::MethodFuncCall *const sizeCall = any_cast<hldb::MethodFuncCall>(size->getPathElems()->at(1));
  ASSERT_NE(sizeCall, nullptr) << "'size' without parens should resolve to a MethodFuncCall, not a plain RefObj";
  EXPECT_EQ(sizeCall->getName(), "size");
  EXPECT_EQ(sizeCall->getArguments(), nullptr) << "size() takes no arguments";
}

// --- structural completeness / design-level typespecs ----------------------

TEST_F(QueuesBoundedTest, ModuleHasNoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

TEST_F(QueuesBoundedTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(QueuesBoundedTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "work@top");
}

TEST_F(QueuesBoundedTest, DesignHasIntTypespecSigned) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(QueuesBoundedTest, DesignHasStringTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  ASSERT_GT(m_design->getTypespecs()->size(), 2u);
  EXPECT_NE(any_cast<hldb::StringTypespec>(m_design->getTypespecs()->at(2)), nullptr);
}

// --- compiler diagnostics: KNOWN BUG, "q.size" wrongly flagged -------------

TEST_F(QueuesBoundedTest, CompilerReportsNoErrors) {
  // bounded.sv is valid SystemVerilog; a correct compiler reports zero
  // errors. KNOWN BUG: this build raises 1 spurious ELAB_ILLEGAL_IMPLICIT_NET
  // for "q.size" (see the file-level comment above), so this currently FAILS.
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(QueuesBoundedTest, NoIllegalImplicitNetErrorForSize) {
  // KNOWN BUG: currently raises 1 ELAB_ILLEGAL_IMPLICIT_NET for the
  // parenthesis-less "q.size" at line 30, column 33. This assertion
  // encodes the spec-correct expectation (zero such errors) and FAILS
  // until the parser recognizes "size" without parens as a MethodFuncCall.
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
