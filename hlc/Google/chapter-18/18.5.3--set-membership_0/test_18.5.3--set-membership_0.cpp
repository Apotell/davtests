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

// Source under test: tests/Google/chapter-18/18.5.3--set-membership_0.sv
// (file name uses the 1800-2017 numbering; in 1800-2023 set membership in
//  constraints is described in Sec 18.5.3 intro + Sec 11.4.13)
//
//   class a;
//       rand int b;
//       constraint c { b inside {3, 10}; }
//   endclass
//
// IEEE 1800-2023 Sec 11.4.13 / 18.5.3:
//   'inside' is the set membership operator. Inside a constraint it is a
//   plain relational test: b must be one of {3, 10}, each equally likely.
//   It is NOT a distribution (no weights), so it must be modelled as an
//   operation, not as a 'distribution' object.
// IEEE 1800-2023 Sec 37 (expressions, detail 3): operands are given in BNF
//   order -> lhs 'b' first, then the range_list items 3 and 10 in order.
//   The standard does not fix whether the range_list items are flat
//   operands or wrapped in one list operand, so both layouts are accepted.
// IEEE 1800-2023 Sec 37.34 detail 5: constraint items keep source order.
//
// What we have to check:
//   - Class 'a' is non-virtual and declares rand int 'b' (rand, signed int,
//     no initializer).
//   - Constraint 'c' exists, is not pure, is enabled, has exactly one item.
//   - That item is an Operation with op type vpiInsideOp (not vpiEqOp, not a
//     Distribution).
//   - First operand is ref 'b' (bound to a::b); the set items are exactly
//     constant 3, constant 10, in order. The set keeps both values (not
//     folded to one).
//   - No illegal-constraint error.
//
// Checked:
//   - All items in "What we have to check" above. 'Enabled' is in its own
//     test, ConstraintsShouldBeEnabledByDefaultButAreNot (HLC never sets it).
//
// Not checked:
//   - The randomized value of b (3 or 10): runtime only.
//   - Exact vpiConstType of the literals 3/10 (tool-representation detail).

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>

#include <string_view>
#include <vector>

namespace hlc {
class SetMembership0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.3--set-membership_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *findClassA() {
    return hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  }

  // Items of the range list of '<expr> inside {...}', in source order. They
  // are either the operands after the first, or the operands of a single
  // list operation that is the second operand.
  static std::vector<const hldb::Any *> insideSetItems(const hldb::Operation *op) {
    std::vector<const hldb::Any *> items;
    if (op->getOperands() == nullptr || op->getOperands()->size() < 2) return items;
    if (op->getOperands()->size() == 2) {
      const hldb::Operation *const list = any_cast<hldb::Operation>(op->getOperands()->at(1));
      if (list != nullptr) {
        if (list->getOperands() != nullptr) {
          for (const hldb::Any *const item : *list->getOperands()) items.emplace_back(item);
        }
        return items;
      }
    }
    for (size_t i = 1; i < op->getOperands()->size(); ++i) items.emplace_back(op->getOperands()->at(i));
    return items;
  }
};

TEST_F(SetMembership0Test, ClassAndRandVariable) {
  const hldb::ClassDefn *const a = findClassA();
  ASSERT_NE(a, nullptr) << "class 'a' not found";
  EXPECT_FALSE(a->getVirtual());
  EXPECT_EQ(a->getExtends(), nullptr);

  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", a->getVariables());
  ASSERT_NE(b, nullptr) << "'b' not found in 'a'";
  EXPECT_EQ(b->getRandType(), vpiRand) << "'b' is declared 'rand'";
  EXPECT_EQ(b->getValue(), nullptr) << "'b' has no declaration initializer";
  const hldb::RefTypespec *const rt = b->getTypespec();
  ASSERT_NE(rt, nullptr);
  ASSERT_NE(rt->getActual(), nullptr);
  ASSERT_EQ(rt->getActual()->getAnyType(), hldb::AnyType::IntTypespec) << "'b' is 'int'";
  EXPECT_TRUE(rt->getActual<hldb::IntTypespec>()->getSigned()) << "int is signed (Sec 6.11)";
}

TEST_F(SetMembership0Test, ConstraintHoldsSingleInsideOperation) {
  const hldb::ClassDefn *const a = findClassA();
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->getConstraints(), nullptr);
  EXPECT_EQ(a->getConstraints()->size(), 1u);
  const hldb::Constraint *const c = hldb::findByName<hldb::Constraint>("c", a->getConstraints());
  ASSERT_NE(c, nullptr) << "constraint 'c' not found";
  EXPECT_FALSE(c->getPure());

  ASSERT_NE(c->getConstraintItems(), nullptr);
  ASSERT_EQ(c->getConstraintItems()->size(), 1u) << "single item: b inside {3, 10}";
  const hldb::Any *const item = c->getConstraintItems()->front();
  ASSERT_NE(item, nullptr);
  EXPECT_NE(item->getAnyType(), hldb::AnyType::Distribution) << "'inside' has no weights; it is not a dist";
  ASSERT_EQ(item->getAnyType(), hldb::AnyType::Operation);
  EXPECT_EQ(any_cast<hldb::Operation>(item)->getOpType(), vpiInsideOp);
}

TEST_F(SetMembership0Test, InsideOperandsAreBThenSetInOrder) {
  const hldb::ClassDefn *const a = findClassA();
  ASSERT_NE(a, nullptr);
  const hldb::Constraint *const c = hldb::findByName<hldb::Constraint>("c", a->getConstraints());
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getConstraintItems(), nullptr);
  ASSERT_FALSE(c->getConstraintItems()->empty());
  const hldb::Operation *const op = any_cast<hldb::Operation>(c->getConstraintItems()->front());
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_FALSE(op->getOperands()->empty());

  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(op->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr) << "first operand is the tested expression 'b'";
  EXPECT_EQ(lhs->getName(), "b");
  EXPECT_EQ(lhs->getActual(), hldb::findByName<hldb::Variable>("b", a->getVariables())) << "'b' binds to a::b";

  const std::vector<const hldb::Any *> items = insideSetItems(op);
  ASSERT_EQ(items.size(), 2u) << "set {3, 10} keeps both values";
  const hldb::Constant *const v0 = any_cast<hldb::Constant>(items[0]);
  const hldb::Constant *const v1 = any_cast<hldb::Constant>(items[1]);
  ASSERT_NE(v0, nullptr);
  ASSERT_NE(v1, nullptr);
  EXPECT_EQ(v0->getDecompile(), "3");
  EXPECT_EQ(v1->getDecompile(), "10");
}

TEST_F(SetMembership0Test, ConstraintsShouldBeEnabledByDefaultButAreNot) {
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

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
