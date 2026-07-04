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

// Validates the UHDM graph for a module using the enum num() method:
//   module top();
//     typedef enum {a, b, c, d} e;
//     initial begin
//       e val = a;
//       int n = val.num();
//     end
//   endmodule
//
// Checked:
//   - design has module work@top
//   - module has TypedefTypespec "e" → EnumTypespec with 4 consts (a, b, c, d)
//   - Initial → Begin has 2 Variables (val, n) and NO assignment statements
//     (inline initializers are stored as vpiValue, not as stmt assignments)
//   - val: TypedefTypespec, inline init RefObj "a" → EnumConst
//   - n: IntTypespec ("int" keyword), inline init HierPath "val.num()"
//   - HierPath pathElems[0] is RefObj "val", pathElems[1] is FuncCall "num" (no args)
//   - HierPath receiver RefObj "val" resolves to the local Variable
//
// Not checked:
//   - actual count returned by num() — 4 (enum member count), runtime-only

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/design.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/func_call.h>
#include <hldb/hier_path.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/scope.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>

namespace hlc {

class EnumNum : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.19.5.5--enum_num.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(EnumNum, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Module typespec — TypedefTypespec "e" → EnumTypespec with 4 consts
// ---------------------------------------------------------------------------
TEST_F(EnumNum, TypedefEExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td = hldb::findByName<hldb::TypedefTypespec>("e", top->getTypespecs());
  ASSERT_NE(td, nullptr);
  EXPECT_NE(td->getTypedefAlias()->getActual<hldb::EnumTypespec>(), nullptr);
}

TEST_F(EnumNum, EnumHasFourConsts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
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
// Begin block — 2 variables, NO assignment statements
// ---------------------------------------------------------------------------
TEST_F(EnumNum, BeginHasTwoVariablesNoStmts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getVariables(), nullptr);
  EXPECT_EQ(blk->getVariables()->size(), 2u);
  EXPECT_TRUE(blk->getStmts() == nullptr || blk->getStmts()->empty())
      << "int n = val.num() is an inline initializer, not an assignment statement";
}

// ---------------------------------------------------------------------------
// Variable "val" — TypedefTypespec, inline init = EnumConst "a"
// ---------------------------------------------------------------------------
TEST_F(EnumNum, ValVariableDeclaredWithInitA) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Variable *const val = blk->getVariables()->at(0);
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getName(), "val");
  EXPECT_NE(val->getTypespec()->getActual<hldb::TypedefTypespec>(), nullptr);
  const hldb::RefObj *const initVal = val->getValue<hldb::RefObj>();
  ASSERT_NE(initVal, nullptr);
  EXPECT_EQ(initVal->getName(), "a");
  EXPECT_NE(initVal->getActual<hldb::EnumConst>(), nullptr);
}

// ---------------------------------------------------------------------------
// Variable "n" — IntTypespec, inline init = HierPath "val.num()"
// ---------------------------------------------------------------------------
TEST_F(EnumNum, NVariableIsIntType) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Variable *const n = blk->getVariables()->at(1);
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(n->getName(), "n");
  EXPECT_NE(n->getTypespec()->getActual<hldb::IntTypespec>(), nullptr)
      << "int keyword maps to IntTypespec (not IntegerTypespec)";
}

TEST_F(EnumNum, NInitializerIsHierPath) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Variable *const n = blk->getVariables()->at(1);
  ASSERT_NE(n, nullptr);
  const hldb::HierPath *const hp = n->getValue<hldb::HierPath>();
  ASSERT_NE(hp, nullptr) << "n's vpiValue should be HierPath (inline initializer int n = val.num())";
  EXPECT_EQ(hp->getName(), "val.num()");
}

TEST_F(EnumNum, HierPathReceiverAndFuncCall) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Variable *const n = blk->getVariables()->at(1);
  ASSERT_NE(n, nullptr);
  const hldb::HierPath *const hp = n->getValue<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  ASSERT_NE(hp->getPathElems(), nullptr);
  ASSERT_EQ(hp->getPathElems()->size(), 2u);
  const hldb::RefObj *const receiver = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(receiver, nullptr);
  EXPECT_EQ(receiver->getName(), "val");
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "num");
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty()) << "num() takes no arguments";
}

TEST_F(EnumNum, HierPathReceiverResolvesToVariable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Variable *const n = blk->getVariables()->at(1);
  ASSERT_NE(n, nullptr);
  const hldb::HierPath *const hp = n->getValue<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  const hldb::RefObj *const receiver = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(receiver, nullptr);
  EXPECT_NE(receiver->getActual<hldb::Variable>(), nullptr)
      << "receiver RefObj 'val' in val.num() should resolve to the local Variable";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
