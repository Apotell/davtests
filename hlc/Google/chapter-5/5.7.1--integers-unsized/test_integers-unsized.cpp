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

// Validates that unsized integer literals in decimal, hexadecimal, and octal
// bases are parsed and stored correctly in UHDM.
//
// SV source (module top):
//   logic [31:0] a;
//   initial begin
//     a = 659;       // unsized decimal
//     a = 'h 837FF;  // unsized hexadecimal (space between base and value)
//     a = 'o7460;    // unsized octal
//   end
//
// UHDM representation and size semantics:
//   659      → constType: unsigned int (9), size: 64  (default integer width)
//   'h 837FF → constType: hexadecimal (5),  size: -1  (truly unsized)
//   'o7460   → constType: octal (4),         size: -1  (truly unsized)
//
// The asymmetry: bare decimal constants are assigned size 64 (Surelog's default
// integer width), while base-prefixed unsized constants ('h, 'o, 'b) use -1.
// The space in 'h 837FF is stripped; getValue() and getDecompile() both show
// the compact form without whitespace.

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

class IntegersUnsized : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.7.1--integers-unsized.hlc"});

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
TEST_F(IntegersUnsized, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found";
}

TEST_F(IntegersUnsized, OneNetExists) {
  const uhdm::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr);
  EXPECT_EQ(m->getNets()->size(), 1u) << "expected 1 net: a [31:0]";
}

// ---------------------------------------------------------------------------
// Initial block — Begin with 3 statements all assigning to net 'a'
// ---------------------------------------------------------------------------
TEST_F(IntegersUnsized, InitialBlockHasBegin) {
  ASSERT_NE(getBegin(m_design), nullptr);
}

TEST_F(IntegersUnsized, BeginHasThreeStatements) {
  const uhdm::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 3u);
}

TEST_F(IntegersUnsized, AllAssignmentsAreBlocking) {
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
// a = 659  — bare unsized decimal constant
// Stored as unsigned int (9) with size 64 (Surelog's default integer width).
// This differs from base-prefixed unsized constants which use size -1.
// ---------------------------------------------------------------------------
TEST_F(IntegersUnsized, DecimalConstType) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 0);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), 9)
      << "659: bare decimal → constType unsigned int (9)";
}

TEST_F(IntegersUnsized, DecimalSizeIs64) {
  // Bare decimal is sized to 64 by Surelog (default integer width), not -1
  const uhdm::Assignment *const assign = getAssignment(m_design, 0);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 64)
      << "bare decimal constant gets size 64, not -1";
}

TEST_F(IntegersUnsized, DecimalGetValue) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 0);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue(), "659");
}

TEST_F(IntegersUnsized, DecimalGetDecompile) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 0);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "659");
}

// ---------------------------------------------------------------------------
// a = 'h 837FF  — unsized hexadecimal (space between 'h and value stripped)
// Base-prefixed unsized constant: size -1, space removed from output.
// ---------------------------------------------------------------------------
TEST_F(IntegersUnsized, HexConstType) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 1);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), 5)
      << "'h 837FF: constType should be hexadecimal (5)";
}

TEST_F(IntegersUnsized, HexSizeIsMinusOne) {
  // Base-prefixed unsized → size -1 (contrast with bare decimal → size 64)
  const uhdm::Assignment *const assign = getAssignment(m_design, 1);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), -1)
      << "base-prefixed unsized hex constant should have size -1";
}

TEST_F(IntegersUnsized, HexGetValue) {
  // Space between 'h and 837FF is stripped
  const uhdm::Assignment *const assign = getAssignment(m_design, 1);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue(), "837FF");
}

TEST_F(IntegersUnsized, HexGetDecompile) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 1);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "'h837FF");
}

// ---------------------------------------------------------------------------
// a = 'o7460  — unsized octal constant
// vpiOctConst == 4; size -1 like other base-prefixed unsized constants.
// ---------------------------------------------------------------------------
TEST_F(IntegersUnsized, OctalConstType) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 2);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), 4)
      << "'o7460: constType should be octal (4)";
}

TEST_F(IntegersUnsized, OctalSizeIsMinusOne) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 2);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), -1)
      << "unsized octal constant should have size -1";
}

TEST_F(IntegersUnsized, OctalGetValue) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 2);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue(), "7460");
}

TEST_F(IntegersUnsized, OctalGetDecompile) {
  const uhdm::Assignment *const assign = getAssignment(m_design, 2);
  ASSERT_NE(assign, nullptr);
  const auto *c = assign->getRhs<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "'o7460");
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
