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
// SV source under test: tests/Google/chapter-18/18.5.14--soft-constraints_0.sv
// (IEEE 1800-2017 18.5.14 == IEEE 1800-2023 18.5.13 "Soft constraints",
// which is the text used here)
//
//   class a;
//       rand int b;
//
//       constraint c {
//           soft b > 4;
//           soft b < 12; }
//   endclass
//
// ---------------------------------------------------------------------------
// WHAT WE HAVE TO CHECK (derived from the SV text + IEEE 1800-2023 only):
//   1. The file is legal -> no fatal/syntax/error. 18.5.13: "A soft
//      constraint is an expression_or_dist constraint expression preceded
//      by soft"; 18.5.13.1: soft is allowed on rand variables.
//   2. Class 'a' declares exactly one variable 'rand int b' -> vpiRand
//      (not randc: soft constraints may not be specified for randc
//      variables, 18.5.13.1), int typespec.
//   3. Class 'a' has exactly one constraint 'c': parent 'a', non-static
//      (vpiAutomatic == 1, 37.34 detail 1), active (18.9), in-body
//      (vpiAccessType 0), not virtual, not pure.
//   4. c holds exactly two items in source order (37.34 detail 5):
//        [0] "b > 4"  : vpiGtOp, vpiSoft == 1 (37.38), b bound to a::b,
//                       right operand literal 4,
//        [1] "b < 12" : vpiLtOp, vpiSoft == 1, b bound to a::b, literal 12.
//      Operand order is preserved (variable left, literal right) and the
//      'soft' qualifier is kept on EACH expression, not dropped or merged.
//
// CHECKED (TEST_F below):
//   - items 1 to 4.
//
// NOT CHECKED (and why):
//   - Solver behavior ("when there is no solution ... the solver shall
//     discard that soft constraint") and the resulting value 4 < b < 12:
//     run-time only, and there is no randomize() call in this file (see
//     soft-constraints_1 for the run-time variant).
//   - Relative priority of [1] over [0] (18.5.13.1, later is higher): it is
//     a solver concept with no object-model property; only the source order
//     that defines it (item 4) is checkable.
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
}  // namespace

namespace hlc {
class SoftConstraints0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.14--soft-constraints_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::ClassDefn *getClassA() const { return hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses()); }
};

// Check 1: legal file.
TEST_F(SoftConstraints0Test, CompilerReportsNoErrors) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0) << "soft constraints on a rand variable are legal (IEEE 1800-2023 Sec 18.5.13)";
}

// Check 2: class 'a' with 'rand int b'.
TEST_F(SoftConstraints0Test, ClassAHasRandIntB) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr) << "class 'a' not found";
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_EQ(cls->getVariables()->size(), 1u);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", cls->getVariables());
  ASSERT_NE(b, nullptr) << "variable 'b' not found";
  EXPECT_EQ(b->getRandType(), vpiRand) << "soft constraints only on rand (not randc) variables (18.5.13.1)";
  EXPECT_EQ(typespecKind(b), hldb::AnyType::IntTypespec);
}

// Check 3: one constraint 'c' with the default properties.
TEST_F(SoftConstraints0Test, ClassAHasOneConstraintC) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getConstraints(), nullptr);
  ASSERT_EQ(cls->getConstraints()->size(), 1u);
  const hldb::Constraint *const c = cls->getConstraints()->at(0);
  EXPECT_EQ(c->getName(), "c");
  EXPECT_EQ(c->getParent(), static_cast<const hldb::Any *>(cls));
  EXPECT_TRUE(c->getAutomatic()) << "not static, so vpiAutomatic is 1 (37.34 detail 1)";
  EXPECT_TRUE(c->getIsConstraintEnabled()) << "initially active (18.9)";
  EXPECT_EQ(c->getAccessType(), 0) << "in-body, not extern";
  EXPECT_FALSE(c->getVirtual());
  EXPECT_FALSE(c->getPure());
}

// Check 4: c is "soft b > 4; soft b < 12;" in that order.
TEST_F(SoftConstraints0Test, CHasTwoSoftItemsInOrder) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", cls->getVariables());
  ASSERT_NE(b, nullptr);
  const hldb::Constraint *const c = hldb::findByName<hldb::Constraint>("c", cls->getConstraints());
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getConstraintItems(), nullptr);
  ASSERT_EQ(c->getConstraintItems()->size(), 2u) << "c has two constraint items";
  expectRelationOnLiteral(c->getConstraintItems()->at(0), vpiGtOp, b, "4", true);
  expectRelationOnLiteral(c->getConstraintItems()->at(1), vpiLtOp, b, "12", true);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
