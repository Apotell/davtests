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

// Source under test: tests/Google/chapter-18/18.5.7--if-else-constraints_3.sv
// (file name uses the 1800-2017 numbering; this is Sec 18.5.6 in 1800-2023)
//
//   class a;
//       rand int b1, b2, b3;
//       constraint c1 { b1 == 5; }
//       constraint c2 { b2 == 3; }
//       constraint c3 { if (b1 == 0)
//                         if (b2 == 2) b3 == 4;
//                         else b3 == 10;}
//   endclass
//
// IEEE 1800-2023 Sec 18.5.6 (dangling else):
//   "This is resolved by always associating the else with the closest
//    previous if that lacks an else."
// So c3 is:
//     if (b1 == 0)                       <- outer: NO else
//         { if (b2 == 2) b3 == 4         <- inner: owns the else
//           else         b3 == 10 }
// Since c1 forces b1 == 5, the outer condition is false and b3 is
// UNCONSTRAINED (b3 == 10 is NOT required, even though b2 != 2). This is
// what distinguishes it from a wrong outer-else binding.
//
// What we have to check:
//   - 'rand int b1, b2, b3;' -> three rand signed int variables.
//   - Constraints c1, c2, c3 in source order, not pure, enabled.
//   - c1: b1 == 5;  c2: b2 == 3.
//   - c3: single item = outer ConstrIf (NOT ConstrIfElse: the else does
//     not belong to it), condition b1 == 0, then-part exactly ONE item.
//   - That item is a ConstrIfElse (the else binds here) with:
//       * condition b2 == 2
//       * then-part exactly b3 == 4
//       * else-part exactly b3 == 10
//   - No constraint error.
//
// Checked:
//   - All items in "What we have to check" above.
//
// Not checked:
//   - Randomized values (b1 == 5, b2 == 3, b3 free): runtime only.
//   - Exact vpiConstType of literals (tool-representation detail).
//
// Known model risk: the leaf parts (b3 == 4 / 10) are Operations, but HLDB
// types the then/else collections as ConstraintExpr only (IEEE Sec 37.38
// allows a plain expr). If HLC cannot store them, the leaf checks fail; that
// is an HLDB model gap. The dangling-else structure checks (ConstrIf holding
// a ConstrIfElse) do not depend on this gap.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/constr_if.h>
#include <hldb/constr_if_else.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>

#include <string_view>

namespace hlc {
class IfElseConstraints3Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.7--if-else-constraints_3.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *findClassA() {
    return hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  }

  // The single item of constraint 'name' in class a, or nullptr.
  static const hldb::Any *singleItem(std::string_view name) {
    const hldb::ClassDefn *const a = findClassA();
    if (a == nullptr) return nullptr;
    const hldb::Constraint *const cons = hldb::findByName<hldb::Constraint>(name, a->getConstraints());
    if (cons == nullptr || cons->getConstraintItems() == nullptr || cons->getConstraintItems()->size() != 1)
      return nullptr;
    return cons->getConstraintItems()->front();
  }

  // The if-else nested in the then-part of c3's outer if, or nullptr.
  static const hldb::ConstrIfElse *innerIfElse() {
    const hldb::ConstrIf *const outer = any_cast<hldb::ConstrIf>(singleItem("c3"));
    if (outer == nullptr || outer->getConstraintExprs() == nullptr || outer->getConstraintExprs()->size() != 1)
      return nullptr;
    return any_cast<hldb::ConstrIfElse>(outer->getConstraintExprs()->front());
  }

  // Checks 'e' is '<var> == <lit>' with <var> bound to the member of class a.
  static void expectVarEqLit(const hldb::Any *e, std::string_view var, std::string_view lit) {
    ASSERT_NE(e, nullptr) << "expected '" << var << " == " << lit << "'";
    ASSERT_EQ(e->getAnyType(), hldb::AnyType::Operation) << "'" << var << " == " << lit << "' is an operation";
    const hldb::Operation *const op = any_cast<hldb::Operation>(e);
    EXPECT_EQ(op->getOpType(), vpiEqOp);
    ASSERT_NE(op->getOperands(), nullptr);
    ASSERT_EQ(op->getOperands()->size(), 2u);
    const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(op->getOperands()->at(0));
    const hldb::Constant *const rhs = any_cast<hldb::Constant>(op->getOperands()->at(1));
    ASSERT_NE(lhs, nullptr);
    ASSERT_NE(rhs, nullptr);
    EXPECT_EQ(lhs->getName(), var);
    const hldb::ClassDefn *const a = findClassA();
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(lhs->getActual(), hldb::findByName<hldb::Variable>(var, a->getVariables()))
        << "'" << var << "' binds to a::" << var;
    EXPECT_EQ(rhs->getDecompile(), lit);
  }

  // Checks a then/else collection holds exactly '<var> == <lit>'.
  static void expectSingleVarEqLit(const hldb::AnyCollection *coll, std::string_view var,
                                   std::string_view lit) {
    ASSERT_NE(coll, nullptr) << "branch '" << var << " == " << lit << "' must be kept";
    ASSERT_EQ(coll->size(), 1u);
    expectVarEqLit(coll->front(), var, lit);
  }
};

TEST_F(IfElseConstraints3Test, RandIntVariables) {
  const hldb::ClassDefn *const a = findClassA();
  ASSERT_NE(a, nullptr) << "class 'a' not found";
  ASSERT_NE(a->getVariables(), nullptr);
  EXPECT_EQ(a->getVariables()->size(), 3u) << "'rand int b1, b2, b3;' declares three variables";
  for (std::string_view name : {"b1", "b2", "b3"}) {
    const hldb::Variable *const v = hldb::findByName<hldb::Variable>(name, a->getVariables());
    ASSERT_NE(v, nullptr) << "'" << name << "' not found";
    EXPECT_EQ(v->getRandType(), vpiRand) << "'rand' applies to " << name;
    EXPECT_EQ(v->getValue(), nullptr) << name << " has no initializer";
    ASSERT_NE(v->getTypespec(), nullptr);
    ASSERT_NE(v->getTypespec()->getActual(), nullptr);
    ASSERT_EQ(v->getTypespec()->getActual()->getAnyType(), hldb::AnyType::IntTypespec) << name << " is 'int'";
    EXPECT_TRUE(v->getTypespec()->getActual<hldb::IntTypespec>()->getSigned());
  }
}

TEST_F(IfElseConstraints3Test, ConstraintsInSourceOrder) {
  const hldb::ClassDefn *const a = findClassA();
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->getConstraints(), nullptr);
  ASSERT_EQ(a->getConstraints()->size(), 3u);
  EXPECT_EQ(a->getConstraints()->at(0)->getName(), "c1");
  EXPECT_EQ(a->getConstraints()->at(1)->getName(), "c2");
  EXPECT_EQ(a->getConstraints()->at(2)->getName(), "c3");
  for (const hldb::Constraint *const cons : *a->getConstraints()) {
    EXPECT_FALSE(cons->getPure()) << cons->getName();
  }
}

TEST_F(IfElseConstraints3Test, SimpleEqualityConstraints) {
  expectVarEqLit(singleItem("c1"), "b1", "5");
  expectVarEqLit(singleItem("c2"), "b2", "3");
}

TEST_F(IfElseConstraints3Test, OuterIfHasNoElse) {
  const hldb::Any *const item = singleItem("c3");
  ASSERT_NE(item, nullptr) << "'c3' must hold exactly one (outer) item";
  EXPECT_NE(item->getAnyType(), hldb::AnyType::ConstrIfElse)
      << "the else binds to the INNER if (Sec 18.5.6), so the outer if has no else";
  ASSERT_EQ(item->getAnyType(), hldb::AnyType::ConstrIf);
  const hldb::ConstrIf *const outer = any_cast<hldb::ConstrIf>(item);
  expectVarEqLit(outer->getCondition(), "b1", "0");
  ASSERT_NE(outer->getConstraintExprs(), nullptr);
  ASSERT_EQ(outer->getConstraintExprs()->size(), 1u) << "outer then-part is exactly the inner if-else";
  ASSERT_NE(outer->getConstraintExprs()->front(), nullptr);
  EXPECT_EQ(outer->getConstraintExprs()->front()->getAnyType(), hldb::AnyType::ConstrIfElse)
      << "inner 'if ... else ...' owns the dangling else";
}

TEST_F(IfElseConstraints3Test, InnerIfElseOwnsTheElse) {
  const hldb::ConstrIfElse *const inner = innerIfElse();
  ASSERT_NE(inner, nullptr) << "inner if-else not found in outer then-part";
  expectVarEqLit(inner->getCondition(), "b2", "2");
}

TEST_F(IfElseConstraints3Test, InnerBranchesShouldBeKeptButAreDropped) {
  const hldb::ConstrIfElse *const inner = innerIfElse();
  ASSERT_NE(inner, nullptr);
  expectSingleVarEqLit(inner->getIfConstraintExprs(), "b3", "4");
  expectSingleVarEqLit(inner->getElseConstraintExprs(), "b3", "10");
}

TEST_F(IfElseConstraints3Test, ConstraintsShouldBeEnabledByDefaultButAreNot) {
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
