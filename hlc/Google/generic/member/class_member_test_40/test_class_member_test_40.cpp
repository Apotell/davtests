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

// Source under test: tests/Google/generic/member/class_member_test_40.sv
//
//   class constructible;
//   function new ();
//   endfunction
//   endclass
//
//   module test;
//   endmodule
//
// Checked (IEEE 1800-2023):
//   - Sec 8.3: class "constructible" exists, not virtual, with no extends,
//     no properties and exactly one method.
//   - Sec 8.7: "function new ();" is the class's user-defined constructor: a
//     method Function named "new", public, with automatic lifetime (Sec
//     8.6), not virtual (a constructor is not virtual), not pure, and with
//     no formals (an empty port list "()").
//   - Sec 23.2: module "test" exists.
//   - The file is legal: no fatal, syntax or error diagnostics.
//
// Not checked:
//   - The constructor's return type representation: Sec 8.7 gives "new" no
//     declared return type, and how HLDB models that is not fixed.
//   - The empty body's statement representation.
//   - vpiFullName on any object (never asserted per the test writing guide).

#include <hlc/Tests/Test.h>

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/module.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

namespace hlc {
class ClassMemberTest40Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_member_test_40.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *getConstructible() {
    return hldb::findByName<hldb::ClassDefn>("constructible", m_design->getAllClasses());
  }

  static const hldb::Function *getConstructor() {
    const hldb::ClassDefn *const cls = getConstructible();
    if (cls == nullptr || cls->getMethods() == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Function>("new", cls->getMethods());
  }
};

TEST_F(ClassMemberTest40Test, ClassConstructibleExists) {
  const hldb::ClassDefn *const cls = getConstructible();
  ASSERT_NE(cls, nullptr) << "'class constructible;' should create a ClassDefn";
  EXPECT_FALSE(cls->getVirtual()) << "'constructible' is not declared virtual";
  EXPECT_EQ(cls->getExtends(), nullptr) << "'constructible' has no extends clause";
  EXPECT_TRUE(cls->getVariables() == nullptr || cls->getVariables()->empty())
      << "'constructible' declares no properties";
  ASSERT_NE(cls->getMethods(), nullptr);
  EXPECT_EQ(cls->getMethods()->size(), 1u) << "'constructible' defines exactly one method, 'new'";
}

TEST_F(ClassMemberTest40Test, NewIsConstructorMethod) {
  const hldb::Function *const ctor = getConstructor();
  ASSERT_NE(ctor, nullptr) << "Sec 8.7: 'function new ();' should be a Function named 'new' in constructible";
  EXPECT_TRUE(ctor->getMethod()) << "'new' is defined inside a class, so it is a method";
  EXPECT_EQ(ctor->getVisibility(), vpiPublicVis) << "no local/protected qualifier: visibility is public";
  EXPECT_TRUE(ctor->getAutomatic()) << "Sec 8.6: class methods have automatic lifetime";
  EXPECT_FALSE(ctor->getVirtual()) << "Sec 8.7: the constructor is not virtual";
  EXPECT_FALSE(ctor->getPure()) << "'new' has a body, so it is not pure";
  EXPECT_EQ(ctor->getParent(), getConstructible()) << "the constructor should be owned by 'constructible'";
}

TEST_F(ClassMemberTest40Test, NewHasNoFormals) {
  const hldb::Function *const ctor = getConstructor();
  ASSERT_NE(ctor, nullptr);
  EXPECT_TRUE(ctor->getIODecls() == nullptr || ctor->getIODecls()->empty())
      << "'function new ();' has an empty port list, so it has no formals";
}

TEST_F(ClassMemberTest40Test, ModuleTestExists) {
  EXPECT_NE(hldb::findByName<hldb::Module>("test", m_design->getAllModules()), nullptr)
      << "'module test; endmodule' should create a Module";
}

TEST_F(ClassMemberTest40Test, CompilesWithNoErrors) {
  const hlc::EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0) << "the source is legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
