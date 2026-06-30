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

// Validates the UHDM graph for a module using enum in a numerical expression:
//   module top();
//     typedef enum {a, b, c, d} e;
//     initial begin
//       integer i;
//       e val;
//       val = a;
//       i = val * 4;
//     end
//   endmodule
//
// Checked:
//   - design has module work@top
//   - module has TypedefTypespec "e" → EnumTypespec with 4 consts (a, b, c, d)
//   - Begin block has 2 variables: i (IntegerTypespec) and val (TypedefTypespec e)
//   - 2 statements: val=a (rhs RefObj → EnumConst) and i=val*4 (rhs vpiMultOp)
//   - multiply operands are RefObj "val" and Constant "4"
//   - multiply constant 4 is stored as vpiUIntConst
//
// Not checked:
//   - result type of the multiply expression

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
#include <uhdm/integer_typespec.h>
#include <uhdm/module.h>
#include <uhdm/operation.h>
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/scope.h>
#include <uhdm/typedef_typespec.h>
#include <uhdm/variable.h>
#include <uhdm/vpi_user.h>

namespace SURELOG {

class EnumNumericalExpr : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.19.4--enum_numerical_expr.hlc"});

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

TEST_F(EnumNumericalExpr, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

TEST_F(EnumNumericalExpr, TypedefEWithFourConsts) {
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
// Begin block — 2 variables: i (IntegerTypespec) and val (TypedefTypespec)
// ---------------------------------------------------------------------------
TEST_F(EnumNumericalExpr, BeginHasTwoVariables) {
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
}

TEST_F(EnumNumericalExpr, IVariableIsIntegerType) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      dynamic_cast<const uhdm::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::Variable *const i = blk->getVariables()->at(0);
  ASSERT_NE(i, nullptr);
  EXPECT_EQ(i->getName(), "i");
  EXPECT_NE(i->getTypespec()->getActual<uhdm::IntegerTypespec>(), nullptr);
}

TEST_F(EnumNumericalExpr, ValVariableIsTypedefType) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      dynamic_cast<const uhdm::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  const uhdm::Variable *const val = blk->getVariables()->at(1);
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getName(), "val");
  EXPECT_NE(val->getTypespec()->getActual<uhdm::TypedefTypespec>(), nullptr);
}

// ---------------------------------------------------------------------------
// Statements — 2 assignments
// ---------------------------------------------------------------------------
TEST_F(EnumNumericalExpr, TwoStatements) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Initial *const init =
      dynamic_cast<const uhdm::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const uhdm::Begin *const blk = init->getStmt<uhdm::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  EXPECT_EQ(blk->getStmts()->size(), 2u);
}

TEST_F(EnumNumericalExpr, FirstAssignmentValEqualsA) {
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
  EXPECT_EQ(assign->getLhs<uhdm::RefObj>()->getName(), "val");
  const uhdm::RefObj *const rhs = assign->getRhs<uhdm::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "a");
  EXPECT_NE(rhs->getActual<uhdm::EnumConst>(), nullptr);
}

// ---------------------------------------------------------------------------
// 2nd assignment: i = val * 4 — rhs is Operation(multiply, val, 4)
// ---------------------------------------------------------------------------
TEST_F(EnumNumericalExpr, SecondAssignmentRhsIsMultiplyOp) {
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
  EXPECT_EQ(assign->getLhs<uhdm::RefObj>()->getName(), "i");
  const uhdm::Operation *const op = assign->getRhs<uhdm::Operation>();
  ASSERT_NE(op, nullptr) << "i = val*4 rhs should be an Operation";
  EXPECT_EQ(op->getOpType(), vpiMultOp);
}

TEST_F(EnumNumericalExpr, MultiplyOperandsAreValAnd4) {
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
  const uhdm::Operation *const op = assign->getRhs<uhdm::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const uhdm::RefObj *const lhsOp = any_cast<uhdm::RefObj>(op->getOperands()->at(0));
  ASSERT_NE(lhsOp, nullptr);
  EXPECT_EQ(lhsOp->getName(), "val");
  const uhdm::Constant *const rhsOp = any_cast<uhdm::Constant>(op->getOperands()->at(1));
  ASSERT_NE(rhsOp, nullptr);
  EXPECT_EQ(rhsOp->getDecompile(), "4");
}

TEST_F(EnumNumericalExpr, MultiplyConstant4IsUIntConst) {
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
  const uhdm::Operation *const op = assign->getRhs<uhdm::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const uhdm::Constant *const rhsOp =
      any_cast<uhdm::Constant>(op->getOperands()->at(1));
  ASSERT_NE(rhsOp, nullptr);
  EXPECT_EQ(rhsOp->getConstType(), vpiUIntConst)
      << "Surelog stores unsized integer literals as vpiUIntConst, not vpiIntConst";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
