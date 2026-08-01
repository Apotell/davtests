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

// Validates that single-bit fill constants ('0, '1, 'x, 'z) are parsed and
// stored correctly in UHDM.
//
// SV source (module top):
//   logic [15:0] a, b, c, d;
//   initial begin
//     a = '0;   // fill all 16 bits with 0
//     b = '1;   // fill all 16 bits with 1
//     c = 'x;   // fill all 16 bits with x
//     d = 'z;   // fill all 16 bits with z
//   end
//
// UHDM representation:
//   '0, '1, 'x, 'z -> Constant, vpiConstType: binary (3), vpiSize: 1
//     Decompile: "'0" / "'1" / "'x" / "'z"
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
#include <hldb/process_stmt.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>

namespace hlc {

class IntegersLeftPaddingBit : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.7.1--integers-left-padding-bit.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("top", d->getAllModules());
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

// ----
// Module structure
// ----
TEST_F(IntegersLeftPaddingBit, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'top' not found"; }

TEST_F(IntegersLeftPaddingBit, FourVariablesExist) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getVariables(), nullptr);
  EXPECT_EQ(m->getVariables()->size(), 4u) << "expected 4 variables: a, b, c, d (logic[15:0])";
}

// `logic` has no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8 none of
// a, b, c, d must appear in the module's net collection.
TEST_F(IntegersLeftPaddingBit, ModuleHasNoNets) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getNets() || m->getNets()->empty()) << "bare 'logic' declarations must not appear as Nets";
}

// ----
// Initial block -- Begin wrapper with 4 blocking assignments
// ----
TEST_F(IntegersLeftPaddingBit, InitialBlockHasBegin) {
  ASSERT_NE(getBegin(m_design), nullptr) << "Initial block should contain a Begin statement";
}

TEST_F(IntegersLeftPaddingBit, BeginHasFourStatements) {
  const hldb::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 4u) << "Begin block should have 4 assignment statements";
}

TEST_F(IntegersLeftPaddingBit, AllAssignmentsAreBlocking) {
  const hldb::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  for (std::size_t i = 0; i < begin->getStmts()->size(); ++i) {
    const auto *assign = any_cast<const hldb::Assignment *>((*begin->getStmts())[i]);
    ASSERT_NE(assign, nullptr) << "stmt[" << i << "] is not an Assignment";
    EXPECT_TRUE(assign->getBlocking()) << "assignment[" << i << "] should be blocking (=)";
  }
}

// ----
// '0 fill constant -- binary (3), size 1, decompile "'0"
// ----
TEST_F(IntegersLeftPaddingBit, AssignmentA_RhsIsBinaryConstant) {
  // a = '0
  const hldb::Assignment *const assign = getAssignment(m_design, 0);
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "'0 constant should be present as RHS";
  // vpiUdpConst == 3, but in this context binary (3) means fill-0
  EXPECT_EQ(rhs->getConstType(), vpiBinaryConst) << "'0 fill constant should have constType binary (3)";
}

TEST_F(IntegersLeftPaddingBit, AssignmentA_RhsSizeIsOne) {
  const hldb::Assignment *const assign = getAssignment(m_design, 0);
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getSize(), 1) << "'0 fill constant is stored as a single-bit value";
}

TEST_F(IntegersLeftPaddingBit, AssignmentA_RhsDecompile) {
  const hldb::Assignment *const assign = getAssignment(m_design, 0);
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "'0");
}

// ----
// '1 fill constant -- binary (3), size 1, decompile "'1"
// ----
TEST_F(IntegersLeftPaddingBit, AssignmentB_RhsIsBinaryConstant) {
  // b = '1
  const hldb::Assignment *const assign = getAssignment(m_design, 1);
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "'1 constant should be present as RHS";
  EXPECT_EQ(rhs->getConstType(), vpiBinaryConst) << "'1 fill constant should have constType binary (3)";
}

TEST_F(IntegersLeftPaddingBit, AssignmentB_RhsSizeIsOne) {
  const hldb::Assignment *const assign = getAssignment(m_design, 1);
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getSize(), 1);
}

TEST_F(IntegersLeftPaddingBit, AssignmentB_RhsDecompile) {
  const hldb::Assignment *const assign = getAssignment(m_design, 1);
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "'1");
}

// ----
// 'x fill constant -- binary (3), size 1, decompile "'x"
// ----
TEST_F(IntegersLeftPaddingBit, AssignmentC_RhsIsBinaryConstant) {
  // c = 'x
  const hldb::Assignment *const assign = getAssignment(m_design, 2);
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "'x constant should be present as RHS";
  EXPECT_EQ(rhs->getConstType(), vpiBinaryConst) << "'x fill constant should have constType binary (3)";
}

TEST_F(IntegersLeftPaddingBit, AssignmentC_RhsSizeIsOne) {
  const hldb::Assignment *const assign = getAssignment(m_design, 2);
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getSize(), 1);
}

TEST_F(IntegersLeftPaddingBit, AssignmentC_RhsDecompile) {
  const hldb::Assignment *const assign = getAssignment(m_design, 2);
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "'x");
}

// ----
// 'z fill constant -- binary (3), size 1, decompile "'z"
// ----
TEST_F(IntegersLeftPaddingBit, AssignmentD_RhsIsBinaryConstant) {
  // d = 'z
  const hldb::Assignment *const assign = getAssignment(m_design, 3);
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "'z constant should be present as RHS";
  EXPECT_EQ(rhs->getConstType(), vpiBinaryConst) << "'z fill constant should have constType binary (3)";
}

TEST_F(IntegersLeftPaddingBit, AssignmentD_RhsSizeIsOne) {
  const hldb::Assignment *const assign = getAssignment(m_design, 3);
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getSize(), 1);
}

TEST_F(IntegersLeftPaddingBit, AssignmentD_RhsDecompile) {
  const hldb::Assignment *const assign = getAssignment(m_design, 3);
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "'z");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
