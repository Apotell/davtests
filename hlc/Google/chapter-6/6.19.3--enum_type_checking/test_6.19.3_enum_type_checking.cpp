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

// Validates the UHDM graph for a module using enum type checking:
//   module top();
//     typedef enum {a, b, c, d} e;
//     initial begin
//       e val;
//       val = a;
//     end
//   endmodule
//
// Checked:
//   - design has module top
//   - module has TypedefTypespec "e" → EnumTypespec with 4 consts (a, b, c, d)
//   - Initial → Begin block has 1 Variable "val"
//   - "val" RefTypespec name is "e", vpiActual resolves to TypedefTypespec
//     (NOT LogicTypespec — local vars resolve differently than module-level nets)
//   - Begin has 1 blocking assignment val=a; lhs is RefObj "val"
//   - assignment rhs is RefObj "a" → EnumConst
//   - Variable "val" has no compile-time initial value
//   - enum consts a, b, c, d have no stored implicit default value (HLC does
//     not materialize the implicit values 0, 1, 2, 3)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/scope.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>

namespace hlc {

class EnumTypeChecking : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.19.3--enum_type_checking.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(EnumTypeChecking, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Module typespec — TypedefTypespec "e" → EnumTypespec with 4 consts
// ---------------------------------------------------------------------------
TEST_F(EnumTypeChecking, TypedefEExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td = hldb::findByName<hldb::TypedefTypespec>("e", top->getTypespecs());
  ASSERT_NE(td, nullptr);
  EXPECT_EQ(td->getName(), "e");
}

TEST_F(EnumTypeChecking, TypedefEAliasIsEnum) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td = hldb::findByName<hldb::TypedefTypespec>("e", top->getTypespecs());
  ASSERT_NE(td, nullptr);
  EXPECT_NE(td->getTypedefAlias()->getActual<hldb::EnumTypespec>(), nullptr);
}

TEST_F(EnumTypeChecking, EnumHasFourConsts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td = hldb::findByName<hldb::TypedefTypespec>("e", top->getTypespecs());
  ASSERT_NE(td, nullptr);
  const hldb::EnumTypespec *const enumTs = td->getTypedefAlias()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  ASSERT_NE(enumTs->getEnumConsts(), nullptr);
  EXPECT_EQ(enumTs->getEnumConsts()->size(), 4u);
  EXPECT_EQ(enumTs->getEnumConsts()->at(0)->getName(), "a");
  EXPECT_EQ(enumTs->getEnumConsts()->at(1)->getName(), "b");
  EXPECT_EQ(enumTs->getEnumConsts()->at(2)->getName(), "c");
  EXPECT_EQ(enumTs->getEnumConsts()->at(3)->getName(), "d");
}

// ---------------------------------------------------------------------------
// Initial → Begin → Variable "val" (RefTypespec → TypedefTypespec)
// ---------------------------------------------------------------------------
TEST_F(EnumTypeChecking, BeginHasVariableVal) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getVariables(), nullptr);
  ASSERT_EQ(blk->getVariables()->size(), 1u);
  const hldb::Variable *const val = blk->getVariables()->at(0);
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getName(), "val");
}

TEST_F(EnumTypeChecking, ValTypespecIsTypedef) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Variable *const val = blk->getVariables()->at(0);
  ASSERT_NE(val, nullptr);
  const hldb::RefTypespec *const rts = val->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_EQ(rts->getName(), "e");
  EXPECT_NE(rts->getActual<hldb::TypedefTypespec>(), nullptr)
      << "local variable 'val' typespec resolves to TypedefTypespec (not LogicTypespec)";
}

// ---------------------------------------------------------------------------
// Assignment: val = a — rhs is RefObj → EnumConst "a"
// ---------------------------------------------------------------------------
TEST_F(EnumTypeChecking, AssignmentRhsIsEnumConst) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 1u);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "val");
  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "rhs of val=a should be a RefObj";
  EXPECT_EQ(rhs->getName(), "a");
  EXPECT_NE(rhs->getActual<hldb::EnumConst>(), nullptr) << "rhs RefObj 'a' should resolve to EnumConst";
}

// ---------------------------------------------------------------------------
// Variable "val" — no compile-time initial value (declared without init)
// ---------------------------------------------------------------------------
TEST_F(EnumTypeChecking, ValVariableHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Variable *const val = blk->getVariables()->at(0);
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getValue<hldb::Any>(), nullptr);
}

// ---------------------------------------------------------------------------
// Enum consts a, b, c, d have no stored implicit default value (0, 1, 2, 3)
// ---------------------------------------------------------------------------
TEST_F(EnumTypeChecking, EnumConstsHaveNoImplicitDefaultValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td = hldb::findByName<hldb::TypedefTypespec>("e", top->getTypespecs());
  ASSERT_NE(td, nullptr);
  const hldb::EnumTypespec *const enumTs = td->getTypedefAlias()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  const auto *consts = enumTs->getEnumConsts();
  ASSERT_NE(consts, nullptr);
  ASSERT_EQ(consts->size(), 4u);
  EXPECT_EQ(consts->at(0)->getValue<hldb::Constant>(), nullptr) << "'a' implicit default value 0 is not stored";
  EXPECT_EQ(consts->at(1)->getValue<hldb::Constant>(), nullptr) << "'b' implicit default value 1 is not stored";
  EXPECT_EQ(consts->at(2)->getValue<hldb::Constant>(), nullptr) << "'c' implicit default value 2 is not stored";
  EXPECT_EQ(consts->at(3)->getValue<hldb::Constant>(), nullptr) << "'d' implicit default value 3 is not stored";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
