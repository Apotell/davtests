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
// SV source under test: tests/Google/chapter-18/18.5.14.2--discarding-soft-constraints_0.sv
// (IEEE 1800-2017 18.5.14.2 == IEEE 1800-2023 18.5.13.2 "Disabling soft
// constraints", which is the text used here)
//
//   class a;
//       rand int b;
//
//       constraint c1 {
//           soft b > 4;
//           soft b < 12; }
//
//       constraint c2 { disable soft b; }
//       constraint c3 { soft b == 20; }
//   endclass
//
// ---------------------------------------------------------------------------
// WHAT WE HAVE TO CHECK (derived from the SV text + IEEE 1800-2023 only):
//   1. The file is legal -> no fatal/syntax/error. Syntax 18-2 allows
//      "disable soft constraint_primary ;" as a constraint_expression, and
//      b is a random variable as 18.5.13.2 requires.
//   2. Class 'a' declares exactly 'rand int b' (vpiRand, int).
//   3. Class 'a' has exactly three constraints in syntactic order c1, c2,
//      c3 (37.34 detail 4). This order matters: 18.5.13.1 gives later
//      constraints higher priority, and 18.5.13.2 discards only the LOWER
//      priority soft constraints, so c1 is below c2 and c3 is above it.
//      Each: parent 'a', non-static (vpiAutomatic == 1), active (18.9),
//      in-body (vpiAccessType 0).
//   4. c1 holds, in order: [0] soft "b > 4" (vpiGtOp, vpiSoft == 1),
//      [1] soft "b < 12" (vpiLtOp, vpiSoft == 1), both bound to a::b.
//   5. c2 holds exactly one item, a "soft disable" constraint expression
//      (37.38) -- NOT a soft relational expression -- whose expression is a
//      reference bound to the random variable a::b.
//   6. c3 holds exactly one item: soft "b == 20" (vpiEqOp, vpiSoft == 1)
//      bound to a::b.
//
// CHECKED (TEST_F below):
//   - items 1 to 6.
//
// NOT CHECKED (and why):
//   - The discard effect itself (c2 discards c1's two soft constraints, c3
//     survives, so b == 20): solver/run-time only and there is no
//     randomize() call here (see discarding-soft-constraints_1/_3/_5 for the
//     run-time variant).
// ---------------------------------------------------------------------------

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>
#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/soft_disable.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

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

// Checks that 'item' is "disable soft <var>;".
void expectSoftDisableOf(const hldb::Any *item, const hldb::Variable *var) {
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(item->getAnyType(), hldb::AnyType::SoftDisable) << "'disable soft' is a soft disable item (37.38)";
  const hldb::SoftDisable *const disable = any_cast<hldb::SoftDisable>(item);
  ASSERT_NE(disable, nullptr);
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(disable->getExpr());
  ASSERT_NE(ref, nullptr) << "'disable soft' names the random variable '" << var->getName() << "'";
  EXPECT_EQ(ref->getName(), var->getName());
  EXPECT_EQ(ref->getActual(), static_cast<const hldb::Any *>(var))
      << "'" << ref->getName() << "' binds to the variable";
}
}  // namespace

namespace hlc {
class DiscardingSoftConstraints0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.14.2--discarding-soft-constraints_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::ClassDefn *getClassA() const { return hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses()); }
};

// Check 1: legal file.
TEST_F(DiscardingSoftConstraints0Test, CompilerReportsNoErrors) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0) << "'disable soft b;' on a rand variable is legal (IEEE 1800-2023 Sec 18.5.13.2)";
}

// Check 2: class 'a' with 'rand int b'.
TEST_F(DiscardingSoftConstraints0Test, ClassAHasRandIntB) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr) << "class 'a' not found";
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_EQ(cls->getVariables()->size(), 1u);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", cls->getVariables());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getRandType(), vpiRand) << "disable soft applies to a random variable (18.5.13.2)";
  EXPECT_EQ(typespecKind(b), hldb::AnyType::IntTypespec);
}

// Check 3: three constraints c1, c2, c3 in declaration order.
TEST_F(DiscardingSoftConstraints0Test, ConstraintsC1C2C3InDeclarationOrder) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getConstraints(), nullptr);
  ASSERT_EQ(cls->getConstraints()->size(), 3u);
  EXPECT_EQ(cls->getConstraints()->at(0)->getName(), "c1");
  EXPECT_EQ(cls->getConstraints()->at(1)->getName(), "c2");
  EXPECT_EQ(cls->getConstraints()->at(2)->getName(), "c3");
  for (const hldb::Constraint *const c : *cls->getConstraints()) {
    EXPECT_EQ(c->getParent(), static_cast<const hldb::Any *>(cls)) << c->getName();
    EXPECT_TRUE(c->getAutomatic()) << c->getName() << ": not static, so vpiAutomatic is 1 (37.34 detail 1)";
    EXPECT_TRUE(c->getIsConstraintEnabled()) << c->getName() << ": initially active (18.9)";
    EXPECT_EQ(c->getAccessType(), 0) << c->getName() << ": in-body, not extern";
  }
}

// Check 4: c1 is "soft b > 4; soft b < 12;".
TEST_F(DiscardingSoftConstraints0Test, C1HasTwoSoftItemsInOrder) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", cls->getVariables());
  ASSERT_NE(b, nullptr);
  const hldb::Constraint *const c1 = hldb::findByName<hldb::Constraint>("c1", cls->getConstraints());
  ASSERT_NE(c1, nullptr);
  ASSERT_NE(c1->getConstraintItems(), nullptr);
  ASSERT_EQ(c1->getConstraintItems()->size(), 2u);
  expectRelationOnLiteral(c1->getConstraintItems()->at(0), vpiGtOp, b, "4", true);
  expectRelationOnLiteral(c1->getConstraintItems()->at(1), vpiLtOp, b, "12", true);
}

// Check 5: c2 is "disable soft b;".
TEST_F(DiscardingSoftConstraints0Test, C2IsDisableSoftB) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", cls->getVariables());
  ASSERT_NE(b, nullptr);
  const hldb::Constraint *const c2 = hldb::findByName<hldb::Constraint>("c2", cls->getConstraints());
  ASSERT_NE(c2, nullptr);
  ASSERT_NE(c2->getConstraintItems(), nullptr);
  ASSERT_EQ(c2->getConstraintItems()->size(), 1u);
  expectSoftDisableOf(c2->getConstraintItems()->at(0), b);
}

// Check 6: c3 is "soft b == 20".
TEST_F(DiscardingSoftConstraints0Test, C3IsSoftBEqualsTwenty) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", cls->getVariables());
  ASSERT_NE(b, nullptr);
  const hldb::Constraint *const c3 = hldb::findByName<hldb::Constraint>("c3", cls->getConstraints());
  ASSERT_NE(c3, nullptr);
  ASSERT_NE(c3->getConstraintItems(), nullptr);
  ASSERT_EQ(c3->getConstraintItems()->size(), 1u);
  expectRelationOnLiteral(c3->getConstraintItems()->at(0), vpiEqOp, b, "20", true);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
