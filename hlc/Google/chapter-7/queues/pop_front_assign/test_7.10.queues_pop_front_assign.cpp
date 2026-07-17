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

// Tests for pop_front_assign.sv (tags: 7.10.4)
//   module top ();
//     int q[$];
//     int r;
//     initial begin
//       q.push_back(2);
//       q.push_back(3);
//       q.push_back(4);
//       r = q[0];
//       q = q[1:$];
//       $display(":assert: (%d == 2)", q.size);
//       $display(":assert: (%d == 2)", r);
//       $display(":assert: (%d == 3)", q[0]);
//     end
//   endmodule
//
// IEEE 1800-2017 7.10.4 "Deleting queue elements": popping the first
// element can be expressed either with the built-in "pop_front()" method
// or, equivalently, by re-assigning the queue to the slice that excludes
// its first index -- "q = q[1:$]" is equivalent to
// "void'(q.pop_front())". "q[0]" (7.10.1) is a bit-select that reads the
// first element.
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
//   - "r = q[0]": Assignment (blocking) whose rhs is a BitSelect "q[0]"
//     with prefix RefObj "q" (resolved) and index Constant "0" -- reads
//     the first element before it is popped
//   - "q = q[1:$]": Assignment (blocking) whose rhs is a PartSelect
//     "q[1:$]" with prefix RefObj "q" (resolved); its Range has left
//     bound Constant "1" (vpiUIntConst) and right bound Constant "$"
//     (vpiUnboundedConst) -- the pop_front-via-slice pattern
//   - "q.size" must resolve like "q.size()" would (RefObj "q" resolved +
//     MethodFuncCall "size" taking no arguments) -- see the KNOWN BUG note
//     below
//   - "r" and the final "q[0]" resolve to their respective declarations,
//     confirming the value read before the pop (into "r") and the new
//     front element (via "q[0]" after the pop) are both structurally
//     captured
//   - the initial process' Begin block has exactly 8 statements in source
//     order
//   - design-level typespecs (3): ModuleTypespec, IntTypespec, StringTypespec
//
// KNOWN COMPILER BUG (not a defect in pop_front_assign.sv):
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
//   pop_back_assing/pop_back_assing.sv and chapter-7/queues/
//   pop_back/pop_back.sv). That the parenthesized form works is
//   independently verified by chapter-5/5.13-builtin-methods-arrays.sv
//   ("array.size()") and chapter-7/queues/persistence/persistence.sv
//   ("q.delete()"). SixthStmtDisplayAssertsSizeTwo and the two
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
#include <hldb/part_select.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class QueuesPopFrontAssignTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "pop_front_assign.hlc"}); }
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

TEST_F(QueuesPopFrontAssignTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(QueuesPopFrontAssignTest, ModuleHasTwoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 2u);
}

TEST_F(QueuesPopFrontAssignTest, NetQExists) { EXPECT_NE(getNetQ(), nullptr); }

TEST_F(QueuesPopFrontAssignTest, NetRExists) { EXPECT_NE(getNetR(), nullptr); }

// --- net "q": unbounded queue "int q[$]" ------------------------------------

TEST_F(QueuesPopFrontAssignTest, NetQArrayTypeIsQueue) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiQueueArray) << "7.10: 'int q[$]' must be modeled as a queue array";
}

TEST_F(QueuesPopFrontAssignTest, NetQArrayIsNotPacked) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  EXPECT_FALSE(at->getPacked()) << "a queue dimension is an unpacked dimension";
}

TEST_F(QueuesPopFrontAssignTest, NetQRangeLeftIsUnboundedDollar) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr);
  const hldb::Constant *const dollar = at->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(dollar, nullptr);
  EXPECT_EQ(dollar->getDecompile(), "$");
  EXPECT_EQ(dollar->getConstType(), vpiUnboundedConst);
}

TEST_F(QueuesPopFrontAssignTest, NetQElemTypespecIsSignedIntTypespec) {
  const hldb::ArrayTypespec *const at = getQArrayTypespec();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::IntTypespec *const elem = at->getElemTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "element type of 'int q[$]' should resolve to IntTypespec";
  EXPECT_TRUE(elem->getSigned());
}

TEST_F(QueuesPopFrontAssignTest, NetQHasNoInitialValue) {
  const hldb::Net *const q = getNetQ();
  ASSERT_NE(q, nullptr);
  EXPECT_EQ(q->getValue(), nullptr);
}

// --- net "r": plain "int r;" -------------------------------------------------

TEST_F(QueuesPopFrontAssignTest, NetRTypespecIsSignedIntTypespec) {
  const hldb::Net *const r = getNetR();
  ASSERT_NE(r, nullptr);
  ASSERT_NE(r->getTypespec(), nullptr);
  const hldb::IntTypespec *const it = r->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>();
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(QueuesPopFrontAssignTest, NetRHasNoInitialValue) {
  const hldb::Net *const r = getNetR();
  ASSERT_NE(r, nullptr);
  EXPECT_EQ(r->getValue(), nullptr);
}

// --- initial process structure ----------------------------------------------

TEST_F(QueuesPopFrontAssignTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(QueuesPopFrontAssignTest, InitialBeginHasEightStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 8u);
}

// --- q.push_back(2/3/4) ------------------------------------------------------

TEST_F(QueuesPopFrontAssignTest, FirstPushBackHasArgTwo) { ExpectPushBack(0, "2"); }
TEST_F(QueuesPopFrontAssignTest, SecondPushBackHasArgThree) { ExpectPushBack(1, "3"); }
TEST_F(QueuesPopFrontAssignTest, ThirdPushBackHasArgFour) { ExpectPushBack(2, "4"); }

// --- r = q[0]; reads the first element before the pop -----------------------

TEST_F(QueuesPopFrontAssignTest, FourthStmtAssignmentIsBlockingRAssignedQAtZero) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(3));
  ASSERT_NE(assign, nullptr) << "'r = q[0]' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking());

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "r");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr);

  const hldb::BitSelect *const rhs = assign->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr) << "'q[0]' should be a BitSelect";
  EXPECT_EQ(rhs->getName(), "q[0]");
  const hldb::RefObj *const prefix = rhs->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->getName(), "q");
  EXPECT_NE(prefix->getActual<hldb::Net>(), nullptr);
  const hldb::Constant *const index = rhs->getIndex<hldb::Constant>();
  ASSERT_NE(index, nullptr);
  EXPECT_EQ(index->getDecompile(), "0");
}

// --- q = q[1:$]; pops the first element (7.10.4) ----------------------------

TEST_F(QueuesPopFrontAssignTest, FifthStmtAssignmentIsBlockingWithLhsQ) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(4));
  ASSERT_NE(assign, nullptr) << "'q = q[1:$]' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "q");
  EXPECT_NE(lhs->getActual<hldb::Net>(), nullptr);
}

TEST_F(QueuesPopFrontAssignTest, FifthStmtAssignmentRhsIsQOneToDollarPartSelect) {
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
  EXPECT_NE(prefix->getActual<hldb::Net>(), nullptr);

  ASSERT_NE(rhs->getRange(), nullptr);
  const hldb::Constant *const left = rhs->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "1");
  EXPECT_EQ(left->getConstType(), vpiUIntConst);

  const hldb::Constant *const right = rhs->getRange()->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "$");
  EXPECT_EQ(right->getConstType(), vpiUnboundedConst);
}

// --- $display(":assert: (%d == 2)", q.size) ---------------------------------

TEST_F(QueuesPopFrontAssignTest, SixthStmtDisplayAssertsSizeTwo) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(begin->getStmts()->at(5));
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

// --- $display(":assert: (%d == 2)", r) --------------------------------------

TEST_F(QueuesPopFrontAssignTest, SeventhStmtDisplayAssertsREqualsTwo) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(begin->getStmts()->at(6));
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

// --- $display(":assert: (%d == 3)", q[0]) -----------------------------------

TEST_F(QueuesPopFrontAssignTest, EighthStmtDisplayAssertsQAtZeroEqualsThree) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::SysFuncCall *const disp = any_cast<hldb::SysFuncCall>(begin->getStmts()->at(7));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 2u);

  const hldb::Constant *const fmt = any_cast<hldb::Constant>(disp->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getValue(), ":assert: (%d == 3)");

  const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(disp->getArguments()->at(1));
  ASSERT_NE(sel, nullptr) << "'q[0]' should be a BitSelect -- confirms the new front element after the pop";
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

TEST_F(QueuesPopFrontAssignTest, ModuleHasNoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

TEST_F(QueuesPopFrontAssignTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(QueuesPopFrontAssignTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "work@top");
}

TEST_F(QueuesPopFrontAssignTest, DesignHasIntTypespecSigned) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(QueuesPopFrontAssignTest, DesignHasStringTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  ASSERT_GT(m_design->getTypespecs()->size(), 2u);
  EXPECT_NE(any_cast<hldb::StringTypespec>(m_design->getTypespecs()->at(2)), nullptr);
}

// --- compiler diagnostics: KNOWN BUG, "q.size" wrongly flagged -------------

TEST_F(QueuesPopFrontAssignTest, CompilerReportsNoErrors) {
  // pop_front_assign.sv is valid SystemVerilog; a correct compiler
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

TEST_F(QueuesPopFrontAssignTest, NoIllegalImplicitNetErrorForSize) {
  // KNOWN BUG: currently raises 1 ELAB_ILLEGAL_IMPLICIT_NET for the
  // parenthesis-less "q.size" at line 27, column 35. This assertion
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
