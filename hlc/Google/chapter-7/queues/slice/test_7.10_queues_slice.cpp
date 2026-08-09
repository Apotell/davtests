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

// Tests for slice.sv (tags: 7.10.1 7.10.2)
//   module top ();
//     int q[$:5];
//     int r[$];
//     initial begin
//       q.push_back(0); ... q.push_back(5);
//       $display(":assert: (%d == 6)", q.size);
//       r = q[ 2 : 4 ];   $display(":assert: (%d == 3)", r.size);
//       r = q[ 4 : 2 ];   $display(":assert: (%d == 0)", r.size);
//       r = q[ 2 : 2 ];   $display(":assert: (%d == 1)", r.size);
//       r = q[ -2 : 2 ];  $display(":assert: (%d == 3)", r.size);
//       r = q[ 2 : 10 ];  $display(":assert: (%d == 4)", r.size);
//     end
//   endmodule
//
// IEEE 1800-2017 7.10.1 "Queue index and slice addressing": "q[a:b]"
// selects the slice of "q" from index "a" to index "b" inclusive, and
// assigning that slice to another queue variable ("r") both copies the
// selected elements and lets the parser resolve each range bound as an
// arbitrary constant expression, including negative and out-of-range
// literals -- the source comments call out that a > b yields an empty
// queue, a == b yields a single-element queue, a negative bound behaves
// as if clamped to 0, and a bound beyond the queue's current bound
// behaves as if clamped to '$'. This test only checks how the parser
// structures each slice expression, not the runtime clamping semantics
// (which HLC, a static parser/elaborator, does not execute).
//
// Checked:
//   - design has module top with exactly 2 nets: "q" (bounded queue
//     of int, bound 5) and "r" (unbounded queue of int)
//   - net "q": ArrayTypespec vpiArrayType=queue(4), unpacked, ElemTypespec
//     -> IntTypespec (signed); range left bound Constant "$"
//     (vpiConstType=unbounded), right bound Constant "5" (vpiUIntConst)
//   - net "r": ArrayTypespec vpiArrayType=queue(4), unpacked, ElemTypespec
//     -> IntTypespec (signed); range left bound Constant "$"
//     (vpiConstType=unbounded), NO right bound (unbounded queue)
//   - the 6 "q.push_back(N)" calls are each parsed as a HierPath with a
//     RefObj "q" (resolved to Net "q") and a MethodFuncCall "push_back"
//     carrying 1 Constant argument, in source order (0..5)
//   - each "r = q[a:b]" is an Assignment (blocking) whose rhs is a
//     PartSelect "q[a:b]" with prefix RefObj "q" (resolved); the 5 cases
//     exercise distinct bound shapes:
//       - q[2:4]:  both bounds plain Constants (the normal case)
//       - q[4:2]:  both bounds plain Constants, left > right (the
//                  "a > b gives empty queue" case -- parsed identically
//                  to a normal range; range validity is a runtime/
//                  elaboration concern, not a parse-tree difference)
//       - q[2:2]:  both bounds equal Constants (single-element case)
//       - q[-2:2]: left bound is a unary-minus Operation (vpiMinusOp)
//                  wrapping Constant "2" -- confirms a negative index is
//                  parsed as a real unary-minus expression, not folded
//                  into a signed literal
//       - q[2:10]: right bound "10" is a plain Constant even though it
//                  exceeds q's declared bound of 5 -- confirms the parser
//                  does not reject or clamp out-of-range literals itself
//   - "q.size"/"r.size" must each resolve like "q.size()"/"r.size()"
//     would (RefObj resolved + MethodFuncCall "size" taking no
//     arguments) -- see the KNOWN BUG note below
//   - the initial process' Begin block has exactly 17 statements in
//     source order
//   - design-level typespecs (3): ModuleTypespec, IntTypespec, StringTypespec
//
// KNOWN COMPILER BUG (not a defect in slice.sv):
//   IEEE 1800-2017 7.24.4 permits the built-in ".size" method to be called
//   with or without parentheses. This HLC build never resolves the
//   parenthesis-less form: instead of a MethodFuncCall, "size" in each
//   "q.size"/"r.size" is left as an unresolved RefObj, and a spurious
//   COMP_FAILED_TO_BIND ("Failed to bind") is raised for each
//   of the 6 occurrences (same gap tracked across chapter-7/queues/
//   bounded/bounded.sv, chapter-7/queues/delete/delete.sv, chapter-7/
//   queues/delete_assign/delete_assign.sv, chapter-7/queues/
//   insert_assign/insert_assign.sv, chapter-7/queues/insert/insert.sv,
//   chapter-7/queues/max-size/max-size.sv, chapter-7/queues/
//   persistence/persistence.sv, chapter-7/queues/
//   pop_back_assing/pop_back_assing.sv, chapter-7/queues/pop_back/pop_back.sv,
//   chapter-7/queues/pop_front/pop_front.sv, chapter-7/queues/
//   pop_front_assign/pop_front_assign.sv, chapter-7/queues/
//   push_back/push_back.sv, chapter-7/queues/push_front/push_front.sv,
//   chapter-7/queues/push_front_assign/push_front_assign.sv and
//   chapter-7/queues/size/size.sv). That the parenthesized form works is
//   independently verified by chapter-5/5.13-builtin-methods-arrays.sv
//   ("array.size()") and chapter-7/queues/persistence/persistence.sv
//   ("q.delete()"). The six size-check tests below and the two
//   error-count tests assert the IEEE-mandated behavior and will FAIL
//   until the parser is fixed.

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
#include <hldb/variable.h>
#include <hldb/operation.h>
#include <hldb/part_select.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class QueuesSliceTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "slice.hlc"}); }
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

  static const hldb::ArrayTypespec *getArrayTypespec(const hldb::Variable *net) {
    if (net == nullptr || net->getTypespec() == nullptr) return nullptr;
    return net->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
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
    EXPECT_NE(qRef->getActual<hldb::Variable>(), nullptr);

    const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->getName(), "push_back");
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 1u);
    const hldb::Constant *const arg = any_cast<hldb::Constant>(call->getArguments()->at(0));
    ASSERT_NE(arg, nullptr);
    EXPECT_EQ(arg->getDecompile(), value);
  }

  // Verifies stmt[index] is "$display(fmt, <netName>.size)" and that
  // ".size" resolves like ".size()" would (IEEE 1800-2017 7.24.4).
  static void ExpectDisplayWithResolvedSize(size_t index, std::string_view fmt, std::string_view netName) {
    const hldb::Begin *const begin = getInitialBegin();
    ASSERT_NE(begin, nullptr);
    ASSERT_GT(begin->getStmts()->size(), index);
    const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(begin->getStmts()->at(index));
    ASSERT_NE(disp, nullptr) << "stmt[" << index << "] should be a $display SysFuncCall";
    EXPECT_EQ(disp->getName(), "$display");
    ASSERT_NE(disp->getArguments(), nullptr);
    ASSERT_EQ(disp->getArguments()->size(), 2u);

    const hldb::Constant *const fmtArg = any_cast<hldb::Constant>(disp->getArguments()->at(0));
    ASSERT_NE(fmtArg, nullptr);
    EXPECT_EQ(fmtArg->getValue(), fmt);

    const hldb::HierPath *const size = any_cast<hldb::HierPath>(disp->getArguments()->at(1));
    ASSERT_NE(size, nullptr);
    ASSERT_NE(size->getPathElems(), nullptr);
    ASSERT_EQ(size->getPathElems()->size(), 2u);

    const hldb::RefObj *const netRef = any_cast<hldb::RefObj>(size->getPathElems()->at(0));
    ASSERT_NE(netRef, nullptr);
    EXPECT_EQ(netRef->getName(), netName);
    EXPECT_NE(netRef->getActual<hldb::Variable>(), nullptr);

    const hldb::MethodFuncCall *const sizeCall = any_cast<hldb::MethodFuncCall>(size->getPathElems()->at(1));
    ASSERT_NE(sizeCall, nullptr) << "'size' without parens should resolve to a MethodFuncCall, not a plain RefObj";
    EXPECT_EQ(sizeCall->getName(), "size");
    EXPECT_EQ(sizeCall->getArguments(), nullptr) << "size() takes no arguments";
  }
};

// --- module / nets ----

TEST_F(QueuesSliceTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(QueuesSliceTest, ModuleHasTwoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 2u);
}

TEST_F(QueuesSliceTest, NetQExists) { EXPECT_NE(getNetQ(), nullptr); }

TEST_F(QueuesSliceTest, NetRExists) { EXPECT_NE(getNetR(), nullptr); }

// --- net "q": bounded queue "int q[$:5]" ----

TEST_F(QueuesSliceTest, NetQArrayTypeIsQueue) {
  const hldb::ArrayTypespec *const at = getArrayTypespec(getNetQ());
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiQueueArray) << "7.10: 'int q[$:5]' must be modeled as a queue array";
}

TEST_F(QueuesSliceTest, NetQArrayIsNotPacked) {
  const hldb::ArrayTypespec *const at = getArrayTypespec(getNetQ());
  ASSERT_NE(at, nullptr);
  EXPECT_FALSE(at->getPacked()) << "a queue dimension is an unpacked dimension";
}

TEST_F(QueuesSliceTest, NetQRangeLeftIsUnboundedDollar) {
  const hldb::ArrayTypespec *const at = getArrayTypespec(getNetQ());
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr);
  const hldb::Constant *const dollar = at->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(dollar, nullptr);
  EXPECT_EQ(dollar->getDecompile(), "$");
  EXPECT_EQ(dollar->getConstType(), vpiUnboundedConst);
}

TEST_F(QueuesSliceTest, NetQRangeRightIsBoundConstantFive) {
  const hldb::ArrayTypespec *const at = getArrayTypespec(getNetQ());
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr);
  const hldb::Constant *const bound = at->getRange()->getRightExpr<hldb::Constant>();
  ASSERT_NE(bound, nullptr) << "7.10.2.7: bounded queue must keep its ':5' bound as the range right expr";
  EXPECT_EQ(bound->getDecompile(), "5");
  EXPECT_EQ(bound->getConstType(), vpiUIntConst);
}

TEST_F(QueuesSliceTest, NetQElemTypespecIsSignedIntTypespec) {
  const hldb::ArrayTypespec *const at = getArrayTypespec(getNetQ());
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::IntTypespec *const elem = at->getElemTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "element type of 'int q[$:5]' should resolve to IntTypespec";
  EXPECT_TRUE(elem->getSigned());
}

TEST_F(QueuesSliceTest, NetQHasNoInitialValue) {
  const hldb::Variable *const q = getNetQ();
  ASSERT_NE(q, nullptr);
  EXPECT_EQ(q->getValue(), nullptr);
}

// --- net "r": unbounded queue "int r[$]" ----

TEST_F(QueuesSliceTest, NetRArrayTypeIsQueue) {
  const hldb::ArrayTypespec *const at = getArrayTypespec(getNetR());
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiQueueArray) << "7.10: 'int r[$]' must be modeled as a queue array";
}

TEST_F(QueuesSliceTest, NetRRangeLeftIsUnboundedDollar) {
  const hldb::ArrayTypespec *const at = getArrayTypespec(getNetR());
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr);
  const hldb::Constant *const dollar = at->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(dollar, nullptr);
  EXPECT_EQ(dollar->getDecompile(), "$");
  EXPECT_EQ(dollar->getConstType(), vpiUnboundedConst);
}

TEST_F(QueuesSliceTest, NetRRangeHasNoRightExpr) {
  const hldb::ArrayTypespec *const at = getArrayTypespec(getNetR());
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr);
  EXPECT_EQ(at->getRange()->getRightExpr(), nullptr) << "'int r[$]' is unbounded -- no ':N' bound";
}

TEST_F(QueuesSliceTest, NetRElemTypespecIsSignedIntTypespec) {
  const hldb::ArrayTypespec *const at = getArrayTypespec(getNetR());
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  const hldb::IntTypespec *const elem = at->getElemTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(elem, nullptr) << "element type of 'int r[$]' should resolve to IntTypespec";
  EXPECT_TRUE(elem->getSigned());
}

TEST_F(QueuesSliceTest, NetRHasNoInitialValue) {
  const hldb::Variable *const r = getNetR();
  ASSERT_NE(r, nullptr);
  EXPECT_EQ(r->getValue(), nullptr);
}

// --- initial process structure ----

TEST_F(QueuesSliceTest, ModuleHasOneInitialProcess) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  EXPECT_NE(any_cast<hldb::Initial>(top->getProcesses()->at(0)), nullptr);
}

TEST_F(QueuesSliceTest, InitialBeginHasSeventeenStmts) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 17u);
}

// --- q.push_back(0..5): fill q up to its bound ----

TEST_F(QueuesSliceTest, PushBacksZeroThroughFive) {
  for (uint32_t i = 0; i <= 5u; ++i) {
    ExpectPushBack(i, std::to_string(i));
  }
}

// --- $display(":assert: (%d == 6)", q.size) ----

TEST_F(QueuesSliceTest, SeventhStmtDisplayAssertsQSizeSix) {
  ExpectDisplayWithResolvedSize(6, ":assert: (%d == 6)", "q");
}

// --- r = q[2:4]: the normal case ----

TEST_F(QueuesSliceTest, FirstSliceAssignmentIsRAssignedQTwoToFour) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(7));
  ASSERT_NE(assign, nullptr) << "'r = q[2:4]' should be an Assignment";
  EXPECT_TRUE(assign->getBlocking());

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "r");
  EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr);

  const hldb::PartSelect *const rhs = assign->getRhs<hldb::PartSelect>();
  ASSERT_NE(rhs, nullptr) << "'q[2:4]' should be a PartSelect";
  EXPECT_EQ(rhs->getName(), "q[2:4]");
  const hldb::RefObj *const prefix = rhs->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->getName(), "q");
  EXPECT_NE(prefix->getActual<hldb::Variable>(), nullptr);

  ASSERT_NE(rhs->getRange(), nullptr);
  const hldb::Constant *const left = rhs->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "2");
  const hldb::Constant *const right = rhs->getRange()->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "4");
}

TEST_F(QueuesSliceTest, EighthStmtDisplayAssertsRSizeThree) {
  ExpectDisplayWithResolvedSize(8, ":assert: (%d == 3)", "r");
}

// --- r = q[4:2]: left > right, "gives empty queue" ----

TEST_F(QueuesSliceTest, SecondSliceAssignmentIsRAssignedQFourToTwo) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(9));
  ASSERT_NE(assign, nullptr) << "'r = q[4:2]' should be an Assignment";
  const hldb::PartSelect *const rhs = assign->getRhs<hldb::PartSelect>();
  ASSERT_NE(rhs, nullptr) << "'q[4:2]' should still be a PartSelect even though left > right";
  EXPECT_EQ(rhs->getName(), "q[4:2]");

  ASSERT_NE(rhs->getRange(), nullptr);
  const hldb::Constant *const left = rhs->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "4");
  const hldb::Constant *const right = rhs->getRange()->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "2");
}

TEST_F(QueuesSliceTest, TenthStmtDisplayAssertsRSizeZero) {
  ExpectDisplayWithResolvedSize(10, ":assert: (%d == 0)", "r");
}

// --- r = q[2:2]: left == right, single-element slice ----

TEST_F(QueuesSliceTest, ThirdSliceAssignmentIsRAssignedQTwoToTwo) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(11));
  ASSERT_NE(assign, nullptr) << "'r = q[2:2]' should be an Assignment";
  const hldb::PartSelect *const rhs = assign->getRhs<hldb::PartSelect>();
  ASSERT_NE(rhs, nullptr) << "'q[2:2]' should be a PartSelect";
  EXPECT_EQ(rhs->getName(), "q[2:2]");

  ASSERT_NE(rhs->getRange(), nullptr);
  const hldb::Constant *const left = rhs->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "2");
  const hldb::Constant *const right = rhs->getRange()->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "2");
}

TEST_F(QueuesSliceTest, TwelfthStmtDisplayAssertsRSizeOne) {
  ExpectDisplayWithResolvedSize(12, ":assert: (%d == 1)", "r");
}

// --- r = q[-2:2]: negative left bound, parsed as unary-minus Operation ----

TEST_F(QueuesSliceTest, FourthSliceAssignmentIsRAssignedQNegativeTwoToTwo) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(13));
  ASSERT_NE(assign, nullptr) << "'r = q[-2:2]' should be an Assignment";
  const hldb::PartSelect *const rhs = assign->getRhs<hldb::PartSelect>();
  ASSERT_NE(rhs, nullptr) << "'q[-2:2]' should be a PartSelect";
  EXPECT_EQ(rhs->getName(), "q[-2:2]");
  const hldb::RefObj *const prefix = rhs->getPrefix<hldb::RefObj>();
  ASSERT_NE(prefix, nullptr);
  EXPECT_EQ(prefix->getName(), "q");

  ASSERT_NE(rhs->getRange(), nullptr);
  const hldb::Operation *const left = rhs->getRange()->getLeftExpr<hldb::Operation>();
  ASSERT_NE(left, nullptr) << "'-2' should be a unary-minus Operation, not a folded negative Constant";
  EXPECT_EQ(left->getOpType(), vpiMinusOp);
  ASSERT_NE(left->getOperands(), nullptr);
  ASSERT_EQ(left->getOperands()->size(), 1u);
  const hldb::Constant *const leftOperand = any_cast<hldb::Constant>(left->getOperands()->at(0));
  ASSERT_NE(leftOperand, nullptr);
  EXPECT_EQ(leftOperand->getDecompile(), "2");

  const hldb::Constant *const right = rhs->getRange()->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "2");
}

TEST_F(QueuesSliceTest, FourteenthStmtDisplayAssertsRSizeThree) {
  ExpectDisplayWithResolvedSize(14, ":assert: (%d == 3)", "r");
}

// --- r = q[2:10]: right bound exceeds q's declared bound ----

TEST_F(QueuesSliceTest, FifthSliceAssignmentIsRAssignedQTwoToTen) {
  const hldb::Begin *const begin = getInitialBegin();
  ASSERT_NE(begin, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(begin->getStmts()->at(15));
  ASSERT_NE(assign, nullptr) << "'r = q[2:10]' should be an Assignment";
  const hldb::PartSelect *const rhs = assign->getRhs<hldb::PartSelect>();
  ASSERT_NE(rhs, nullptr) << "'q[2:10]' should be a PartSelect";
  EXPECT_EQ(rhs->getName(), "q[2:10]");

  ASSERT_NE(rhs->getRange(), nullptr);
  const hldb::Constant *const left = rhs->getRange()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "2");
  const hldb::Constant *const right = rhs->getRange()->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr) << "the out-of-range literal '10' should still parse as a plain Constant";
  EXPECT_EQ(right->getDecompile(), "10");
}

TEST_F(QueuesSliceTest, SixteenthStmtDisplayAssertsRSizeFour) {
  ExpectDisplayWithResolvedSize(16, ":assert: (%d == 4)", "r");
}

// --- structural completeness / design-level typespecs ----

TEST_F(QueuesSliceTest, ModuleHasNoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getContAssigns(), nullptr);
}

TEST_F(QueuesSliceTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(QueuesSliceTest, DesignHasModuleTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::ModuleTypespec *const mt = any_cast<hldb::ModuleTypespec>(m_design->getTypespecs()->at(0));
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

TEST_F(QueuesSliceTest, DesignHasIntTypespecSigned) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  const hldb::IntTypespec *const it = any_cast<hldb::IntTypespec>(m_design->getTypespecs()->at(1));
  ASSERT_NE(it, nullptr);
  EXPECT_TRUE(it->getSigned());
}

TEST_F(QueuesSliceTest, DesignHasStringTypespec) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  ASSERT_GT(m_design->getTypespecs()->size(), 2u);
  EXPECT_NE(any_cast<hldb::StringTypespec>(m_design->getTypespecs()->at(2)), nullptr);
}

TEST_F(QueuesSliceTest, NoBindErrorForSize) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "size"), nullptr)
      << "'q.size' without parens should not raise COMP_FAILED_TO_BIND (IEEE 1800-2017 7.24.4 permits "
         "omitting parens on a no-arg built-in method call)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
