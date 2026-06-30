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

// Validates the UHDM graph for a module using an enum cast expression:
//   module top();
//     typedef enum {a, b, c, d} e;
//     initial begin
//       e val;
//       val = a;
//       val = e'(val+1);
//     end
//   endmodule
//
// Checked:
//   - design has module work@top
//   - module has TypedefTypespec "e" → EnumTypespec with 4 consts (a, b, c, d)
//   - Begin block has 1 variable "val"
//   - 2 statements; 2nd stmt rhs is Operation(vpiCastOp)
//   - cast operation typespec is RefTypespec → TypedefTypespec "e"
//   - cast operand is Operation(vpiAddOp) with operands RefObj "val" and Constant "1"
//   - 1st assignment (val=a) rhs is RefObj "a" → EnumConst
//   - add inner constant 1 is stored as vpiUIntConst
//
// Not checked:
//   - (all observable graph properties of this design are verified above)

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
#include <uhdm/operation.h>
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/scope.h>
#include <uhdm/typedef_typespec.h>
#include <uhdm/variable.h>
#include <uhdm/vpi_user.h>

namespace SURELOG {

class EnumNumericalExprCast : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.19.4--enum_numerical_expr_cast.hlc"});

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

TEST_F(EnumNumericalExprCast, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

TEST_F(EnumNumericalExprCast, TypedefEExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::TypedefTypespec *const td =
      uhdm::findByName<uhdm::TypedefTypespec>("e", top->getTypespecs());
  ASSERT_NE(td, nullptr);
  EXPECT_NE(td->getTypedefAlias()->getActual<uhdm::EnumTypespec>(), nullptr);
}

TEST_F(EnumNumericalExprCast, EnumHasFourConsts) {
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
// Begin → 1 variable "val"
// ---------------------------------------------------------------------------
TEST_F(EnumNumericalExprCast, BeginHasVariableVal) {
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
// 2nd assignment: val = e'(val+1) — rhs is Operation(cast)
// ---------------------------------------------------------------------------
TEST_F(EnumNumericalExprCast, SecondAssignmentRhsIsCastOp) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      dynamic_cast<const uhdm::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 2u);
  const uhdm::Assignment *const assign =
      any_cast<uhdm::Assignment>(blk->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  const uhdm::Operation *const castOp = assign->getRhs<uhdm::Operation>();
  ASSERT_NE(castOp, nullptr) << "e'(val+1) rhs should be an Operation";
  EXPECT_EQ(castOp->getOpType(), vpiCastOp);
}

TEST_F(EnumNumericalExprCast, CastOpTypespecIsTypedefE) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      dynamic_cast<const uhdm::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::Assignment *const assign =
      any_cast<uhdm::Assignment>(blk->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  const uhdm::Operation *const castOp = assign->getRhs<uhdm::Operation>();
  ASSERT_NE(castOp, nullptr);
  const uhdm::RefTypespec *const rts = castOp->getTypespec();
  ASSERT_NE(rts, nullptr) << "cast operation should carry the cast-target typespec";
  EXPECT_EQ(rts->getName(), "e");
  EXPECT_NE(rts->getActual<uhdm::TypedefTypespec>(), nullptr);
}

TEST_F(EnumNumericalExprCast, CastOperandIsAddOp) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      dynamic_cast<const uhdm::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::Assignment *const assign =
      any_cast<uhdm::Assignment>(blk->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  const uhdm::Operation *const castOp = assign->getRhs<uhdm::Operation>();
  ASSERT_NE(castOp, nullptr);
  ASSERT_NE(castOp->getOperands(), nullptr);
  ASSERT_EQ(castOp->getOperands()->size(), 1u);
  const uhdm::Operation *const addOp =
      any_cast<uhdm::Operation>(castOp->getOperands()->at(0));
  ASSERT_NE(addOp, nullptr) << "cast operand should be an add Operation";
  EXPECT_EQ(addOp->getOpType(), vpiAddOp);
  ASSERT_NE(addOp->getOperands(), nullptr);
  ASSERT_EQ(addOp->getOperands()->size(), 2u);
  const uhdm::RefObj *const lhsOp = any_cast<uhdm::RefObj>(addOp->getOperands()->at(0));
  ASSERT_NE(lhsOp, nullptr);
  EXPECT_EQ(lhsOp->getName(), "val");
  const uhdm::Constant *const rhsOp = any_cast<uhdm::Constant>(addOp->getOperands()->at(1));
  ASSERT_NE(rhsOp, nullptr);
  EXPECT_EQ(rhsOp->getDecompile(), "1");
}

// ---------------------------------------------------------------------------
// 1st assignment: val = a — rhs is RefObj → EnumConst "a"
// ---------------------------------------------------------------------------
TEST_F(EnumNumericalExprCast, FirstAssignmentRhsIsEnumConstA) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      dynamic_cast<const uhdm::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::Assignment *const assign =
      any_cast<uhdm::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  const uhdm::RefObj *const rhs = assign->getRhs<uhdm::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "a");
  EXPECT_NE(rhs->getActual<uhdm::EnumConst>(), nullptr);
}

TEST_F(EnumNumericalExprCast, CastAddConstant1IsUIntConst) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      dynamic_cast<const uhdm::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::Assignment *const assign =
      any_cast<uhdm::Assignment>(blk->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  const uhdm::Operation *const castOp = assign->getRhs<uhdm::Operation>();
  ASSERT_NE(castOp, nullptr);
  ASSERT_NE(castOp->getOperands(), nullptr);
  const uhdm::Operation *const addOp =
      any_cast<uhdm::Operation>(castOp->getOperands()->at(0));
  ASSERT_NE(addOp, nullptr);
  ASSERT_NE(addOp->getOperands(), nullptr);
  ASSERT_EQ(addOp->getOperands()->size(), 2u);
  const uhdm::Constant *const rhsOp =
      any_cast<uhdm::Constant>(addOp->getOperands()->at(1));
  ASSERT_NE(rhsOp, nullptr);
  EXPECT_EQ(rhsOp->getConstType(), vpiUIntConst)
      << "Surelog stores unsized integer literals as vpiUIntConst, not vpiIntConst";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
