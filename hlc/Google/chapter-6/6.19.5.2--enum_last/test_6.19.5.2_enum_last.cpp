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

// Validates the UHDM graph for a module using the enum last() method:
//   module top();
//     typedef enum {a, b, c, d} e;
//     initial begin
//       e val = a;
//       val = val.last();
//     end
//   endmodule
//
// Checked:
//   - design has module work@top
//   - module has TypedefTypespec "e" → EnumTypespec with 4 consts (a, b, c, d)
//   - Initial → Begin has 1 Variable "val" (TypedefTypespec, inline init RefObj "a" → EnumConst)
//   - Begin has 1 blocking assignment: val = val.last()
//   - assignment rhs is HierPath "val.last()"
//   - HierPath pathElems[0] is RefObj "val", pathElems[1] is FuncCall "last" (no args)
//   - HierPath receiver RefObj "val" resolves to the local Variable
//
// Not checked:
//   - return value type of last() — enum type, available only at simulation runtime

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/design.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/func_call.h>
#include <hldb/hier_path.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/scope.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>

namespace hlc {

class EnumLast : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.19.5.2--enum_last.hlc"});

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

TEST_F(EnumLast, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Module typespec — TypedefTypespec "e" → EnumTypespec with 4 consts
// ---------------------------------------------------------------------------
TEST_F(EnumLast, TypedefEExists) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td =
      hldb::findByName<hldb::TypedefTypespec>("e", top->getTypespecs());
  ASSERT_NE(td, nullptr);
  EXPECT_NE(td->getTypedefAlias()->getActual<hldb::EnumTypespec>(), nullptr);
}

TEST_F(EnumLast, EnumHasFourConsts) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td =
      hldb::findByName<hldb::TypedefTypespec>("e", top->getTypespecs());
  ASSERT_NE(td, nullptr);
  const hldb::EnumTypespec *const enumTs =
      td->getTypedefAlias()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(enumTs, nullptr);
  ASSERT_NE(enumTs->getEnumConsts(), nullptr);
  EXPECT_EQ(enumTs->getEnumConsts()->size(), 4u);
  EXPECT_EQ(enumTs->getEnumConsts()->at(0)->getName(), "a");
  EXPECT_EQ(enumTs->getEnumConsts()->at(1)->getName(), "b");
  EXPECT_EQ(enumTs->getEnumConsts()->at(2)->getName(), "c");
  EXPECT_EQ(enumTs->getEnumConsts()->at(3)->getName(), "d");
}

// ---------------------------------------------------------------------------
// Variable "val" — declared with inline initializer EnumConst "a"
// ---------------------------------------------------------------------------
TEST_F(EnumLast, ValVariableDeclaredWithInitA) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init =
      dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getVariables(), nullptr);
  ASSERT_EQ(blk->getVariables()->size(), 1u);
  const hldb::Variable *const val = blk->getVariables()->at(0);
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getName(), "val");
  EXPECT_NE(val->getTypespec()->getActual<hldb::TypedefTypespec>(), nullptr);
  const hldb::RefObj *const initVal = val->getValue<hldb::RefObj>();
  ASSERT_NE(initVal, nullptr) << "val inline initializer should be RefObj → EnumConst";
  EXPECT_EQ(initVal->getName(), "a");
  EXPECT_NE(initVal->getActual<hldb::EnumConst>(), nullptr);
}

// ---------------------------------------------------------------------------
// Assignment: val = val.last() — rhs is HierPath "val.last()"
// ---------------------------------------------------------------------------
TEST_F(EnumLast, AssignmentRhsIsHierPath) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init =
      dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 1u);
  const hldb::Assignment *const assign =
      any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "val");
  const hldb::HierPath *const hp = assign->getRhs<hldb::HierPath>();
  ASSERT_NE(hp, nullptr) << "val.last() rhs should be a HierPath";
  EXPECT_EQ(hp->getName(), "val.last()");
}

TEST_F(EnumLast, HierPathReceiverAndFuncCall) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init =
      dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign =
      any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  const hldb::HierPath *const hp = assign->getRhs<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  ASSERT_NE(hp->getPathElems(), nullptr);
  ASSERT_EQ(hp->getPathElems()->size(), 2u);
  const hldb::RefObj *const receiver = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(receiver, nullptr);
  EXPECT_EQ(receiver->getName(), "val");
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(hp->getPathElems()->at(1));
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "last");
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty())
      << "last() takes no arguments";
}

TEST_F(EnumLast, HierPathReceiverResolvesToVariable) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init =
      dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign =
      any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  const hldb::HierPath *const hp = assign->getRhs<hldb::HierPath>();
  ASSERT_NE(hp, nullptr);
  const hldb::RefObj *const receiver = any_cast<hldb::RefObj>(hp->getPathElems()->at(0));
  ASSERT_NE(receiver, nullptr);
  EXPECT_NE(receiver->getActual<hldb::Variable>(), nullptr)
      << "receiver RefObj 'val' in val.last() should resolve to the local Variable";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
