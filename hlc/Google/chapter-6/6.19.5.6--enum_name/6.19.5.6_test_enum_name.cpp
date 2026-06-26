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

// Validates the UHDM graph for a module using the enum name() method:
//   module top();
//     typedef enum {a, b, c, d} e;
//     initial begin
//       e val = a;
//       string s = val.name();
//     end
//   endmodule
//
// Checked:
//   - design has module work@top
//   - module has TypedefTypespec "e" → EnumTypespec with 4 consts (a, b, c, d)
//   - Initial → Begin has 2 Variables (val, s) and NO assignment statements
//     (inline initializers are stored as vpiValue, not as stmt assignments)
//   - val: TypedefTypespec, inline init RefObj "a" → EnumConst
//   - s: StringTypespec ("string" keyword), inline init HierPath "val.name()"
//   - HierPath pathElems[0] is RefObj "val", pathElems[1] is FuncCall "name" (no args)
//   - HierPath receiver RefObj "val" resolves to the local Variable
//
// Not checked:
//   - actual value returned by name() — the string name of the enum value, runtime-only

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/begin.h>
#include <uhdm/design.h>
#include <uhdm/enum_const.h>
#include <uhdm/enum_typespec.h>
#include <uhdm/func_call.h>
#include <uhdm/hier_path.h>
#include <uhdm/initial.h>
#include <uhdm/module.h>
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/scope.h>
#include <uhdm/string_typespec.h>
#include <uhdm/typedef_typespec.h>
#include <uhdm/variable.h>

namespace SURELOG {

class EnumName : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.19.5.6--enum_name.hlc"});

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

TEST_F(EnumName, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Module typespec — TypedefTypespec "e" → EnumTypespec with 4 consts
// ---------------------------------------------------------------------------
TEST_F(EnumName, TypedefEExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::TypedefTypespec *const td =
      uhdm::findByName<uhdm::TypedefTypespec>("e", top->getTypespecs());
  ASSERT_NE(td, nullptr);
  EXPECT_NE(td->getTypedefAlias()->getActual<uhdm::EnumTypespec>(), nullptr);
}

TEST_F(EnumName, EnumHasFourConsts) {
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
  EXPECT_EQ(enumTs->getEnumConsts()->size(), 4u);
  EXPECT_EQ(enumTs->getEnumConsts()->at(0)->getName(), "a");
  EXPECT_EQ(enumTs->getEnumConsts()->at(1)->getName(), "b");
  EXPECT_EQ(enumTs->getEnumConsts()->at(2)->getName(), "c");
  EXPECT_EQ(enumTs->getEnumConsts()->at(3)->getName(), "d");
}

// ---------------------------------------------------------------------------
// Begin block — 2 variables, NO assignment statements
// ---------------------------------------------------------------------------
TEST_F(EnumName, BeginHasTwoVariablesNoStmts) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      dynamic_cast<const uhdm::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getVariables(), nullptr);
  EXPECT_EQ(blk->getVariables()->size(), 2u);
  EXPECT_TRUE(blk->getStmts() == nullptr || blk->getStmts()->empty())
      << "string s = val.name() is an inline initializer, not an assignment statement";
}

// ---------------------------------------------------------------------------
// Variable "val" — TypedefTypespec, inline init = EnumConst "a"
// ---------------------------------------------------------------------------
TEST_F(EnumName, ValVariableDeclaredWithInitA) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      dynamic_cast<const uhdm::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::Variable *const val = blk->getVariables()->at(0);
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getName(), "val");
  EXPECT_NE(val->getTypespec()->getActual<uhdm::TypedefTypespec>(), nullptr);
  const uhdm::RefObj *const initVal = val->getValue<uhdm::RefObj>();
  ASSERT_NE(initVal, nullptr);
  EXPECT_EQ(initVal->getName(), "a");
  EXPECT_NE(initVal->getActual<uhdm::EnumConst>(), nullptr);
}

// ---------------------------------------------------------------------------
// Variable "s" — StringTypespec, inline init = HierPath "val.name()"
// ---------------------------------------------------------------------------
TEST_F(EnumName, SVariableIsStringType) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      dynamic_cast<const uhdm::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::Variable *const s = blk->getVariables()->at(1);
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->getName(), "s");
  EXPECT_NE(s->getTypespec()->getActual<uhdm::StringTypespec>(), nullptr)
      << "string keyword maps to StringTypespec";
}

TEST_F(EnumName, SInitializerIsHierPath) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      dynamic_cast<const uhdm::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::Variable *const s = blk->getVariables()->at(1);
  ASSERT_NE(s, nullptr);
  const uhdm::HierPath *const hp = s->getValue<uhdm::HierPath>();
  ASSERT_NE(hp, nullptr)
      << "s's vpiValue should be HierPath (inline initializer string s = val.name())";
  EXPECT_EQ(hp->getName(), "val.name()");
}

TEST_F(EnumName, HierPathReceiverAndFuncCall) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      dynamic_cast<const uhdm::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::Variable *const s = blk->getVariables()->at(1);
  ASSERT_NE(s, nullptr);
  const uhdm::HierPath *const hp = s->getValue<uhdm::HierPath>();
  ASSERT_NE(hp, nullptr);
  ASSERT_NE(hp->getPathElems(), nullptr);
  ASSERT_EQ(hp->getPathElems()->size(), 2u);
  const uhdm::RefObj *const receiver = any_cast<uhdm::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(receiver, nullptr);
  EXPECT_EQ(receiver->getName(), "val");
  const uhdm::FuncCall *const call = any_cast<uhdm::FuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "name");
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty())
      << "name() takes no arguments";
}

TEST_F(EnumName, HierPathReceiverResolvesToVariable) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      dynamic_cast<const uhdm::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::Variable *const s = blk->getVariables()->at(1);
  ASSERT_NE(s, nullptr);
  const uhdm::HierPath *const hp = s->getValue<uhdm::HierPath>();
  ASSERT_NE(hp, nullptr);
  const uhdm::RefObj *const receiver = any_cast<uhdm::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(receiver, nullptr);
  EXPECT_NE(receiver->getActual<uhdm::Variable>(), nullptr)
      << "receiver RefObj 'val' in val.name() should resolve to the local Variable";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
