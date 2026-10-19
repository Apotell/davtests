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

// Source under test: tests/Google/chapter-18/18.5.4--distribution_2.sv
// (:should_fail_because: distribution shall not be applied to randc
//  variables; file name uses 1800-2017 numbering, Sec 18.5.3 in 1800-2023)
//
//   class a;
//       randc int b;
//       constraint c { b dist {3 := 0, 10 := 5}; }
//   endclass
//
// IEEE 1800-2023 Sec 18.5.3, Limitations:
//   "A dist operation shall not be applied to randc variables."
//   "A dist expression requires that the expression contain at least one
//    rand variable."
// 'b' is randc (Sec 18.4.2), and it is the only variable in the dist
// expression, so BOTH limitations are violated. This file MUST be rejected.
//
// What we have to check:
//   - 'b' is declared randc (vpiRandC), not rand, int, no initializer.
//   - The dist is still parsed and modelled (Distribution with lhs 'b' and
//     items 3 := 0, 10 := 5) so the error can point at it.
//   - The compiler reports an illegal-constraint error tied to 'b' (the
//     randc variable under dist) or to 'c' (the offending constraint).
//
// Checked:
//   - All items in "What we have to check" above.
//
// Not checked:
//   - The exact error code. HLC has no error dedicated to "dist on randc";
//     COMP_ILLEGAL_CONSTRAINT is the closest existing code and is ASSUMED
//     here. If HLC adds a dedicated code, update the last test.
//   - The exact line/column of the diagnostic (not mandated by IEEE).

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

namespace hlc {
class Distribution2Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.4--distribution_2.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *findClassA() {
    return hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  }
};

TEST_F(Distribution2Test, VariableIsRandc) {
  const hldb::ClassDefn *const a = findClassA();
  ASSERT_NE(a, nullptr) << "class 'a' not found";
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", a->getVariables());
  ASSERT_NE(b, nullptr) << "'b' not found in 'a'";
  EXPECT_EQ(b->getRandType(), vpiRandC) << "'b' is declared 'randc' (Sec 18.4.2)";
  EXPECT_EQ(b->getValue(), nullptr) << "'b' has no declaration initializer";
  const hldb::RefTypespec *const rt = b->getTypespec();
  ASSERT_NE(rt, nullptr);
  ASSERT_NE(rt->getActual(), nullptr);
  EXPECT_EQ(rt->getActual()->getAnyType(), hldb::AnyType::IntTypespec) << "'b' is 'int'";
}

TEST_F(Distribution2Test, DistributionIsStillModelled) {
  const hldb::ClassDefn *const a = findClassA();
  ASSERT_NE(a, nullptr);
  const hldb::Constraint *const c = hldb::findByName<hldb::Constraint>("c", a->getConstraints());
  ASSERT_NE(c, nullptr) << "constraint 'c' not found";
  ASSERT_NE(c->getConstraintItems(), nullptr);
  ASSERT_EQ(c->getConstraintItems()->size(), 1u);
  const hldb::Distribution *const dist = any_cast<hldb::Distribution>(c->getConstraintItems()->front());
  ASSERT_NE(dist, nullptr) << "'b dist {...}' must be modelled as a distribution";

  const hldb::RefObj *const lhs = dist->getExpr<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "b");
  EXPECT_EQ(lhs->getActual(), hldb::findByName<hldb::Variable>("b", a->getVariables())) << "'b' binds to a::b";

  ASSERT_NE(dist->getDistItems(), nullptr);
  ASSERT_EQ(dist->getDistItems()->size(), 2u) << "two items: 3 := 0, 10 := 5";
  const hldb::DistItem *const i0 = dist->getDistItems()->at(0);
  const hldb::DistItem *const i1 = dist->getDistItems()->at(1);
  ASSERT_NE(i0, nullptr);
  ASSERT_NE(i1, nullptr);
  EXPECT_EQ(i0->getDistType(), vpiEqualDist);
  EXPECT_EQ(i1->getDistType(), vpiEqualDist);
  ASSERT_NE(i0->getValueRange<hldb::Constant>(), nullptr);
  ASSERT_NE(i0->getWeight<hldb::Constant>(), nullptr);
  ASSERT_NE(i1->getValueRange<hldb::Constant>(), nullptr);
  ASSERT_NE(i1->getWeight<hldb::Constant>(), nullptr);
  EXPECT_EQ(i0->getValueRange<hldb::Constant>()->getDecompile(), "3");
  EXPECT_EQ(i0->getWeight<hldb::Constant>()->getDecompile(), "0");
  EXPECT_EQ(i1->getValueRange<hldb::Constant>()->getDecompile(), "10");
  EXPECT_EQ(i1->getWeight<hldb::Constant>()->getDecompile(), "5");
}

TEST_F(Distribution2Test, DistOnRandcVariableIsError) {
  // Sec 18.5.3: "A dist operation shall not be applied to randc variables."
  const Error *err = findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "b");
  if (err == nullptr) err = findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "c");
  EXPECT_NE(err, nullptr) << "dist must not be applied to randc variable 'b' (IEEE 1800-2023 Sec 18.5.3)";
}
}  // namespace hlc

#ifndef DAVTESTS_BULK_BUILD
int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
#endif
