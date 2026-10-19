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

// Source under test: tests/Google/chapter-18/18.5.7--if-else-constraints_0.sv
// (file name uses the 1800-2017 numbering; this is Sec 18.5.6 in 1800-2023)
//
//   class a;
//       rand int b1, b2;
//       constraint c1 { b1 == 5; }
//       constraint c2 { if(b1 == 5) b2 == 10; }
//   endclass
//
// IEEE 1800-2023 Sec 18.5.6 (if-else constraints):
//   constraint_expression ::= if ( expression ) constraint_set
//                             [ else constraint_set ]
//   If the expression is true, the first constraint set shall be satisfied;
//   the else part is OPTIONAL. Here there is no else, so when b1 != 5 the
//   value of b2 is unconstrained. With c1 forcing b1 == 5, b2 must be 10.
// IEEE 1800-2023 Sec 37.38: 'constr if' (no else) and 'constr if else' are
//   distinct objects; both carry vpiCondition and a vpiConstraintExpr
//   iteration for the then-part. Only 'constr if else' has vpiElseConst.
//
// What we have to check:
//   - 'rand int b1, b2;' -> two rand signed int variables, no initializer.
//   - Constraints c1, c2 in source order, not pure, enabled.
//   - c1: single item b1 == 5.
//   - c2: single item that is a ConstrIf (NOT ConstrIfElse, since there is
//     no 'else'), with:
//       * condition = b1 == 5
//       * then-part = exactly one expression b2 == 10
//   - No constraint error.
//
// Checked:
//   - All items in "What we have to check" above.
//
// Not checked:
//   - Randomized values (b1 == 5, b2 == 10): runtime only.
//   - Exact vpiConstType of literals (tool-representation detail).
//
// Known model risk: HLDB types ConstrIf::getConstraintExprs() as a
// ConstraintExpr collection, but IEEE Sec 37.38 allows a plain expr in it.
// If HLC cannot store 'b2 == 10' there, the then-part check fails; that is an
// HLDB model gap, not a test error.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/constr_if.h>
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
class IfElseConstraints0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.7--if-else-constraints_0.hlc"}); }
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

TEST_F(IfElseConstraints0Test, RandIntVariables) {
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

TEST_F(IfElseConstraints0Test, ConstraintsInSourceOrder) {
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

TEST_F(IfElseConstraints0Test, C1IsB1Eq5) { expectVarEqLit(singleItem("c1"), "b1", "5"); }

TEST_F(IfElseConstraints0Test, C2IsIfWithoutElse) {
  const hldb::Any *const item = singleItem("c2");
  ASSERT_NE(item, nullptr) << "'c2' must hold exactly one item";
  EXPECT_NE(item->getAnyType(), hldb::AnyType::ConstrIfElse) << "there is no 'else' in c2";
  ASSERT_EQ(item->getAnyType(), hldb::AnyType::ConstrIf) << "'if (...) ...' is a constr if (Sec 37.38)";
  const hldb::ConstrIf *const cif = any_cast<hldb::ConstrIf>(item);

  expectVarEqLit(cif->getCondition(), "b1", "5");
}

TEST_F(IfElseConstraints0Test, ThenPartShouldBeKeptButIsDropped) {
  const hldb::ConstrIf *const cif = any_cast<hldb::ConstrIf>(singleItem("c2"));
  ASSERT_NE(cif, nullptr);
  ASSERT_NE(cif->getConstraintExprs(), nullptr) << "then-part must be kept";
  ASSERT_EQ(cif->getConstraintExprs()->size(), 1u) << "then-part is the single 'b2 == 10'";
  expectVarEqLit(cif->getConstraintExprs()->front(), "b2", "10");
}

TEST_F(IfElseConstraints0Test, NoConstraintError) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "c1"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "c2"), nullptr);
}
TEST_F(IfElseConstraints0Test, ConstraintsShouldBeEnabledByDefaultButAreNot) {
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
