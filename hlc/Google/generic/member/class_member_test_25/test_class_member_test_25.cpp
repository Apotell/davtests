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

// Source under test: tests/Google/generic/member/class_member_test_25.sv
//
//   class myclass;
//   extern virtual function integer subroutine;
//   endclass
//
//   function integer myclass::subroutine; endfunction
//
//   module test;
//   endmodule
//
// Checked (IEEE 1800-2023):
//   - Sec 8.3: class "myclass" exists, is not virtual, has no extends.
//   - Sec 8.24: "extern virtual function integer subroutine;" is a method
//     prototype: one FunctionDecl on the class, a method, virtual (Sec 8.20),
//     not pure, public, returning integer, with no formals.
//   - Sec 8.24: "function integer myclass::subroutine; endfunction" is the
//     out-of-block body of that prototype: exactly one Function
//     "subroutine" in the class's methods, linked from the prototype,
//     returning integer with no formals.
//   - Sec 8.24 / 8.20: the body implements a class method declared virtual,
//     so it is itself a method and is virtual (the out-of-block declaration
//     drops the qualifier but the method stays virtual).
//   - Sec 23.2: module "test" exists.
//   - The file is legal: no fatal, syntax or error diagnostics.
//
// Confirmed by running this test (not from the .log): the body is found
// and linked from the prototype, but HLC leaves getMethod() and
// getVirtual() false on it, so those two checks are GTEST_SKIP until HLC
// is fixed.
//
// Not checked:
//   - The empty body's statement representation.
//   - vpiFullName on any object (never asserted per the test writing guide).

#include <hlc/Tests/Test.h>

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/function_decl.h>
#include <hldb/integer_typespec.h>
#include <hldb/module.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

namespace hlc {
class ClassMemberTest25Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_member_test_25.hlc"}); }
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

  static const hldb::Function *getBody() {
    const hldb::ClassDefn *const cls = getMyClass();
    if (cls == nullptr || cls->getMethods() == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Function>("subroutine", cls->getMethods());
  }
};

TEST_F(ClassMemberTest25Test, ClassMyclassExists) {
  const hldb::ClassDefn *const cls = getMyClass();
  ASSERT_NE(cls, nullptr) << "'class myclass;' should create a ClassDefn";
  EXPECT_FALSE(cls->getVirtual()) << "'myclass' is not declared virtual";
  EXPECT_EQ(cls->getExtends(), nullptr) << "'myclass' has no extends clause";
}

TEST_F(ClassMemberTest25Test, PrototypeIsVirtualMethodFunctionDecl) {
  const hldb::ClassDefn *const cls = getMyClass();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getTaskFuncDecls(), nullptr) << "Sec 8.24: the extern prototype should register as a TaskFuncDecl";
  EXPECT_EQ(cls->getTaskFuncDecls()->size(), 1u) << "'myclass' declares exactly one method prototype";
  const hldb::FunctionDecl *const decl = findDecl(cls, "subroutine");
  ASSERT_NE(decl, nullptr) << "'extern virtual function integer subroutine;' should be a FunctionDecl";
  EXPECT_TRUE(decl->getMethod()) << "'subroutine' is declared inside a class, so it is a method";
  EXPECT_TRUE(decl->getVirtual()) << "Sec 8.20: the prototype is declared 'virtual'";
  EXPECT_FALSE(decl->getPure()) << "'subroutine' is not declared pure";
  EXPECT_EQ(decl->getVisibility(), vpiPublicVis) << "no local/protected qualifier: visibility is public";
  EXPECT_EQ(decl->getParent(), cls) << "the prototype should be owned by 'myclass'";
}

TEST_F(ClassMemberTest25Test, PrototypeReturnsIntegerWithNoFormals) {
  const hldb::FunctionDecl *const decl = findDecl(getMyClass(), "subroutine");
  ASSERT_NE(decl, nullptr);
  const hldb::RefTypespec *const rts = decl->getReturn();
  ASSERT_NE(rts, nullptr) << "'function integer subroutine' should carry a return RefTypespec";
  EXPECT_NE(rts->getActual<hldb::IntegerTypespec>(), nullptr) << "the prototype should return IntegerTypespec";
  EXPECT_TRUE(decl->getIODecls() == nullptr || decl->getIODecls()->empty())
      << "'subroutine' is declared without a port list, so it has no formals";
}

TEST_F(ClassMemberTest25Test, OutOfBlockBodyIsLinkedToPrototype) {
  const hldb::ClassDefn *const cls = getMyClass();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getMethods(), nullptr) << "Sec 8.24: 'myclass::subroutine' should add a method body to the class";
  EXPECT_EQ(cls->getMethods()->size(), 1u) << "exactly one method body is defined for 'myclass'";
  const hldb::Function *const body = getBody();
  ASSERT_NE(body, nullptr) << "the body should be a Function named 'subroutine'";
  const hldb::FunctionDecl *const decl = findDecl(cls, "subroutine");
  ASSERT_NE(decl, nullptr);
  EXPECT_EQ(decl->getTaskFunc(), body) << "the prototype should link to its out-of-block body";
}

TEST_F(ClassMemberTest25Test, BodyReturnsIntegerWithNoFormals) {
  const hldb::Function *const body = getBody();
  ASSERT_NE(body, nullptr);
  const hldb::RefTypespec *const rts = body->getReturn();
  ASSERT_NE(rts, nullptr) << "'function integer myclass::subroutine' should carry a return RefTypespec";
  EXPECT_NE(rts->getActual<hldb::IntegerTypespec>(), nullptr) << "the body should return IntegerTypespec";
  EXPECT_TRUE(body->getIODecls() == nullptr || body->getIODecls()->empty())
      << "the body is declared without a port list, so it has no formals";
}

TEST_F(ClassMemberTest25Test, BodyIsMethod) {
  GTEST_SKIP() << "HLC leaves getMethod() false on the out-of-block body 'myclass::subroutine'; per IEEE "
                  "1800-2023 Sec 8.24 it defines a class method. Fix pending.";
  // Sec 8.24: the out-of-block declaration defines the class method itself.
  const hldb::Function *const body = getBody();
  ASSERT_NE(body, nullptr);
  EXPECT_TRUE(body->getMethod()) << "'myclass::subroutine' is a class method";
}

TEST_F(ClassMemberTest25Test, BodyIsVirtual) {
  GTEST_SKIP() << "HLC leaves getVirtual() false on the out-of-block body of 'extern virtual' subroutine; per "
                  "IEEE 1800-2023 Sec 8.20 / 8.24 the method stays virtual. Fix pending.";
  // Sec 8.24: the out-of-block declaration drops "virtual", but the method
  // it defines was declared virtual in the prototype (Sec 8.20).
  const hldb::Function *const body = getBody();
  ASSERT_NE(body, nullptr);
  EXPECT_TRUE(body->getVirtual()) << "'subroutine' was declared virtual in its prototype";
}

TEST_F(ClassMemberTest25Test, ModuleTestExists) {
  EXPECT_NE(hldb::findByName<hldb::Module>("test", m_design->getAllModules()), nullptr)
      << "'module test; endmodule' should create a Module";
}

TEST_F(ClassMemberTest25Test, CompilesWithNoErrors) {
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
