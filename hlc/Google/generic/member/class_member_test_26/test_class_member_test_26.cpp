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

// Source under test: tests/Google/generic/member/class_member_test_26.sv
//
//   virtual class myclass;
//   pure virtual function integer subroutine;
//   pure virtual function integer compute(int a, bit b);
//   endclass
//
//   module test;
//   endmodule
//
// Checked (IEEE 1800-2023):
//   - Sec 8.21: "myclass" is an abstract (virtual) class with no extends.
//   - Sec 8.21: each "pure virtual" method is a prototype only: two
//     FunctionDecls on the class, both methods, virtual, pure and public,
//     and no method bodies at all.
//   - "subroutine" returns integer and has no formals.
//   - "compute" returns integer and has exactly two formals in order:
//     "a" of type int (signed, Sec 6.11) and "b" of type bit (unsigned),
//     both defaulting to direction input (Sec 13.5).
//   - Sec 23.2: module "test" exists.
//   - The file is legal: no fatal, syntax or error diagnostics.
//
// Not checked:
//   - vpiFullName on any object (never asserted per the test writing guide).

#include <hlc/Tests/Test.h>

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>

#include <hldb/Utils.h>
#include <hldb/bit_typespec.h>
#include <hldb/class_defn.h>
#include <hldb/design.h>
#include <hldb/function_decl.h>
#include <hldb/int_typespec.h>
#include <hldb/integer_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

namespace hlc {
class ClassMemberTest26Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_member_test_26.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *getMyClass() {
    return hldb::findByName<hldb::ClassDefn>("myclass", m_design->getAllClasses());
  }

  static const hldb::FunctionDecl *findDecl(std::string_view name) {
    const hldb::ClassDefn *const cls = getMyClass();
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

  // Common shape of both "pure virtual function integer ..." prototypes.
  static void expectPureVirtualIntegerPrototype(const hldb::FunctionDecl *const decl) {
    ASSERT_NE(decl, nullptr);
    EXPECT_TRUE(decl->getMethod()) << "a prototype declared inside a class is a method";
    EXPECT_TRUE(decl->getVirtual()) << "Sec 8.21: the prototype is declared 'virtual'";
    EXPECT_TRUE(decl->getPure()) << "Sec 8.21: the prototype is declared 'pure'";
    EXPECT_EQ(decl->getVisibility(), vpiPublicVis) << "no local/protected qualifier: visibility is public";
    EXPECT_EQ(decl->getParent(), getMyClass()) << "the prototype should be owned by 'myclass'";
    ASSERT_NE(decl->getReturn(), nullptr) << "'function integer' should carry a return RefTypespec";
    EXPECT_NE(decl->getReturn()->getActual<hldb::IntegerTypespec>(), nullptr) << "the return type is integer";
  }
};

TEST_F(ClassMemberTest26Test, VirtualClassMyclassExists) {
  const hldb::ClassDefn *const cls = getMyClass();
  ASSERT_NE(cls, nullptr) << "'virtual class myclass;' should create a ClassDefn";
  EXPECT_TRUE(cls->getVirtual()) << "Sec 8.21: 'myclass' is declared virtual (abstract)";
  EXPECT_EQ(cls->getExtends(), nullptr) << "'myclass' has no extends clause";
}

TEST_F(ClassMemberTest26Test, ClassHasTwoPrototypesAndNoBodies) {
  const hldb::ClassDefn *const cls = getMyClass();
  ASSERT_NE(cls, nullptr);
  ASSERT_NE(cls->getTaskFuncDecls(), nullptr) << "the pure virtual prototypes should register as TaskFuncDecls";
  EXPECT_EQ(cls->getTaskFuncDecls()->size(), 2u) << "'myclass' declares exactly two method prototypes";
  EXPECT_TRUE(cls->getMethods() == nullptr || cls->getMethods()->empty())
      << "Sec 8.21: a pure virtual method has no implementation";
}

TEST_F(ClassMemberTest26Test, SubroutineIsPureVirtualWithNoFormals) {
  const hldb::FunctionDecl *const decl = findDecl("subroutine");
  ASSERT_NE(decl, nullptr) << "'pure virtual function integer subroutine;' should be a FunctionDecl";
  expectPureVirtualIntegerPrototype(decl);
  EXPECT_TRUE(decl->getIODecls() == nullptr || decl->getIODecls()->empty())
      << "'subroutine' is declared without a port list, so it has no formals";
}

TEST_F(ClassMemberTest26Test, ComputeIsPureVirtual) {
  const hldb::FunctionDecl *const decl = findDecl("compute");
  ASSERT_NE(decl, nullptr) << "'pure virtual function integer compute(...)' should be a FunctionDecl";
  expectPureVirtualIntegerPrototype(decl);
}

TEST_F(ClassMemberTest26Test, ComputeHasFormalsAAndBInOrder) {
  const hldb::FunctionDecl *const decl = findDecl("compute");
  ASSERT_NE(decl, nullptr);
  ASSERT_NE(decl->getIODecls(), nullptr) << "'compute(int a, bit b)' declares two formals";
  ASSERT_EQ(decl->getIODecls()->size(), 2u);
  EXPECT_EQ(decl->getIODecls()->at(0)->getName(), "a") << "the first formal is 'a'";
  EXPECT_EQ(decl->getIODecls()->at(1)->getName(), "b") << "the second formal is 'b'";
}

TEST_F(ClassMemberTest26Test, ComputeFormalAIsInputInt) {
  const hldb::FunctionDecl *const decl = findDecl("compute");
  ASSERT_NE(decl, nullptr);
  ASSERT_NE(decl->getIODecls(), nullptr);
  const hldb::IODecl *const a = hldb::findByName<hldb::IODecl>("a", decl->getIODecls());
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getDirection(), vpiInput) << "Sec 13.5: a formal with no direction defaults to input";
  ASSERT_NE(a->getTypespec(), nullptr);
  const hldb::IntTypespec *const intTs = a->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(intTs, nullptr) << "'int a' should resolve to IntTypespec";
  EXPECT_TRUE(intTs->getSigned()) << "Sec 6.11: int is signed by default";
}

TEST_F(ClassMemberTest26Test, ComputeFormalBIsInputBit) {
  const hldb::FunctionDecl *const decl = findDecl("compute");
  ASSERT_NE(decl, nullptr);
  ASSERT_NE(decl->getIODecls(), nullptr);
  const hldb::IODecl *const b = hldb::findByName<hldb::IODecl>("b", decl->getIODecls());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getDirection(), vpiInput) << "Sec 13.5: a formal with no direction defaults to input";
  ASSERT_NE(b->getTypespec(), nullptr);
  const hldb::BitTypespec *const bitTs = b->getTypespec()->getActual<hldb::BitTypespec>();
  ASSERT_NE(bitTs, nullptr) << "'bit b' should resolve to BitTypespec";
  EXPECT_FALSE(bitTs->getSigned()) << "Sec 6.11: bit is unsigned by default";
}

TEST_F(ClassMemberTest26Test, ModuleTestExists) {
  EXPECT_NE(hldb::findByName<hldb::Module>("test", m_design->getAllModules()), nullptr)
      << "'module test; endmodule' should create a Module";
}

TEST_F(ClassMemberTest26Test, CompilesWithNoErrors) {
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
