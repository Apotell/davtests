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

// Validates that unsized hexadecimal literals containing X/Z digits are parsed
// correctly and stored in UHDM with their raw values (no left-padding applied
// at compile time).
//
// SV source (module top):
//   logic [11:0] a, b, c, d;
//   logic [84:0] e, f, g;
//   initial begin
//     a = 'h x;    // yields xxx     when left-padded to 12 bits
//     b = 'h 3x;   // yields 03x     when left-padded to 12 bits
//     c = 'h z3;   // yields zz3     when left-padded to 12 bits
//     d = 'h 0z3;  // yields 0z3     when left-padded to 12 bits
//     e = 'h5;     // yields {82{0},3'b101} when left-padded to 85 bits
//     f = 'hx;     // yields {85{1'hx}}    when left-padded to 85 bits
//     g = 'hz;     // yields {85{1'hz}}    when left-padded to 85 bits
//   end
//
// Left-padding is a simulation-time elaboration concept; UHDM stores the raw
// constant text exactly as written.  All 7 constants have:
//   vpiConstType: hexadecimal (5)
//   vpiSize:      -1           (unsized -- no explicit bit-width prefix)
//
// getValue() returns the hex digits only (e.g. "x", "3x", "z3", "5").
// getDecompile() returns the full unsized form (e.g. "'hx", "'h3x", "'hz3").

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

class IntegersLeftPadding : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.7.1--integers-left-padding.hlc"}); }
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

// Returns the i-th statement from the begin block cast to Assignment,
// or nullptr if the index is out of range or the cast fails.
static const hldb::Assignment *getAssignment(const hldb::Design *d, std::size_t index) {
  const hldb::Begin *begin = getBegin(d);
  if (!begin || !begin->getStmts()) return nullptr;
  if (index >= begin->getStmts()->size()) return nullptr;
  return any_cast<const hldb::Assignment *>((*begin->getStmts())[index]);
}

// ----
// Module structure
// ----
TEST_F(IntegersLeftPadding, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'top' not found"; }

TEST_F(IntegersLeftPadding, SevenVariablesExist) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getVariables(), nullptr);
  EXPECT_EQ(m->getVariables()->size(), 7u)
      << "expected 7 variables: a, b, c, d (logic[11:0]) and e, f, g (logic[84:0])";
}

// `logic` has no net-type keyword, so per IEEE 1800-2023 Sec 6.7/6.8 none of
// the 7 declarations must appear in the module's net collection.
TEST_F(IntegersLeftPadding, ModuleHasNoNets) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getNets() || m->getNets()->empty()) << "bare 'logic' declarations must not appear as Nets";
}

// ----
// Initial block -- Begin wrapper containing 7 blocking assignments
// ----
TEST_F(IntegersLeftPadding, InitialBlockHasBegin) {
  ASSERT_NE(getBegin(m_design), nullptr) << "Initial block should contain a Begin statement";
}

TEST_F(IntegersLeftPadding, BeginHasSevenStatements) {
  const hldb::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 7u) << "Begin block should have 7 assignment statements";
}

// ----
// All 7 assignments are blocking (=, not <=)
// ----
TEST_F(IntegersLeftPadding, AllAssignmentsAreBlocking) {
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
// All RHS constants are hexadecimal (vpiConstType == 5)
// ----
TEST_F(IntegersLeftPadding, AllRhsConstantsAreHexadecimal) {
  const hldb::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  for (std::size_t i = 0; i < begin->getStmts()->size(); ++i) {
    const auto *assign = any_cast<const hldb::Assignment *>((*begin->getStmts())[i]);
    ASSERT_NE(assign, nullptr) << "stmt[" << i << "] is not an Assignment";
    const auto *rhs = assign->getRhs<hldb::Constant>();
    ASSERT_NE(rhs, nullptr) << "RHS of assignment[" << i << "] is not a Constant";
    // vpiHexConst == 5
    EXPECT_EQ(rhs->getConstType(), 5) << "assignment[" << i << "] RHS should be a hexadecimal constant (5)";
  }
}

// ----
// All RHS constants are unsized (vpiSize == -1 -- no explicit bit-width prefix)
// ----
TEST_F(IntegersLeftPadding, AllRhsConstantsAreUnsized) {
  const hldb::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  for (std::size_t i = 0; i < begin->getStmts()->size(); ++i) {
    const auto *assign = any_cast<const hldb::Assignment *>((*begin->getStmts())[i]);
    ASSERT_NE(assign, nullptr) << "stmt[" << i << "] is not an Assignment";
    const auto *rhs = assign->getRhs<hldb::Constant>();
    ASSERT_NE(rhs, nullptr) << "RHS of assignment[" << i << "] is not a Constant";
    EXPECT_EQ(rhs->getSize(), -1) << "assignment[" << i << "] RHS should be unsized (size == -1)";
  }
}

// ----
// Individual assignment RHS values -- raw hex digits as stored by UHDM.
// Left-padding to the LHS width ('h x -> xxx for 12-bit) is a simulation-time
// concept and is NOT applied here.
// ----
TEST_F(IntegersLeftPadding, AssignmentA_getValue) {
  // a = 'h x  ->  getValue() == "x"
  const hldb::Assignment *const assign = getAssignment(m_design, 0);
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getValue(), std::string_view("x"));
}

TEST_F(IntegersLeftPadding, AssignmentB_getValue) {
  // b = 'h 3x  ->  getValue() == "3x"
  const hldb::Assignment *const assign = getAssignment(m_design, 1);
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getValue(), std::string_view("3x"));
}

TEST_F(IntegersLeftPadding, AssignmentC_getValue) {
  // c = 'h z3  ->  getValue() == "z3"
  const hldb::Assignment *const assign = getAssignment(m_design, 2);
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getValue(), std::string_view("z3"));
}

TEST_F(IntegersLeftPadding, AssignmentD_getValue) {
  // d = 'h 0z3  ->  getValue() == "0z3"
  const hldb::Assignment *const assign = getAssignment(m_design, 3);
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getValue(), std::string_view("0z3"));
}

TEST_F(IntegersLeftPadding, AssignmentE_getValue) {
  // e = 'h5  ->  getValue() == "5"
  const hldb::Assignment *const assign = getAssignment(m_design, 4);
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getValue(), std::string_view("5"));
}

TEST_F(IntegersLeftPadding, AssignmentF_getValue) {
  // f = 'hx  ->  getValue() == "x"
  const hldb::Assignment *const assign = getAssignment(m_design, 5);
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getValue(), std::string_view("x"));
}

TEST_F(IntegersLeftPadding, AssignmentG_getValue) {
  // g = 'hz  ->  getValue() == "z"
  const hldb::Assignment *const assign = getAssignment(m_design, 6);
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getValue(), std::string_view("z"));
}

// ----
// Decompile forms -- UHDM reconstructs the full unsized 'h<digits> notation.
// ----
TEST_F(IntegersLeftPadding, AssignmentA_getDecompile) {
  const hldb::Assignment *const assign = getAssignment(m_design, 0);
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("'h x"));
}

TEST_F(IntegersLeftPadding, AssignmentB_getDecompile) {
  const hldb::Assignment *const assign = getAssignment(m_design, 1);
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("'h 3x"));
}

TEST_F(IntegersLeftPadding, AssignmentC_getDecompile) {
  const hldb::Assignment *const assign = getAssignment(m_design, 2);
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("'h z3"));
}

TEST_F(IntegersLeftPadding, AssignmentG_getDecompile) {
  const hldb::Assignment *const assign = getAssignment(m_design, 6);
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), std::string_view("'hz"));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
