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
//   vpiSize:      -1           (unsized — no explicit bit-width prefix)
//
// getValue() returns the hex digits only (e.g. "x", "3x", "z3", "5").
// getDecompile() returns the full unsized form (e.g. "'hx", "'h3x", "'hz3").

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/assignment.h>
#include <uhdm/begin.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/initial.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/process_stmt.h>
#include <uhdm/ref_obj.h>

namespace SURELOG {

class IntegersLeftPadding : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.7.1--integers-left-padding.hlc"});

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

static const uhdm::Module *getTop(const uhdm::Design *d) {
  return uhdm::findByName<uhdm::Module>("work@top", d->getAllModules());
}

static const uhdm::Begin *getBegin(const uhdm::Design *d) {
  const uhdm::Module *m = getTop(d);
  if (!m || !m->getProcesses() || m->getProcesses()->empty()) return nullptr;
  const auto *initial =
      any_cast<const uhdm::Initial *>((*m->getProcesses())[0]);
  if (!initial) return nullptr;
  return initial->getStmt<uhdm::Begin>();
}

// Returns the i-th statement from the begin block cast to Assignment,
// or nullptr if the index is out of range or the cast fails.
static const uhdm::Assignment *getAssignment(const uhdm::Design *d,
                                              std::size_t index) {
  const uhdm::Begin *begin = getBegin(d);
  if (!begin || !begin->getStmts()) return nullptr;
  if (index >= begin->getStmts()->size()) return nullptr;
  return any_cast<const uhdm::Assignment *>((*begin->getStmts())[index]);
}

// ---------------------------------------------------------------------------
// Module structure
// ---------------------------------------------------------------------------
TEST_F(IntegersLeftPadding, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found";
}

TEST_F(IntegersLeftPadding, SevenNetsExist) {
  const uhdm::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr);
  EXPECT_EQ(m->getNets()->size(), 7u)
      << "expected 7 nets: a, b, c, d (logic[11:0]) and e, f, g (logic[84:0])";
}

// ---------------------------------------------------------------------------
// Initial block — Begin wrapper containing 7 blocking assignments
// ---------------------------------------------------------------------------
TEST_F(IntegersLeftPadding, InitialBlockHasBegin) {
  ASSERT_NE(getBegin(m_design), nullptr)
      << "Initial block should contain a Begin statement";
}

TEST_F(IntegersLeftPadding, BeginHasSevenStatements) {
  const uhdm::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 7u)
      << "Begin block should have 7 assignment statements";
}

// ---------------------------------------------------------------------------
// All 7 assignments are blocking (=, not <=)
// ---------------------------------------------------------------------------
TEST_F(IntegersLeftPadding, AllAssignmentsAreBlocking) {
  const uhdm::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  for (std::size_t i = 0; i < begin->getStmts()->size(); ++i) {
    const auto *assign =
        any_cast<const uhdm::Assignment *>((*begin->getStmts())[i]);
    ASSERT_NE(assign, nullptr) << "stmt[" << i << "] is not an Assignment";
    EXPECT_TRUE(assign->getBlocking())
        << "assignment[" << i << "] should be blocking (=)";
  }
}

// ---------------------------------------------------------------------------
// All RHS constants are hexadecimal (vpiConstType == 5)
// ---------------------------------------------------------------------------
TEST_F(IntegersLeftPadding, AllRhsConstantsAreHexadecimal) {
  const uhdm::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  for (std::size_t i = 0; i < begin->getStmts()->size(); ++i) {
    const auto *assign =
        any_cast<const uhdm::Assignment *>((*begin->getStmts())[i]);
    ASSERT_NE(assign, nullptr) << "stmt[" << i << "] is not an Assignment";
    const auto *rhs = assign->getRhs<uhdm::Constant>();
    ASSERT_NE(rhs, nullptr) << "RHS of assignment[" << i << "] is not a Constant";
    // vpiHexConst == 5
    EXPECT_EQ(rhs->getConstType(), 5)
        << "assignment[" << i << "] RHS should be a hexadecimal constant (5)";
  }
}

// ---------------------------------------------------------------------------
// All RHS constants are unsized (vpiSize == -1 — no explicit bit-width prefix)
// ---------------------------------------------------------------------------
TEST_F(IntegersLeftPadding, AllRhsConstantsAreUnsized) {
  const uhdm::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  for (std::size_t i = 0; i < begin->getStmts()->size(); ++i) {
    const auto *assign =
        any_cast<const uhdm::Assignment *>((*begin->getStmts())[i]);
    ASSERT_NE(assign, nullptr) << "stmt[" << i << "] is not an Assignment";
    const auto *rhs = assign->getRhs<uhdm::Constant>();
    ASSERT_NE(rhs, nullptr) << "RHS of assignment[" << i << "] is not a Constant";
    EXPECT_EQ(rhs->getSize(), -1)
        << "assignment[" << i << "] RHS should be unsized (size == -1)";
  }
}

// ---------------------------------------------------------------------------
// Individual assignment RHS values — raw hex digits as stored by UHDM.
// Left-padding to the LHS width ('h x → xxx for 12-bit) is a simulation-time
// concept and is NOT applied here.
// ---------------------------------------------------------------------------
TEST_F(IntegersLeftPadding, AssignmentA_getValue) {
  // a = 'h x  →  getValue() == "x"
  const uhdm::Assignment *const assign = getAssignment(m_design, 0);
  ASSERT_NE(assign, nullptr);
  const uhdm::Constant *const rhs = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getValue(), "x");
}

TEST_F(IntegersLeftPadding, AssignmentB_getValue) {
  // b = 'h 3x  →  getValue() == "3x"
  const uhdm::Assignment *const assign = getAssignment(m_design, 1);
  ASSERT_NE(assign, nullptr);
  const uhdm::Constant *const rhs = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getValue(), "3x");
}

TEST_F(IntegersLeftPadding, AssignmentC_getValue) {
  // c = 'h z3  →  getValue() == "z3"
  const uhdm::Assignment *const assign = getAssignment(m_design, 2);
  ASSERT_NE(assign, nullptr);
  const uhdm::Constant *const rhs = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getValue(), "z3");
}

TEST_F(IntegersLeftPadding, AssignmentD_getValue) {
  // d = 'h 0z3  →  getValue() == "0z3"
  const uhdm::Assignment *const assign = getAssignment(m_design, 3);
  ASSERT_NE(assign, nullptr);
  const uhdm::Constant *const rhs = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getValue(), "0z3");
}

TEST_F(IntegersLeftPadding, AssignmentE_getValue) {
  // e = 'h5  →  getValue() == "5"
  const uhdm::Assignment *const assign = getAssignment(m_design, 4);
  ASSERT_NE(assign, nullptr);
  const uhdm::Constant *const rhs = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getValue(), "5");
}

TEST_F(IntegersLeftPadding, AssignmentF_getValue) {
  // f = 'hx  →  getValue() == "x"
  const uhdm::Assignment *const assign = getAssignment(m_design, 5);
  ASSERT_NE(assign, nullptr);
  const uhdm::Constant *const rhs = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getValue(), "x");
}

TEST_F(IntegersLeftPadding, AssignmentG_getValue) {
  // g = 'hz  →  getValue() == "z"
  const uhdm::Assignment *const assign = getAssignment(m_design, 6);
  ASSERT_NE(assign, nullptr);
  const uhdm::Constant *const rhs = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getValue(), "z");
}

// ---------------------------------------------------------------------------
// Decompile forms — UHDM reconstructs the full unsized 'h<digits> notation.
// ---------------------------------------------------------------------------
TEST_F(IntegersLeftPadding, AssignmentA_getDecompile) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 0);
  ASSERT_NE(assign, nullptr);
  const uhdm::Constant *const rhs = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "'hx");
}

TEST_F(IntegersLeftPadding, AssignmentB_getDecompile) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 1);
  ASSERT_NE(assign, nullptr);
  const uhdm::Constant *const rhs = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "'h3x");
}

TEST_F(IntegersLeftPadding, AssignmentC_getDecompile) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 2);
  ASSERT_NE(assign, nullptr);
  const uhdm::Constant *const rhs = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "'hz3");
}

TEST_F(IntegersLeftPadding, AssignmentG_getDecompile) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 6);
  ASSERT_NE(assign, nullptr);
  const uhdm::Constant *const rhs = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "'hz");
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
