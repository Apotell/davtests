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

// Validates the UHDM graph for a module with an INVALID enum assignment
// (should_fail_because: enum enforces strict type checking rules):
//   module top();
//     typedef enum {a, b, c, d} e;
//     initial begin
//       e val;
//       val = 1;
//     end
//   endmodule
//
// Checked:
//   - design has module work@top
//   - module has TypedefTypespec "e" → EnumTypespec with 4 consts (a, b, c, d)
//   - Begin block has 1 Variable "val"
//   - assignment rhs is Constant "1" (vpiUIntConst) — NOT RefObj → EnumConst
//   - assignment rhs is confirmed NOT a RefObj (no enum resolution happened)
//
// Not checked:
//   - Surelog does not emit a compile error for this invalid assignment;
//     this test validates only the UHDM representation, not semantic enforcement

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/assignment.h>
#include <uhdm/begin.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/enum_const.h>
#include <uhdm/enum_typespec.h>
#include <uhdm/initial.h>
#include <uhdm/module.h>
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/scope.h>
#include <uhdm/typedef_typespec.h>
#include <uhdm/variable.h>
#include <uhdm/vpi_user.h>

namespace SURELOG {

class EnumTypeCheckingInv : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.19.3--enum_type_checking_inv.hlc"});

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

TEST_F(EnumTypeCheckingInv, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Module typespec — same typedef enum {a,b,c,d} as the valid variant
// ---------------------------------------------------------------------------
TEST_F(EnumTypeCheckingInv, TypedefEExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::TypedefTypespec *const td =
      uhdm::findByName<uhdm::TypedefTypespec>("e", top->getTypespecs());
  ASSERT_NE(td, nullptr);
  EXPECT_NE(td->getTypedefAlias()->getActual<uhdm::EnumTypespec>(), nullptr);
}

TEST_F(EnumTypeCheckingInv, EnumHasFourConsts) {
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
// Variable "val" in begin block
// ---------------------------------------------------------------------------
TEST_F(EnumTypeCheckingInv, BeginHasVariableVal) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      dynamic_cast<const uhdm::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getVariables(), nullptr);
  ASSERT_EQ(blk->getVariables()->size(), 1u);
  EXPECT_EQ(blk->getVariables()->at(0)->getName(), "val");
}

// ---------------------------------------------------------------------------
// Assignment: val = 1 — rhs is Constant "1" (not RefObj → EnumConst)
// ---------------------------------------------------------------------------
TEST_F(EnumTypeCheckingInv, AssignmentRhsIsIntegerConstant) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      dynamic_cast<const uhdm::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 1u);
  const uhdm::Assignment *const assign =
      any_cast<uhdm::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const uhdm::RefObj *const lhs = assign->getLhs<uhdm::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "val");
  // rhs is a Constant "1" — NOT a RefObj → EnumConst
  const uhdm::Constant *const rhs = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(rhs, nullptr) << "invalid assignment rhs should be stored as Constant in UHDM";
  EXPECT_EQ(rhs->getConstType(), vpiUIntConst);
  EXPECT_EQ(rhs->getDecompile(), "1");
  EXPECT_EQ(assign->getRhs<uhdm::RefObj>(), nullptr)
      << "rhs is an integer literal, not a RefObj → EnumConst";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
