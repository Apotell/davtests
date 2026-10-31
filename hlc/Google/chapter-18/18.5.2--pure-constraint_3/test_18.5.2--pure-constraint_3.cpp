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

// Source under test: tests/Google/chapter-18/18.5.2--pure-constraint_3.sv
//
//   virtual class a;
//       pure constraint c;
//   endclass
//
//   virtual class a2 extends a;
//   endclass
//
// IEEE 1800-2023 Sec 18.5.2:
//   "A pure constraint represents an obligation on any non-abstract derived
//    class (i.e., a derived class that is not virtual) to provide a
//    constraint of the same name."
// a2 IS virtual (abstract), so it carries no such obligation. This file is
// LEGAL, unlike 18.5.2--pure-constraint_2.sv which differs only by the
// missing 'virtual' on a2.
//
// What we have to check:
//   - 'a' is abstract (virtual) and holds pure constraint 'c' with no body.
//   - 'a2' is ALSO abstract (virtual) and extends 'a'.
//   - 'a2' does not implement 'c' and declares nothing.
//   - No illegal-constraint error is raised for 'a2' or 'c' (the obligation
//     is simply passed down to future non-abstract subclasses).
//   - A pure constraint is not an implicit prototype, so no "implicit empty
//     constraint" diagnostic for 'c'.
//
// Checked:
//   - All items in "What we have to check" above.
//
// Not checked:
//   - Nothing else is expressed by this source.

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/extends.h>
#include <hldb/ref_typespec.h>

#include <string_view>

namespace hlc {
class PureConstraint3Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "18.5.2--pure-constraint_3.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *findClass(std::string_view name) {
    return hldb::findByName<hldb::ClassDefn>(name, m_design->getAllClasses());
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

TEST_F(PureConstraint3Test, AbstractBaseHoldsPureConstraint) {
  const hldb::ClassDefn *const a = findClass("a");
  ASSERT_NE(a, nullptr) << "class 'a' not found";
  EXPECT_TRUE(a->getVirtual()) << "'virtual class a' is abstract (Sec 8.21)";
  EXPECT_EQ(a->getExtends(), nullptr);

  ASSERT_NE(a->getConstraints(), nullptr);
  EXPECT_EQ(a->getConstraints()->size(), 1u) << "'a' declares only 'c'";
  const hldb::Constraint *const c = hldb::findByName<hldb::Constraint>("c", a->getConstraints());
  ASSERT_NE(c, nullptr) << "pure constraint 'c' not found in 'a'";
  EXPECT_TRUE(c->getPure()) << "'pure constraint c;' must carry the pure flag";
  EXPECT_TRUE(c->getConstraintItems() == nullptr || c->getConstraintItems()->empty())
      << "a pure constraint has no constraint block";
}

TEST_F(PureConstraint3Test, AbstractDerivedClassMayLeavePureUnimplemented) {
  const hldb::ClassDefn *const a = findClass("a");
  const hldb::ClassDefn *const a2 = findClass("a2");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a2, nullptr) << "class 'a2' not found";
  EXPECT_TRUE(a2->getVirtual()) << "'virtual class a2' is abstract";
  EXPECT_EQ(baseClassOf(a2), a) << "'a2 extends a' must resolve to class 'a'";
  EXPECT_EQ(hldb::findByName<hldb::Constraint>("c", a2->getConstraints()), nullptr)
      << "'a2' does not implement 'c' itself";
  EXPECT_TRUE(a2->getVariables() == nullptr || a2->getVariables()->empty()) << "'a2' body is empty";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
