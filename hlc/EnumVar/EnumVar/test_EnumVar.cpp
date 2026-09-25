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

// Validates the UHDM graph for enum-typed variables declared via
// package-scoped typedef names, one directly and one through a further
// typedef alias:
//   package pkg;
//     typedef enum logic[7:0] {
//       ONE,
//       TWO,
//       THREE
//     } enum_t;
//
//     typedef enum_t alias_t;
//   endpackage
//
//   module dut;
//     pkg::enum_t a;
//     pkg::alias_t b;
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.19.1 "Defining new data types
// as enumerated types", p.121): "A type name can be given so that the
// same type can be used in many places" -- "typedef enum ... {ONE, TWO,
// THREE} enum_t;" declares such a type name inside package "pkg", and
// "pkg::enum_t a;" (Sec 26.3, "package_scope") uses it via an explicit
// package-scope resolution operator to declare variable "a".
// "typedef enum_t alias_t;" (Sec 6.18 "Type declarations") further
// declares "alias_t" as another name for the same type, and "pkg::alias_t
// b;" declares "b" through that alias.
//
//   Also (6.8): a type_identifier-declared variable is still a variable
//   declaration, never a net -- "a" and "b" must be Variable, not Net.
//
// Checked:
//   - design has package "pkg" and module "dut"
//   - pkg owns a TypedefTypespec named "enum_t" whose alias resolves to an
//     EnumTypespec with 3 consts: ONE, TWO, THREE (in order)
//   - the enum's base typespec resolves to LogicTypespec (logic[7:0])
//   - pkg owns a second TypedefTypespec named "alias_t" whose alias
//     resolves to the "enum_t" TypedefTypespec (a typedef of a typedef)
//   - dut has exactly 2 variables, "a" and "b", neither duplicated as a Net
//   - "a"'s typespec resolves (getActual<TypedefTypespec>()) to a
//     TypedefTypespec
//   - "b"'s typespec resolves (getActual<TypedefTypespec>()) to a
//     TypedefTypespec
//   - "a" and "b" have no initial value
//   - dut has no processes

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/package.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class EnumVarTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "EnumVar.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("pkg", m_design->getAllPackages());
  }

  static const hldb::Module *getDut() { return hldb::findByName<hldb::Module>("dut", m_design->getAllModules()); }

  static const hldb::TypedefTypespec *getPkgTypedef(std::string_view name) {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr || pkg->getTypespecs() == nullptr) return nullptr;
    return hldb::findByName<hldb::TypedefTypespec>(name, pkg->getTypespecs());
  }
};

// ---------------------------------------------------------------------------
// Existence
// ---------------------------------------------------------------------------

TEST_F(EnumVarTest, PackagePkgExists) { EXPECT_NE(getPkg(), nullptr) << "package 'pkg' not found"; }

TEST_F(EnumVarTest, ModuleDutExists) { EXPECT_NE(getDut(), nullptr) << "module 'dut' not found"; }

// ---------------------------------------------------------------------------
// pkg::enum_t -> TypedefTypespec -> EnumTypespec, 3 consts ONE/TWO/THREE
// ---------------------------------------------------------------------------

TEST_F(EnumVarTest, PkgHasTypedefEnumT) {
  const hldb::TypedefTypespec *const tt = getPkgTypedef("enum_t");
  ASSERT_NE(tt, nullptr) << "package should own a TypedefTypespec named 'enum_t'";
  EXPECT_EQ(tt->getName(), std::string_view("enum_t"));
}

TEST_F(EnumVarTest, EnumTAliasIsEnumTypespecWithLogicBase) {
  const hldb::TypedefTypespec *const tt = getPkgTypedef("enum_t");
  ASSERT_NE(tt, nullptr);
  const hldb::Typedef *const td = tt->getTypedef();
  ASSERT_NE(td, nullptr);
  const hldb::RefTypespec *const alias = td->getAlias();
  ASSERT_NE(alias, nullptr);
  const hldb::EnumTypespec *const enumTs = alias->getActual<hldb::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr) << "typedef 'enum_t' alias should point to the EnumTypespec";
  const hldb::Enum *const e = enumTs->getEnum();
  ASSERT_NE(e, nullptr);
  const hldb::RefTypespec *const base = e->getBaseTypespec();
  ASSERT_NE(base, nullptr) << "'enum logic[7:0]' should have an explicit base typespec";
  EXPECT_NE(base->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(EnumVarTest, EnumTHasThreeConstsOneTwoThree) {
  const hldb::TypedefTypespec *const tt = getPkgTypedef("enum_t");
  ASSERT_NE(tt, nullptr);
  const hldb::Typedef *const td = tt->getTypedef();
  ASSERT_NE(td, nullptr);
  const hldb::EnumTypespec *const enumTs = td->getAlias()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  const hldb::Enum *const e = enumTs->getEnum();
  ASSERT_NE(e, nullptr);
  ASSERT_NE(e->getEnumConsts(), nullptr);
  ASSERT_EQ(e->getEnumConsts()->size(), 3u);
  EXPECT_EQ(e->getEnumConsts()->at(0)->getName(), std::string_view("ONE"));
  EXPECT_EQ(e->getEnumConsts()->at(1)->getName(), std::string_view("TWO"));
  EXPECT_EQ(e->getEnumConsts()->at(2)->getName(), std::string_view("THREE"));
}

// ---------------------------------------------------------------------------
// pkg::alias_t -> TypedefTypespec whose alias resolves to the "enum_t"
// TypedefTypespec (typedef of a typedef, Sec 6.18)
// ---------------------------------------------------------------------------

TEST_F(EnumVarTest, PkgHasTypedefAliasT) {
  const hldb::TypedefTypespec *const tt = getPkgTypedef("alias_t");
  ASSERT_NE(tt, nullptr) << "package should own a TypedefTypespec named 'alias_t'";
  EXPECT_EQ(tt->getName(), std::string_view("alias_t"));
}

TEST_F(EnumVarTest, AliasTAliasResolvesToEnumTTypedef) {
  const hldb::TypedefTypespec *const tt = getPkgTypedef("alias_t");
  ASSERT_NE(tt, nullptr);
  const hldb::Typedef *const td = tt->getTypedef();
  ASSERT_NE(td, nullptr);
  const hldb::RefTypespec *const alias = td->getAlias();
  ASSERT_NE(alias, nullptr);
  const hldb::TypedefTypespec *const target = alias->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(target, nullptr) << "'typedef enum_t alias_t' alias should resolve to the 'enum_t' TypedefTypespec";
  EXPECT_EQ(target->getName(), std::string_view("enum_t"));
}

// ---------------------------------------------------------------------------
// dut: variables "a" (pkg::enum_t) and "b" (pkg::alias_t), never Nets
// ---------------------------------------------------------------------------

TEST_F(EnumVarTest, DutHasTwoVariables) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getVariables(), nullptr);
  EXPECT_EQ(dut->getVariables()->size(), 2u);
}

TEST_F(EnumVarTest, VariableAIsVariableNotNetResolvesToTypedef) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", dut->getVariables());
  ASSERT_NE(a, nullptr) << "'pkg::enum_t a;' should be a Variable per IEEE 1800-2023 6.8";
  EXPECT_EQ(hldb::findByName<hldb::Net>("a", dut->getNets()), nullptr);
  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::TypedefTypespec>(), nullptr)
      << "'a's typespec should resolve to the 'enum_t' TypedefTypespec";
}

TEST_F(EnumVarTest, VariableBIsVariableNotNetResolvesToTypedef) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", dut->getVariables());
  ASSERT_NE(b, nullptr) << "'pkg::alias_t b;' should be a Variable per IEEE 1800-2023 6.8";
  EXPECT_EQ(hldb::findByName<hldb::Net>("b", dut->getNets()), nullptr);
  const hldb::RefTypespec *const rts = b->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::TypedefTypespec>(), nullptr)
      << "'b's typespec should resolve to the 'alias_t' TypedefTypespec";
}

TEST_F(EnumVarTest, VariableAAndBHaveNoInitialValue) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", dut->getVariables());
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", dut->getVariables());
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(a->getValue<hldb::Any>(), nullptr);
  EXPECT_EQ(b->getValue<hldb::Any>(), nullptr);
}

TEST_F(EnumVarTest, DutHasNoProcesses) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  EXPECT_TRUE(dut->getProcesses() == nullptr || dut->getProcesses()->empty());
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

TEST_F(EnumVarTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
