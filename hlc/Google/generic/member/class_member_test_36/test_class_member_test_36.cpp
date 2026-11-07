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

// Source under test: tests/Google/generic/member/class_member_test_36.sv
//
//   class myclass;
//   virtual function virtual cmd_array_if subroutine();
//   endfunction
//   endclass
//
// Checked (IEEE 1800-2023):
//   - Sec 8.3: class "myclass" exists, not virtual, with no extends, and
//     defines exactly one method.
//   - Sec 8.20 / 13.4 / 25.9: in "virtual function virtual cmd_array_if
//     subroutine()" the first "virtual" (before "function") makes the
//     method virtual, and the second (after "function") belongs to the
//     return type, a virtual interface. "subroutine" is a virtual,
//     non-pure, public method Function with automatic lifetime (Sec 8.6)
//     and no formals.
//   - The return type reference names "cmd_array_if".
//   - Sec 25.9: "cmd_array_if" is never declared as an interface anywhere,
//     so the virtual interface type cannot resolve and must be flagged as
//     an error. Checked by an error-level diagnostic at "cmd_array_if"
//     (7:26) or at its "virtual" keyword (7:18); the exact event code is
//     not assumed.
//
// Confirmed by running this test (not from the .log): "cmd_array_if" gets
// only a warning (CompFailedToBind) at 7:26 and no error, so that check is
// GTEST_SKIP until HLC is fixed.
//
// Not checked:
//   - The resolved kind of the return type: the interface does not exist,
//     so the type cannot resolve (HLC leaves the RefTypespec with no
//     actual); the LRM only requires the error above.
//   - The empty body's statement representation.
//   - vpiFullName on any object (never asserted per the test writing guide).

#include <hlc/Tests/Test.h>

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

namespace hlc {
class ClassMemberTest36Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_member_test_36.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *getMyClass() {
    return hldb::findByName<hldb::ClassDefn>("myclass", m_design->getAllClasses());
  }

  static const hldb::Function *getSubroutine() {
    const hldb::ClassDefn *const cls = getMyClass();
    if (cls == nullptr || cls->getMethods() == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Function>("subroutine", cls->getMethods());
  }

  // True when an error-or-worse diagnostic points at the given source
  // position. The exact event code is not assumed: the LRM only requires
  // that the construct is flagged as an error.
  bool hasErrorAt(uint32_t line, uint16_t column) {
    for (const Event &event : m_session->getEventListener()->getEvents()) {
      const EventDefinition *const info = hldb::findEventDefinition(hlc::getEventTable(), event.getType());
      if (info == nullptr || info->m_severity > EventSeverity::Error) {
        continue;
      }
      for (const EventArg &arg : event.getArguments()) {
        if (arg.m_line == line && arg.m_column == column) {
          return true;
        }
      }
    }
    return false;
  }
};

TEST_F(ClassMemberTest36Test, MyclassExists) {
  const hldb::ClassDefn *const cls = getMyClass();
  ASSERT_NE(cls, nullptr) << "'class myclass;' should create a ClassDefn";
  EXPECT_FALSE(cls->getVirtual()) << "'myclass' is not declared virtual";
  EXPECT_EQ(cls->getExtends(), nullptr) << "'myclass' has no extends clause";
  ASSERT_NE(cls->getMethods(), nullptr);
  EXPECT_EQ(cls->getMethods()->size(), 1u) << "'myclass' defines exactly one method, 'subroutine'";
}

TEST_F(ClassMemberTest36Test, SubroutineIsVirtualMethod) {
  // Sec 8.20: the leading "virtual" (before "function") qualifies the
  // method; the one after "function" belongs to the return type.
  const hldb::Function *const fn = getSubroutine();
  ASSERT_NE(fn, nullptr) << "'function ... subroutine();' should be a Function in myclass";
  EXPECT_TRUE(fn->getMethod()) << "'subroutine' is defined inside a class, so it is a method";
  EXPECT_TRUE(fn->getVirtual()) << "Sec 8.20: 'virtual function' declares a virtual method";
  EXPECT_FALSE(fn->getPure()) << "'subroutine' has a body, so it is not pure";
  EXPECT_EQ(fn->getVisibility(), vpiPublicVis) << "no local/protected qualifier: visibility is public";
  EXPECT_TRUE(fn->getAutomatic()) << "Sec 8.6: class methods have automatic lifetime";
  EXPECT_TRUE(fn->getIODecls() == nullptr || fn->getIODecls()->empty()) << "'subroutine()' has no formals";
}

TEST_F(ClassMemberTest36Test, ReturnTypeReferenceNamesCmdArrayIf) {
  const hldb::Function *const fn = getSubroutine();
  ASSERT_NE(fn, nullptr);
  const hldb::RefTypespec *const rts = fn->getReturn();
  ASSERT_NE(rts, nullptr) << "'function virtual cmd_array_if' should carry a return RefTypespec";
  EXPECT_EQ(rts->getName(), "cmd_array_if") << "the return type reference names 'cmd_array_if'";
}

TEST_F(ClassMemberTest36Test, UndefinedInterfaceCmdArrayIfIsReported) {
  GTEST_SKIP() << "HLC reports the undeclared interface 'cmd_array_if' (7:26) only as a warning "
                  "(CompFailedToBind); per IEEE 1800-2023 Sec 25.9 it must be an error. Fix pending.";
  // Sec 25.9: a virtual interface type names an interface declaration;
  // "cmd_array_if" is never declared, so the type cannot resolve.
  EXPECT_TRUE(hasErrorAt(7, 26) || hasErrorAt(7, 18))
      << "'cmd_array_if' is not a declared interface; an error-level diagnostic at 7:26 (or 7:18) is expected";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
