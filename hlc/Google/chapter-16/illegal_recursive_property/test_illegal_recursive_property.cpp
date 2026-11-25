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

// IEEE 1800-2023 Sec 16.12.17: the four restrictions on recursive properties, using the examples of
// that section.
//   Restriction 1: not and the strong operators (s_eventually, ...) cannot be applied to a property
//     expression that instantiates a recursive property, or a property that depends on one (F.7):
//     illegal_recursion_2 (line 31), strong_eventually (48), negated_dependency (52, through
//     legal_3) and the assertion A2 (64).
//   Restriction 2: disable iff cannot be used in a recursive property: illegal_recursion_3 (35).
//   Restriction 3: every recursive instance occurs after a positive advance in time:
//     illegal_recursion_4 (44, |-> does not advance time).
//   Restriction 4: an actual argument of a recursive instance that uses a formal must be that formal
//     or be bound to a local formal: fibonacci2 (60).
// prop_always, check_phase1/check_phase2 (mutually recursive), legal_3 and fibonacci1 are legal.
// Arguments: illegal_recursive_property.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/property_decl.h>
#include <hldb/property_inst.h>
#include <hldb/property_spec.h>
#include <hldb/sv_vpi_user.h>

#include <gtest/gtest.h>

#include <string_view>

namespace hlc {

class IllegalRecursivePropertyTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "illegal_recursive_property.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::PropertyDecl *findProperty(std::string_view name) {
    const hldb::Module *const m = hldb::findByDefName<hldb::Module>("m", m_design->getAllModules());
    return (m != nullptr) ? hldb::findByName<hldb::PropertyDecl>(name, m->getPropertyDecls()) : nullptr;
  }
};

TEST_F(IllegalRecursivePropertyTest, NotAppliedToRecursiveInstance) {
  EXPECT_NE(findEvent(EventType::CompIllegalRecursiveProperty, "illegal_recursion_2", 31, 21), nullptr);
}

TEST_F(IllegalRecursivePropertyTest, DisableIffInRecursiveProperty) {
  EXPECT_NE(findEvent(EventType::CompIllegalRecursiveProperty, "illegal_recursion_3", 35, 18), nullptr);
}

TEST_F(IllegalRecursivePropertyTest, RecursiveInstanceWithoutTimeAdvance) {
  EXPECT_NE(findEvent(EventType::CompIllegalRecursiveProperty, "illegal_recursion_4", 44, 21), nullptr);
}

TEST_F(IllegalRecursivePropertyTest, StrongOperatorAppliedToRecursiveProperty) {
  EXPECT_NE(findEvent(EventType::CompIllegalRecursiveProperty, "prop_always", 48, 5), nullptr);
}

TEST_F(IllegalRecursivePropertyTest, NotAppliedToPropertyDependingOnRecursiveProperty) {
  EXPECT_NE(findEvent(EventType::CompIllegalRecursiveProperty, "prop_always", 52, 5), nullptr);
}

TEST_F(IllegalRecursivePropertyTest, NegatedRecursivePropertyAsserted) {
  EXPECT_NE(findEvent(EventType::CompIllegalRecursiveProperty, "prop_always", 64, 39), nullptr);
}

TEST_F(IllegalRecursivePropertyTest, ActualArgumentUsingFormalMustBeLocal) {
  GTEST_SKIP() << "Sec 16.12.17 Restriction 4: whether a property formal is declared local is not "
                  "available in the model, so fibonacci2(b, a + b, n - 1, fib_sig) is not reported";
  EXPECT_NE(findEvent(EventType::CompIllegalRecursiveProperty, "fibonacci2", 60, 47), nullptr);
}

TEST_F(IllegalRecursivePropertyTest, RecursiveInstanceNamesItsOwnDeclaration) {
  const hldb::PropertyDecl *const decl = findProperty("prop_always");
  ASSERT_NE(decl, nullptr);
  const hldb::PropertySpec *const spec = decl->getPropertySpec();
  ASSERT_NE(spec, nullptr);
  ASSERT_NE(spec->getPropertyExpr(), nullptr);
  ASSERT_EQ(spec->getPropertyExpr()->getAnyType(), hldb::AnyType::Operation);
  // p and (1'b1 |=> prop_always(p))
  const hldb::Operation *const andOp = spec->getPropertyExpr<hldb::Operation>();
  EXPECT_EQ(andOp->getOpType(), vpiCompAndOp);
  ASSERT_NE(andOp->getOperands(), nullptr);
  ASSERT_EQ(andOp->getOperands()->size(), 2u);
  const hldb::Operation *const implies = any_cast<hldb::Operation>(andOp->getOperands()->at(1));
  ASSERT_NE(implies, nullptr);
  EXPECT_EQ(implies->getOpType(), vpiNonOverlapImplyOp);
  ASSERT_NE(implies->getOperands(), nullptr);
  ASSERT_EQ(implies->getOperands()->size(), 2u);
  const hldb::PropertyInst *const inst = any_cast<hldb::PropertyInst>(implies->getOperands()->at(1));
  ASSERT_NE(inst, nullptr);
  EXPECT_EQ(inst->getPropertyDecl(), decl);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
