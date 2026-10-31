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

// Source under test: tests/Google/chapter-18/18.5.2--constraint-inheritance_0.sv
//
//   class a;
//       rand int b;
//       constraint c { b == 5; };
//   endclass
//
//   class a2 extends a;
//       rand int b2;
//       constraint c2 { b2 == b; }
//   endclass
//
// IEEE 1800-2023 Sec 18.5.2 (Constraint inheritance):
//   "A derived class shall inherit all constraints from its superclass. ...
//    Any constraint in a derived class that does not have the same name as a
//    constraint in the superclass shall be an additional constraint."
// So a2 has TWO active constraints: inherited 'c' (b == 5) and its own 'c2'
// (b2 == b). Together they force b2 == 5. 'b' used inside c2 is the member
// inherited from 'a' (Sec 8.13), so the reference must bind to a::b.
//
// What we have to check:
//   - Class 'a': non-virtual, one rand int 'b', one constraint 'c'.
//   - 'b' is declared 'rand' (Sec 18.4) and is of type int (signed, Sec 6.11).
//   - Constraint 'c' has exactly one item: the equality b == 5.
//   - Class 'a2' extends 'a' (Sec 8.13) and 'a' lists 'a2' as derived class.
//   - Class 'a2': non-virtual, one rand int 'b2', one constraint 'c2'.
//   - Constraint 'c2' has exactly one item: the equality b2 == b.
//   - In c2, 'b2' binds to a2::b2 and 'b' binds to the INHERITED a::b.
//   - c and c2 have different names, so c2 is additional (no override,
//     no "multiply defined constraint" error).
//   - Constraints are not pure and are enabled by default (Sec 18.9).
//
// Checked:
//   - All items in "What we have to check" above.
//   - vpiConstType of the literal '5' (test LiteralIsStandardDecimalConst):
//     IEEE Annex K lists vpiDecConst ("decimal integer") and vpiIntConst
//     ("integer constant") as the standard kinds for it, so either is
//     accepted; the non-standard vpiUIntConst is not.
//
// Not checked:
//   - The randomized value b2 == 5 (and b == 5): only exists after
//     randomize() runs; HLC is a compiler, it never executes randomize().
//   - Whether a2's own constraint collection also lists the inherited 'c':
//     the IEEE VPI model does not fix this, so it is not asserted.

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
class ConstraintInheritance0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.2--constraint-inheritance_0.hlc"}); }
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

  // Asserts 'var' is a rand, signed int class property.
  static void expectRandInt(const hldb::Variable *var) {
    EXPECT_EQ(var->getRandType(), vpiRand) << var->getName() << " is declared 'rand'";
    const hldb::RefTypespec *const rt = var->getTypespec();
    ASSERT_NE(rt, nullptr);
    ASSERT_NE(rt->getActual(), nullptr);
    ASSERT_EQ(rt->getActual()->getAnyType(), hldb::AnyType::IntTypespec) << var->getName() << " is 'int'";
    EXPECT_TRUE(rt->getActual<hldb::IntTypespec>()->getSigned()) << "int is signed (Sec 6.11)";
  }

  // Returns the single '==' operation of constraint 'cons', or nullptr.
  static const hldb::Operation *singleEqItem(const hldb::Constraint *cons) {
    if (cons->getConstraintItems() == nullptr || cons->getConstraintItems()->size() != 1) return nullptr;
    const hldb::Operation *const op = any_cast<hldb::Operation>(cons->getConstraintItems()->front());
    if (op == nullptr || op->getOpType() != vpiEqOp) return nullptr;
    if (op->getOperands() == nullptr || op->getOperands()->size() != 2) return nullptr;
    return op;
  }
};

TEST_F(ConstraintInheritance0Test, BaseClassDeclaration) {
  const hldb::ClassDefn *const a = findClass("a");
  ASSERT_NE(a, nullptr) << "class 'a' not found";
  EXPECT_FALSE(a->getVirtual()) << "'a' is a plain (non-abstract) class";
  EXPECT_EQ(a->getExtends(), nullptr) << "'a' has no superclass";

  ASSERT_NE(a->getVariables(), nullptr);
  EXPECT_EQ(a->getVariables()->size(), 1u) << "'a' declares only 'b'";
  const hldb::Variable *const b = findVariable(a, "b");
  ASSERT_NE(b, nullptr);
  expectRandInt(b);
  EXPECT_EQ(b->getValue(), nullptr) << "'b' has no declaration initializer";

  ASSERT_NE(a->getConstraints(), nullptr);
  EXPECT_EQ(a->getConstraints()->size(), 1u) << "'a' declares only 'c'";
}

TEST_F(ConstraintInheritance0Test, BaseConstraintBodyIsBEq5) {
  const hldb::ClassDefn *const a = findClass("a");
  ASSERT_NE(a, nullptr);
  const hldb::Constraint *const c = findConstraint(a, "c");
  ASSERT_NE(c, nullptr) << "constraint 'c' not found in 'a'";
  EXPECT_FALSE(c->getPure()) << "'c' has a body, it is not pure";

  const hldb::Operation *const op = singleEqItem(c);
  ASSERT_NE(op, nullptr) << "'c' must hold exactly one item: b == 5";

  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(op->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "b");
  EXPECT_EQ(lhs->getActual(), findVariable(a, "b")) << "'b' binds to a::b";

  const hldb::Constant *const rhs = any_cast<hldb::Constant>(op->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr) << "rhs of b == 5 is a literal";
  EXPECT_EQ(rhs->getDecompile(), "5");
}

TEST_F(ConstraintInheritance0Test, DerivedClassExtendsBase) {
  const hldb::ClassDefn *const a = findClass("a");
  const hldb::ClassDefn *const a2 = findClass("a2");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a2, nullptr) << "class 'a2' not found";
  EXPECT_FALSE(a2->getVirtual()) << "'a2' is a plain (non-abstract) class";

  ASSERT_NE(a2->getExtends(), nullptr) << "'a2' has an extends clause";
  EXPECT_EQ(baseClassOf(a2), a) << "'a2 extends a' must resolve to class 'a'";
}

TEST_F(ConstraintInheritance0Test, BaseShouldListDerivedClassButDoesNot) {
  // IEEE 1800-2023 Sec 37.31 (class defn): vpiDerivedClasses iterates the
  // classes that extend this one, so 'a' must list 'a2'.
  const hldb::ClassDefn *const a = findClass("a");
  const hldb::ClassDefn *const a2 = findClass("a2");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a2, nullptr);
  ASSERT_NE(a->getDerivedClasses(), nullptr) << "'a' should list its derived class 'a2'";
  const hldb::ClassDefn *const derived = hldb::findByName<hldb::ClassDefn>("a2", a->getDerivedClasses());
  EXPECT_EQ(derived, a2) << "'a' must list 'a2' as a derived class";
}

TEST_F(ConstraintInheritance0Test, DerivedClassDeclaration) {
  const hldb::ClassDefn *const a2 = findClass("a2");
  ASSERT_NE(a2, nullptr);

  const hldb::Variable *const b2 = findVariable(a2, "b2");
  ASSERT_NE(b2, nullptr) << "'b2' not found in 'a2'";
  expectRandInt(b2);
  EXPECT_EQ(b2->getValue(), nullptr) << "'b2' has no declaration initializer";
  EXPECT_EQ(findVariable(a2, "b"), nullptr) << "'b' is inherited, not redeclared in 'a2'";

  const hldb::Constraint *const c2 = findConstraint(a2, "c2");
  ASSERT_NE(c2, nullptr) << "constraint 'c2' not found in 'a2'";
  EXPECT_FALSE(c2->getPure());
}

TEST_F(ConstraintInheritance0Test, DerivedConstraintUsesInheritedMember) {
  const hldb::ClassDefn *const a = findClass("a");
  const hldb::ClassDefn *const a2 = findClass("a2");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a2, nullptr);
  const hldb::Constraint *const c2 = findConstraint(a2, "c2");
  ASSERT_NE(c2, nullptr);

  const hldb::Operation *const op = singleEqItem(c2);
  ASSERT_NE(op, nullptr) << "'c2' must hold exactly one item: b2 == b";

  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(op->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "b2");
  EXPECT_EQ(lhs->getActual(), findVariable(a2, "b2")) << "'b2' binds to a2::b2";

  const hldb::RefObj *const rhs = any_cast<hldb::RefObj>(op->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr) << "rhs of b2 == b is a variable reference, not a literal";
  EXPECT_EQ(rhs->getName(), "b");
  ASSERT_NE(rhs->getActual(), nullptr) << "'b' in c2 must resolve";
  EXPECT_EQ(rhs->getActual(), findVariable(a, "b")) << "'b' in c2 must bind to the inherited a::b (Sec 8.13)";
}

TEST_F(ConstraintInheritance0Test, ConstraintsShouldBeEnabledByDefaultButAreNot) {
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
TEST_F(ConstraintInheritance0Test, LiteralIsStandardDecimalConst) {
  // The '5' in 'b == 5' is an unsized decimal literal (Sec 5.7.1); its
  // vpiConstType must be one of the standard decimal kinds (Annex K).
  const hldb::ClassDefn *const a = findClass("a");
  ASSERT_NE(a, nullptr);
  const hldb::Constraint *const c = findConstraint(a, "c");
  ASSERT_NE(c, nullptr);
  const hldb::Operation *const op = singleEqItem(c);
  ASSERT_NE(op, nullptr);
  const hldb::Constant *const five = any_cast<hldb::Constant>(op->getOperands()->at(1));
  ASSERT_NE(five, nullptr);
  EXPECT_TRUE(five->getConstType() == vpiDecConst || five->getConstType() == vpiIntConst)
      << "unsized decimal '5' should be vpiDecConst or vpiIntConst, got " << five->getConstType();
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
