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

// Tests for push_front_assign.sv (tags: 7.10.4)
//   module top ();
//     int q[$];
//     initial begin
//       q = { 2, q };
//       q = { 3, q };
//       q = { 4, q };
//       $display(":assert: (%d == 3)", q.size);
//       $display(":assert: (%d == 4)", q[0]);
//     end
//   endmodule
//
// IEEE 1800-2017 7.10.4 "Inserting queue elements": prepending an element
// can be expressed either with the built-in "push_front()" method or,
// equivalently, by re-assigning the queue to the concatenation of the new
// element with the queue's current (self-referential) value --
// "q = {2, q}" is equivalent to "q.push_front(2)". Applying this 3 times
// with 2, 3, then 4 leaves the queue as {4, 3, 2} -- the LAST value
// prepended (4) ends up at the front (index 0).
//
// Checked:
//   - design has module work@top with exactly 1 net: "q" (unbounded
//     queue of int)
//   - net "q": ArrayTypespec vpiArrayType=queue(4), unpacked, ElemTypespec
//     -> IntTypespec (signed); range left bound Constant "$"
//     (vpiConstType=unbounded)
//   - each "q = {N, q}" is an Assignment (blocking) whose rhs is a
//     concatenation Operation with exactly 2 operands: a Constant (the
//     new front value N) and a self-referential RefObj "q" resolved to
//     the same Net "q" being assigned -- confirming the parser correctly
//     captures the queue referencing itself on the rhs of its own
//     assignment, in source order (2, 3, 4)
//   - "q.size" must resolve like "q.size()" would (RefObj "q" resolved +
//     MethodFuncCall "size" taking no arguments) -- see the KNOWN BUG note
//     below
//   - "q[0]" is a BitSelect with prefix RefObj "q" (resolved) and index
//     Constant "0" -- confirms the LAST prepend (value 4) becomes the
//     front element, matching push_front's insertion order
//   - the initial process' Begin block has exactly 5 statements in source
//     order
//   - design-level typespecs (3): ModuleTypespec, IntTypespec, StringTypespec
//
// KNOWN COMPILER BUG (not a defect in push_front_assign.sv):
//   IEEE 1800-2017 7.24.4 permits the built-in ".size" method to be called
//   with or without parentheses. This HLC build never resolves the
//   parenthesis-less form: instead of a MethodFuncCall, "size" in "q.size"
//   is left as an unresolved RefObj, and a spurious
//   ELAB_ILLEGAL_IMPLICIT_NET ("Illegal implicit net") is raised for it
//   (same gap tracked across chapter-7/queues/bounded/bounded.sv,
//   chapter-7/queues/delete/delete.sv, chapter-7/queues/
//   delete_assign/delete_assign.sv, chapter-7/queues/
//   insert_assign/insert_assign.sv, chapter-7/queues/insert/insert.sv,
//   chapter-7/queues/max-size/max-size.sv, chapter-7/queues/
//   persistence/persistence.sv, chapter-7/queues/
//   pop_back_assing/pop_back_assing.sv, chapter-7/queues/pop_back/pop_back.sv,
//   chapter-7/queues/pop_front/pop_front.sv, chapter-7/queues/
//   pop_front_assign/pop_front_assign.sv, chapter-7/queues/
//   push_back/push_back.sv and chapter-7/queues/push_front/push_front.sv).
//   That the parenthesized form works is independently verified by
//   chapter-5/5.13-builtin-methods-arrays.sv ("array.size()") and
//   chapter-7/queues/persistence/persistence.sv ("q.delete()").
//   FourthStmtDisplayAssertsSizeThree and the two error-count tests below
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
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class QueuesPushFrontAssignTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "push_front_assign.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()); }

  static const hldb::Net *getNetQ() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Net>("q", top->getNets());
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

  // Verifies stmt[index] is "q = {value, q}": Assignment (blocking) with
  // lhs RefObj "q" (resolved) and rhs a 2-operand concatenation Operation
  // whose operands are Constant(value) and a self-referential RefObj "q"
  // (resolved to the same Net).
  static void ExpectPrependAssignment(size_t index, std::string_view value) {
    const hldb::Begin *const begin = getInitialBegin();
    ASSERT_NE(begin, nullptr);
    ASSERT_NE(begin->getStmts(), nullptr);
    ASSERT_GT(begin->getStmts()->size(), index);
    const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(index));
    ASSERT_NE(assign, nullptr) << "stmt[" << index << "] should be an Assignment (q = {" << value << ", q})";
    EXPECT_TRUE(assign->getBlocking());

    const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getName(), "q");
    EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr);

    const hldb::Operation *const rhs = assign->getRhs<hldb::Operation>();
    ASSERT_NE(rhs, nullptr) << "'{" << value << ", q}' should be a concatenation Operation";
    EXPECT_EQ(rhs->getOpType(), vpiConcatOp);
    ASSERT_NE(rhs->getOperands(), nullptr);
    ASSERT_EQ(rhs->getOperands()->size(), 2u);

    const hldb::Constant *const newFront = any_cast<hldb::Constant>(rhs->getOperands()->at(0));
    ASSERT_NE(newFront, nullptr) << "operand[0] (the new front value) should be a Constant";
    EXPECT_EQ(newFront->getDecompile(), value);

    const hldb::RefObj *const qSelfRef = any_cast<hldb::RefObj>(rhs->getOperands()->at(1));
    ASSERT_NE(qSelfRef, nullptr) << "operand[1] (the queue's current value) should be a self-referential RefObj";
    EXPECT_EQ(qSelfRef->getName(), "q");
    EXPECT_NE(qSelfRef->getActual<hldb::Net>(), nullptr) << "'q' on the rhs should resolve to the same declared Net";
  }
};

// --- module / net ------------------------------------------------------------

TEST_F(QueuesPushFrontAssignTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(QueuesPushFrontAssignTest, ModuleHasOneNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(QueuesPushFrontAssignTest, NetQExists) { EXPECT_NE(getNetQ(), nullptr); }

// --- net "q": unbounded queue "int q[$]" ------------------------------------

TEST_F(QueuesPushFrontAssignTest, NetQArrayTypeIsQueue) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiQueueArray) << "7.10: 'int q[$]' must be modeled as a queue array";
}

TEST_F(QueuesPushFrontAssignTest, NetQArrayIsNotPacked) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  EXPECT_FALSE(at->getPacked()) << "a queue dimension is an unpacked dimension";
}

TEST_F(QueuesPushFrontAssignTest, NetQRangeLeftIsUnboundedDollar) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr);
  const hldb::Constant *const dollar = at->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(dollar, nullptr);
  EXPECT_EQ(dollar->getDecompile(), "$");
  EXPECT_EQ(dollar->getConstType(), vpiUnboundedConst);
}

TEST_F(QueuesPushFrontAssignTest, NetQElemTypespecIsSignedIntTypespec) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::IntTypespec *const elem = at->getElemTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "element type of 'int q[$]' should resolve to IntTypespec";
  EXPECT_TRUE(elem->getSigned());
}

TEST_F(QueuesPushFrontAssignTest, NetQHasNoInitialValue) {
  const hldb::Net *const q = getNetQ();
  ASSERT_NE(q, nullptr);
  EXPECT_EQ(q->getValue(), nullptr);
}

// --- initial process structure ----------------------------------------------

TEST_F(QueuesPushFrontAssignTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(QueuesPushFrontAssignTest, InitialBeginHasFiveStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 5u);
}

// --- q = {2, q}; q = {3, q}; q = {4, q}: push_front via self-concat -------

TEST_F(QueuesPushFrontAssignTest, FirstAssignmentPrependsTwo) { ExpectPrependAssignment(0, "2"); }
TEST_F(QueuesPushFrontAssignTest, SecondAssignmentPrependsThree) { ExpectPrependAssignment(1, "3"); }
TEST_F(QueuesPushFrontAssignTest, ThirdAssignmentPrependsFour) { ExpectPrependAssignment(2, "4"); }

// --- $display(":assert: (%d == 3)", q.size) ---------------------------------

TEST_F(QueuesPushFrontAssignTest, FourthStmtDisplayAssertsSizeThree) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(3));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 2u);

  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 3)");

  const hldb::HierPath *const size = any_cast<hldb::HierPath>(disp->getArguments()->at(1));
  ASSERT_NE(size, nullptr);
  EXPECT_EQ(size->getName(), "q.size");
  ASSERT_NE(size->getPathElems(), nullptr);
  ASSERT_EQ(size->getPathElems()->size(), 2u);

  const hldb::RefObj *const qRef = any_cast<hldb::RefObj>(size->getPathElems()->at(0));
  ASSERT_NE(qRef, nullptr);
  EXPECT_EQ(qRef->getName(), "q");
  EXPECT_NE(qRef->getActual<hldb::Net>(), nullptr);

  const hldb::MethodFuncCall *const sizeCall = any_cast<hldb::MethodFuncCall>(size->getPathElems()->at(1));
  ASSERT_NE(sizeCall, nullptr) << "'size' without parens should resolve to a MethodFuncCall, not a plain RefObj";
  EXPECT_EQ(sizeCall->getName(), "size");
  EXPECT_EQ(sizeCall->getArguments(), nullptr) << "size() takes no arguments";
}

// --- $display(":assert: (%d == 4)", q[0]) -----------------------------------

TEST_F(QueuesPushFrontAssignTest, FifthStmtDisplayAssertsQAtZeroEqualsFour) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(4));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 2u);

  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 4)");

  const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(disp->getArguments()->at(1));
  ASSERT_NE(sel, nullptr) << "'q[0]' should be a BitSelect -- confirms the LAST prepend becomes the front element";
  EXPECT_EQ(sel->getName(), "q[0]");
  const hldb::RefObj *const prefix = sel->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->getName(), "q");
  EXPECT_NE(prefix->getActual<hldb::Net>(), nullptr);
  const hldb::Constant *const index = sel->getIndex<hldb::Constant>();
  ASSERT_NE(index, nullptr);
  EXPECT_EQ(index->getDecompile(), "0");
}

// --- structural completeness / design-level typespecs -----------------------

TEST_F(QueuesPushFrontAssignTest, ModuleHasNoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

TEST_F(QueuesPushFrontAssignTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(QueuesPushFrontAssignTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "work@top");
}

TEST_F(QueuesPushFrontAssignTest, DesignHasIntTypespecSigned) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(QueuesPushFrontAssignTest, DesignHasStringTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  ASSERT_GT(m_design->getTypespecs()->size(), 2u);
  EXPECT_NE(any_cast<hldb::StringTypespec>(m_design->getTypespecs()->at(2)), nullptr);
}

// --- compiler diagnostics: KNOWN BUG, "q.size" wrongly flagged -------------

TEST_F(QueuesPushFrontAssignTest, CompilerReportsNoErrors) {
  // push_front_assign.sv is valid SystemVerilog; a correct compiler
  // reports zero errors. KNOWN BUG: this build raises 1 spurious
  // ELAB_ILLEGAL_IMPLICIT_NET for "q.size", so this currently FAILS. See
  // the file-level comment above.
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(QueuesPushFrontAssignTest, NoIllegalImplicitNetErrorForSize) {
  // KNOWN BUG: currently raises 1 ELAB_ILLEGAL_IMPLICIT_NET for the
  // parenthesis-less "q.size" at line 24, column 35. This assertion
  // encodes the spec-correct expectation (zero such errors) and FAILS
  // until the parser recognizes parenthesis-less no-arg built-in method
  // calls.
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
