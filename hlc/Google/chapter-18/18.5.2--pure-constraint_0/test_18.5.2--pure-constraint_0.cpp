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

// Source under test: tests/Google/chapter-18/18.5.2--pure-constraint_0.sv
//
//   virtual class a;
//       pure constraint c;
//   endclass
//
//   class a2 extends a;
//       rand int b2;
//       constraint c { b2 == 5; }
//   endclass
//
// IEEE 1800-2023 Sec 18.5.2:
//   "An abstract class (i.e., a class declared using the syntax virtual class)
//    may contain pure constraints. ... A pure constraint represents an
//    obligation on any non-abstract derived class ... to provide a constraint
//    of the same name."
//   "any class (whether abstract or not) may contain a constraint block ... of
//    the same name as a pure constraint that the class inherits; such a
//    constraint shall override the pure constraint, and shall be a non-pure
//    constraint for the class".
// This file is LEGAL: a2 is non-abstract and fulfils the obligation by
// providing its own 'c'.
//
// What we have to check:
//   - 'a' is an abstract (virtual) class (Sec 8.21) with no superclass.
//   - 'a' holds exactly one constraint 'c', flagged pure, with no body.
//   - 'a' declares no variables.
//   - 'a2' is a NON-virtual class that extends 'a'.
//   - 'a2' declares rand int 'b2' (rand, signed int).
//   - 'a2' declares its own 'c', which is NOT pure and has the body b2 == 5,
//     where 'b2' binds to a2::b2.
//   - No error: pure constraint in an abstract class is legal, and
//     overriding it in a2 is legal (not a duplicate / illegal constraint).
//   - A pure constraint is not an implicit prototype, so no "implicit empty
//     constraint" diagnostic must be raised for 'c' (Sec 18.5.1 vs 18.5.2).
//
// Checked:
//   - All items in "What we have to check" above.
//
// Not checked:
//   - Randomized value b2 == 5: runtime only (randomize() is never executed
//     by HLC).
//   - Exact vpiConstType of the literal '5' (tool-representation detail).

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/constant.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/extends.h>
#include <hldb/int_typespec.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>

#include <string_view>

namespace hlc {
class PureConstraint0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.2--pure-constraint_0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *findClass(std::string_view name) {
    return hldb::findByName<hldb::ClassDefn>(name, m_design->getAllClasses());
  }

  static const hldb::Constraint *findConstraint(const hldb::ClassDefn *cls, std::string_view name) {
    return hldb::findByName<hldb::Constraint>(name, cls->getConstraints());
  }

  static const hldb::Variable *findVariable(const hldb::ClassDefn *cls, std::string_view name) {
    return hldb::findByName<hldb::Variable>(name, cls->getVariables());
  }

  // Base class named by 'extends', or nullptr.
  static const hldb::ClassDefn *baseClassOf(const hldb::ClassDefn *cls) {
    const hldb::Extends *const ext = cls->getExtends();
    if (ext == nullptr || ext->getClassTypespecs() == nullptr || ext->getClassTypespecs()->empty()) return nullptr;
    const hldb::RefTypespec *const rt = ext->getClassTypespecs()->front();
    const hldb::ClassTypespec *const ct = rt->getActual<hldb::ClassTypespec>();
    return (ct == nullptr) ? nullptr : ct->getClassDefn();
  }
};

TEST_F(PureConstraint0Test, AbstractBaseHoldsPureConstraint) {
  const hldb::ClassDefn *const a = findClass("a");
  ASSERT_NE(a, nullptr) << "class 'a' not found";
  EXPECT_TRUE(a->getVirtual()) << "'virtual class a' is abstract (Sec 8.21)";
  EXPECT_EQ(a->getExtends(), nullptr) << "'a' has no superclass";
  EXPECT_TRUE(a->getVariables() == nullptr || a->getVariables()->empty()) << "'a' declares no variables";

  ASSERT_NE(a->getConstraints(), nullptr);
  EXPECT_EQ(a->getConstraints()->size(), 1u) << "'a' declares only 'c'";
  const hldb::Constraint *const c = findConstraint(a, "c");
  ASSERT_NE(c, nullptr) << "pure constraint 'c' not found in 'a'";
  EXPECT_TRUE(c->getPure()) << "'pure constraint c;' must carry the pure flag";
  EXPECT_TRUE(c->getConstraintItems() == nullptr || c->getConstraintItems()->empty())
      << "a pure constraint has no constraint block";
}

TEST_F(PureConstraint0Test, ConcreteDerivedClassExtendsAbstractBase) {
  const hldb::ClassDefn *const a = findClass("a");
  const hldb::ClassDefn *const a2 = findClass("a2");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a2, nullptr) << "class 'a2' not found";
  EXPECT_FALSE(a2->getVirtual()) << "'a2' is a non-abstract class";
  EXPECT_EQ(baseClassOf(a2), a) << "'a2 extends a' must resolve to class 'a'";

  const hldb::Variable *const b2 = findVariable(a2, "b2");
  ASSERT_NE(b2, nullptr) << "'b2' not found in 'a2'";
  EXPECT_EQ(b2->getRandType(), vpiRand) << "'b2' is declared 'rand'";
  EXPECT_EQ(b2->getValue(), nullptr) << "'b2' has no declaration initializer";
  const hldb::RefTypespec *const rt = b2->getTypespec();
  ASSERT_NE(rt, nullptr);
  ASSERT_NE(rt->getActual(), nullptr);
  ASSERT_EQ(rt->getActual()->getAnyType(), hldb::AnyType::IntTypespec) << "'b2' is 'int'";
  EXPECT_TRUE(rt->getActual<hldb::IntTypespec>()->getSigned()) << "int is signed (Sec 6.11)";
}

TEST_F(PureConstraint0Test, DerivedConstraintOverridesPureAndIsNotPure) {
  const hldb::ClassDefn *const a2 = findClass("a2");
  ASSERT_NE(a2, nullptr);
  ASSERT_NE(a2->getConstraints(), nullptr);
  const hldb::Constraint *const c = findConstraint(a2, "c");
  ASSERT_NE(c, nullptr) << "'a2' must declare its own constraint 'c'";
  EXPECT_FALSE(c->getPure()) << "the overriding 'c' is a non-pure constraint (Sec 18.5.2)";

  ASSERT_NE(c->getConstraintItems(), nullptr);
  ASSERT_EQ(c->getConstraintItems()->size(), 1u) << "body is the single item b2 == 5";
  const hldb::Operation *const op = any_cast<hldb::Operation>(c->getConstraintItems()->front());
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiEqOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);

  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(op->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "b2");
  EXPECT_EQ(lhs->getActual(), findVariable(a2, "b2")) << "'b2' binds to a2::b2";

  const hldb::Constant *const rhs = any_cast<hldb::Constant>(op->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "5");
}

TEST_F(PureConstraint0Test, ConstraintsShouldBeEnabledByDefaultButAreNot) {
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
