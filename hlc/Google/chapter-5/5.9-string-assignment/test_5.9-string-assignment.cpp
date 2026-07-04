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

// Spec-based validation of string assignment to integral types per
// IEEE 1800-2017 §5.9.
//
// Key §5.9 rules under test:
//   1. A string literal can be assigned to any integral variable (byte, bit,
//      logic). The string is treated as a sequence of 8-bit ASCII characters.
//   2. The number of bits required to hold a string is 8 × number of chars.
//      A 1-character string like "a" occupies exactly 8 bits.
//   3. When the string is shorter than the target variable, the string is
//      left-justified with zeros padded to the right. When longer, leftmost
//      characters are truncated. For a 1-char string to an 8-bit type:
//      no truncation or padding occurs.
//   4. The three integral types in this file differ in their typespecs:
//      byte   → ByteTypespec (implicit 8-bit, 2-state, signed)
//      bit    → BitTypespec  (2-state, unsigned, explicit range required)
//      logic  → LogicTypespec (4-state, explicit range required)
//
// SV source (module top):
//   byte        a;        // implicit 8-bit, signed, 2-state
//   bit   [7:0] b;        // explicit 8-bit, unsigned, 2-state
//   logic [7:0] c;        // explicit 8-bit, 4-state
//   initial begin
//     a = "a";            // assignment 0 — string "a" to byte
//     b = "b";            // assignment 1 — string "b" to bit [7:0]
//     c = "c";            // assignment 2 — string "c" to logic [7:0]
//   end
//
// UHDM representation:
//   All three RHS string literals: constType = vpiStringConst (6), size = 8.
//   The LHS nets carry their declared typespecs (ByteTypespec, BitTypespec,
//   LogicTypespec), which Surelog correctly distinguishes.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_typespec.h>
#include <hldb/byte_typespec.h>
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
#include <hldb/string_typespec.h>

namespace hlc {

class StringAssignment : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.9-string-assignment.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@top", d->getAllModules());
}

static const hldb::Net *getNet(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getNets()) return nullptr;
  return hldb::findByName<hldb::Net>(name, m->getNets());
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
TEST_F(StringAssignment, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found"; }

TEST_F(StringAssignment, ThreeNetsExist) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr);
  EXPECT_EQ(m->getNets()->size(), 3u) << "expected 3 nets: a (byte), b (bit), c (logic)";
}

// ---------------------------------------------------------------------------
// §5.9: integral type declarations — each type has a distinct UHDM typespec.
// byte → ByteTypespec, bit [7:0] → BitTypespec, logic [7:0] → LogicTypespec.
// ---------------------------------------------------------------------------
TEST_F(StringAssignment, NetA_HasByteTypespec) {
  const hldb::Net *const net = getNet(m_design, "a");
  ASSERT_NE(net, nullptr);
  ASSERT_NE(net->getTypespec(), nullptr) << "net 'a' has no typespec";
  EXPECT_NE(net->getTypespec()->getActual<hldb::ByteTypespec>(), nullptr)
      << "§5.9: 'byte a' must produce a ByteTypespec";
}

TEST_F(StringAssignment, NetB_HasBitTypespec) {
  const hldb::Net *const net = getNet(m_design, "b");
  ASSERT_NE(net, nullptr);
  ASSERT_NE(net->getTypespec(), nullptr) << "net 'b' has no typespec";
  EXPECT_NE(net->getTypespec()->getActual<hldb::BitTypespec>(), nullptr)
      << "§5.9: 'bit [7:0] b' must produce a BitTypespec";
}

TEST_F(StringAssignment, NetC_HasLogicTypespec) {
  const hldb::Net *const net = getNet(m_design, "c");
  ASSERT_NE(net, nullptr);
  ASSERT_NE(net->getTypespec(), nullptr) << "net 'c' has no typespec";
  EXPECT_NE(net->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr)
      << "§5.9: 'logic [7:0] c' must produce a LogicTypespec";
}

// ---------------------------------------------------------------------------
// §5.9: 'byte' is an implicit 8-bit type — no explicit packed dimension is
// needed in the source, so ByteTypespec carries no range in UHDM.
// This distinguishes 'byte a' from 'bit signed [7:0] a'.
// ---------------------------------------------------------------------------
TEST_F(StringAssignment, NetA_ByteTypespec_HasNoExplicitRange) {
  const hldb::Net *const net = getNet(m_design, "a");
  ASSERT_NE(net, nullptr);
  ASSERT_NE(net->getTypespec(), nullptr);
  const auto *bt = net->getTypespec()->getActual<hldb::ByteTypespec>();
  ASSERT_NE(bt, nullptr);
  EXPECT_TRUE(!bt->getRanges() || bt->getRanges()->empty())
      << "§5.9: 'byte' is an implicit 8-bit keyword type with no explicit "
         "packed dimension range in UHDM";
}

// ---------------------------------------------------------------------------
// §5.9: 'bit [7:0]' has an explicit packed dimension — BitTypespec must carry
// a [7:0] range.
// ---------------------------------------------------------------------------
TEST_F(StringAssignment, NetB_RangeLeftIs7) {
  const hldb::Net *const net = getNet(m_design, "b");
  ASSERT_NE(net, nullptr);
  ASSERT_NE(net->getTypespec(), nullptr);
  const auto *bt = net->getTypespec()->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  const hldb::RangeCollection *const ranges = bt->getRanges();
  ASSERT_NE(ranges, nullptr);
  ASSERT_FALSE(ranges->empty());
  const auto *left = ranges->front()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "7") << "bit [7:0]: left bound must be 7";
}

TEST_F(StringAssignment, NetB_RangeRightIs0) {
  const hldb::Net *const net = getNet(m_design, "b");
  ASSERT_NE(net, nullptr);
  ASSERT_NE(net->getTypespec(), nullptr);
  const auto *bt = net->getTypespec()->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  const hldb::RangeCollection *const ranges = bt->getRanges();
  ASSERT_NE(ranges, nullptr);
  ASSERT_FALSE(ranges->empty());
  const auto *right = ranges->front()->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0") << "bit [7:0]: right bound must be 0";
}

// ---------------------------------------------------------------------------
// §5.9: 'logic [7:0]' has an explicit packed dimension — LogicTypespec must
// carry a [7:0] range.
// ---------------------------------------------------------------------------
TEST_F(StringAssignment, NetC_RangeLeftIs7) {
  const hldb::Net *const net = getNet(m_design, "c");
  ASSERT_NE(net, nullptr);
  ASSERT_NE(net->getTypespec(), nullptr);
  const auto *lt = net->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr);
  const hldb::RangeCollection *const ranges = lt->getRanges();
  ASSERT_NE(ranges, nullptr);
  ASSERT_FALSE(ranges->empty());
  const auto *left = ranges->front()->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "7") << "logic [7:0]: left bound must be 7";
}

TEST_F(StringAssignment, NetC_RangeRightIs0) {
  const hldb::Net *const net = getNet(m_design, "c");
  ASSERT_NE(net, nullptr);
  ASSERT_NE(net->getTypespec(), nullptr);
  const auto *lt = net->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr);
  const hldb::RangeCollection *const ranges = lt->getRanges();
  ASSERT_NE(ranges, nullptr);
  ASSERT_FALSE(ranges->empty());
  const auto *right = ranges->front()->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0") << "logic [7:0]: right bound must be 0";
}

// ---------------------------------------------------------------------------
// Initial block
// ---------------------------------------------------------------------------
TEST_F(StringAssignment, InitialBlockHasBegin) { ASSERT_NE(getBegin(m_design), nullptr); }

TEST_F(StringAssignment, BeginHasThreeStatements) {
  const hldb::Begin *const begin = getBegin(m_design);
  ASSERT_NE(begin, nullptr);
  ASSERT_NE(begin->getStmts(), nullptr);
  EXPECT_EQ(begin->getStmts()->size(), 3u) << "expected 3 blocking assignments: a=\"a\", b=\"b\", c=\"c\"";
}

TEST_F(StringAssignment, AllAssignmentsAreBlocking) {
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
// §5.9: each RHS string literal must be vpiStringConst (6).
// The size must be 8 bits — one character × 8 bits per character.
// ---------------------------------------------------------------------------
TEST_F(StringAssignment, AllRhsAreStringConstType) {
  for (std::size_t i = 0; i < 3; ++i) {
    const auto *assign = getAssignment(m_design, i);
    ASSERT_NE(assign, nullptr) << "stmt[" << i << "] is null";
    const auto *c = assign->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr) << "stmt[" << i << "] RHS is not a Constant";
    EXPECT_EQ(c->getConstType(), 6) << "stmt[" << i << "]: §5.9 string literal must be vpiStringConst (6)";
  }
}

TEST_F(StringAssignment, AllRhsHaveSize8) {
  // §5.9: "The number of bits required to hold a string is 8 times the number
  // of characters in the string." — 1 character = 8 bits.
  for (std::size_t i = 0; i < 3; ++i) {
    const auto *assign = getAssignment(m_design, i);
    ASSERT_NE(assign, nullptr) << "stmt[" << i << "] is null";
    const auto *c = assign->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr) << "stmt[" << i << "] RHS is not a Constant";
    EXPECT_EQ(c->getSize(), 8) << "stmt[" << i << "]: §5.9: 1-character string = 8 bits";
  }
}

TEST_F(StringAssignment, AllRhsHaveStringTypespec) {
  for (std::size_t i = 0; i < 3; ++i) {
    const auto *assign = getAssignment(m_design, i);
    ASSERT_NE(assign, nullptr) << "stmt[" << i << "] is null";
    const auto *c = assign->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr) << "stmt[" << i << "] RHS is not a Constant";
    ASSERT_NE(c->getTypespec(), nullptr) << "stmt[" << i << "] RHS has no typespec";
    EXPECT_NE(c->getTypespec()->getActual<hldb::StringTypespec>(), nullptr)
        << "stmt[" << i << "]: §5.9 string literal must have StringTypespec";
  }
}

// ---------------------------------------------------------------------------
// §5.9: per-assignment value checks.
// Each string literal carries the source character as its value.
// ---------------------------------------------------------------------------
TEST_F(StringAssignment, AssignmentA_ValueIsCharA) {
  const auto *c = getAssignment(m_design, 0)->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue(), "a") << "§5.9: a = \"a\" — string constant value must be \"a\"";
}

TEST_F(StringAssignment, AssignmentB_ValueIsCharB) {
  const auto *c = getAssignment(m_design, 1)->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue(), "b") << "§5.9: b = \"b\" — string constant value must be \"b\"";
}

TEST_F(StringAssignment, AssignmentC_ValueIsCharC) {
  const auto *c = getAssignment(m_design, 2)->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue(), "c") << "§5.9: c = \"c\" — string constant value must be \"c\"";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
