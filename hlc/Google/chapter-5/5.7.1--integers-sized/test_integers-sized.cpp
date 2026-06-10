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

// Validates that sized integer literals of various bases (binary, decimal,
// hexadecimal) including X/Z values are parsed and stored correctly in UHDM.
//
// SV source (module top):
//   logic  [3:0] a;   logic  [4:0] b;   logic [ 2:0] c;
//   logic [11:0] d;   logic [15:0] e;
//   initial begin
//     a = 4'b1001;  // 4-bit binary
//     b = 5'D3;     // 5-bit decimal (uppercase base letter)
//     c = 3'b01x;   // 3-bit binary with unknown LSB
//     d = 12'hx;    // 12-bit all-unknown hex
//     e = 16'hz;    // 16-bit high-impedance hex
//   end
//
// UHDM stores each RHS as a Constant with:
//   vpiConstType: binary(3) / decimal(1) / hexadecimal(5)
//   vpiSize:      the explicit bit-width from the literal
//   getValue():   the digit string as written (e.g. "1001", "01x", "x", "z")
//   getDecompile(): the full base-prefixed form (e.g. "4'b1001", "5'D3")
//
// Notable: uppercase base letter '5'D3' → constType decimal (1), decompile
// preserves the original capitalisation "5'D3".

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

class IntegersSized : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.7.1--integers-sized.hlc"});

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
TEST_F(IntegersSized, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found";
}

TEST_F(IntegersSized, FiveNetsExist) {
  const uhdm::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr);
  EXPECT_EQ(m->getNets()->size(), 5u)
      << "expected 5 nets: a [3:0], b [4:0], c [2:0], d [11:0], e [15:0]";
}

// ---------------------------------------------------------------------------
// Initial block
// ---------------------------------------------------------------------------
TEST_F(IntegersSized, InitialBlockHasBegin) {
  ASSERT_NE(getBegin(m_design), nullptr);
}

TEST_F(IntegersSized, BeginHasFiveStatements) {
  const uhdm::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 5u);
}

TEST_F(IntegersSized, AllAssignmentsAreBlocking) {
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
// a = 4'b1001  — 4-bit binary constant
// ---------------------------------------------------------------------------
TEST_F(IntegersSized, AssignmentA_ConstType) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 0);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), 3) << "4'b1001: constType should be binary (3)";
}

TEST_F(IntegersSized, AssignmentA_SizeAndDecompile) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 0);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 4);
  EXPECT_EQ(c->getDecompile(), "4'b1001");
}

TEST_F(IntegersSized, AssignmentA_getValue) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 0);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue(), "1001");
}

// ---------------------------------------------------------------------------
// b = 5'D3  — 5-bit decimal, uppercase base letter 'D'
// ---------------------------------------------------------------------------
TEST_F(IntegersSized, AssignmentB_ConstType) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 1);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  // Uppercase 'D' is accepted; constType is still decimal (1)
  EXPECT_EQ(c->getConstType(), 1) << "5'D3: constType should be decimal (1)";
}

TEST_F(IntegersSized, AssignmentB_SizeAndDecompile) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 1);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 5);
  // Decompile preserves the original uppercase base letter
  EXPECT_EQ(c->getDecompile(), "5'D3");
}

TEST_F(IntegersSized, AssignmentB_getValue) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 1);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue(), "3");
}

// ---------------------------------------------------------------------------
// c = 3'b01x  — 3-bit binary with unknown (X) LSB
// ---------------------------------------------------------------------------
TEST_F(IntegersSized, AssignmentC_ConstType) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 2);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), 3) << "3'b01x: constType should be binary (3)";
}

TEST_F(IntegersSized, AssignmentC_SizeAndDecompile) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 2);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 3);
  EXPECT_EQ(c->getDecompile(), "3'b01x");
}

TEST_F(IntegersSized, AssignmentC_getValue) {
  // The 'x' digit is stored as-is in the raw value string
  const uhdm::Assignment *const assign = getAssignment(m_design, 2);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue(), "01x");
}

// ---------------------------------------------------------------------------
// d = 12'hx  — 12-bit all-unknown hexadecimal
// ---------------------------------------------------------------------------
TEST_F(IntegersSized, AssignmentD_ConstType) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 3);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), 5) << "12'hx: constType should be hexadecimal (5)";
}

TEST_F(IntegersSized, AssignmentD_SizeAndDecompile) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 3);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 12);
  EXPECT_EQ(c->getDecompile(), "12'hx");
}

TEST_F(IntegersSized, AssignmentD_getValue) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 3);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue(), "x");
}

// ---------------------------------------------------------------------------
// e = 16'hz  — 16-bit high-impedance hexadecimal
// ---------------------------------------------------------------------------
TEST_F(IntegersSized, AssignmentE_ConstType) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 4);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), 5) << "16'hz: constType should be hexadecimal (5)";
}

TEST_F(IntegersSized, AssignmentE_SizeAndDecompile) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 4);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 16);
  EXPECT_EQ(c->getDecompile(), "16'hz");
}

TEST_F(IntegersSized, AssignmentE_getValue) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 4);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue(), "z");
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
