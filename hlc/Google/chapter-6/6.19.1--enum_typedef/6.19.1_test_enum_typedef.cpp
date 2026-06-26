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

// Validates the UHDM graph for a module using a typedef enum:
//   module top();
//     typedef enum {a, b, c} e;
//     e val;
//   endmodule
//
// Checked:
//   - design has module work@top with 1 net ('val')
//   - net 'val' RefTypespec name is "e"
//   - net 'val' RefTypespec vpiActual resolves to LogicTypespec (enum base type in UHDM)
//   - module owns a TypedefTypespec named "e"
//   - TypedefTypespec 'e' alias RefTypespec vpiActual resolves to EnumTypespec
//   - EnumTypespec has 3 consts: a, b, c
//   - net 'val' has no initial value
//
// Not checked:
//   - enum const default values (a=0, b=1, c=2) — Surelog may not store implicit values

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/design.h>
#include <uhdm/enum_const.h>
#include <uhdm/enum_typespec.h>
#include <uhdm/logic_typespec.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/scope.h>
#include <uhdm/typedef_typespec.h>

namespace SURELOG {

class EnumTypedef : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.19.1--enum_typedef.hlc"});

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

TEST_F(EnumTypedef, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

TEST_F(EnumTypedef, OneNetExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

// ---------------------------------------------------------------------------
// Net 'val' — typespec RefTypespec named "e" resolves to LogicTypespec
// ---------------------------------------------------------------------------
TEST_F(EnumTypedef, ValNetTypespecNameIsE) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const val = uhdm::findByName<uhdm::Net>("val", top->getNets());
  ASSERT_NE(val, nullptr);
  const uhdm::RefTypespec *const rts = val->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_EQ(rts->getName(), "e");
}

TEST_F(EnumTypedef, ValNetTypespecActualIsLogic) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const val = uhdm::findByName<uhdm::Net>("val", top->getNets());
  ASSERT_NE(val, nullptr);
  EXPECT_NE(val->getTypespec()->getActual<uhdm::LogicTypespec>(), nullptr)
      << "enum 'e' base type in UHDM resolves to LogicTypespec";
}

// ---------------------------------------------------------------------------
// Module typespec — TypedefTypespec named "e" whose alias → EnumTypespec
// ---------------------------------------------------------------------------
TEST_F(EnumTypedef, ModuleHasTypedefTypespecE) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::TypedefTypespec *const td =
      uhdm::findByName<uhdm::TypedefTypespec>("e", top->getTypespecs());
  ASSERT_NE(td, nullptr) << "module should own a TypedefTypespec named 'e'";
  EXPECT_EQ(td->getName(), "e");
}

TEST_F(EnumTypedef, TypedefEAliasIsEnumTypespec) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::TypedefTypespec *const td =
      uhdm::findByName<uhdm::TypedefTypespec>("e", top->getTypespecs());
  ASSERT_NE(td, nullptr);
  const uhdm::RefTypespec *const alias = td->getTypedefAlias();
  ASSERT_NE(alias, nullptr);
  EXPECT_NE(alias->getActual<uhdm::EnumTypespec>(), nullptr)
      << "typedef 'e' alias should point to the EnumTypespec";
}

// ---------------------------------------------------------------------------
// EnumTypespec — 3 constants: a, b, c
// ---------------------------------------------------------------------------
TEST_F(EnumTypedef, EnumHasThreeConsts) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::TypedefTypespec *const td =
      uhdm::findByName<uhdm::TypedefTypespec>("e", top->getTypespecs());
  ASSERT_NE(td, nullptr);
  const uhdm::EnumTypespec *const enumTs =
      td->getTypedefAlias()->getActual<uhdm::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  ASSERT_NE(enumTs->getEnumConsts(), nullptr);
  EXPECT_EQ(enumTs->getEnumConsts()->size(), 3u);
}

TEST_F(EnumTypedef, EnumConstsAreABC) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::TypedefTypespec *const td =
      uhdm::findByName<uhdm::TypedefTypespec>("e", top->getTypespecs());
  ASSERT_NE(td, nullptr);
  const uhdm::EnumTypespec *const enumTs =
      td->getTypedefAlias()->getActual<uhdm::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  const auto *consts = enumTs->getEnumConsts();
  ASSERT_NE(consts, nullptr);
  ASSERT_EQ(consts->size(), 3u);
  EXPECT_EQ(consts->at(0)->getName(), "a");
  EXPECT_EQ(consts->at(1)->getName(), "b");
  EXPECT_EQ(consts->at(2)->getName(), "c");
}

TEST_F(EnumTypedef, ValNetHasNoInitialValue) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const val = uhdm::findByName<uhdm::Net>("val", top->getNets());
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getValue<uhdm::Any>(), nullptr);
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
