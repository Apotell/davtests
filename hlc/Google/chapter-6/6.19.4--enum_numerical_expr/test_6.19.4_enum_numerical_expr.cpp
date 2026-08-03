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
// What to check and why (IEEE 1800-2023 6.19.4 "Enumerated types in
// numerical expressions", p.122): "An enum variable or identifier used
// as part of an expression is automatically cast to the base type of
// the enum declaration" -- "i = val * 4;" here uses the enum variable
// "val" directly in a numerical expression with no cast required for
// this direction (enum-to-int is automatic); no :should_fail_because: tag.
//
// Checked:
//   - design has module top
//   - module has TypedefTypespec "e" -> EnumTypespec with 4 consts (a, b, c, d)
//   - Begin block has 2 variables: i (IntegerTypespec) and val (TypedefTypespec e)
//   - 2 statements: val=a (rhs RefObj -> EnumConst) and i=val*4 (rhs vpiMultOp)
//   - multiply operands are RefObj "val" and Constant "4"
//   - multiply constant 4 is stored as vpiUIntConst
//   - multiply Operation carries no static result typespec

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
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
#include <hldb/integer_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/scope.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class EnumNumericalExprTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.19.4--enum_numerical_expr.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(EnumNumericalExprTest, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

TEST_F(EnumNumericalExprTest, TypedefEWithFourConsts) {
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
// Begin block -- 2 variables: i (IntegerTypespec) and val (TypedefTypespec)
// ---------------------------------------------------------------------------
TEST_F(EnumNumericalExprTest, BeginHasTwoVariables) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getVariables(), nullptr);
  EXPECT_EQ(blk->getVariables()->size(), 2u);
}

TEST_F(EnumNumericalExprTest, IVariableIsIntegerType) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Variable *const i = blk->getVariables()->at(0);
  ASSERT_NE(i, nullptr);
  EXPECT_EQ(i->getName(), "i");
  EXPECT_NE(i->getTypespec()->getActual<hldb::IntegerTypespec>(), nullptr);
}

TEST_F(EnumNumericalExprTest, ValVariableIsTypedefType) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Variable *const val = blk->getVariables()->at(1);
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getName(), "val");
  EXPECT_NE(val->getTypespec()->getActual<hldb::TypedefTypespec>(), nullptr);
}

// ---------------------------------------------------------------------------
// Statements -- 2 assignments
// ---------------------------------------------------------------------------
TEST_F(EnumNumericalExprTest, TwoStatements) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  EXPECT_EQ(blk->getStmts()->size(), 2u);
}

TEST_F(EnumNumericalExprTest, FirstAssignmentValEqualsA) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "val");
  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "a");
  EXPECT_NE(rhs->getActual<hldb::EnumConst>(), nullptr);
}

// ---------------------------------------------------------------------------
// 2nd assignment: i = val * 4 -- rhs is Operation(multiply, val, 4)
// ---------------------------------------------------------------------------
TEST_F(EnumNumericalExprTest, SecondAssignmentRhsIsMultiplyOp) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "i");
  const hldb::Operation *const op = assign->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "i = val*4 rhs should be an Operation";
  EXPECT_EQ(op->getOpType(), vpiMultOp);
}

TEST_F(EnumNumericalExprTest, MultiplyOperandsAreValAnd4) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  const hldb::Operation *const op = assign->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const hldb::RefObj *const lhsOp = any_cast<hldb::RefObj>(op->getOperands()->at(0));
  ASSERT_NE(lhsOp, nullptr);
  EXPECT_EQ(lhsOp->getName(), "val");
  const hldb::Constant *const rhsOp = any_cast<hldb::Constant>(op->getOperands()->at(1));
  ASSERT_NE(rhsOp, nullptr);
  EXPECT_EQ(rhsOp->getDecompile(), "4");
}

TEST_F(EnumNumericalExprTest, MultiplyConstant4IsUIntConst) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  const hldb::Operation *const op = assign->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const hldb::Constant *const rhsOp = any_cast<hldb::Constant>(op->getOperands()->at(1));
  ASSERT_NE(rhsOp, nullptr);
  EXPECT_EQ(rhsOp->getConstType(), vpiUIntConst)
      << "HLDB stores unsized integer literals as vpiUIntConst, not vpiIntConst";
}

TEST_F(EnumNumericalExprTest, MultiplyOpHasNoStaticResultTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = dynamic_cast<const hldb::Initial *>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  const hldb::Operation *const op = assign->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getTypespec(), nullptr) << "plain multiply Operation (no cast) carries no result typespec";
}

TEST_F(EnumNumericalExprTest, CompilerReportsZeroErrors) {
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
