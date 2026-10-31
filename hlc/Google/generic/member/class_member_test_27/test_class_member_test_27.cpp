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

// Source under test: tests/Google/generic/member/class_member_test_27.sv
//
//   class report_server; endclass
//   typedef int uvm_phase;
//
//   class myclass;
//   virtual function void starter(uvm_phase phase);
//     report_server new_server = new;
//   endfunction : starter
//   endclass
//
//   module test;
//   endmodule
//
// Checked (IEEE 1800-2023):
//   - Sec 8.3: class "report_server" exists, empty, with no extends.
//   - Sec 6.18: compilation-unit typedef "uvm_phase" aliases int.
//   - Sec 8.3: class "myclass" exists, not virtual, with no extends.
//   - Sec 8.20 / 8.6: "starter" is the class's only method body: a virtual,
//     non-pure, public Function with automatic lifetime (class methods are
//     automatic), returning void, ending with label "starter" (Sec 13.4).
//   - Sec 13.5: exactly one formal "phase", direction input, whose type
//     reference names "uvm_phase" and resolves to int.
//   - Sec 8.7 / 6.21: local variable "new_server" in "starter" is a
//     report_server class handle with automatic lifetime, initialized by
//     the constructor call "new".
//   - Sec 23.2: module "test" exists.
//   - The file is legal: no fatal, syntax or error diagnostics.
//
// Not checked:
//   - The exact node kind used for the "new" constructor call.
//   - vpiFullName on any object (never asserted per the test writing guide).

#include <hlc/Tests/Test.h>

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>

#include <hldb/Utils.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>
#include <hldb/void_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {
class ClassMemberTest27Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "class_member_test_27.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::ClassDefn *getClass(std::string_view name) {
    return hldb::findByName<hldb::ClassDefn>(name, m_design->getAllClasses());
  }

  static const hldb::Function *getStarter() {
    const hldb::ClassDefn *const cls = getClass("myclass");
    if (cls == nullptr || cls->getMethods() == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Function>("starter", cls->getMethods());
  }

  // Follows a RefTypespec through any typedef layers to the final type.
  static const hldb::Typespec *resolve(const hldb::RefTypespec *rts) {
    while (rts != nullptr) {
      const hldb::Typespec *const actual = rts->getActual();
      const hldb::TypedefTypespec *const tdts = any_cast<hldb::TypedefTypespec>(actual);
      if (tdts == nullptr) {
        return actual;
      }
      if (tdts->getTypedef() == nullptr) {
        return nullptr;
      }
      rts = tdts->getTypedef()->getAlias();
    }
    return nullptr;
  }
};

TEST_F(ClassMemberTest27Test, ReportServerClassExistsAndIsEmpty) {
  const hldb::ClassDefn *const cls = getClass("report_server");
  ASSERT_NE(cls, nullptr) << "'class report_server; endclass' should create a ClassDefn";
  EXPECT_EQ(cls->getExtends(), nullptr) << "'report_server' has no extends clause";
  EXPECT_TRUE(cls->getVariables() == nullptr || cls->getVariables()->empty()) << "'report_server' has no properties";
  EXPECT_TRUE(cls->getMethods() == nullptr || cls->getMethods()->empty()) << "'report_server' has no methods";
}

TEST_F(ClassMemberTest27Test, TypedefUvmPhaseAliasesInt) {
  ASSERT_NE(m_design->getTypedefs(), nullptr) << "'typedef int uvm_phase;' is a compilation-unit typedef";
  const hldb::Typedef *const td = hldb::findByName<hldb::Typedef>("uvm_phase", m_design->getTypedefs());
  ASSERT_NE(td, nullptr) << "typedef 'uvm_phase' should exist";
  ASSERT_NE(td->getAlias(), nullptr);
  EXPECT_NE(any_cast<hldb::IntTypespec>(resolve(td->getAlias())), nullptr) << "Sec 6.18: 'uvm_phase' aliases int";
}

TEST_F(ClassMemberTest27Test, MyclassExists) {
  const hldb::ClassDefn *const cls = getClass("myclass");
  ASSERT_NE(cls, nullptr) << "'class myclass;' should create a ClassDefn";
  EXPECT_FALSE(cls->getVirtual()) << "'myclass' is not declared virtual";
  EXPECT_EQ(cls->getExtends(), nullptr) << "'myclass' has no extends clause";
  ASSERT_NE(cls->getMethods(), nullptr);
  EXPECT_EQ(cls->getMethods()->size(), 1u) << "'myclass' defines exactly one method, 'starter'";
}

TEST_F(ClassMemberTest27Test, StarterIsVirtualVoidMethod) {
  const hldb::Function *const starter = getStarter();
  ASSERT_NE(starter, nullptr) << "'virtual function void starter(...)' should be a Function in myclass";
  EXPECT_TRUE(starter->getMethod()) << "'starter' is defined inside a class, so it is a method";
  EXPECT_TRUE(starter->getVirtual()) << "Sec 8.20: 'starter' is declared virtual";
  EXPECT_FALSE(starter->getPure()) << "'starter' has a body, so it is not pure";
  EXPECT_EQ(starter->getVisibility(), vpiPublicVis) << "no local/protected qualifier: visibility is public";
  EXPECT_TRUE(starter->getAutomatic()) << "Sec 8.6: class methods have automatic lifetime";
  ASSERT_NE(starter->getReturn(), nullptr) << "'function void starter' should carry a return RefTypespec";
  EXPECT_NE(starter->getReturn()->getActual<hldb::VoidTypespec>(), nullptr) << "'starter' returns void";
}

TEST_F(ClassMemberTest27Test, StarterEndLabel) {
  const hldb::Function *const starter = getStarter();
  ASSERT_NE(starter, nullptr);
  EXPECT_EQ(starter->getEndLabel(), "starter") << "Sec 13.4: 'endfunction : starter' carries the end label";
}

TEST_F(ClassMemberTest27Test, StarterFormalPhaseIsInputUvmPhase) {
  const hldb::Function *const starter = getStarter();
  ASSERT_NE(starter, nullptr);
  ASSERT_NE(starter->getIODecls(), nullptr) << "'starter(uvm_phase phase)' declares one formal";
  ASSERT_EQ(starter->getIODecls()->size(), 1u);
  const hldb::IODecl *const phase = starter->getIODecls()->at(0);
  ASSERT_NE(phase, nullptr);
  EXPECT_EQ(phase->getName(), "phase") << "the formal should be named 'phase'";
  EXPECT_EQ(phase->getDirection(), vpiInput) << "Sec 13.5: a formal with no direction defaults to input";
  ASSERT_NE(phase->getTypespec(), nullptr);
  EXPECT_EQ(phase->getTypespec()->getName(), "uvm_phase") << "the formal's type reference names 'uvm_phase'";
  EXPECT_NE(any_cast<hldb::IntTypespec>(resolve(phase->getTypespec())), nullptr)
      << "Sec 6.18: 'uvm_phase' resolves to int";
}

TEST_F(ClassMemberTest27Test, LocalNewServerIsReportServerHandleInitializedByNew) {
  const hldb::Function *const starter = getStarter();
  ASSERT_NE(starter, nullptr);
  ASSERT_NE(starter->getVariables(), nullptr) << "'report_server new_server = new;' declares a local variable";
  const hldb::Variable *const newServer = hldb::findByName<hldb::Variable>("new_server", starter->getVariables());
  ASSERT_NE(newServer, nullptr) << "local variable 'new_server' should exist in 'starter'";
  EXPECT_TRUE(newServer->getAutomatic()) << "Sec 6.21: a variable local to a class method is automatic";

  ASSERT_NE(newServer->getTypespec(), nullptr);
  const hldb::ClassTypespec *const classTs = newServer->getTypespec()->getActual<hldb::ClassTypespec>();
  ASSERT_NE(classTs, nullptr) << "'new_server' should be a class handle";
  EXPECT_EQ(classTs->getClassDefn(), getClass("report_server")) << "'new_server' is a report_server handle";

  ASSERT_NE(newServer->getValue(), nullptr) << "Sec 8.7: '= new' initializes the handle";
  EXPECT_EQ(newServer->getValue()->getName(), "new") << "the initializer is the constructor call 'new'";
}

TEST_F(ClassMemberTest27Test, ModuleTestExists) {
  EXPECT_NE(hldb::findByName<hldb::Module>("test", m_design->getAllModules()), nullptr)
      << "'module test; endmodule' should create a Module";
}

TEST_F(ClassMemberTest27Test, CompilesWithNoErrors) {
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
