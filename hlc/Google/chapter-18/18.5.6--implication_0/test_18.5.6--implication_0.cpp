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

// Source under test: tests/Google/chapter-18/18.5.6--implication_0.sv
// (file name uses the 1800-2017 numbering; this is Sec 18.5.5 in 1800-2023)
//
//   class a;
//       rand int b1, b2;
//       constraint c1 { b1 == 5; }
//       constraint c2 { b1 == 5 -> b2 == 10; }
//   endclass
//
// IEEE 1800-2023 Sec 18.5.5 (Implication):
//   constraint_expression ::= expression -> constraint_set
//   'a -> b' is equivalent to (!a || b): if the condition holds, the
//   constraint set shall be satisfied, otherwise it is unconstrained.
// '==' binds tighter than '->', so c2 is (b1 == 5) -> (b2 == 10); it must
// NOT be parsed as b1 == (5 -> b2) == 10. With c1 forcing b1 == 5, the
// solver must produce b2 == 10.
// Note: the same text is also a legal 'expression' using the logical
// implication operator of Sec 11.4.7 (vpiImplyOp), which has identical
// meaning (!a || b). Both shapes are accepted here; what matters is the
// condition / body split.
// IEEE 1800-2023 Sec 37.38: implication -> vpiCondition (expr) and
//   vpiConstraintExpr iteration (the body, in source order).
//
// What we have to check:
//   - 'rand int b1, b2;' -> two rand signed int variables, no initializer.
//   - Constraints c1, c2 in source order, not pure, enabled.
//   - c1: single item b1 == 5 (vpiEqOp, b1 bound to a::b1, literal 5).
//   - c2: single item that is an implication (Implication object or
//     vpiImplyOp operation) with:
//       * condition = b1 == 5   (the whole equality, not just '5')
//       * body      = exactly one expression b2 == 10
//   - No constraint error.
//
// Checked:
//   - All items in "What we have to check" above.
//
// Not checked:
//   - Randomized values b1 == 5 / b2 == 10: runtime only.
//   - Exact vpiConstType of literals (tool-representation detail).
//
// Known model risk: HLDB types Implication::getConstraintExprs() as a
// ConstraintExpr collection, but IEEE Sec 37.38 allows a plain expr (here
// the Operation b2 == 10) in it. If HLC cannot store the Operation there,
// the body assertion fails; that is an HLDB model gap, not a test error.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/implication.h>
#include <hldb/int_typespec.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>

#include <string_view>

namespace hlc {
class Implication0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.6--implication_0.hlc"}); }
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
};

TEST_F(Implication0Test, RandIntVariables) {
  const hldb::ClassDefn *const a = findClassA();
  ASSERT_NE(a, nullptr) << "class 'a' not found";
  ASSERT_NE(a->getVariables(), nullptr);
  EXPECT_EQ(a->getVariables()->size(), 2u) << "'rand int b1, b2;' declares two variables";
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

TEST_F(Implication0Test, ConstraintsInSourceOrder) {
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

TEST_F(Implication0Test, C1IsB1Eq5) { expectVarEqLit(singleItem("c1"), "b1", "5"); }

TEST_F(Implication0Test, C2IsImplicationWithConditionAndBody) {
  const hldb::Any *const item = singleItem("c2");
  ASSERT_NE(item, nullptr) << "'c2' must hold exactly one item";

  const hldb::Any *cond = nullptr;
  const hldb::Any *body = nullptr;
  if (const hldb::Implication *const imp = any_cast<hldb::Implication>(item)) {
    // Sec 18.5.5 constraint form: expression -> constraint_set.
    cond = imp->getCondition();
    ASSERT_NE(imp->getConstraintExprs(), nullptr) << "implication must keep its constraint set";
    ASSERT_EQ(imp->getConstraintExprs()->size(), 1u) << "constraint set is the single 'b2 == 10'";
    body = imp->getConstraintExprs()->front();
  } else {
    // Sec 11.4.7 expression form: logical implication operator.
    ASSERT_EQ(item->getAnyType(), hldb::AnyType::Operation) << "'c2' must be an implication";
    const hldb::Operation *const op = any_cast<hldb::Operation>(item);
    ASSERT_EQ(op->getOpType(), vpiImplyOp) << "top-level operator of c2 must be '->', not '=='";
    ASSERT_NE(op->getOperands(), nullptr);
    ASSERT_EQ(op->getOperands()->size(), 2u);
    cond = op->getOperands()->at(0);
    body = op->getOperands()->at(1);
  }
  expectVarEqLit(cond, "b1", "5");
  expectVarEqLit(body, "b2", "10");
}

TEST_F(Implication0Test, ConstraintsShouldBeEnabledByDefaultButAreNot) {
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
