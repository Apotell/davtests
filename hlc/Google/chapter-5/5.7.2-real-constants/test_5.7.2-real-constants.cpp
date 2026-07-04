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

// Spec-based validation of real literal constants per IEEE 1800-2017 §5.7.2.
//
// Key rules under test:
//   1. Real literal constants are IEEE 754 double-precision (64-bit).
//      → constType must be vpiRealConst (2), size must be 64.
//   2. The exponent symbol may be 'e' or 'E' — both are valid.
//      → 1.2E12 and 1.30e-2 must both parse as real constants.
//   3. Scientific notation without a decimal point is valid (e.g. 23E10).
//      → Only decimal-point forms require a digit on each side.
//   4. Underscores are ignored — 236.123_763_e-12 == 2.36123763e-10.
//      → getValue() must yield the underscore-stripped numeric value.
//
// SV source (module top):
//   logic [31:0] a;
//   initial begin
//     a = 1.2;              // assignment 0
//     a = 0.1;              // assignment 1
//     a = 2394.26331;       // assignment 2
//     a = 1.2E12;           // assignment 3 — uppercase E
//     a = 1.30e-2;          // assignment 4 — lowercase e
//     a = 0.1e-0;           // assignment 5
//     a = 23E10;            // assignment 6 — no decimal point
//     a = 29E-2;            // assignment 7
//     a = 236.123_763_e-12; // assignment 8 — underscores must be ignored
//   end
//
// KNOWN SURELOG BUG (assignment 8):
//   The underscore immediately before the exponent causes Surelog to evaluate
//   the value as 0. The spec-correct value is 236.123763e-12 = 2.36123763e-10.
//   Test AssignmentI_UnderscoresIgnoredPerSpec will FAIL until this is fixed.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/process_stmt.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>

#include <string>

namespace hlc {

class RealConstants : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.7.2-real-constants.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@top", d->getAllModules());
}

static const hldb::Net *getNetA(const hldb::Design *d) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getNets()) return nullptr;
  return hldb::findByName<hldb::Net>("a", m->getNets());
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
TEST_F(RealConstants, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found"; }

TEST_F(RealConstants, OneNetExists) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr);
  EXPECT_EQ(m->getNets()->size(), 1u) << "expected 1 net: a [31:0]";
}

// ---------------------------------------------------------------------------
// Net 'a' packed dimension — logic [31:0]
// §5.7.2 assigns 1.2 (and other real literals) to logic [31:0] a.
// Verifies the compiler correctly parsed the net declaration.
// ---------------------------------------------------------------------------
TEST_F(RealConstants, NetA_HasLogicTypespec) {
  const hldb::Net *const net = getNetA(m_design);
  ASSERT_NE(net, nullptr);
  ASSERT_NE(net->getTypespec(), nullptr);
  EXPECT_NE(net->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr) << "net 'a' should have a LogicTypespec";
}

TEST_F(RealConstants, NetA_RangeLeftBoundIs31) {
  const hldb::Net *const net = getNetA(m_design);
  ASSERT_NE(net, nullptr);
  ASSERT_NE(net->getTypespec(), nullptr);
  const auto *logic = net->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(logic, nullptr);
  const hldb::RangeCollection *const ranges = logic->getRanges();
  ASSERT_NE(ranges, nullptr);
  ASSERT_FALSE(ranges->empty());
  const hldb::Constant *const left = ranges->front()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "31") << "logic [31:0]: left bound is 31";
}

TEST_F(RealConstants, NetA_RangeRightBoundIs0) {
  const hldb::Net *const net = getNetA(m_design);
  ASSERT_NE(net, nullptr);
  ASSERT_NE(net->getTypespec(), nullptr);
  const auto *logic = net->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(logic, nullptr);
  const hldb::RangeCollection *const ranges = logic->getRanges();
  ASSERT_NE(ranges, nullptr);
  ASSERT_FALSE(ranges->empty());
  const hldb::Constant *const right = ranges->front()->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0") << "logic [31:0]: right bound is 0";
}

// ---------------------------------------------------------------------------
// Initial block
// ---------------------------------------------------------------------------
TEST_F(RealConstants, InitialBlockHasBegin) { ASSERT_NE(getBegin(m_design), nullptr); }

TEST_F(RealConstants, BeginHasNineStatements) {
  const hldb::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 9u);
}

TEST_F(RealConstants, AllAssignmentsAreBlocking) {
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
// IEEE 1800-2017 §5.7.2: real literal constants are IEEE 754 double-precision.
// In UHDM: constType = vpiRealConst (2), size = 64 (bits).
// All 9 literals in the source file are valid per §5.7.2.
// ---------------------------------------------------------------------------
TEST_F(RealConstants, AllRhsAreRealConstType) {
  // §5.7.2: real literal constants → vpiRealConst (2)
  const hldb::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  for (std::size_t i = 0; i < begin->getStmts()->size(); ++i) {
    const auto *assign = any_cast<const hldb::Assignment *>((*begin->getStmts())[i]);
    ASSERT_NE(assign, nullptr) << "stmt[" << i << "] is not an Assignment";
    const auto *c = assign->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr) << "stmt[" << i << "] RHS is not a Constant";
    EXPECT_EQ(c->getConstType(), 2) << "stmt[" << i << "]: §5.7.2 requires real type (constType 2)";
  }
}

TEST_F(RealConstants, AllRhsHaveSize64) {
  // §5.7.2 + IEEE 754: double-precision = 64 bits
  const hldb::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  for (std::size_t i = 0; i < begin->getStmts()->size(); ++i) {
    const auto *assign = any_cast<const hldb::Assignment *>((*begin->getStmts())[i]);
    ASSERT_NE(assign, nullptr) << "stmt[" << i << "] is not an Assignment";
    const auto *c = assign->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr) << "stmt[" << i << "] RHS is not a Constant";
    EXPECT_EQ(c->getSize(), 64) << "stmt[" << i << "]: IEEE 754 double-precision requires 64 bits";
  }
}

// ---------------------------------------------------------------------------
// Per-literal numeric value tests — expected values derived from §5.7.2.
// getValue() returns the evaluated numeric string; std::stod converts it.
// ---------------------------------------------------------------------------

// a = 1.2
TEST_F(RealConstants, AssignmentA_Value) {
  const auto *c = getAssignment(m_design, 0)->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_NEAR(std::stod(std::string(c->getValue())), 1.2, 1e-10) << "§5.7.2 example: 1.2 must evaluate to 1.2";
}

// a = 0.1
TEST_F(RealConstants, AssignmentB_Value) {
  const auto *c = getAssignment(m_design, 1)->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_NEAR(std::stod(std::string(c->getValue())), 0.1, 1e-10) << "§5.7.2 example: 0.1 must evaluate to 0.1";
}

// a = 2394.26331
TEST_F(RealConstants, AssignmentC_Value) {
  const auto *c = getAssignment(m_design, 2)->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_NEAR(std::stod(std::string(c->getValue())), 2394.26331, 1e-4)
      << "§5.7.2 example: 2394.26331 must evaluate to 2394.26331";
}

// a = 1.2E12 — §5.7.2: exponent symbol can be e or E
TEST_F(RealConstants, AssignmentD_UpperCaseEValue) {
  const auto *c = getAssignment(m_design, 3)->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_NEAR(std::stod(std::string(c->getValue())), 1.2e12, 1e2)
      << "§5.7.2: uppercase E exponent must evaluate to 1.2e12";
}

// a = 1.30e-2 — §5.7.2: exponent symbol can be e or E
TEST_F(RealConstants, AssignmentE_LowerCaseEValue) {
  const auto *c = getAssignment(m_design, 4)->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_NEAR(std::stod(std::string(c->getValue())), 0.013, 1e-10)
      << "§5.7.2: lowercase e exponent must evaluate to 0.013";
}

// a = 0.1e-0
TEST_F(RealConstants, AssignmentF_Value) {
  const auto *c = getAssignment(m_design, 5)->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_NEAR(std::stod(std::string(c->getValue())), 0.1, 1e-10) << "§5.7.2: 0.1e-0 must evaluate to 0.1";
}

// a = 23E10 — §5.7.2: scientific notation without a decimal point is valid
TEST_F(RealConstants, AssignmentG_NoDecimalPointValue) {
  const auto *c = getAssignment(m_design, 6)->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_NEAR(std::stod(std::string(c->getValue())), 23e10, 1e2)
      << "§5.7.2: 23E10 (no decimal point) must evaluate to 2.3e11";
}

// a = 29E-2
TEST_F(RealConstants, AssignmentH_Value) {
  const auto *c = getAssignment(m_design, 7)->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_NEAR(std::stod(std::string(c->getValue())), 0.29, 1e-10) << "§5.7.2: 29E-2 must evaluate to 0.29";
}

// a = 236.123_763_e-12
// §5.7.2: "underscores are ignored" — value must equal 236.123763e-12.
// SURELOG BUG: Surelog evaluates this as 0 because the underscore immediately
// before the exponent disrupts its parser. This test will FAIL until fixed.
TEST_F(RealConstants, AssignmentI_UnderscoresIgnoredPerSpec) {
  const auto *c = getAssignment(m_design, 8)->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  // Per §5.7.2: underscores in real literals are ignored.
  // 236.123_763_e-12 with underscores stripped = 236.123763e-12 = 2.36123763e-10
  EXPECT_NEAR(std::stod(std::string(c->getValue())), 236.123763e-12, 1e-22)
      << "§5.7.2: underscores must be ignored — 236.123_763_e-12 should "
         "equal 2.36123763e-10, not 0 (Surelog bug)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
