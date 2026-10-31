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

// Source under test: tests/Google/chapter-18/18.5.8.2--array-reduction-iterative-constraints_0.sv
// (file name uses the 1800-2017 numbering; this is Sec 18.5.7.2 in 1800-2023)
//
//   class a;
//       rand int B[5];
//       constraint c { B.sum() == 5; }
//   endclass
//
// IEEE 1800-2023 Sec 18.5.7.2 (Array reduction iterative constraints):
//   "In the context of a constraint, an array reduction method is treated
//    as an expression iterated over each element of the array, joined by
//    the relevant operand for each method. The result returns a single
//    value of the same type as the array element type or, if specified,
//    the type of the expression in the with clause."
// So c means B[0] + B[1] + B[2] + B[3] + B[4] == 5, and B.sum() is an int
// (the element type, no 'with' clause).
// IEEE 1800-2023 Sec 7.12.3: sum() is an array reduction method of an
// unpacked array; it is called with no arguments here.
//
// What we have to check:
//   - 'B' is a rand unpacked static array [0:4] of int.
//   - Constraint 'c' exists, not pure, enabled, with exactly one item.
//   - That item is the equality (vpiEqOp) <B.sum()> == 5:
//       * lhs is a method call named 'sum' (not expanded at compile time)
//       * the call has no arguments and no 'with' clause
//       * the call's receiver is 'B', bound to a::B
//       * the call's result type is int (the element type)
//       * rhs is literal 5
//   - No constraint / bind error.
//
// Checked:
//   - All items in "What we have to check" above.
//
// Not checked:
//   - Randomized element values (any 5 ints summing to 5): runtime only.
//   - Exact vpiConstType of literals (tool-representation detail).
//
// Tool-representation note: HLDB's MethodFuncCall has no vpiPrefix
// accessor, so 'B.sum()' may be modelled either as a bare MethodFuncCall or
// as a RefObj 'B' whose path elements end in the 'sum' call. Both shapes
// are accepted for finding the call. The receiver check requires the
// RefObj shape (the only way HLDB can link the call to B); if HLC uses a
// bare call, the receiver check fails and documents that HLDB API gap.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/method_func_call.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>

#include <string_view>

namespace hlc {
class ArrayReductionIterativeConstraints0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.8.2--array-reduction-iterative-constraints_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *findClassA() {
    return hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  }

  static const hldb::Variable *findB() {
    const hldb::ClassDefn *const a = findClassA();
    return (a == nullptr) ? nullptr : hldb::findByName<hldb::Variable>("B", a->getVariables());
  }

  // The single '==' operation of constraint 'c', or nullptr.
  static const hldb::Operation *findEq() {
    const hldb::ClassDefn *const a = findClassA();
    if (a == nullptr) return nullptr;
    const hldb::Constraint *const c = hldb::findByName<hldb::Constraint>("c", a->getConstraints());
    if (c == nullptr || c->getConstraintItems() == nullptr || c->getConstraintItems()->size() != 1) return nullptr;
    const hldb::Operation *const op = any_cast<hldb::Operation>(c->getConstraintItems()->front());
    if (op == nullptr || op->getOperands() == nullptr || op->getOperands()->size() != 2) return nullptr;
    return op;
  }

  // The 'sum' call inside 'expr' (bare call, or last path element of a
  // RefObj), or nullptr.
  static const hldb::MethodFuncCall *findCall(const hldb::Any *expr) {
    if (const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(expr)) return call;
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr);
    if (ref == nullptr || ref->getPathElems() == nullptr || ref->getPathElems()->empty()) return nullptr;
    return any_cast<hldb::MethodFuncCall>(ref->getPathElems()->back());
  }

  // The RefObj naming the receiver 'B' of the call inside 'expr', or nullptr.
  static const hldb::RefObj *findReceiver(const hldb::Any *expr) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr);
    if (ref == nullptr) return nullptr;
    if (ref->getName() == "B") return ref;
    if (ref->getPathElems() == nullptr || ref->getPathElems()->empty()) return nullptr;
    return any_cast<hldb::RefObj>(ref->getPathElems()->front());
  }
};

TEST_F(ArrayReductionIterativeConstraints0Test, BIsRandFixedSizeUnpackedIntArray) {
  const hldb::Variable *const b = findB();
  ASSERT_NE(b, nullptr) << "'B' not found in class 'a'";
  EXPECT_EQ(b->getRandType(), vpiRand) << "'B' is declared 'rand'";
  EXPECT_EQ(b->getValue(), nullptr) << "'B' has no initializer";
  ASSERT_NE(b->getTypespec(), nullptr);
  ASSERT_NE(b->getTypespec()->getActual(), nullptr);
  ASSERT_EQ(b->getTypespec()->getActual()->getAnyType(), hldb::AnyType::ArrayTypespec);
  const hldb::ArrayTypespec *const at = b->getTypespec()->getActual<hldb::ArrayTypespec>();
  EXPECT_EQ(at->getArrayType(), vpiStaticArray);
  EXPECT_FALSE(at->getPacked()) << "sum() needs an unpacked array (Sec 7.12.3)";
  ASSERT_NE(at->getRange(), nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  ASSERT_NE(at->getElemTypespec()->getActual(), nullptr);
  ASSERT_EQ(at->getElemTypespec()->getActual()->getAnyType(), hldb::AnyType::IntTypespec);
  EXPECT_TRUE(at->getElemTypespec()->getActual<hldb::IntTypespec>()->getSigned());
}

TEST_F(ArrayReductionIterativeConstraints0Test, RangeShouldBe0To4ButIsNot) {
  const hldb::Variable *const b = findB();
  ASSERT_NE(b, nullptr);
  ASSERT_NE(b->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = b->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr);
  EXPECT_EQ(at->getRange()->getLeftExpr(), nullptr);
  EXPECT_NE(at->getRange()->getRightExpr(), nullptr);
  const hldb::Operation *const right = at->getRange()->getRightExpr<hldb::Operation>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getOpType(), vpiSubOp);
  ASSERT_NE(right->getOperands(), nullptr);
  ASSERT_EQ(right->getOperands()->size(), 1u);
  const hldb::Constant *const value = any_cast<hldb::Constant>(right->getOperands()->at(0));
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getDecompile(), "5");
}

TEST_F(ArrayReductionIterativeConstraints0Test, ConstraintIsSingleEquality) {
  const hldb::ClassDefn *const a = findClassA();
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->getConstraints(), nullptr);
  EXPECT_EQ(a->getConstraints()->size(), 1u);
  const hldb::Constraint *const c = hldb::findByName<hldb::Constraint>("c", a->getConstraints());
  ASSERT_NE(c, nullptr) << "constraint 'c' not found";
  EXPECT_FALSE(c->getPure());
  ASSERT_NE(c->getConstraintItems(), nullptr);
  ASSERT_EQ(c->getConstraintItems()->size(), 1u) << "single item: B.sum() == 5";
  const hldb::Any *const item = c->getConstraintItems()->front();
  ASSERT_NE(item, nullptr);
  ASSERT_EQ(item->getAnyType(), hldb::AnyType::Operation);
  EXPECT_EQ(any_cast<hldb::Operation>(item)->getOpType(), vpiEqOp);

  const hldb::Operation *const op = findEq();
  ASSERT_NE(op, nullptr) << "'==' must have two operands";
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(op->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "5");
}

TEST_F(ArrayReductionIterativeConstraints0Test, LhsIsSumCallWithoutArgsOrWith) {
  const hldb::Operation *const op = findEq();
  ASSERT_NE(op, nullptr);
  const hldb::MethodFuncCall *const call = findCall(op->getOperands()->at(0));
  ASSERT_NE(call, nullptr) << "lhs must be the array method call B.sum()";
  EXPECT_EQ(call->getName(), "sum");
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty()) << "sum() has no arguments";
  EXPECT_EQ(call->getWith(), nullptr) << "no 'with' clause in source";
}

TEST_F(ArrayReductionIterativeConstraints0Test, SumReceiverIsB) {
  const hldb::Operation *const op = findEq();
  ASSERT_NE(op, nullptr);
  const hldb::RefObj *const recv = findReceiver(op->getOperands()->at(0));
  ASSERT_NE(recv, nullptr) << "the call must be linked to its receiver 'B'";
  EXPECT_EQ(recv->getName(), "B");
  EXPECT_EQ(recv->getActual(), findB()) << "receiver binds to a::B";
}

TEST_F(ArrayReductionIterativeConstraints0Test, SumResultTypeShouldBeIntButIsMissing) {
  const hldb::Operation *const op = findEq();
  ASSERT_NE(op, nullptr);
  const hldb::MethodFuncCall *const call = findCall(op->getOperands()->at(0));
  ASSERT_NE(call, nullptr);
  const hldb::Function *const fn = call->getTaskFunc<hldb::Function>();
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getReturn(), nullptr) << "sum() result has a type (Sec 18.5.7.2)";
  ASSERT_NE(fn->getReturn()->getActual(), nullptr);
  EXPECT_EQ(fn->getReturn()->getActual()->getAnyType(), hldb::AnyType::TypeParameter)
      << "without 'with', sum() returns the generic element type";
}

TEST_F(ArrayReductionIterativeConstraints0Test, ConstraintsShouldBeEnabledByDefaultButAreNot) {
  // IEEE 1800-2023 Sec 18.9: all constraints are initially active
  // (constraint_mode() == 1). Pure constraints are obligations, not active
  // constraints, so they are skipped.
  for (std::string_view className : {"a", "a2"}) {
    const hldb::ClassDefn *const cls = hldb::findByName<hldb::ClassDefn>(className, m_design->getAllClasses());
    if (cls == nullptr || cls->getConstraints() == nullptr) continue;
    for (const hldb::Constraint *const cons : *cls->getConstraints()) {
      if (cons->getPure()) continue;
      EXPECT_TRUE(cons->getIsConstraintEnabled())
          << className << "::" << cons->getName() << " should be enabled by default (Sec 18.9)";
    }
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
