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
// SV source under test: tests/Google/chapter-18/18.5.10--variable-ordering_0.sv
// (tag 18.5.10 is the IEEE 1800-2017 number; the same clause is 18.5.9
// "Variable ordering" in IEEE 1800-2023, which is the text used here)
//
//   class a;
//       rand bit b1;
//       rand int b2;
//
//       constraint c1 { b1 -> b2 == 0; }
//       constraint c2 { solve b1 before b2; }
//   endclass
//
// ---------------------------------------------------------------------------
// WHAT WE HAVE TO CHECK (derived from the SV text + IEEE 1800-2023 only):
//   1. The file is legal: both variables are rand (not randc), integral, and
//      there is no circular ordering -> no fatal/syntax/semantic error.
//   2. Class 'a' exists at compilation-unit scope.
//   3. b1 is declared 'rand bit'  -> vpiRandType == vpiRand, bit typespec.
//      b2 is declared 'rand int'  -> vpiRandType == vpiRand, int typespec.
//      (18.5.9: only rand variables are allowed in solve...before.)
//   4. Class 'a' has exactly two constraints, returned in syntactic order
//      c1 then c2 (37.34 detail 4). Each one:
//        - has class 'a' as parent (37.34 vpiParent),
//        - is active by default (18.9 "All constraints are initially active"),
//        - is NOT static, so vpiAutomatic must be 1 (37.34 detail 1:
//          vpiAutomatic == 0 means "declared static"),
//        - is declared in-body, so vpiAccessType is 0, not vpiExternAcc
//          (37.34 detail 3), and is neither virtual nor pure.
//   5. c1 holds one item: the implication "b1 -> (b2 == 0)" (18.5.6).
//      The condition must bind to variable b1; the consequent must be the
//      equality b2 == 0 with operand order preserved (b2 first, 0 second),
//      and b2 must bind to variable b2. It is a hard constraint (no 'soft').
//   6. c2 holds one item: a constraint ordering (37.34) whose solve-before
//      list is exactly [b1] and solve-after list is exactly [b2], each
//      bound to the right class variable. The ordering must not be swapped.
//
// CHECKED (TEST_F below):
//   - items 1 to 6 above.
//
// NOT CHECKED (and why):
//   - The changed value probabilities of Table 18-2 (s chosen 50/50 first).
//     That is constraint-solver behavior at randomize() time; HLC is a
//     compiler/elaborator and there is no randomize() call in this file.
//   - "solve...before does not change the solution space": also solver-only.
//   - Note on item 5: 18.5.6 writes the construct as
//     "expression -> constraint_set", but the same text also parses as an
//     expression_or_dist using the logical implication operator (11.4.7).
//     Both mean (!b1 || (b2 == 0)), so the test accepts either an
//     Implication object (37.38) or an Operation with vpiImplyOp, and then
//     checks the condition/consequent contents strictly.
// ---------------------------------------------------------------------------

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/Tests/Test.h>
#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/constraint.h>
#include <hldb/constraint_ordering.h>
#include <hldb/design.h>
#include <hldb/implication.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace {
struct ImplicationParts {
  const hldb::Any *m_condition = nullptr;
  const hldb::Any *m_consequent = nullptr;
};

// Accepts both legal models of "cond -> expr" (see NOT CHECKED note above).
ImplicationParts getImplicationParts(const hldb::Any *item) {
  ImplicationParts parts;
  if (const hldb::Implication *const impl = any_cast<hldb::Implication>(item)) {
    parts.m_condition = impl->getCondition();
    if ((impl->getConstraintExprs() != nullptr) && (impl->getConstraintExprs()->size() == 1)) {
      parts.m_consequent = impl->getConstraintExprs()->at(0);
    }
  } else if (const hldb::Operation *const op = any_cast<hldb::Operation>(item)) {
    if ((op->getOpType() == vpiImplyOp) && (op->getOperands() != nullptr) && (op->getOperands()->size() == 2)) {
      parts.m_condition = op->getOperands()->at(0);
      parts.m_consequent = op->getOperands()->at(1);
    }
  }
  return parts;
}

hldb::AnyType typespecKind(const hldb::Variable *var) {
  const hldb::RefTypespec *const ref = var->getTypespec();
  if ((ref == nullptr) || (ref->getActual() == nullptr)) return hldb::AnyType::Any;
  return ref->getActual()->getAnyType();
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
class VariableOrdering0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.10--variable-ordering_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::ClassDefn *getClassA() const { return hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses()); }
};

// Check 1: legal file -> no fatal, syntax or semantic error.
TEST_F(VariableOrdering0Test, CompilerReportsNoErrors) {
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0) << "rand b1/b2 with 'solve b1 before b2' is legal per IEEE 1800-2023 Sec 18.5.9";
}

// Check 2: class 'a' exists, is a plain (non-virtual, non-derived) class.
TEST_F(VariableOrdering0Test, ClassAExists) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr) << "class 'a' not found";
  EXPECT_EQ(cls->getName(), "a");
  EXPECT_FALSE(cls->getVirtual());
  EXPECT_EQ(cls->getExtends(), nullptr) << "class 'a' has no 'extends' clause";
}

// Check 3: b1 is 'rand bit', b2 is 'rand int'.
TEST_F(VariableOrdering0Test, B1IsRandBitAndB2IsRandInt) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_EQ(cls->getVariables()->size(), 2u) << "class 'a' declares exactly b1 and b2";

  const hldb::Variable *const b1 = hldb::findByName<hldb::Variable>("b1", cls->getVariables());
  ASSERT_NE(b1, nullptr) << "variable 'b1' not found";
  EXPECT_EQ(b1->getRandType(), vpiRand) << "b1 is declared 'rand'";
  EXPECT_EQ(typespecKind(b1), hldb::AnyType::BitTypespec) << "b1 is declared 'bit'";

  const hldb::Variable *const b2 = hldb::findByName<hldb::Variable>("b2", cls->getVariables());
  ASSERT_NE(b2, nullptr) << "variable 'b2' not found";
  EXPECT_EQ(b2->getRandType(), vpiRand) << "b2 is declared 'rand' (not 'randc')";
  EXPECT_EQ(typespecKind(b2), hldb::AnyType::IntTypespec) << "b2 is declared 'int'";
}

// Check 4: two constraints c1, c2 in declaration order, with the right properties.
TEST_F(VariableOrdering0Test, ConstraintsC1AndC2InDeclarationOrder) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getConstraints(), nullptr);
  ASSERT_EQ(cls->getConstraints()->size(), 2u);
  EXPECT_EQ(cls->getConstraints()->at(0)->getName(), "c1") << "37.34 detail 4: syntactic order";
  EXPECT_EQ(cls->getConstraints()->at(1)->getName(), "c2") << "37.34 detail 4: syntactic order";

  for (const hldb::Constraint *const c : *cls->getConstraints()) {
    EXPECT_EQ(c->getParent(), static_cast<const hldb::Any *>(cls)) << c->getName() << ": parent is class 'a'";
    EXPECT_TRUE(c->getIsConstraintEnabled()) << c->getName() << ": constraints are initially active (18.9)";
    EXPECT_TRUE(c->getAutomatic()) << c->getName() << ": not declared static, so vpiAutomatic is 1 (37.34 detail 1)";
    EXPECT_EQ(c->getAccessType(), 0) << c->getName() << ": declared in-body, not extern (37.34 detail 3)";
    EXPECT_FALSE(c->getVirtual()) << c->getName();
    EXPECT_FALSE(c->getPure()) << c->getName();
  }
}

// Check 5: c1 is "b1 -> (b2 == 0)".
TEST_F(VariableOrdering0Test, C1IsB1ImpliesB2EqualsZero) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const b1 = hldb::findByName<hldb::Variable>("b1", cls->getVariables());
  const hldb::Variable *const b2 = hldb::findByName<hldb::Variable>("b2", cls->getVariables());
  ASSERT_NE(b1, nullptr);
  ASSERT_NE(b2, nullptr);

  const hldb::Constraint *const c1 = hldb::findByName<hldb::Constraint>("c1", cls->getConstraints());
  ASSERT_NE(c1, nullptr) << "constraint 'c1' not found";
  ASSERT_NE(c1->getConstraintItems(), nullptr);
  ASSERT_EQ(c1->getConstraintItems()->size(), 1u) << "c1 has exactly one constraint item";

  const ImplicationParts parts = getImplicationParts(c1->getConstraintItems()->at(0));
  ASSERT_NE(parts.m_condition, nullptr) << "c1 item should be an implication 'b1 -> ...' (18.5.6)";
  ASSERT_NE(parts.m_consequent, nullptr) << "c1 implication should have exactly one consequent";
  expectRefTo(parts.m_condition, b1);

  const hldb::Operation *const eq = any_cast<hldb::Operation>(parts.m_consequent);
  ASSERT_NE(eq, nullptr) << "consequent should be the equality 'b2 == 0'";
  EXPECT_EQ(eq->getOpType(), vpiEqOp);
  EXPECT_FALSE(eq->getSoft()) << "c1 is a hard constraint";
  ASSERT_NE(eq->getOperands(), nullptr);
  ASSERT_EQ(eq->getOperands()->size(), 2u);
  expectRefTo(eq->getOperands()->at(0), b2);
  const hldb::Constant *const zero = any_cast<hldb::Constant>(eq->getOperands()->at(1));
  ASSERT_NE(zero, nullptr) << "right operand of '==' is the literal 0";
  EXPECT_EQ(zero->getDecompile(), "0");
}

// Check 6: c2 is "solve b1 before b2" -> before = [b1], after = [b2].
TEST_F(VariableOrdering0Test, C2IsSolveB1BeforeB2) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const b1 = hldb::findByName<hldb::Variable>("b1", cls->getVariables());
  const hldb::Variable *const b2 = hldb::findByName<hldb::Variable>("b2", cls->getVariables());
  ASSERT_NE(b1, nullptr);
  ASSERT_NE(b2, nullptr);

  const hldb::Constraint *const c2 = hldb::findByName<hldb::Constraint>("c2", cls->getConstraints());
  ASSERT_NE(c2, nullptr) << "constraint 'c2' not found";
  ASSERT_NE(c2->getConstraintItems(), nullptr);
  ASSERT_EQ(c2->getConstraintItems()->size(), 1u) << "c2 has exactly one constraint item";

  const hldb::ConstraintOrdering *const order = any_cast<hldb::ConstraintOrdering>(c2->getConstraintItems()->at(0));
  ASSERT_NE(order, nullptr) << "c2 item should be a ConstraintOrdering (37.34)";

  ASSERT_NE(order->getSolveBefores(), nullptr);
  ASSERT_EQ(order->getSolveBefores()->size(), 1u) << "solve-before list is exactly [b1]";
  expectRefTo(order->getSolveBefores()->at(0), b1);

  ASSERT_NE(order->getSolveAfters(), nullptr);
  ASSERT_EQ(order->getSolveAfters()->size(), 1u) << "solve-after list is exactly [b2]";
  expectRefTo(order->getSolveAfters()->at(0), b2);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
