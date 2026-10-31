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

// Source under test: tests/Google/generic/member/class_member_test_20.sv
//
//   class myclass;
//   extern function void subr(ducktype #(3) x);
//   endclass
//
// Checked (IEEE 1800-2023):
//   - Sec 8.3: class "myclass" exists, is not virtual, has no extends.
//   - Sec 8.24: "extern function void subr(...)" is a method prototype only:
//     it registers as a FunctionDecl on the class, with the default (public)
//     visibility, not virtual/pure, and no in-class Function body.
//   - Sec 13.4 / 8.24: the prototype returns void and declares exactly one
//     formal "x", whose direction defaults to input (Sec 13.5).
//   - The formal's declared type name is "ducktype" (a parameterized class
//     type reference "ducktype #(3)", Sec 8.25).
//   - Sec 8.27 / 6.18: "ducktype" is never declared anywhere in the
//     compilation unit, so the reference is undefined and the compiler must
//     flag it as an error: an error-level diagnostic must point at its
//     position (7:27). The exact event code is not assumed.
//
// Not checked:
//   - Any diagnostic on "subr" itself (missing out-of-block body): Sec 8.24
//     does not make a never-defined extern prototype an explicit error.
//   - vpiFullName on any object (never asserted per the test writing guide).

#include <hlc/Tests/Test.h>

#include <hlc/Common/Session.h>
#include <hlc/Events/EventDefinition.h>
#include <hlc/Events/EventListener.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/design.h>
#include <hldb/function_decl.h>
#include <hldb/io_decl.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/void_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {
class ClassMemberTest20Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_member_test_20.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *getMyClass() {
    return hldb::findByName<hldb::ClassDefn>("myclass", m_design->getAllClasses());
  }

  static const hldb::FunctionDecl *findDecl(const hldb::ClassDefn *const cls, std::string_view name) {
    if (cls == nullptr || cls->getTaskFuncDecls() == nullptr) {
      return nullptr;
    }
    for (const hldb::Any *const decl : *cls->getTaskFuncDecls()) {
      const hldb::FunctionDecl *const fd = any_cast<hldb::FunctionDecl>(decl);
      if (fd != nullptr && fd->getName() == name) {
        return fd;
      }
    }
    return nullptr;
  }

  static const hldb::IODecl *getFormalX() {
    const hldb::FunctionDecl *const decl = findDecl(getMyClass(), "subr");
    if (decl == nullptr || decl->getIODecls() == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::IODecl>("x", decl->getIODecls());
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

TEST_F(ClassMemberTest20Test, ClassMyclassExists) {
  const hldb::ClassDefn *const cls = getMyClass();
  ASSERT_NE(cls, nullptr) << "'class myclass;' should create a ClassDefn";
  EXPECT_FALSE(cls->getVirtual()) << "'myclass' is not declared virtual";
  EXPECT_EQ(cls->getExtends(), nullptr) << "'myclass' has no extends clause";
}

TEST_F(ClassMemberTest20Test, SubrIsRegisteredAsFunctionDecl) {
  const hldb::ClassDefn *const cls = getMyClass();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getTaskFuncDecls(), nullptr) << "Sec 8.24: the extern prototype should register as a TaskFuncDecl";
  EXPECT_EQ(cls->getTaskFuncDecls()->size(), 1u) << "'myclass' declares exactly one method prototype";
  EXPECT_NE(findDecl(cls, "subr"), nullptr) << "'extern function void subr(...)' should be a FunctionDecl";
}

TEST_F(ClassMemberTest20Test, SubrHasNoInClassBody) {
  // Sec 8.24: "extern" means the body lives outside the class, so no
  // in-class Function definition named "subr" should exist.
  const hldb::ClassDefn *const cls = getMyClass();
  ASSERT_NE(cls, nullptr);
  EXPECT_TRUE(cls->getMethods() == nullptr || cls->getMethods()->empty())
      << "an extern prototype has no in-class method body";
}

TEST_F(ClassMemberTest20Test, SubrQualifiers) {
  const hldb::FunctionDecl *const decl = findDecl(getMyClass(), "subr");
  ASSERT_NE(decl, nullptr);
  EXPECT_TRUE(decl->getMethod()) << "'subr' is declared inside a class, so it is a method";
  EXPECT_EQ(decl->getVisibility(), vpiPublicVis) << "no local/protected qualifier: visibility is public";
  EXPECT_FALSE(decl->getVirtual()) << "'subr' is not declared virtual";
  EXPECT_FALSE(decl->getPure()) << "'subr' is not declared pure";
  EXPECT_EQ(decl->getParent(), getMyClass()) << "the prototype should be owned by 'myclass'";
}

TEST_F(ClassMemberTest20Test, SubrReturnsVoid) {
  const hldb::FunctionDecl *const decl = findDecl(getMyClass(), "subr");
  ASSERT_NE(decl, nullptr);
  const hldb::RefTypespec *const rts = decl->getReturn();
  ASSERT_NE(rts, nullptr) << "'function void subr' should carry a return RefTypespec";
  EXPECT_NE(rts->getActual<hldb::VoidTypespec>(), nullptr) << "'function void subr' should return VoidTypespec";
}

TEST_F(ClassMemberTest20Test, SubrHasOneInputFormalX) {
  const hldb::FunctionDecl *const decl = findDecl(getMyClass(), "subr");
  ASSERT_NE(decl, nullptr);
  ASSERT_NE(decl->getIODecls(), nullptr) << "'subr(ducktype #(3) x)' declares one formal";
  EXPECT_EQ(decl->getIODecls()->size(), 1u);
  const hldb::IODecl *const x = getFormalX();
  ASSERT_NE(x, nullptr) << "the formal should be named 'x'";
  EXPECT_EQ(x->getDirection(), vpiInput) << "Sec 13.5: a formal with no direction defaults to input";
}

TEST_F(ClassMemberTest20Test, FormalXTypeIsDucktype) {
  const hldb::IODecl *const x = getFormalX();
  ASSERT_NE(x, nullptr);
  const hldb::RefTypespec *const rts = x->getTypespec();
  ASSERT_NE(rts, nullptr) << "'ducktype #(3) x' should carry a RefTypespec";
  EXPECT_EQ(rts->getName(), "ducktype") << "the formal's type reference should name 'ducktype'";
}

TEST_F(ClassMemberTest20Test, UndefinedTypeDucktypeIsReported) {
  // Sec 8.27: a reference to a class that was never declared is undefined
  // and "the compiler flags it as an error"; Sec 6.18 likewise requires a
  // type identifier to resolve to a data type. "ducktype" starts at 7:27.
  EXPECT_TRUE(hasErrorAt(7, 27)) << "'ducktype' is never declared; an error-level diagnostic at 7:27 is expected";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
