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

// Source under test: tests/Google/chapter-18/18.5.8.1--foreach-iterative-constraints_0.sv
// (file name uses the 1800-2017 numbering; this is Sec 18.5.7.1 in 1800-2023)
//
//   class a;
//       rand int B[5];
//       constraint c { foreach ( B [ i ] ) B[i] == 5; }
//   endclass
//
// IEEE 1800-2023 Sec 7.4.2: 'int B[5]' is a fixed-size unpacked array,
// equivalent to 'int B[0:4]' (5 elements, each a signed int).
// IEEE 1800-2023 Sec 18.5.7.1 (foreach iterative constraints):
//   foreach ( array_identifier [ loop_variables ] ) constraint_set
//   - Each loop variable corresponds to one dimension of the array; here
//     'i' iterates over the single dimension, 0 to 4.
//   - The scope of 'i' is the foreach construct; its type is implicitly
//     that of the array index.
//   - It shall be an error for a loop variable to have the same name as
//     the array ('i' != 'B', so legal).
// So c means: B[0] == 5, B[1] == 5, ..., B[4] == 5 (sum 25).
// IEEE 1800-2023 Sec 37.38: constr foreach -> vpiVariables (the array),
//   vpiLoopVars (index variables, left-to-right), vpiConstraintExpr (body).
//
// What we have to check:
//   - 'B' is a rand variable whose type is an UNPACKED, STATIC
//     (fixed-size) array (vpiStaticArray) with range [0:4] and element
//     type int (signed).
//   - Constraint 'c' exists, not pure, enabled, with exactly one item.
//   - That item is a ConstrForeach (not expanded into 5 separate equalities
//     at compile time).
//   - The foreach array reference is 'B', bound to a::B.
//   - Exactly ONE loop variable, named 'i'.
//   - Body = exactly one expression B[i] == 5:
//       * lhs is a select of array 'B' (element select)
//       * its index is a reference to 'i' bound to the foreach loop var
//         (not to some class member)
//       * rhs is literal 5
//   - No constraint / bind error ('i' must bind inside the foreach).
//
// Checked:
//   - All items in "What we have to check" above.
//
// Not checked:
//   - Randomized element values (all 5): runtime only.
//   - Exact select class used for B[i] (BitSelect vs VarSelect): both are
//     accepted, IEEE only requires an element select of B indexed by i.
//   - Exact vpiConstType of literals (tool-representation detail).
//
// Known model risk: HLDB types ConstrForeach::getConstraintExprs() as a
// ConstraintExpr collection, but IEEE Sec 37.38 allows a plain expr (here
// the Operation B[i] == 5) in it. If HLC cannot store it there, the body
// check fails; that is an HLDB model gap, not a test error.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/bit_select.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/constr_foreach.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/var_select.h>
#include <hldb/variable.h>

#include <string_view>

namespace hlc {
class ForeachIterativeConstraints0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.8.1--foreach-iterative-constraints_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *findClassA() {
    return hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  }

  static const hldb::Variable *findB() {
    const hldb::ClassDefn *const a = findClassA();
    return (a == nullptr) ? nullptr : hldb::findByName<hldb::Variable>("B", a->getVariables());
  }

  // The single ConstrForeach item of constraint 'c', or nullptr.
  static const hldb::ConstrForeach *findForeach() {
    const hldb::ClassDefn *const a = findClassA();
    if (a == nullptr) return nullptr;
    const hldb::Constraint *const c = hldb::findByName<hldb::Constraint>("c", a->getConstraints());
    if (c == nullptr || c->getConstraintItems() == nullptr || c->getConstraintItems()->size() != 1) return nullptr;
    return any_cast<hldb::ConstrForeach>(c->getConstraintItems()->front());
  }

  // Index expression of an element select (BitSelect or VarSelect), or nullptr.
  static const hldb::Any *selectIndex(const hldb::Any *sel) {
    if (const hldb::BitSelect *const bs = any_cast<hldb::BitSelect>(sel)) return bs->getIndex();
    if (const hldb::VarSelect *const vs = any_cast<hldb::VarSelect>(sel)) return vs->getIndex();
    return nullptr;
  }
};

TEST_F(ForeachIterativeConstraints0Test, BIsRandFixedSizeUnpackedIntArray) {
  const hldb::Variable *const b = findB();
  ASSERT_NE(b, nullptr) << "'B' not found in class 'a'";
  EXPECT_EQ(b->getRandType(), vpiRand) << "'B' is declared 'rand'";
  EXPECT_EQ(b->getValue(), nullptr) << "'B' has no initializer";

  const hldb::RefTypespec *const rt = b->getTypespec();
  ASSERT_NE(rt, nullptr);
  ASSERT_NE(rt->getActual(), nullptr);
  ASSERT_EQ(rt->getActual()->getAnyType(), hldb::AnyType::ArrayTypespec) << "'B[5]' is an array";
  const hldb::ArrayTypespec *const at = rt->getActual<hldb::ArrayTypespec>();
  EXPECT_EQ(at->getArrayType(), vpiStaticArray) << "[5] is a fixed-size (static) array";
  EXPECT_FALSE(at->getPacked()) << "dimension after the name is unpacked (Sec 7.4)";

  ASSERT_NE(at->getRange(), nullptr) << "[5] has a range";

  ASSERT_NE(at->getElemTypespec(), nullptr);
  ASSERT_NE(at->getElemTypespec()->getActual(), nullptr);
  ASSERT_EQ(at->getElemTypespec()->getActual()->getAnyType(), hldb::AnyType::IntTypespec) << "element type is int";
  EXPECT_TRUE(at->getElemTypespec()->getActual<hldb::IntTypespec>()->getSigned());
}

TEST_F(ForeachIterativeConstraints0Test, RangeShouldBe0To4ButIsNot) {
  const hldb::Variable *const b = findB();
  ASSERT_NE(b, nullptr);
  ASSERT_NE(b->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = b->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  const hldb::Range *const range = at->getRange();
  ASSERT_NE(range, nullptr);
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

TEST_F(ForeachIterativeConstraints0Test, ConstraintHoldsSingleForeach) {
  const hldb::ClassDefn *const a = findClassA();
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->getConstraints(), nullptr);
  EXPECT_EQ(a->getConstraints()->size(), 1u);
  const hldb::Constraint *const c = hldb::findByName<hldb::Constraint>("c", a->getConstraints());
  ASSERT_NE(c, nullptr) << "constraint 'c' not found";
  EXPECT_FALSE(c->getPure());
  ASSERT_NE(c->getConstraintItems(), nullptr);
  ASSERT_EQ(c->getConstraintItems()->size(), 1u) << "one foreach item, not 5 unrolled equalities";
  ASSERT_NE(c->getConstraintItems()->front(), nullptr);
  EXPECT_EQ(c->getConstraintItems()->front()->getAnyType(), hldb::AnyType::ConstrForeach);
}

TEST_F(ForeachIterativeConstraints0Test, ForeachArrayAndLoopVariable) {
  const hldb::ConstrForeach *const fe = findForeach();
  ASSERT_NE(fe, nullptr);

  const hldb::RefObj *const arr = fe->getVariable<hldb::RefObj>();
  ASSERT_NE(arr, nullptr) << "foreach must keep the iterated array (vpiVariables)";
  EXPECT_EQ(arr->getName(), "B");
  EXPECT_EQ(arr->getActual(), findB()) << "foreach array binds to a::B";
}

TEST_F(ForeachIterativeConstraints0Test, LoopVariableShouldBeKeptButIsNot) {
  const hldb::ConstrForeach *const fe = findForeach();
  ASSERT_NE(fe, nullptr);
  const hldb::RefObj *const arr = fe->getVariable<hldb::RefObj>();
  ASSERT_NE(arr, nullptr);
  ASSERT_NE(fe->getLoopVars(), nullptr);
  ASSERT_EQ(fe->getLoopVars()->size(), 1u) << "one loop variable for the one dimension";
  const hldb::Any *const i = fe->getLoopVars()->front();
  ASSERT_NE(i, nullptr);
  EXPECT_EQ(i->getName(), "i");
  EXPECT_NE(i->getName(), arr->getName()) << "loop var must not reuse the array name (Sec 18.5.7.1)";
}

TEST_F(ForeachIterativeConstraints0Test, BodyShouldBeBiEq5ButIsDropped) {
  const hldb::ConstrForeach *const fe = findForeach();
  ASSERT_NE(fe, nullptr);
  ASSERT_NE(fe->getConstraintExprs(), nullptr) << "foreach body must be kept";
  ASSERT_EQ(fe->getConstraintExprs()->size(), 1u) << "body is the single 'B[i] == 5'";
  const hldb::Any *const body = fe->getConstraintExprs()->front();
  ASSERT_NE(body, nullptr);
  ASSERT_EQ(body->getAnyType(), hldb::AnyType::Operation);
  const hldb::Operation *const op = any_cast<hldb::Operation>(body);
  EXPECT_EQ(op->getOpType(), vpiEqOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);

  const hldb::Any *const lhs = op->getOperands()->at(0);
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "B[i]") << "lhs selects an element of B";
  const hldb::Any *const idx = selectIndex(lhs);
  ASSERT_NE(idx, nullptr) << "lhs must be an element select B[...]";
  const hldb::RefObj *const iRef = any_cast<hldb::RefObj>(idx);
  ASSERT_NE(iRef, nullptr) << "index is a reference to the loop variable";
  EXPECT_EQ(iRef->getName(), "i");
  ASSERT_NE(fe->getLoopVars(), nullptr);
  ASSERT_FALSE(fe->getLoopVars()->empty());
  EXPECT_EQ(iRef->getActual(), fe->getLoopVars()->front()) << "'i' in B[i] binds to the foreach loop variable";

  const hldb::Constant *const rhs = any_cast<hldb::Constant>(op->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "5");
}

TEST_F(ForeachIterativeConstraints0Test, NoConstraintOrBindError) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "c"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "i"), nullptr) << "'i' is declared by the foreach";
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "B"), nullptr);
}
TEST_F(ForeachIterativeConstraints0Test, ConstraintsShouldBeEnabledByDefaultButAreNot) {
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

#ifndef DAVTESTS_BULK_BUILD
int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
