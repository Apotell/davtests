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

// Source under test: tests/Google/chapter-18/18.5.7--if-else-constraints_2.sv
// (file name uses the 1800-2017 numbering; this is Sec 18.5.6 in 1800-2023)
//
//   class a;
//       rand int b1, b2;
//       constraint c1 { b1 == 5; }
//       constraint c2 { if (b1 == 0) b2 == 10;
//                       else if (b1 == 5) b2 == 3;
//                       else b2 == 15;}
//   endclass
//
// IEEE 1800-2023 Sec 18.5.6: 'else if' is not a separate keyword. The
// grammar is 'if (expr) constraint_set [else constraint_set]', and the
// second 'if ... else ...' is simply the constraint_set of the first
// else. So c2 is ONE outer if-else whose else-part holds ONE nested
// if-else:
//     if (b1 == 0)  b2 == 10
//     else          { if (b1 == 5) b2 == 3  else b2 == 15 }
// c1 forces b1 == 5 -> outer condition false -> inner condition true ->
// b2 must be 3.
//
// What we have to check:
//   - 'rand int b1, b2;' -> two rand signed int variables, no initializer.
//   - Constraints c1, c2 in source order, not pure, enabled.
//   - c1: single item b1 == 5.
//   - c2: single item = outer ConstrIfElse:
//       * condition b1 == 0
//       * then-part exactly b2 == 10
//       * else-part exactly ONE item: a nested ConstrIfElse (the chain is
//         nested, not flattened into a 3-way object or a sibling item)
//   - Nested ConstrIfElse:
//       * condition b1 == 5
//       * then-part exactly b2 == 3
//       * else-part exactly b2 == 15
//   - No constraint error.
//
// Checked:
//   - All items in "What we have to check" above.
//
// Not checked:
//   - Randomized values (b1 == 5, b2 == 3): runtime only.
//   - Exact vpiConstType of literals (tool-representation detail).
//
// Known model risk: the leaf parts (b2 == 10 / 3 / 15) are Operations, but
// HLDB types the then/else collections as ConstraintExpr only (IEEE
// Sec 37.38 allows a plain expr). If HLC cannot store them, the leaf checks
// fail; that is an HLDB model gap. The nested ConstrIfElse IS a
// ConstraintExpr, so the nesting check does not depend on this gap.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
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
class IfElseConstraints2Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.7--if-else-constraints_2.hlc"}); }
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

  // The outer if-else of c2, or nullptr.
  static const hldb::ConstrIfElse *outerIfElse() { return any_cast<hldb::ConstrIfElse>(singleItem("c2")); }

  // The if-else nested in the else-part of the outer one, or nullptr.
  static const hldb::ConstrIfElse *innerIfElse() {
    const hldb::ConstrIfElse *const outer = outerIfElse();
    if (outer == nullptr || outer->getElseConstraintExprs() == nullptr || outer->getElseConstraintExprs()->size() != 1)
      return nullptr;
    return any_cast<hldb::ConstrIfElse>(outer->getElseConstraintExprs()->front());
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

TEST_F(IfElseConstraints2Test, RandIntVariables) {
  const hldb::ClassDefn *const a = findClassA();
  ASSERT_NE(a, nullptr) << "class 'a' not found";
  ASSERT_NE(a->getVariables(), nullptr);
  EXPECT_EQ(a->getVariables()->size(), 2u);
  for (std::string_view name : {"b1", "b2"}) {
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

TEST_F(IfElseConstraints2Test, ConstraintsInSourceOrder) {
  const hldb::ClassDefn *const a = findClassA();
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->getConstraints(), nullptr);
  ASSERT_EQ(a->getConstraints()->size(), 2u);
  EXPECT_EQ(a->getConstraints()->at(0)->getName(), "c1");
  EXPECT_EQ(a->getConstraints()->at(1)->getName(), "c2");
  for (const hldb::Constraint *const cons : *a->getConstraints()) {
    EXPECT_FALSE(cons->getPure()) << cons->getName();
  }
}

TEST_F(IfElseConstraints2Test, C1IsB1Eq5) { expectVarEqLit(singleItem("c1"), "b1", "5"); }

TEST_F(IfElseConstraints2Test, OuterIfElse) {
  const hldb::Any *const item = singleItem("c2");
  ASSERT_NE(item, nullptr) << "'c2' must hold exactly one (outer) item";
  ASSERT_EQ(item->getAnyType(), hldb::AnyType::ConstrIfElse);
  const hldb::ConstrIfElse *const outer = outerIfElse();
  ASSERT_NE(outer, nullptr);
  expectVarEqLit(outer->getCondition(), "b1", "0");
}

TEST_F(IfElseConstraints2Test, LeafBranchesShouldBeKeptButAreDropped) {
  const hldb::ConstrIfElse *const outer = outerIfElse();
  ASSERT_NE(outer, nullptr);
  expectSingleVarEqLit(outer->getIfConstraintExprs(), "b2", "10");
  const hldb::ConstrIfElse *const inner = innerIfElse();
  ASSERT_NE(inner, nullptr);
  expectSingleVarEqLit(inner->getIfConstraintExprs(), "b2", "3");
  expectSingleVarEqLit(inner->getElseConstraintExprs(), "b2", "15");
}

TEST_F(IfElseConstraints2Test, ElseIfIsNestedInOuterElse) {
  const hldb::ConstrIfElse *const outer = outerIfElse();
  ASSERT_NE(outer, nullptr);
  ASSERT_NE(outer->getElseConstraintExprs(), nullptr) << "outer else-part must be kept";
  ASSERT_EQ(outer->getElseConstraintExprs()->size(), 1u) << "outer else-part is exactly the nested if-else";
  const hldb::Any *const nested = outer->getElseConstraintExprs()->front();
  ASSERT_NE(nested, nullptr);
  EXPECT_EQ(nested->getAnyType(), hldb::AnyType::ConstrIfElse) << "'else if (...) ... else ...' nests a constr if else";
}

TEST_F(IfElseConstraints2Test, InnerIfElse) {
  const hldb::ConstrIfElse *const inner = innerIfElse();
  ASSERT_NE(inner, nullptr) << "nested if-else not found in outer else-part";
  expectVarEqLit(inner->getCondition(), "b1", "5");
}

TEST_F(IfElseConstraints2Test, ConstraintsShouldBeEnabledByDefaultButAreNot) {
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
