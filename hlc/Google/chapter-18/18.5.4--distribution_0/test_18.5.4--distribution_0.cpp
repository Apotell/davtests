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

// Source under test: tests/Google/chapter-18/18.5.4--distribution_0.sv
// (file name uses the 1800-2017 numbering; this is Sec 18.5.3 in 1800-2023)
//
//   class a;
//       rand int b;
//       constraint c { b dist {3 := 1, 10 := 2}; }
//   endclass
//
// IEEE 1800-2023 Sec 18.5.3 (Distribution):
//   - 'expr dist { dist_list }' is a distribution: a set-membership test plus
//     a weight per item. b may only be 3 or 10, with ratio 1:2.
//   - ':=' assigns the weight to each value (vpiEqualDist), ':/' divides it
//     across a range (vpiDivDist).
//   - A dist shall not be applied to randc; it needs at least one rand
//     variable. 'b' is 'rand', so this file is LEGAL.
// IEEE 1800-2023 Sec 37.34: distribution -> vpiExpr (lhs) + dist items; each
//   dist item -> vpiValueRange, vpiWeight, vpiDistType. Items keep order.
//
// What we have to check:
//   - Class 'a' declares rand (NOT randc) int 'b', signed, no initializer.
//   - Constraint 'c' exists, is not pure, enabled, has exactly one item.
//   - That item is a Distribution (not an inside/eq Operation), not 'soft'.
//   - Distribution lhs expression is ref 'b' bound to a::b.
//   - Exactly 2 dist items, in source order:
//       item0: value 3,  weight 1, type vpiEqualDist (':=')
//       item1: value 10, weight 2, type vpiEqualDist (':=')
//   - Weights are kept as written (not normalized, 2 is not merged with 1).
//   - No illegal-constraint error (dist on a rand var is legal).
//
// Checked:
//   - All items in "What we have to check" above.
//
// Not checked:
//   - The actual 1:2 probability of 3 vs 10: needs the solver at runtime.
//   - Exact vpiConstType of the literals (tool-representation detail).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/dist_item.h>
#include <hldb/distribution.h>
#include <hldb/int_typespec.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>

#include <string_view>

namespace hlc {
class Distribution0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.4--distribution_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *findClassA() {
    return hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  }

  // The single Distribution item of constraint 'c', or nullptr.
  static const hldb::Distribution *findDist() {
    const hldb::ClassDefn *const a = findClassA();
    if (a == nullptr) return nullptr;
    const hldb::Constraint *const c = hldb::findByName<hldb::Constraint>("c", a->getConstraints());
    if (c == nullptr || c->getConstraintItems() == nullptr || c->getConstraintItems()->size() != 1) return nullptr;
    return any_cast<hldb::Distribution>(c->getConstraintItems()->front());
  }

  // Checks one ':=' / ':/' dist item: value, weight and kind.
  static void expectDistItem(const hldb::DistItem *item, std::string_view value, std::string_view weight,
                             int32_t distType) {
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(item->getDistType(), distType);
    const hldb::Constant *const v = item->getValueRange<hldb::Constant>();
    ASSERT_NE(v, nullptr) << "value range is a single literal";
    EXPECT_EQ(v->getDecompile(), value);
    ASSERT_NE(item->getWeight(), nullptr) << "weight is written explicitly";
    const hldb::Constant *const w = item->getWeight<hldb::Constant>();
    ASSERT_NE(w, nullptr);
    EXPECT_EQ(w->getDecompile(), weight);
  }
};

TEST_F(Distribution0Test, ClassAndRandVariable) {
  const hldb::ClassDefn *const a = findClassA();
  ASSERT_NE(a, nullptr) << "class 'a' not found";
  EXPECT_FALSE(a->getVirtual());

  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", a->getVariables());
  ASSERT_NE(b, nullptr) << "'b' not found in 'a'";
  EXPECT_EQ(b->getRandType(), vpiRand) << "'b' is 'rand' (not randc), so dist is legal";
  EXPECT_EQ(b->getValue(), nullptr) << "'b' has no declaration initializer";
  const hldb::RefTypespec *const rt = b->getTypespec();
  ASSERT_NE(rt, nullptr);
  ASSERT_NE(rt->getActual(), nullptr);
  ASSERT_EQ(rt->getActual()->getAnyType(), hldb::AnyType::IntTypespec) << "'b' is 'int'";
  EXPECT_TRUE(rt->getActual<hldb::IntTypespec>()->getSigned()) << "int is signed (Sec 6.11)";
}

TEST_F(Distribution0Test, ConstraintHoldsSingleDistribution) {
  const hldb::ClassDefn *const a = findClassA();
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->getConstraints(), nullptr);
  EXPECT_EQ(a->getConstraints()->size(), 1u);
  const hldb::Constraint *const c = hldb::findByName<hldb::Constraint>("c", a->getConstraints());
  ASSERT_NE(c, nullptr) << "constraint 'c' not found";
  EXPECT_FALSE(c->getPure());

  ASSERT_NE(c->getConstraintItems(), nullptr);
  ASSERT_EQ(c->getConstraintItems()->size(), 1u) << "single item: b dist {...}";
  const hldb::Any *const item = c->getConstraintItems()->front();
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(item->getAnyType(), hldb::AnyType::Distribution) << "'dist' must be modelled as a distribution";

  const hldb::Distribution *const dist = findDist();
  ASSERT_NE(dist, nullptr);
  EXPECT_FALSE(dist->getSoft()) << "no 'soft' keyword in source";
}

TEST_F(Distribution0Test, DistributionLhsIsB) {
  const hldb::Distribution *const dist = findDist();
  ASSERT_NE(dist, nullptr);
  ASSERT_NE(dist->getExpr(), nullptr) << "dist must keep its left-hand expression";
  const hldb::RefObj *const lhs = dist->getExpr<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "b");
  const hldb::ClassDefn *const a = findClassA();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(lhs->getActual(), hldb::findByName<hldb::Variable>("b", a->getVariables())) << "'b' binds to a::b";
}

TEST_F(Distribution0Test, DistItemsValuesWeightsAndKind) {
  const hldb::Distribution *const dist = findDist();
  ASSERT_NE(dist, nullptr);
  ASSERT_NE(dist->getDistItems(), nullptr);
  ASSERT_EQ(dist->getDistItems()->size(), 2u) << "two items: 3 := 1, 10 := 2";
  expectDistItem(dist->getDistItems()->at(0), "3", "1", vpiEqualDist);
  expectDistItem(dist->getDistItems()->at(1), "10", "2", vpiEqualDist);
}

TEST_F(Distribution0Test, DistOnRandVariableIsLegal) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "c"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "b"), nullptr);
}
TEST_F(Distribution0Test, ConstraintsShouldBeEnabledByDefaultButAreNot) {
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
