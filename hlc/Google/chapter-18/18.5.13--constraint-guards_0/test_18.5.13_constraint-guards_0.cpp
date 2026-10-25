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
// SV source under test: tests/Google/chapter-18/18.5.13--constraint-guards_0.sv
// (IEEE 1800-2017 18.5.13 == IEEE 1800-2023 18.5.12 "Constraint guards",
// which is the text used here)
//
//   class b;
//       int d1;
//   endclass
//
//   class a;
//       rand int b1;
//       b next;
//
//       constraint c1 { if (next == null) b1 == 5; }
//   endclass
//
// ---------------------------------------------------------------------------
// WHAT WE HAVE TO CHECK (derived from the SV text + IEEE 1800-2023 only):
//   1. The file is legal -> no fatal/syntax/error.
//   2. Class 'b' declares one plain (non-rand) variable 'int d1'.
//   3. Class 'a' declares:
//        - 'rand int b1'  -> vpiRand, int typespec,
//        - 'b next'       -> a class handle whose type binds to class 'b',
//          and is NOT rand (vpiNotRand). This matters: 18.5.12 says guard
//          expressions may only involve constants, STATE variables and
//          object handle comparisons, so 'next' must be a state variable.
//   4. Class 'a' has exactly one constraint 'c1': parent 'a', non-static
//      (vpiAutomatic == 1, 37.34 detail 1), active, in-body.
//   5. c1 has one item, an 'if' constraint WITHOUT else (37.38 "constr if",
//      not "constr if else"):
//        - its condition is the handle comparison "next == null": vpiEqOp,
//          left operand binds to the handle 'next', right operand is the
//          constant null (vpiConstType == vpiNullConst),
//        - its body holds exactly one constraint expression, the hard
//          equality "b1 == 5" bound to b1.
//
// CHECKED (TEST_F below):
//   - items 1 to 5.
//
// NOT CHECKED (and why):
//   - The guard's 4-state (FALSE/TRUE/ERROR/RANDOM) evaluation and
//     "constraint is eliminated when the guard is FALSE": these happen when
//     randomize() runs; there is no call here (see constraint-guards_1 for
//     the run-time result b1 == 5 when next is null).
// ---------------------------------------------------------------------------

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/Tests/Test.h>
#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/constant.h>
#include <hldb/constr_if.h>
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

const hldb::ClassDefn *classOf(const hldb::Variable *var) {
  if ((var == nullptr) || (var->getTypespec() == nullptr)) return nullptr;
  const hldb::ClassTypespec *const ts = var->getTypespec()->getActual<hldb::ClassTypespec>();
  return (ts == nullptr) ? nullptr : ts->getClassDefn();
}

// Checks that 'expr' is a reference named like 'var' that binds to 'var'.
void expectRefTo(const hldb::Any *expr, const hldb::Variable *var) {
  ASSERT_NE(expr, nullptr);
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr);
  ASSERT_NE(ref, nullptr) << "operand should be a reference to '" << var->getName() << "'";
  EXPECT_EQ(ref->getName(), var->getName());
  EXPECT_EQ(ref->getActual(), static_cast<const hldb::Any *>(var))
      << "reference '" << ref->getName() << "' should bind to the class variable";
}
}  // namespace

namespace hlc {
class ConstraintGuards0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.13--constraint-guards_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::ClassDefn *getClass(std::string_view name) const {
    return hldb::findByName<hldb::ClassDefn>(name, m_design->getAllClasses());
  }
};

// Check 1: legal file.
TEST_F(ConstraintGuards0Test, CompilerReportsNoErrors) {
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "a handle-vs-null guard is legal (IEEE 1800-2023 Sec 18.5.12)";
}

// Check 2: class 'b' has a plain 'int d1'.
TEST_F(ConstraintGuards0Test, ClassBHasPlainIntD1) {
  const hldb::ClassDefn *const clsB = getClass("b");
  ASSERT_NE(clsB, nullptr) << "class 'b' not found";
  ASSERT_NE(clsB->getVariables(), nullptr);
  EXPECT_EQ(clsB->getVariables()->size(), 1u);
  const hldb::Variable *const d1 = hldb::findByName<hldb::Variable>("d1", clsB->getVariables());
  ASSERT_NE(d1, nullptr) << "variable 'd1' not found";
  EXPECT_EQ(d1->getRandType(), vpiNotRand) << "d1 is not declared rand";
  EXPECT_EQ(typespecKind(d1), hldb::AnyType::IntTypespec);
  EXPECT_EQ(clsB->getConstraints(), nullptr) << "class 'b' declares no constraints";
}

// Check 3: class 'a' has 'rand int b1' and the state handle 'b next'.
TEST_F(ConstraintGuards0Test, ClassAHasRandB1AndStateHandleNext) {
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr) << "class 'a' not found";
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_EQ(cls->getVariables()->size(), 2u);

  const hldb::Variable *const b1 = hldb::findByName<hldb::Variable>("b1", cls->getVariables());
  ASSERT_NE(b1, nullptr) << "variable 'b1' not found";
  EXPECT_EQ(b1->getRandType(), vpiRand);
  EXPECT_EQ(typespecKind(b1), hldb::AnyType::IntTypespec);

  const hldb::Variable *const next = hldb::findByName<hldb::Variable>("next", cls->getVariables());
  ASSERT_NE(next, nullptr) << "variable 'next' not found";
  EXPECT_EQ(next->getRandType(), vpiNotRand) << "next is a state variable (18.5.12 guards use state variables)";
  EXPECT_EQ(classOf(next), getClass("b")) << "next is a handle of class 'b'";
}

// Check 4: one constraint c1.
TEST_F(ConstraintGuards0Test, ClassAHasOneConstraintC1) {
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getConstraints(), nullptr);
  ASSERT_EQ(cls->getConstraints()->size(), 1u);
  const hldb::Constraint *const c1 = cls->getConstraints()->at(0);
  EXPECT_EQ(c1->getName(), "c1");
  EXPECT_EQ(c1->getParent(), static_cast<const hldb::Any *>(cls));
  EXPECT_TRUE(c1->getAutomatic()) << "not static, so vpiAutomatic is 1 (37.34 detail 1)";
  EXPECT_TRUE(c1->getIsConstraintEnabled()) << "initially active (18.9)";
  EXPECT_EQ(c1->getAccessType(), 0);
}

// Check 5: c1 is "if (next == null) b1 == 5;".
TEST_F(ConstraintGuards0Test, C1IsIfNextIsNullThenB1EqualsFive) {
  const hldb::ClassDefn *const cls = getClass("a");
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const b1 = hldb::findByName<hldb::Variable>("b1", cls->getVariables());
  const hldb::Variable *const next = hldb::findByName<hldb::Variable>("next", cls->getVariables());
  ASSERT_NE(b1, nullptr);
  ASSERT_NE(next, nullptr);

  const hldb::Constraint *const c1 = hldb::findByName<hldb::Constraint>("c1", cls->getConstraints());
  ASSERT_NE(c1, nullptr);
  ASSERT_NE(c1->getConstraintItems(), nullptr);
  ASSERT_EQ(c1->getConstraintItems()->size(), 1u);
  const hldb::Any *const item = c1->getConstraintItems()->at(0);
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(item->getAnyType(), hldb::AnyType::ConstrIf) << "'if' without 'else' is a constr if (37.38)";
  const hldb::ConstrIf *const guard = any_cast<hldb::ConstrIf>(item);
  ASSERT_NE(guard, nullptr);

  // Guard: object handle comparison "next == null".
  const hldb::Operation *const cond = any_cast<hldb::Operation>(guard->getCondition());
  ASSERT_NE(cond, nullptr) << "guard condition is 'next == null'";
  EXPECT_EQ(cond->getOpType(), vpiEqOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  expectRefTo(cond->getOperands()->at(0), next);
  const hldb::Constant *const nullLiteral = any_cast<hldb::Constant>(cond->getOperands()->at(1));
  ASSERT_NE(nullLiteral, nullptr) << "right operand of the guard is the constant null";
  EXPECT_EQ(nullLiteral->getConstType(), vpiNullConst);

  // Guarded constraint: "b1 == 5".
  ASSERT_NE(guard->getConstraintExprs(), nullptr);
  ASSERT_EQ(guard->getConstraintExprs()->size(), 1u) << "the guard protects exactly one constraint";
  const hldb::Operation *const eq = any_cast<hldb::Operation>(guard->getConstraintExprs()->at(0));
  ASSERT_NE(eq, nullptr);
  EXPECT_EQ(eq->getOpType(), vpiEqOp);
  EXPECT_FALSE(eq->getSoft());
  ASSERT_NE(eq->getOperands(), nullptr);
  ASSERT_EQ(eq->getOperands()->size(), 2u);
  expectRefTo(eq->getOperands()->at(0), b1);
  const hldb::Constant *const five = any_cast<hldb::Constant>(eq->getOperands()->at(1));
  ASSERT_NE(five, nullptr);
  EXPECT_EQ(five->getDecompile(), "5");
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
