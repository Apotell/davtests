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

// Validates that signed integer literals and negated constants are parsed and
// stored correctly in UHDM.
//
// SV source (module top):
//   logic  [7:0] a;
//   logic  [3:0] b;
//   logic  [3:0] c;
//   logic [15:0] d;
//   initial begin
//     a = -8'd 6;   // negated decimal constant
//     b = 4'shf;    // signed hex constant (no negation)
//     c = -4'sd15;  // negated signed decimal constant
//     d = 16'sd?;   // signed decimal constant with Z value
//   end
//
// UHDM representation:
//   Negated literals (-8'd6, -4'sd15) → RHS is Operation, vpiOpType: minus (1)
//     The Operation holds the positive Constant as its sole operand.
//   Non-negated literals (4'shf, 16'sd?) → RHS is Constant directly.
//
// The signed qualifier ('s') affects the typespec but NOT the constType:
//   4'shf   → constType: hexadecimal (5), size: 4, decompile: "4'shf"
//   16'sd?  → constType: decimal (1),     size: 16, decompile: "16'sd?"
//
// All 4 assignments are blocking (=, not <=).

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_obj.h>

namespace hlc {

class IntegersSigned : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.7.1--integers-signed.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@top", d->getAllModules());
}

static const hldb::Begin *getBegin(const hldb::Design *d) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getProcesses() || m->getProcesses()->empty()) return nullptr;
  const auto *initial = any_cast<const hldb::Initial *>((*m->getProcesses())[0]);
  if (!initial) return nullptr;
  return initial->getStmt<hldb::Begin>();
}

static const hldb::Assignment *getAssignment(const hldb::Design *d, std::size_t index) {
  const hldb::Begin *begin = getBegin(d);
  if (!begin || !begin->getStmts()) return nullptr;
  if (index >= begin->getStmts()->size()) return nullptr;
  return any_cast<const hldb::Assignment *>((*begin->getStmts())[index]);
}

// ---------------------------------------------------------------------------
// Module structure
// ---------------------------------------------------------------------------
TEST_F(IntegersSigned, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found"; }

TEST_F(IntegersSigned, FourNetsExist) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr);
  EXPECT_EQ(m->getNets()->size(), 4u) << "expected 4 nets: a [7:0], b [3:0], c [3:0], d [15:0]";
}

// ---------------------------------------------------------------------------
// Initial block
// ---------------------------------------------------------------------------
TEST_F(IntegersSigned, InitialBlockHasBegin) { ASSERT_NE(getBegin(m_design), nullptr); }

TEST_F(IntegersSigned, BeginHasFourStatements) {
  const hldb::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 4u);
}

TEST_F(IntegersSigned, AllAssignmentsAreBlocking) {
  const hldb::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  for (std::size_t i = 0; i < begin->getStmts()->size(); ++i) {
    const auto *assign = any_cast<const hldb::Assignment *>((*begin->getStmts())[i]);
    ASSERT_NE(assign, nullptr) << "stmt[" << i << "] is not an Assignment";
    EXPECT_TRUE(assign->getBlocking()) << "assignment[" << i << "] should be blocking (=)";
  }
}

// ---------------------------------------------------------------------------
// a = -8'd 6
// Negated decimal: RHS is Operation (minus), operand is Constant 8'd6.
// ---------------------------------------------------------------------------
TEST_F(IntegersSigned, AssignmentA_RhsIsOperation) {
  const hldb::Assignment *const assign = getAssignment(m_design, 0);
  ASSERT_NE(assign, nullptr);
  const auto *op = assign->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "RHS of 'a = -8'd6' should be an Operation (unary minus)";
}

TEST_F(IntegersSigned, AssignmentA_OperationTypeIsMinus) {
  const hldb::Assignment *const assign = getAssignment(m_design, 0);
  ASSERT_NE(assign, nullptr);
  const auto *op = assign->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  // vpiMinusOp == 1
  EXPECT_EQ(op->getOpType(), 1) << "unary minus operation should have opType 1 (minus)";
}

TEST_F(IntegersSigned, AssignmentA_OperandIsDecimalConstant) {
  const hldb::Assignment *const assign = getAssignment(m_design, 0);
  ASSERT_NE(assign, nullptr);
  const auto *op = assign->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 1u);
  const auto *c = any_cast<const hldb::Constant *>((*op->getOperands())[0]);
  ASSERT_NE(c, nullptr);
  // vpiDecConst == 1
  EXPECT_EQ(c->getConstType(), 1) << "operand of -8'd6 should be a decimal constant";
  EXPECT_EQ(c->getSize(), 8);
  EXPECT_EQ(c->getDecompile(), "8'd6");
}

// ---------------------------------------------------------------------------
// b = 4'shf
// Non-negated signed hex: RHS is Constant directly.
// The 's' qualifier is reflected in the typespec, not the constType.
// ---------------------------------------------------------------------------
TEST_F(IntegersSigned, AssignmentB_RhsIsConstant) {
  const hldb::Assignment *const assign = getAssignment(m_design, 1);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "RHS of 'b = 4'shf' should be a Constant (no unary minus)";
}

TEST_F(IntegersSigned, AssignmentB_ConstTypeIsHex) {
  const hldb::Assignment *const assign = getAssignment(m_design, 1);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  // vpiHexConst == 5; 's' qualifier does not change constType
  EXPECT_EQ(c->getConstType(), 5) << "4'shf: signed qualifier does not change constType from hex (5)";
}

TEST_F(IntegersSigned, AssignmentB_SizeAndDecompile) {
  const hldb::Assignment *const assign = getAssignment(m_design, 1);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 4);
  EXPECT_EQ(c->getDecompile(), "4'shf");
}

TEST_F(IntegersSigned, AssignmentB_getValue) {
  const hldb::Assignment *const assign = getAssignment(m_design, 1);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue(), "f");
}

// ---------------------------------------------------------------------------
// c = -4'sd15
// Negated signed decimal: RHS is Operation (minus), operand is 4'sd15.
// ---------------------------------------------------------------------------
TEST_F(IntegersSigned, AssignmentC_RhsIsOperation) {
  const hldb::Assignment *const assign = getAssignment(m_design, 2);
  ASSERT_NE(assign, nullptr);
  const auto *op = assign->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "RHS of 'c = -4'sd15' should be an Operation (unary minus)";
  EXPECT_EQ(op->getOpType(), 1);
}

TEST_F(IntegersSigned, AssignmentC_OperandIsSignedDecimalConstant) {
  const hldb::Assignment *const assign = getAssignment(m_design, 2);
  ASSERT_NE(assign, nullptr);
  const auto *op = assign->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 1u);
  const auto *c = any_cast<const hldb::Constant *>((*op->getOperands())[0]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), 1) << "operand of -4'sd15 should be a decimal constant";
  EXPECT_EQ(c->getSize(), 4);
  EXPECT_EQ(c->getDecompile(), "4'sd15");
}

// ---------------------------------------------------------------------------
// d = 16'sd?
// Non-negated signed decimal with Z value ('?' is shorthand for 'z').
// RHS is Constant directly; getValue() returns "?".
// ---------------------------------------------------------------------------
TEST_F(IntegersSigned, AssignmentD_RhsIsConstant) {
  const hldb::Assignment *const assign = getAssignment(m_design, 3);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "RHS of 'd = 16'sd?' should be a Constant";
}

TEST_F(IntegersSigned, AssignmentD_ConstTypeIsDecimal) {
  const hldb::Assignment *const assign = getAssignment(m_design, 3);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), 1) << "16'sd?: constType should be decimal (1)";
}

TEST_F(IntegersSigned, AssignmentD_SizeAndDecompile) {
  const hldb::Assignment *const assign = getAssignment(m_design, 3);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 16);
  EXPECT_EQ(c->getDecompile(), "16'sd?");
}

TEST_F(IntegersSigned, AssignmentD_getValue) {
  // 16'sd? stores the raw '?' character (SV shorthand for all-Z)
  const hldb::Assignment *const assign = getAssignment(m_design, 3);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue(), "?");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
