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
//   - design has module top with 1 variable ('val') -- IEEE 1800-2023 6.19/6.8:
//     enum-typed (even via typedef) declaration with no net-type keyword is a
//     variable, not a net
//   - variable 'val' RefTypespec name is "e"
//   - variable 'val' RefTypespec vpiActual resolves to TypedefTypespec "e"
//     (not directly to EnumTypespec -- one level of typedef indirection)
//   - module owns a TypedefTypespec named "e"
//   - TypedefTypespec 'e' alias RefTypespec vpiActual resolves to EnumTypespec
//   - EnumTypespec has 3 consts: a, b, c
//   - variable 'val' has no initial value
//   - enum consts a, b, c have no stored implicit default value (HLC does not
//     materialize the implicit values 0, 1, 2)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_typespec.h>
#include <hldb/scope.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>

namespace hlc {

class EnumTypedef : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.19.1--enum_typedef.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(EnumTypedef, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

TEST_F(EnumTypedef, OneVariableExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u);
}

// ----
// Variable 'val' -- typespec RefTypespec named "e" resolves to TypedefTypespec
// ----
TEST_F(EnumTypedef, ValNotInNets) {
  // Per IEEE 1800-2023 Sec 6.7/6.8, 'e' (a typedef'd enum) has no net-type
  // keyword, so 'val' must not also be materialized as a Net.
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || hldb::findByName<hldb::Net>("val", top->getNets()) == nullptr)
      << "'e val' must not appear in vpiNet";
}

TEST_F(EnumTypedef, ValVariableTypespecNameIsE) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const val = hldb::findByName<hldb::Variable>("val", top->getVariables());
  ASSERT_NE(val, nullptr);
  const hldb::RefTypespec *const rts = val->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_EQ(rts->getName(), "e");
}

TEST_F(EnumTypedef, ValVariableTypespecActualIsTypedefTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const val = hldb::findByName<hldb::Variable>("val", top->getVariables());
  ASSERT_NE(val, nullptr);
  EXPECT_NE(val->getTypespec()->getActual<hldb::TypedefTypespec>(), nullptr) << "enum 'e' resolves to TypedefTypespec";
}

// ----
// Module typespec -- TypedefTypespec named "e" whose alias -> EnumTypespec
// ----
TEST_F(EnumTypedef, ModuleHasTypedefTypespecE) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td = hldb::findByName<hldb::TypedefTypespec>("e", top->getTypespecs());
  ASSERT_NE(td, nullptr) << "module should own a TypedefTypespec named 'e'";
  EXPECT_EQ(td->getName(), "e");
}

TEST_F(EnumTypedef, TypedefEAliasIsEnumTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td = hldb::findByName<hldb::TypedefTypespec>("e", top->getTypespecs());
  ASSERT_NE(td, nullptr);
  const hldb::RefTypespec *const alias = td->getTypedefAlias();
  ASSERT_NE(alias, nullptr);
  EXPECT_NE(alias->getActual<hldb::EnumTypespec>(), nullptr) << "typedef 'e' alias should point to the EnumTypespec";
}

// ----
// EnumTypespec -- 3 constants: a, b, c
// ----
TEST_F(EnumTypedef, EnumHasThreeConsts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td = hldb::findByName<hldb::TypedefTypespec>("e", top->getTypespecs());
  ASSERT_NE(td, nullptr);
  const hldb::EnumTypespec *const enumTs = td->getTypedefAlias()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  ASSERT_NE(enumTs->getEnumConsts(), nullptr);
  EXPECT_EQ(enumTs->getEnumConsts()->size(), 3u);
}

TEST_F(EnumTypedef, EnumConstsAreABC) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td = hldb::findByName<hldb::TypedefTypespec>("e", top->getTypespecs());
  ASSERT_NE(td, nullptr);
  const hldb::EnumTypespec *const enumTs = td->getTypedefAlias()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  const auto *consts = enumTs->getEnumConsts();
  ASSERT_NE(consts, nullptr);
  ASSERT_EQ(consts->size(), 3u);
  EXPECT_EQ(consts->at(0)->getName(), "a");
  EXPECT_EQ(consts->at(1)->getName(), "b");
  EXPECT_EQ(consts->at(2)->getName(), "c");
}

TEST_F(EnumTypedef, ValVariableHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const val = hldb::findByName<hldb::Variable>("val", top->getVariables());
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getValue<hldb::Any>(), nullptr);
}

// ----
// Enum consts a, b, c have no stored implicit default value (0, 1, 2)
// ----
TEST_F(EnumTypedef, EnumConstsHaveNoImplicitDefaultValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td = hldb::findByName<hldb::TypedefTypespec>("e", top->getTypespecs());
  ASSERT_NE(td, nullptr);
  const hldb::EnumTypespec *const enumTs = td->getTypedefAlias()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  const auto *consts = enumTs->getEnumConsts();
  ASSERT_NE(consts, nullptr);
  ASSERT_EQ(consts->size(), 3u);
  EXPECT_EQ(consts->at(0)->getValue<hldb::Constant>(), nullptr) << "'a' implicit default value 0 is not stored";
  EXPECT_EQ(consts->at(1)->getValue<hldb::Constant>(), nullptr) << "'b' implicit default value 1 is not stored";
  EXPECT_EQ(consts->at(2)->getValue<hldb::Constant>(), nullptr) << "'c' implicit default value 2 is not stored";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
