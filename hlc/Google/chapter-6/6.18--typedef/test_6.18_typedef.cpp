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

// Validates the UHDM graph for a module using a typedef:
//   module top();
//     typedef logic logic_t;
//     logic_t a;
//   endmodule
//
// Checked:
//   - design has module work@top with 1 net ('a')
//   - net 'a' RefTypespec name is "logic_t"
//   - net 'a' RefTypespec vpiActual resolves to LogicTypespec
//   - module owns a TypedefTypespec named "logic_t"
//   - TypedefTypespec alias RefTypespec vpiActual resolves to LogicTypespec
//   - net 'a' has no initial value
//   - work@top has no processes
//
// Not checked:
//   - (all observable graph properties of this design are verified above)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_typespec.h>
#include <hldb/scope.h>
#include <hldb/typedef_typespec.h>

namespace hlc {

class Typedef : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.18--typedef.hlc"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

TEST_F(Typedef, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr);
}

TEST_F(Typedef, OneNetExists) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

// ---------------------------------------------------------------------------
// Net 'a' — typespec is a RefTypespec named "logic_t" → LogicTypespec
// ---------------------------------------------------------------------------
TEST_F(Typedef, ANetTypespecNameIsLogicT) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_EQ(rts->getName(), "logic_t");
}

TEST_F(Typedef, ANetTypespecActualIsLogic) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr)
      << "net 'a' resolves through logic_t typedef to LogicTypespec";
}

// ---------------------------------------------------------------------------
// Module typespec collection — contains TypedefTypespec named "logic_t"
// ---------------------------------------------------------------------------
TEST_F(Typedef, ModuleHasTypedefTypespec) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td =
      hldb::findByName<hldb::TypedefTypespec>("logic_t", top->getTypespecs());
  ASSERT_NE(td, nullptr) << "module should own a TypedefTypespec named 'logic_t'";
  EXPECT_EQ(td->getName(), "logic_t");
}

TEST_F(Typedef, TypedefAliasResolvesToLogic) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td =
      hldb::findByName<hldb::TypedefTypespec>("logic_t", top->getTypespecs());
  ASSERT_NE(td, nullptr);
  const hldb::RefTypespec *const alias = td->getTypedefAlias();
  ASSERT_NE(alias, nullptr);
  EXPECT_NE(alias->getActual<hldb::LogicTypespec>(), nullptr)
      << "typedef logic_t alias should resolve to LogicTypespec";
}

TEST_F(Typedef, ANetHasNoInitialValue) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getValue<hldb::Any>(), nullptr);
}

TEST_F(Typedef, NoProcesses) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
