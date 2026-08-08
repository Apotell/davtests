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

// Validates the UHDM graph for a module with an anonymous enum:
//   module top();
//     enum {a, b, c} val;
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.19 "Enumerations", p.119):
//   "enum {red, yellow, green} light1, light2; // anonymous int type" --
//   an enum with no explicit base type defaults to int, and members
//   auto-increment from 0. "enum {a, b, c} val;" is exactly this legal,
//   anonymous, auto-incrementing form. No :should_fail_because: tag.
//   Also (6.8): "enum" is a data_type alternative, never net_type --
//   "val" must be a Variable, not a Net (fixed from a prior Net-based
//   version, the same misclassification bug found elsewhere this
//   session).
//
// Checked:
//   - design has module top
//   - module has 1 typespec: anonymous EnumTypespec (no TypedefTypespec wrapper)
//   - anonymous EnumTypespec has 3 consts: a, b, c
//   - top has no processes
//   - variable "val" exists
//   - variable "val" RefTypespec vpiActual resolves directly to EnumTypespec
//     (not through a TypedefTypespec)
//   - variable "val" has no initial value
//   - anonymous EnumTypespec has no explicit base typespec stored (default
//     type is implicit, not materialized as a RefTypespec)
//   - enum consts a, b, c have no stored implicit default value (HLC does not
//     materialize the implicit values 0, 1, 2)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/module.h>
#include <hldb/variable.h>
#include <hldb/ref_typespec.h>

namespace hlc {

class EnumAnonTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.19--enum_anon.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(EnumAnonTest, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Module has 1 typespec: anonymous EnumTypespec (no TypedefTypespec wrapper)
// ---------------------------------------------------------------------------
TEST_F(EnumAnonTest, ModuleHasOneTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  EXPECT_EQ(top->getTypespecs()->size(), 1u);
}

TEST_F(EnumAnonTest, TypespecIsAnonymousEnum) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::EnumTypespec *const enumTs = any_cast<hldb::EnumTypespec>(top->getTypespecs()->at(0));
  ASSERT_NE(enumTs, nullptr) << "anonymous enum has EnumTypespec directly, no TypedefTypespec";
}

TEST_F(EnumAnonTest, EnumHasThreeConsts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::EnumTypespec *const enumTs = any_cast<hldb::EnumTypespec>(top->getTypespecs()->at(0));
  ASSERT_NE(enumTs, nullptr);
  ASSERT_NE(enumTs->getEnumConsts(), nullptr);
  EXPECT_EQ(enumTs->getEnumConsts()->size(), 3u);
  EXPECT_EQ(enumTs->getEnumConsts()->at(0)->getName(), "a");
  EXPECT_EQ(enumTs->getEnumConsts()->at(1)->getName(), "b");
  EXPECT_EQ(enumTs->getEnumConsts()->at(2)->getName(), "c");
}

// ---------------------------------------------------------------------------
// No processes (no initial/always block)
// ---------------------------------------------------------------------------
TEST_F(EnumAnonTest, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

// ---------------------------------------------------------------------------
// Module-level Variable "val" -> EnumTypespec directly (not via TypedefTypespec)
// ---------------------------------------------------------------------------
TEST_F(EnumAnonTest, VariableValExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const val = hldb::findByName<hldb::Variable>("val", top->getVariables());
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getName(), "val");
}

TEST_F(EnumAnonTest, VariableValTypespecIsEnumDirectly) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const val = hldb::findByName<hldb::Variable>("val", top->getVariables());
  ASSERT_NE(val, nullptr);
  const hldb::RefTypespec *const rts = val->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::EnumTypespec>(), nullptr)
      << "anonymous enum: variable's typespec resolves to EnumTypespec directly";
}

TEST_F(EnumAnonTest, VariableValHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const val = hldb::findByName<hldb::Variable>("val", top->getVariables());
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getValue<hldb::Any>(), nullptr);
}

// ---------------------------------------------------------------------------
// Anonymous EnumTypespec has no explicit base typespec (default type)
// ---------------------------------------------------------------------------
TEST_F(EnumAnonTest, EnumHasNoExplicitBaseTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::EnumTypespec *const enumTs = any_cast<hldb::EnumTypespec>(top->getTypespecs()->at(0));
  ASSERT_NE(enumTs, nullptr);
  EXPECT_EQ(enumTs->getBaseTypespec(), nullptr)
      << "enum {a, b, c} with no explicit base type stores no base RefTypespec";
}

// ---------------------------------------------------------------------------
// Enum consts a, b, c have no stored implicit default value (0, 1, 2)
// ---------------------------------------------------------------------------
TEST_F(EnumAnonTest, EnumConstsHaveNoImplicitDefaultValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::EnumTypespec *const enumTs = any_cast<hldb::EnumTypespec>(top->getTypespecs()->at(0));
  ASSERT_NE(enumTs, nullptr);
  const auto *consts = enumTs->getEnumConsts();
  ASSERT_NE(consts, nullptr);
  ASSERT_EQ(consts->size(), 3u);
  EXPECT_EQ(consts->at(0)->getValue<hldb::Any>(), nullptr) << "'a' implicit default value 0 is not stored";
  EXPECT_EQ(consts->at(1)->getValue<hldb::Any>(), nullptr) << "'b' implicit default value 1 is not stored";
  EXPECT_EQ(consts->at(2)->getValue<hldb::Any>(), nullptr) << "'c' implicit default value 2 is not stored";
}

TEST_F(EnumAnonTest, CompilerReportsZeroErrors) {
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
