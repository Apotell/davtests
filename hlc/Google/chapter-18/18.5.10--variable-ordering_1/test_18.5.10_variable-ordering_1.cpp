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
// SV source under test: tests/Google/chapter-18/18.5.10--variable-ordering_1.sv
// (tag 18.5.10 is the IEEE 1800-2017 number; the same clause is 18.5.9
// "Variable ordering" in IEEE 1800-2023, which is the text used here)
//
//   /*
//   :should_fail_because: randc vars are not allowed, they are always solved
//                         before any other
//   :type: simulation
//   */
//   class a;
//       rand bit b1;
//       randc int b2;
//
//       constraint c1 { b1 -> b2 == 0; }
//       constraint c2 { solve b1 before b2; }
//   endclass
//
// ---------------------------------------------------------------------------
// WHAT WE HAVE TO CHECK (derived from the SV text + IEEE 1800-2023 only):
//   1. The file is ILLEGAL. IEEE 1800-2023 Sec 18.5.9 restrictions on
//      variable ordering: "randc variables are not allowed. randc variables
//      are always solved before any other." b2 is 'randc' and appears in
//      'solve b1 before b2', so the compiler shall report an error.
//      - coarse check: at least one fatal/syntax/error diagnostic exists.
//      - specific check: an "illegal constraint" diagnostic
//        (COMP_ILLEGAL_CONSTRAINT) is reported for constraint 'c2' on line
//        23, the line holding 'solve b1 before b2'.
//   2. The model still shows WHY it is illegal:
//      - b1 is 'rand bit'  -> vpiRandType == vpiRand, bit typespec.
//      - b2 is 'randc int' -> vpiRandType == vpiRandC, int typespec.
//   3. c2 is a constraint ordering whose solve-after list is [b2] and that
//      reference binds to the randc variable b2 (the illegal operand), and
//      whose solve-before list is [b1].
//   4. c1 itself is legal (randc variables may appear in normal
//      constraints): one implication "b1 -> (b2 == 0)" bound to b1/b2.
//   5. Constraint order c1, c2 (37.34 detail 4); both have class 'a' as
//      parent; neither is static (vpiAutomatic == 1, 37.34 detail 1).
//
// CHECKED (TEST_F below):
//   - items 1 to 5 above.
//
// NOT CHECKED (and why):
//   - The run-time effect ("randc always solved first") -- solver behavior,
//     HLC has no randomize() execution and there is no call in this file.
//   - The exact error wording/location -- not defined by the standard.
//   - Which symbol the diagnostic names is not defined by the standard; the
//     specific check names the constraint 'c2' and its line, which is what
//     pins the error to the illegal solve...before item.
//   - Item 4 accepts either an Implication object (37.38) or an Operation
//     with vpiImplyOp, because "b1 -> b2 == 0" parses both as an implication
//     constraint (18.5.6) and as a logical implication expression (11.4.7);
//     both mean (!b1 || (b2 == 0)).
// ---------------------------------------------------------------------------

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
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
class VariableOrdering1Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.10--variable-ordering_1.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::ClassDefn *getClassA() const { return hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses()); }
};

// Check 1 (coarse): the illegal randc-in-solve-before must produce a diagnostic.
TEST_F(VariableOrdering1Test, CompilerRejectsRandcVariableInSolveBefore) {
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GT(stats.nbFatal + stats.nbSyntax + stats.nbError, 0)
      << "'solve b1 before b2' with b2 declared randc is illegal: IEEE 1800-2023 Sec 18.5.9 "
         "says randc variables are not allowed in variable ordering";
}

// Check 1 (specific): the illegal-constraint diagnostic is on c2's solve...before item (line 23).
TEST_F(VariableOrdering1Test, IllegalConstraintErrorOnSolveBeforeConstraintC2) {
  EXPECT_NE(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "c2", 23), nullptr)
      << "expected an illegal-constraint error for constraint 'c2' ('solve b1 before b2' with randc b2, line 23) "
         "(IEEE 1800-2023 Sec 18.5.9)";
}

// Check 2: b1 is 'rand bit', b2 is 'randc int'.
TEST_F(VariableOrdering1Test, B1IsRandBitAndB2IsRandcInt) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr) << "class 'a' not found";
  ASSERT_NE(cls->getVariables(), nullptr);
  EXPECT_EQ(cls->getVariables()->size(), 2u) << "class 'a' declares exactly b1 and b2";

  const hldb::Variable *const b1 = hldb::findByName<hldb::Variable>("b1", cls->getVariables());
  ASSERT_NE(b1, nullptr) << "variable 'b1' not found";
  EXPECT_EQ(b1->getRandType(), vpiRand) << "b1 is declared 'rand'";
  EXPECT_EQ(typespecKind(b1), hldb::AnyType::BitTypespec) << "b1 is declared 'bit'";

  const hldb::Variable *const b2 = hldb::findByName<hldb::Variable>("b2", cls->getVariables());
  ASSERT_NE(b2, nullptr) << "variable 'b2' not found";
  EXPECT_EQ(b2->getRandType(), vpiRandC) << "b2 is declared 'randc' (not 'rand')";
  EXPECT_EQ(typespecKind(b2), hldb::AnyType::IntTypespec) << "b2 is declared 'int'";
}

// Check 3: c2 orders b1 before the randc variable b2.
TEST_F(VariableOrdering1Test, C2SolveAfterListReferencesRandcB2) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const b1 = hldb::findByName<hldb::Variable>("b1", cls->getVariables());
  const hldb::Variable *const b2 = hldb::findByName<hldb::Variable>("b2", cls->getVariables());
  ASSERT_NE(b1, nullptr);
  ASSERT_NE(b2, nullptr);

  const hldb::Constraint *const c2 = hldb::findByName<hldb::Constraint>("c2", cls->getConstraints());
  ASSERT_NE(c2, nullptr) << "constraint 'c2' not found";
  ASSERT_NE(c2->getConstraintItems(), nullptr);
  ASSERT_EQ(c2->getConstraintItems()->size(), 1u);

  const hldb::ConstraintOrdering *const order = any_cast<hldb::ConstraintOrdering>(c2->getConstraintItems()->at(0));
  ASSERT_NE(order, nullptr) << "c2 item should be a ConstraintOrdering (37.34)";

  ASSERT_NE(order->getSolveBefores(), nullptr);
  ASSERT_EQ(order->getSolveBefores()->size(), 1u) << "solve-before list is exactly [b1]";
  expectRefTo(order->getSolveBefores()->at(0), b1);

  ASSERT_NE(order->getSolveAfters(), nullptr);
  ASSERT_EQ(order->getSolveAfters()->size(), 1u) << "solve-after list is exactly [b2]";
  expectRefTo(order->getSolveAfters()->at(0), b2);
}

// Check 4: c1 is the (legal) implication "b1 -> (b2 == 0)".
TEST_F(VariableOrdering1Test, C1IsB1ImpliesB2EqualsZero) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  const hldb::Variable *const b1 = hldb::findByName<hldb::Variable>("b1", cls->getVariables());
  const hldb::Variable *const b2 = hldb::findByName<hldb::Variable>("b2", cls->getVariables());
  ASSERT_NE(b1, nullptr);
  ASSERT_NE(b2, nullptr);

  const hldb::Constraint *const c1 = hldb::findByName<hldb::Constraint>("c1", cls->getConstraints());
  ASSERT_NE(c1, nullptr) << "constraint 'c1' not found";
  ASSERT_NE(c1->getConstraintItems(), nullptr);
  ASSERT_EQ(c1->getConstraintItems()->size(), 1u);

  const ImplicationParts parts = getImplicationParts(c1->getConstraintItems()->at(0));
  ASSERT_NE(parts.m_condition, nullptr) << "c1 item should be an implication 'b1 -> ...' (18.5.6)";
  ASSERT_NE(parts.m_consequent, nullptr);
  expectRefTo(parts.m_condition, b1);

  const hldb::Operation *const eq = any_cast<hldb::Operation>(parts.m_consequent);
  ASSERT_NE(eq, nullptr) << "consequent should be the equality 'b2 == 0'";
  EXPECT_EQ(eq->getOpType(), vpiEqOp);
  EXPECT_FALSE(eq->getSoft());
  ASSERT_NE(eq->getOperands(), nullptr);
  ASSERT_EQ(eq->getOperands()->size(), 2u);
  expectRefTo(eq->getOperands()->at(0), b2);
  const hldb::Constant *const zero = any_cast<hldb::Constant>(eq->getOperands()->at(1));
  ASSERT_NE(zero, nullptr);
  EXPECT_EQ(zero->getDecompile(), "0");
}

// Check 5: constraint order and properties.
TEST_F(VariableOrdering1Test, ConstraintsC1AndC2InDeclarationOrder) {
  const hldb::ClassDefn *const cls = getClassA();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getConstraints(), nullptr);
  ASSERT_EQ(cls->getConstraints()->size(), 2u);
  EXPECT_EQ(cls->getConstraints()->at(0)->getName(), "c1");
  EXPECT_EQ(cls->getConstraints()->at(1)->getName(), "c2");
  for (const hldb::Constraint *const c : *cls->getConstraints()) {
    EXPECT_EQ(c->getParent(), static_cast<const hldb::Any *>(cls)) << c->getName() << ": parent is class 'a'";
    EXPECT_TRUE(c->getAutomatic()) << c->getName() << ": not declared static, so vpiAutomatic is 1 (37.34 detail 1)";
    EXPECT_EQ(c->getAccessType(), 0) << c->getName() << ": declared in-body, not extern";
  }
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
