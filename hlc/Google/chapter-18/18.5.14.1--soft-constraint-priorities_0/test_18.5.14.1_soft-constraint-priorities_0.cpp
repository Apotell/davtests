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

// ---------------------------------------------------------------------------
// SV source under test: tests/Google/chapter-18/18.5.14.1--soft-constraint-priorities_0.sv
// (IEEE 1800-2017 18.5.14.1 == IEEE 1800-2023 18.5.13.1 "Soft constraint
// priorities", which is the text used here)
//
//   class a1;
//       rand int b;
//
//       constraint c1 {
//           soft b > 4;
//           soft b < 12; }
//   endclass
//
//   class a2 extends a1;
//       constraint c2 { soft b == 20; }
//   endclass
//
// ---------------------------------------------------------------------------
// WHAT WE HAVE TO CHECK (derived from the SV text + IEEE 1800-2023 only):
//   1. The file is legal -> no fatal/syntax/error.
//   2. Class a1 declares exactly one variable 'rand int b'. It must be
//      'rand': 18.5.13.1 "Soft constraints can only be specified on random
//      variables; they may not be specified for randc variables."
//   3. a1 has exactly one constraint 'c1' (parent a1, non-static ->
//      vpiAutomatic == 1, active, in-body) with TWO items in source order
//      (37.34 detail 5 -- the order is what defines their relative soft
//      priority, "Constraint expressions that appear later in the construct
//      have higher priority"):
//        [0] soft "b > 4"   (vpiGtOp, vpiSoft == 1)
//        [1] soft "b < 12"  (vpiLtOp, vpiSoft == 1)
//   4. a2 extends a1: the extends clause binds to class a1, and a1 lists a2
//      as a derived class.
//   5. a2 does NOT redeclare b (b is inherited), so a2 has no own variable.
//   6. a2 has exactly one own constraint 'c2' (parent a2, non-static,
//      active, in-body) with one item: soft "b == 20" (vpiEqOp,
//      vpiSoft == 1) whose 'b' binds to the INHERITED variable a1::b.
//
// CHECKED (TEST_F below):
//   - items 1 to 6.
//
// NOT CHECKED (and why):
//   - The priority ordering itself (c2 > c1[1] > c1[0], "Constraints in a
//     derived class shall have higher priority than all constraints in its
//     superclasses") and the resulting value: priorities are only used by
//     the solver at randomize() time and are not an object-model property.
//     There is no randomize() call here (see soft-constraint-priorities_1
//     for the run-time result b == 20).
// ---------------------------------------------------------------------------

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/Tests/Test.h>
#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/constant.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/extends.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <algorithm>
#include <string_view>

namespace {
hldb::AnyType typespecKind(const hldb::Variable *var) {
  const hldb::RefTypespec *const ref = var->getTypespec();
  if ((ref == nullptr) || (ref->getActual() == nullptr)) return hldb::AnyType::Any;
  return ref->getActual()->getAnyType();
}

// Checks that 'item' is the relation "<var> <opType> <literal>" with the given soft flag.
void expectRelationOnLiteral(const hldb::Any *item, int32_t opType, const hldb::Variable *var, std::string_view literal,
                             bool soft) {
  const hldb::Operation *const op = any_cast<hldb::Operation>(item);
  ASSERT_NE(op, nullptr) << "constraint item should be an Operation";
  EXPECT_EQ(op->getOpType(), opType);
  EXPECT_EQ(op->getSoft(), soft) << "soft flag (37.38 vpiSoft)";
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(op->getOperands()->at(0));
  ASSERT_NE(ref, nullptr) << "left operand should reference '" << var->getName() << "'";
  EXPECT_EQ(ref->getName(), var->getName());
  EXPECT_EQ(ref->getActual(), static_cast<const hldb::Any *>(var))
      << "'" << ref->getName() << "' binds to the variable";
  const hldb::Constant *const value = any_cast<hldb::Constant>(op->getOperands()->at(1));
  ASSERT_NE(value, nullptr) << "right operand should be the literal " << literal;
  EXPECT_EQ(value->getDecompile(), literal);
}

void expectPlainConstraint(const hldb::Constraint *c, const hldb::ClassDefn *owner) {
  EXPECT_EQ(c->getParent(), static_cast<const hldb::Any *>(owner)) << c->getName() << ": parent class";
  EXPECT_TRUE(c->getAutomatic()) << c->getName() << ": not static, so vpiAutomatic is 1 (37.34 detail 1)";
  EXPECT_TRUE(c->getIsConstraintEnabled()) << c->getName() << ": initially active (18.9)";
  EXPECT_EQ(c->getAccessType(), 0) << c->getName() << ": in-body, not extern";
}
}  // namespace

namespace hlc {
class SoftConstraintPriorities0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.14.1--soft-constraint-priorities_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::ClassDefn *getClass(std::string_view name) const {
    return hldb::findByName<hldb::ClassDefn>(name, m_design->getAllClasses());
  }
};

// Check 1: legal file.
TEST_F(SoftConstraintPriorities0Test, CompilerReportsNoErrors) {
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "soft constraints on a rand variable are legal (IEEE 1800-2023 Sec 18.5.13)";
}

// Check 2: a1 declares 'rand int b'.
TEST_F(SoftConstraintPriorities0Test, A1HasRandIntB) {
  const hldb::ClassDefn *const a1 = getClass("a1");
  ASSERT_NE(a1, nullptr) << "class 'a1' not found";
  ASSERT_NE(a1->getVariables(), nullptr);
  EXPECT_EQ(a1->getVariables()->size(), 1u);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", a1->getVariables());
  ASSERT_NE(b, nullptr) << "variable 'b' not found";
  EXPECT_EQ(b->getRandType(), vpiRand) << "soft constraints only on rand (not randc) variables (18.5.13.1)";
  EXPECT_EQ(typespecKind(b), hldb::AnyType::IntTypespec);
}

// Check 3: a1::c1 is "soft b > 4; soft b < 12;" in that order.
TEST_F(SoftConstraintPriorities0Test, A1C1HasTwoSoftItemsInOrder) {
  const hldb::ClassDefn *const a1 = getClass("a1");
  ASSERT_NE(a1, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", a1->getVariables());
  ASSERT_NE(b, nullptr);
  ASSERT_NE(a1->getConstraints(), nullptr);
  ASSERT_EQ(a1->getConstraints()->size(), 1u);
  const hldb::Constraint *const c1 = a1->getConstraints()->at(0);
  EXPECT_EQ(c1->getName(), "c1");
  expectPlainConstraint(c1, a1);
  ASSERT_NE(c1->getConstraintItems(), nullptr);
  ASSERT_EQ(c1->getConstraintItems()->size(), 2u) << "c1 has two constraint items";
  expectRelationOnLiteral(c1->getConstraintItems()->at(0), vpiGtOp, b, "4", true);
  expectRelationOnLiteral(c1->getConstraintItems()->at(1), vpiLtOp, b, "12", true);
}

// Check 4: a2 extends a1.
TEST_F(SoftConstraintPriorities0Test, A2ExtendsA1) {
  const hldb::ClassDefn *const a1 = getClass("a1");
  const hldb::ClassDefn *const a2 = getClass("a2");
  ASSERT_NE(a1, nullptr);
  ASSERT_NE(a2, nullptr) << "class 'a2' not found";
  ASSERT_NE(a2->getExtends(), nullptr) << "a2 has an 'extends a1' clause";
  ASSERT_NE(a2->getExtends()->getClassTypespecs(), nullptr);
  ASSERT_EQ(a2->getExtends()->getClassTypespecs()->size(), 1u);
  const hldb::ClassTypespec *const base =
      a2->getExtends()->getClassTypespecs()->at(0)->getActual<hldb::ClassTypespec>();
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->getClassDefn(), a1) << "base class binds to a1";

  ASSERT_NE(a1->getDerivedClasses(), nullptr) << "a1 should list a2 as a derived class";
  EXPECT_NE(std::find(a1->getDerivedClasses()->begin(), a1->getDerivedClasses()->end(), a2),
            a1->getDerivedClasses()->end());
  EXPECT_EQ(a1->getExtends(), nullptr) << "a1 has no base class";
}

// Check 5: a2 inherits b and does not redeclare it.
TEST_F(SoftConstraintPriorities0Test, A2DoesNotRedeclareB) {
  const hldb::ClassDefn *const a2 = getClass("a2");
  ASSERT_NE(a2, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("b", a2->getVariables()), nullptr)
      << "b is declared only in a1; a2 inherits it";
}

// Check 6: a2::c2 is "soft b == 20" on the inherited a1::b.
TEST_F(SoftConstraintPriorities0Test, A2C2IsSoftBEqualsTwentyOnInheritedB) {
  const hldb::ClassDefn *const a1 = getClass("a1");
  const hldb::ClassDefn *const a2 = getClass("a2");
  ASSERT_NE(a1, nullptr);
  ASSERT_NE(a2, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", a1->getVariables());
  ASSERT_NE(b, nullptr);
  ASSERT_NE(a2->getConstraints(), nullptr);
  ASSERT_EQ(a2->getConstraints()->size(), 1u) << "a2 declares exactly one own constraint";
  const hldb::Constraint *const c2 = a2->getConstraints()->at(0);
  EXPECT_EQ(c2->getName(), "c2");
  expectPlainConstraint(c2, a2);
  ASSERT_NE(c2->getConstraintItems(), nullptr);
  ASSERT_EQ(c2->getConstraintItems()->size(), 1u);
  expectRelationOnLiteral(c2->getConstraintItems()->at(0), vpiEqOp, b, "20", true);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
