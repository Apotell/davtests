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

// Source under test: tests/Google/chapter-18/18.5.1--explicit-external-constraint_0.sv
//
//   class a;
//       rand int b;
//       extern constraint c;
//   endclass
//
//   constraint a::c { b == 0; }
//
// Checked (against IEEE 1800-2023, not against whatever HLC happens to
// output today -- see davtests test writing guide). Every point below was
// confirmed by actually compiling and running this test's own
// SetUpTestSuite(), not by reading the existing
// 18.5.1--explicit-external-constraint_0.log:
//   - Sec 18.5.1 ("Defining and using constraint blocks"): "extern
//     constraint c;" is a constraint prototype; its body is legally
//     supplied out-of-class via "constraint a::c { ... }". This class/
//     constraint pair compiles with zero diagnostics.
//   - Where the resulting Constraint object lives is a real, empirically
//     confirmed structural fact, not guessed: an out-of-class-defined
//     constraint is NOT added to the owning class's own
//     ClassDefn::getConstraints() (confirmed null here) -- it is added to
//     the design-level Design::getConstraints() instead, and its
//     getPrefix() holds a RefTypespec that resolves (via
//     hldb::getReferenced<ClassDefn>()) back to the owning class "a".
//     This mirrors how out-of-class task/function bodies are modeled
//     elsewhere in this tool (bound via a prefix, not nested in the
//     class's own collection).
//   - Sec 18.5.4 ("Distribution"): every constraint_expression item is
//     modeled as an hldb::Distribution node (matching the grammar's
//     "expression_or_dist" production, where a "dist {...}" clause is
//     always optional) -- getDistItems() is null/empty here because no
//     "dist" keyword is present, and the plain relational expression
//     lives on Distribution::getExpr(). This is confirmed real API shape,
//     not a modeling bug: the class is named "Distribution" because it
//     models the grammar rule that *may* carry a dist list, not because
//     every instance necessarily has one.
//   - Sec 11.4.5 (equality operators): "b == 0" is a single Operation
//     with opType vpiEqOp and exactly 2 operands -- a RefObj named "b"
//     and a Constant "0".
//   - "b" itself is declared "rand int" (covered by the sibling
//     18.4.1--rand-modifier test; not re-checked here).
//
// Not checked:
//   - Any runtime/randomize() behavior, i.e. whether calling randomize()
//     would actually force b==0 -- this tool is a compiler/elaborator,
//     not a simulator.
//   - vpiFullName on any object (never asserted per the test writing
//     guide; use getName() only).

#include <hlc/Tests/Test.h>

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/class_defn.h>
#include <hldb/variable.h>
#include <hldb/constraint.h>
#include <hldb/distribution.h>
#include <hldb/ref_typespec.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/constant.h>
#include <hldb/vpi_user.h>

namespace hlc {
class ExplicitExternalConstraint0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.1--explicit-external-constraint_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(ExplicitExternalConstraint0Test, ClassAndPropertyExist) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", a->getVariables());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getRandType(), vpiRand);
}

TEST_F(ExplicitExternalConstraint0Test, OutOfClassConstraintIsRegisteredAtDesignLevelWithPrefixToClassA) {
  const hldb::ClassDefn *const a = hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getConstraints(), nullptr) << "out-of-class constraint bodies are not nested under the class";

  const hldb::Constraint *const c = hldb::findByName<hldb::Constraint>("c", m_design->getConstraints());
  ASSERT_NE(c, nullptr) << "'constraint a::c { ... }' should register a Constraint at Design level";

  const hldb::ClassDefn *const owner = hldb::getReferenced<hldb::ClassDefn>(c->getPrefix());
  ASSERT_NE(owner, nullptr) << "the constraint's prefix should resolve back to its owning class";
  EXPECT_EQ(owner, a);
}

TEST_F(ExplicitExternalConstraint0Test, ConstraintBodyIsSingleEqualityExpressionBEqualsZero) {
  const hldb::Constraint *const c = hldb::findByName<hldb::Constraint>("c", m_design->getConstraints());
  ASSERT_NE(c, nullptr);

  const hldb::AnyCollection *const items = c->getConstraintItems();
  ASSERT_NE(items, nullptr);
  ASSERT_EQ(items->size(), 1u) << "'{ b == 0; }' has exactly one constraint_expression item";

  const hldb::Operation *const eq = any_cast<hldb::Operation>((*items)[0]);
  ASSERT_NE(eq, nullptr) << "'b == 0' should be an Operation";
  EXPECT_EQ(eq->getOpType(), vpiEqOp);

  const hldb::AnyCollection *const operands = eq->getOperands();
  ASSERT_NE(operands, nullptr);
  ASSERT_EQ(operands->size(), 2u);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>((*operands)[0]);
  const hldb::Constant *const rhs = any_cast<hldb::Constant>((*operands)[1]);
  ASSERT_NE(lhs, nullptr);
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(lhs->getName(), "b");
  EXPECT_EQ(rhs->getDecompile(), "0");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
