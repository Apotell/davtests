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

// Tests for pop_back_assing.sv (tags: 7.10.4)
//   module top ();
//     int q[$];
//     int r;
//     initial begin
//       q = { 2, 3, 4 };
//       r = q[$];
//       q = q[0:$-1]; // void'(q.pop_back()) or q.delete(q.size-1)
//       $display(":assert: (%d == 2)", q.size);
//       $display(":assert: (%d == 4)", r);
//     end
//   endmodule
//
// IEEE 1800-2017 7.10.4 "Deleting queue elements": popping the last element
// can be expressed either with the built-in "pop_back()" method or,
// equivalently, by re-assigning the queue to the slice that excludes its
// last index -- "q = q[0:$-1]" is equivalent to "void'(q.pop_back())".
// "q[$]" (7.10.1) is a bit-select that reads the last element.
//
// Checked:
//   - design has module work@top with exactly 2 nets: "q" (unbounded
//     queue of int) and "r" (plain int)
//   - net "q": ArrayTypespec vpiArrayType=queue(4), unpacked, ElemTypespec
//     -> IntTypespec (signed); range left bound Constant "$"
//     (vpiConstType=unbounded)
//   - net "r": typespec resolves directly to a signed IntTypespec
//   - "q = {2, 3, 4}": Assignment (blocking) whose rhs is a concatenation
//     Operation with 3 Constant operands (2, 3, 4)
//   - "r = q[$]": Assignment (blocking) whose rhs is a BitSelect "q[$]"
//     with prefix RefObj "q" (resolved) and index Constant "$"
//     (vpiConstType=unbounded) -- reads the last element
//   - "q = q[0:$-1]": Assignment (blocking) whose rhs is a PartSelect
//     "q[0:$-1]" with prefix RefObj "q" (resolved); its Range has left
//     bound Constant "0" and right bound a subtract Operation
//     (vpiSubOp) with 2 operands, Constant "$" (unbounded) and
//     Constant "1" -- i.e. the parser correctly captures "$-1" as an
//     arithmetic expression on the range boundary, not just a plain
//     constant, confirming the pop_back-via-slice pattern is parsed
//     structurally intact
//   - "q.size" must resolve like "q.size()" would (RefObj "q" resolved +
//     MethodFuncCall "size" taking no arguments) -- see the KNOWN BUG note
//     below
//   - "r" in the final $display resolves to the declared Net "r"
//   - the initial process' Begin block has exactly 5 statements in source
//     order
//   - design-level typespecs (3): ModuleTypespec, IntTypespec, StringTypespec
//
// KNOWN COMPILER BUG (not a defect in pop_back_assing.sv):
//   IEEE 1800-2017 7.24.4 permits the built-in ".size" method to be called
//   with or without parentheses. This HLC build never resolves the
//   parenthesis-less form: instead of a MethodFuncCall, "size" in "q.size"
//   is left as an unresolved RefObj, and a spurious
//   ELAB_ILLEGAL_IMPLICIT_NET ("Illegal implicit net") is raised for it
//   (same gap tracked across chapter-7/queues/bounded/bounded.sv,
//   chapter-7/queues/delete/delete.sv, chapter-7/queues/
//   delete_assign/delete_assign.sv, chapter-7/queues/
//   insert_assign/insert_assign.sv, chapter-7/queues/insert/insert.sv,
//   chapter-7/queues/max-size/max-size.sv and chapter-7/queues/
//   persistence/persistence.sv). That the parenthesized form works is
//   independently verified by chapter-5/5.13-builtin-methods-arrays.sv
//   ("array.size()") and chapter-7/queues/persistence/persistence.sv
//   ("q.delete()"). FourthStmtDisplayAssertsSizeTwo and the two
//   error-count tests below assert the IEEE-mandated behavior and will
//   FAIL until the parser is fixed.

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
#include <hldb/part_select.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class QueuesPopBackAssignTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "pop_back_assing.hlc"}); }
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
};

// --- module / nets -----------------------------------------------------------

TEST_F(QueuesPopBackAssignTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(QueuesPopBackAssignTest, ModuleHasTwoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 2u);
}

TEST_F(QueuesPopBackAssignTest, NetQExists) { EXPECT_NE(getNetQ(), nullptr); }

TEST_F(QueuesPopBackAssignTest, NetRExists) { EXPECT_NE(getNetR(), nullptr); }

// --- net "q": unbounded queue "int q[$]" ------------------------------------

TEST_F(QueuesPopBackAssignTest, NetQArrayTypeIsQueue) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiQueueArray) << "7.10: 'int q[$]' must be modeled as a queue array";
}

TEST_F(QueuesPopBackAssignTest, NetQArrayIsNotPacked) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  EXPECT_FALSE(at->getPacked()) << "a queue dimension is an unpacked dimension";
}

TEST_F(QueuesPopBackAssignTest, NetQRangeLeftIsUnboundedDollar) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr);
  const hldb::Constant *const dollar = at->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(dollar, nullptr);
  EXPECT_EQ(dollar->getDecompile(), "$");
  EXPECT_EQ(dollar->getConstType(), vpiUnboundedConst);
}

TEST_F(QueuesPopBackAssignTest, NetQElemTypespecIsSignedIntTypespec) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::IntTypespec *const elem = at->getElemTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "element type of 'int q[$]' should resolve to IntTypespec";
  EXPECT_TRUE(elem->getSigned());
}

TEST_F(QueuesPopBackAssignTest, NetQHasNoInitialValue) {
  const hldb::Net *const q = getNetQ();
  ASSERT_NE(q, nullptr);
  EXPECT_EQ(q->getValue(), nullptr);
}

// --- net "r": plain "int r;" -------------------------------------------------

TEST_F(QueuesPopBackAssignTest, NetRTypespecIsSignedIntTypespec) {
  const hldb::Net *const r = getNetR();
  ASSERT_NE(r, nullptr);
  ASSERT_NE(r->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = r->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(QueuesPopBackAssignTest, NetRHasNoInitialValue) {
  const hldb::Net *const r = getNetR();
  ASSERT_NE(r, nullptr);
  EXPECT_EQ(r->getValue(), nullptr);
}

// --- initial process structure ----------------------------------------------

TEST_F(QueuesPopBackAssignTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(QueuesPopBackAssignTest, InitialBeginHasFiveStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 5u);
}

// --- q = {2, 3, 4} -----------------------------------------------------------

TEST_F(QueuesPopBackAssignTest, FirstAssignmentIsBlockingQAssignedThreeElemConcat) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(0));
  ASSERT_NE(assign, nullptr) << "'q = {2, 3, 4}' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking());

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "q");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr);

  const hldb::Operation *const rhs = assign->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "'{2, 3, 4}' should be a concatenation Operation";
  EXPECT_EQ(rhs->getOpType(), vpiConcatOp);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 3u);
  for (uint32_t i = 0; i < 3u; ++i) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(rhs->getOperands()->at(i));
    ASSERT_NE(c, nullptr) << "operand[" << i << "] should be a Constant";
    EXPECT_EQ(c->getDecompile(), std::to_string(i + 2));
  }
}

// --- r = q[$]; reads the last element ----------------------------------------

TEST_F(QueuesPopBackAssignTest, SecondAssignmentIsBlockingRAssignedQAtDollar) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(1));
  ASSERT_NE(assign, nullptr) << "'r = q[$]' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking());

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "r");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr);

  const hldb::BitSelect *const rhs = assign->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr) << "'q[$]' should be a BitSelect";
  EXPECT_EQ(rhs->getName(), "q[$]");
  const hldb::RefObj *const prefix = rhs->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->getName(), "q");
  EXPECT_NE(prefix->getActual<hldb::Net>(), nullptr);

  const hldb::Constant *const index = rhs->getIndex<hldb::Constant>();
  ASSERT_NE(index, nullptr) << "7.10.1: 'q[$]' indexes the last element with the unbounded '$'";
  EXPECT_EQ(index->getDecompile(), "$");
  EXPECT_EQ(index->getConstType(), vpiUnboundedConst);
}

// --- q = q[0:$-1]; pops the last element (7.10.4) ---------------------------

TEST_F(QueuesPopBackAssignTest, ThirdAssignmentIsBlockingWithLhsQ) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(2));
  ASSERT_NE(assign, nullptr) << "'q = q[0:$-1]' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "q");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr);
}

TEST_F(QueuesPopBackAssignTest, ThirdAssignmentRhsIsQZeroToDollarMinusOnePartSelect) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(2));
  ASSERT_NE(assign, nullptr);
  const hldb::PartSelect *const rhs = assign->getRhs<hldb::PartSelect>();
  ASSERT_NE(rhs, nullptr) << "'q[0:$-1]' should be a PartSelect";
  EXPECT_EQ(rhs->getName(), "q[0:$-1]");

  const hldb::RefObj *const prefix = rhs->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->getName(), "q");
  EXPECT_NE(prefix->getActual<hldb::Net>(), nullptr);

  ASSERT_NE(rhs->getRange(), nullptr);
  const hldb::Constant *const left = rhs->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "0");
  EXPECT_EQ(left->getConstType(), vpiUIntConst);
}

TEST_F(QueuesPopBackAssignTest, ThirdAssignmentRangeRightIsDollarMinusOneSubtractOp) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(2));
  ASSERT_NE(assign, nullptr);
  const hldb::PartSelect *const rhs = assign->getRhs<hldb::PartSelect>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getRange(), nullptr);

  const hldb::Operation *const right = rhs->getRange()->getRightExpr<hldb::Operation>();
  ASSERT_NE(right, nullptr) << "'$-1' should be a subtract Operation, not a plain Constant";
  EXPECT_EQ(right->getOpType(), vpiSubOp);
  ASSERT_NE(right->getOperands(), nullptr);
  ASSERT_EQ(right->getOperands()->size(), 2u);

  const hldb::Constant *const dollar = any_cast<hldb::Constant>(right->getOperands()->at(0));
  ASSERT_NE(dollar, nullptr) << "operand[0] should be the unbounded '$'";
  EXPECT_EQ(dollar->getDecompile(), "$");
  EXPECT_EQ(dollar->getConstType(), vpiUnboundedConst);

  const hldb::Constant *const one = any_cast<hldb::Constant>(right->getOperands()->at(1));
  ASSERT_NE(one, nullptr) << "operand[1] should be the Constant '1'";
  EXPECT_EQ(one->getDecompile(), "1");
  EXPECT_EQ(one->getConstType(), vpiUIntConst);
}

// --- $display(":assert: (%d == 2)", q.size) ---------------------------------

TEST_F(QueuesPopBackAssignTest, FourthStmtDisplayAssertsSizeTwo) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(3));
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

  const hldb::MethodFuncCall *const sizeCall = any_cast<hldb::MethodFuncCall>(size->getPathElems()->at(1));
  ASSERT_NE(sizeCall, nullptr) << "'size' without parens should resolve to a MethodFuncCall, not a plain RefObj";
  EXPECT_EQ(sizeCall->getName(), "size");
  EXPECT_EQ(sizeCall->getArguments(), nullptr) << "size() takes no arguments";
}

// --- $display(":assert: (%d == 4)", r) --------------------------------------

TEST_F(QueuesPopBackAssignTest, FifthStmtDisplayAssertsREqualsFour) {
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

  const hldb::RefObj *const rRef = any_cast<hldb::RefObj>(disp->getArguments()->at(1));
  ASSERT_NE(rRef, nullptr);
  EXPECT_EQ(rRef->getName(), "r");
  EXPECT_NE(rRef->getActual<hldb::Net>(), nullptr);
}

// --- structural completeness / design-level typespecs -----------------------

TEST_F(QueuesPopBackAssignTest, ModuleHasNoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

TEST_F(QueuesPopBackAssignTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(QueuesPopBackAssignTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "work@top");
}

TEST_F(QueuesPopBackAssignTest, DesignHasIntTypespecSigned) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(QueuesPopBackAssignTest, DesignHasStringTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  ASSERT_GT(m_design->getTypespecs()->size(), 2u);
  EXPECT_NE(any_cast<hldb::StringTypespec>(m_design->getTypespecs()->at(2)), nullptr);
}

// --- compiler diagnostics: KNOWN BUG, "q.size" wrongly flagged -------------

TEST_F(QueuesPopBackAssignTest, CompilerReportsNoErrors) {
  // pop_back_assing.sv is valid SystemVerilog; a correct compiler reports
  // zero errors. KNOWN BUG: this build raises 1 spurious
  // ELAB_ILLEGAL_IMPLICIT_NET for "q.size", so this currently FAILS. See
  // the file-level comment above.
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

TEST_F(QueuesPopBackAssignTest, NoIllegalImplicitNetErrorForSize) {
  // KNOWN BUG: currently raises 1 ELAB_ILLEGAL_IMPLICIT_NET for the
  // parenthesis-less "q.size" at line 25, column 35. This assertion
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
