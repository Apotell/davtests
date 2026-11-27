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

// IEEE 1800-2023 Sec 8.24: "The out-of-block declaration shall be declared in the same scope as the
// class declaration and shall follow the class declaration." Sec 18.5.1: "An external constraint
// block shall appear in the same scope as the corresponding class declaration and shall appear after
// the class declaration in that scope."
// C2::early (line 18) precedes class C2. C1::wrong (40) and constraint C1::c_wrong (41) are written
// in package other, not in the compilation unit where class C1 is declared. The bodies of C1::late,
// C1::t, C1::new, constraint C1::c_late and pk::C3::f (in the package of C3) are legal.
// Arguments: misplaced_extern_declaration.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/constraint.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/package.h>
#include <hldb/task.h>
#include <hldb/unit.h>

#include <gtest/gtest.h>

#include <string_view>

namespace hlc {

class MisplacedExternDeclarationTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "misplaced_extern_declaration.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *findUnitClass(std::string_view name) {
    if (m_design->getUnits() == nullptr) return nullptr;
    for (const hldb::Unit *unit : *m_design->getUnits()) {
      if (const hldb::ClassDefn *const c = hldb::findByDefName<hldb::ClassDefn>(name, unit->getClassDefns())) {
        return c;
      }
    }
    return nullptr;
  }
};

TEST_F(MisplacedExternDeclarationTest, BodyBeforeItsClass) {
  EXPECT_NE(findEvent(EventType::CompMisplacedExternDeclaration, "early", 18, 6), nullptr);
}

TEST_F(MisplacedExternDeclarationTest, MethodBodyInAnotherScope) {
  EXPECT_NE(findEvent(EventType::CompMisplacedExternDeclaration, "wrong", 40, 12), nullptr);
}

TEST_F(MisplacedExternDeclarationTest, ConstraintBodyInAnotherScope) {
  EXPECT_NE(findEvent(EventType::CompMisplacedExternDeclaration, "c_wrong", 41, 3), nullptr);
}

TEST_F(MisplacedExternDeclarationTest, LegalBodiesBelongToTheirClass) {
  const hldb::ClassDefn *const c1 = findUnitClass("C1");
  ASSERT_NE(c1, nullptr);
  const hldb::Function *const late = hldb::findByName<hldb::Function>("late", c1->getMethods());
  ASSERT_NE(late, nullptr);
  EXPECT_TRUE(late->getMethod());
  EXPECT_EQ(late->getParent(), c1);
  const hldb::Task *const t = hldb::findByName<hldb::Task>("t", c1->getMethods());
  ASSERT_NE(t, nullptr);
  EXPECT_EQ(t->getParent(), c1);
  const hldb::Function *const ctor = hldb::findByName<hldb::Function>("new", c1->getMethods());
  ASSERT_NE(ctor, nullptr);
  EXPECT_EQ(ctor->getParent(), c1);
  const hldb::Constraint *const cLate = hldb::findByName<hldb::Constraint>("c_late", c1->getConstraints());
  ASSERT_NE(cLate, nullptr);
  EXPECT_NE(cLate->getConstraintItems(), nullptr) << "the external body is merged into the prototype";
}

TEST_F(MisplacedExternDeclarationTest, BodyInThePackageOfItsClass) {
  const hldb::Package *const pk = hldb::findByDefName<hldb::Package>("pk", m_design->getAllPackages());
  ASSERT_NE(pk, nullptr);
  const hldb::ClassDefn *const c3 = hldb::findByDefName<hldb::ClassDefn>("C3", pk->getClassDefns());
  ASSERT_NE(c3, nullptr);
  const hldb::Function *const f = hldb::findByName<hldb::Function>("f", c3->getMethods());
  ASSERT_NE(f, nullptr);
  EXPECT_EQ(f->getParent(), c3);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
