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

// Source under test: tests/Google/chapter-18/18.5.5--uniqueness-constraints_0.sv
// (file name uses the 1800-2017 numbering; this is Sec 18.5.4 in 1800-2023)
//
//   class a;
//       rand int b1, b2;
//       constraint c1 { b1 inside {3, 10}; }
//       constraint c2 { b2 inside {3, 10}; }
//       constraint c3 { unique {b1, b2}; }
//   endclass
//
// IEEE 1800-2023 Sec 18.5.4 (Uniqueness constraints):
//   - 'unique { range_list }' makes every member of the group take a
//     different value after randomization.
//   - Each member shall be a singular integral/real variable (or unpacked
//     array / slice); all members shall be of equivalent type.
//   - "No randc variable shall appear in the group."
//   - A group with fewer than two members has no effect.
// Here b1, b2 are both 'rand int' -> legal group of 2. Together with c1/c2
// the only solutions are (b1,b2) = (3,10) or (10,3).
//
// What we have to check:
//   - 'rand int b1, b2;' declares TWO variables and 'rand' applies to BOTH.
//   - Both are int (signed), i.e. equivalent types as Sec 18.5.4 requires.
//   - Three constraints c1, c2, c3 kept in source order (Sec 37.34 det. 4).
//   - c1: b1 inside {3, 10} -> vpiInsideOp, operands b1, 3, 10.
//   - c2: b2 inside {3, 10} -> vpiInsideOp, operands b2, 3, 10.
//   - c3: one Uniqueness item whose value ranges are exactly ref b1, ref b2
//     (in order, bound to the class members).
//   - No randc in the unique group (both members are vpiRand).
//   - No illegal-constraint error.
//
// Checked:
//   - All items in "What we have to check" above.
//
// Not checked:
//   - Whether the 'inside' set / 'unique' group is stored as flat operands or
//     wrapped in one list operation: IEEE does not fix this, so both are
//     accepted. The items themselves and their order are still checked.
//   - The randomized pair (3,10)/(10,3): runtime only.
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
#include <hldb/int_typespec.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/uniqueness.h>
#include <hldb/variable.h>

#include <string_view>
#include <vector>

namespace hlc {
class UniquenessConstraints0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.5--uniqueness-constraints_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *findClassA() {
    return hldb::findByName<hldb::ClassDefn>("a", m_design->getAllClasses());
  }

  static const hldb::Variable *findVariable(std::string_view name) {
    const hldb::ClassDefn *const a = findClassA();
    return (a == nullptr) ? nullptr : hldb::findByName<hldb::Variable>(name, a->getVariables());
  }

  // Checks constraint 'consName' is exactly '<varName> inside {3, 10}'.
  static void expectInside3And10(std::string_view consName, std::string_view varName) {
    const hldb::ClassDefn *const a = findClassA();
    ASSERT_NE(a, nullptr);
    const hldb::Constraint *const cons = hldb::findByName<hldb::Constraint>(consName, a->getConstraints());
    ASSERT_NE(cons, nullptr) << "constraint '" << consName << "' not found";
    EXPECT_FALSE(cons->getPure());
    ASSERT_NE(cons->getConstraintItems(), nullptr);
    ASSERT_EQ(cons->getConstraintItems()->size(), 1u);
    const hldb::Operation *const op = any_cast<hldb::Operation>(cons->getConstraintItems()->front());
    ASSERT_NE(op, nullptr) << consName << ": item is an 'inside' operation";
    EXPECT_EQ(op->getOpType(), vpiInsideOp);
    ASSERT_NE(op->getOperands(), nullptr);
    ASSERT_FALSE(op->getOperands()->empty());
    const std::vector<const hldb::Any *> items = insideSetItems(op);
    ASSERT_EQ(items.size(), 2u) << consName << ": set {3, 10} keeps both values";
    const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(op->getOperands()->at(0));
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getName(), varName);
    EXPECT_EQ(lhs->getActual(), findVariable(varName));
    const hldb::Constant *const v0 = any_cast<hldb::Constant>(items[0]);
    const hldb::Constant *const v1 = any_cast<hldb::Constant>(items[1]);
    ASSERT_NE(v0, nullptr);
    ASSERT_NE(v1, nullptr);
    EXPECT_EQ(v0->getDecompile(), "3");
    EXPECT_EQ(v1->getDecompile(), "10");
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

  // Members of 'unique { ... }', in source order. They are either the value
  // ranges themselves, or the operands of a single list operation that is
  // the only value range.
  static std::vector<const hldb::Any *> uniqueMembers(const hldb::Uniqueness *uq) {
    std::vector<const hldb::Any *> members;
    if (uq->getValueRanges() == nullptr) return members;
    if (uq->getValueRanges()->size() == 1) {
      const hldb::Operation *const list = any_cast<hldb::Operation>(uq->getValueRanges()->front());
      if (list != nullptr) {
        if (list->getOperands() != nullptr) {
          for (const hldb::Any *const m : *list->getOperands()) members.emplace_back(m);
        }
        return members;
      }
    }
    for (const hldb::Any *const m : *uq->getValueRanges()) members.emplace_back(m);
    return members;
  }
};

TEST_F(UniquenessConstraints0Test, RandAppliesToBothDeclarators) {
  const hldb::ClassDefn *const a = findClassA();
  ASSERT_NE(a, nullptr) << "class 'a' not found";
  ASSERT_NE(a->getVariables(), nullptr);
  EXPECT_EQ(a->getVariables()->size(), 2u) << "'rand int b1, b2;' declares two variables";

  for (std::string_view name : {"b1", "b2"}) {
    const hldb::Variable *const v = findVariable(name);
    ASSERT_NE(v, nullptr) << "'" << name << "' not found";
    EXPECT_EQ(v->getRandType(), vpiRand) << "'rand' applies to " << name << " (not randc: Sec 18.5.4)";
    EXPECT_EQ(v->getValue(), nullptr) << name << " has no initializer";
    const hldb::RefTypespec *const rt = v->getTypespec();
    ASSERT_NE(rt, nullptr);
    ASSERT_NE(rt->getActual(), nullptr);
    ASSERT_EQ(rt->getActual()->getAnyType(), hldb::AnyType::IntTypespec) << name << " is 'int'";
    EXPECT_TRUE(rt->getActual<hldb::IntTypespec>()->getSigned());
  }
}

TEST_F(UniquenessConstraints0Test, ThreeConstraintsInSourceOrder) {
  const hldb::ClassDefn *const a = findClassA();
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->getConstraints(), nullptr);
  ASSERT_EQ(a->getConstraints()->size(), 3u);
  EXPECT_EQ(a->getConstraints()->at(0)->getName(), "c1");
  EXPECT_EQ(a->getConstraints()->at(1)->getName(), "c2");
  EXPECT_EQ(a->getConstraints()->at(2)->getName(), "c3");
}

TEST_F(UniquenessConstraints0Test, SetMembershipConstraints) {
  expectInside3And10("c1", "b1");
  expectInside3And10("c2", "b2");
}

TEST_F(UniquenessConstraints0Test, UniqueGroupIsB1AndB2) {
  const hldb::ClassDefn *const a = findClassA();
  ASSERT_NE(a, nullptr);
  const hldb::Constraint *const c3 = hldb::findByName<hldb::Constraint>("c3", a->getConstraints());
  ASSERT_NE(c3, nullptr) << "constraint 'c3' not found";
  EXPECT_FALSE(c3->getPure());
  ASSERT_NE(c3->getConstraintItems(), nullptr);
  ASSERT_EQ(c3->getConstraintItems()->size(), 1u) << "single item: unique {b1, b2}";
  const hldb::Any *const item = c3->getConstraintItems()->front();
  ASSERT_NE(item, nullptr);
  ASSERT_EQ(item->getAnyType(), hldb::AnyType::Uniqueness) << "'unique {...}' is a uniqueness constraint";

  const hldb::Uniqueness *const uq = any_cast<hldb::Uniqueness>(item);
  ASSERT_NE(uq->getValueRanges(), nullptr);
  const std::vector<const hldb::Any *> members = uniqueMembers(uq);
  ASSERT_EQ(members.size(), 2u) << "group has 2 members, so the constraint has effect";
  const hldb::RefObj *const m0 = any_cast<hldb::RefObj>(members[0]);
  const hldb::RefObj *const m1 = any_cast<hldb::RefObj>(members[1]);
  ASSERT_NE(m0, nullptr);
  ASSERT_NE(m1, nullptr);
  EXPECT_EQ(m0->getName(), "b1");
  EXPECT_EQ(m1->getName(), "b2");
  EXPECT_EQ(m0->getActual(), findVariable("b1")) << "'b1' binds to a::b1";
  EXPECT_EQ(m1->getActual(), findVariable("b2")) << "'b2' binds to a::b2";
}

TEST_F(UniquenessConstraints0Test, NoConstraintError) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "c1"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "c2"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_ILLEGAL_CONSTRAINT, "c3"), nullptr);
}
TEST_F(UniquenessConstraints0Test, ConstraintsShouldBeEnabledByDefaultButAreNot) {
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
