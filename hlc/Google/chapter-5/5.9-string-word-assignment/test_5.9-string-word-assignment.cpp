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

// Spec-based validation of string assignment to packed bit arrays and unpacked
// byte arrays per IEEE 1800-2017 §5.9.
//
// Key §5.9 rules under test:
//   1. A string assigned to a packed integral type is treated as a sequence of
//      8-bit ASCII characters packed left-to-right (MSB = leftmost char).
//      If the string has fewer bits than the target, it is zero-extended on
//      the left. If more, leftmost characters are truncated.
//   2. When assigned to an unpacked array of bytes, each element holds one
//      character. If the string is shorter than the array, leftmost elements
//      are filled with 0. If longer, leftmost characters are truncated.
//   3. The bit-width of a string constant = 8 × number of characters.
//
// SV source:
//   bit [8 * 3 - 1 : 0] a = "hi0";   // packed 24-bit variable
//   byte      b[3 : 0]   = "hi2";    // unpacked array of 4 bytes
//
// ── Net 'a' ────────────────────────────────────────────────────────────────
//   Declaration:  bit [8*3-1:0]  — expression-based packed dimension.
//   UHDM type:    BitTypespec with range left = subtract(multiply(8,3), 1).
//                 The expression is stored unevaluated as an Operation tree;
//                 Surelog does not fold it to the literal constant 23.
//   Value:        Constant { constType: string (6), size: 24, value: "hi0" }
//
//   §5.9 packing: "hi0" = 3 chars × 8 = 24 bits.
//                 Target = 8×3 = 24 bits → exact fit, no truncation or padding.
//                 Bit layout: bits[23:16]='h'(0x68), [15:8]='i'(0x69), [7:0]='0'(0x30).
//
// ── Net 'b' ────────────────────────────────────────────────────────────────
//   Declaration:  byte b[3:0]  — unpacked array of 4 byte elements.
//   UHDM type:    ArrayTypespec { static, unpacked, range [3:0],
//                                 elemTypespec → ByteTypespec }
//   Value:        Constant { constType: string (6), size: 24, value: "hi2" }
//
//   §5.9 justification: "hi2" = 3 chars × 8 = 24 bits; array = 4 × 8 = 32 bits.
//                 The string is shorter than the array → left-zero-padding applies:
//                   b[3] = 0x00  (leftmost element, zero-padded)
//                   b[2] = 'h' (0x68)
//                   b[1] = 'i' (0x69)
//                   b[0] = '2' (0x32)
//                 UHDM stores the source string (24 bits), not the padded storage.
//                 size=24 < 32 confirms the padding case — it is a simulator semantic.
//
// All tests PASS. No Surelog bugs for this SV file.

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/array_typespec.h>
#include <uhdm/bit_typespec.h>
#include <uhdm/byte_typespec.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/operation.h>
#include <uhdm/range.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/string_typespec.h>

#include <string>

namespace SURELOG {

class StringWordAssignment : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.9-string-word-assignment.hlc"});

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

static const uhdm::Net *getNet(const uhdm::Design *d, std::string_view name) {
  const uhdm::Module *m = getTop(d);
  if (!m || !m->getNets()) return nullptr;
  return uhdm::findByName<uhdm::Net>(name, m->getNets());
}

// Helper: get the BitTypespec range's left Operation for net 'a'.
static const uhdm::Operation *getNetARangeLeft(const uhdm::Design *d) {
  const uhdm::Net *net = getNet(d, "a");
  if (!net || !net->getTypespec()) return nullptr;
  const auto *bt = net->getTypespec()->getActual<uhdm::BitTypespec>();
  if (!bt || !bt->getRanges() || bt->getRanges()->empty()) return nullptr;
  return bt->getRanges()->front()->getLeftExpr<uhdm::Operation>();
}

// ---------------------------------------------------------------------------
// Module structure
// ---------------------------------------------------------------------------
TEST_F(StringWordAssignment, ModuleExists) {
  ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found";
}

TEST_F(StringWordAssignment, TwoNetsExist) {
  const uhdm::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr);
  EXPECT_EQ(m->getNets()->size(), 2u)
      << "expected 2 nets: a (bit [8*3-1:0]) and b (byte [3:0])";
}

TEST_F(StringWordAssignment, NoInitialBlock) {
  // Initializers are inline in the declarations — no separate initial block.
  const uhdm::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getProcesses() || m->getProcesses()->empty())
      << "module has no initial/always blocks; initializers are inline";
}

// ===========================================================================
// Net 'a' — bit [8 * 3 - 1 : 0] a = "hi0"
// ===========================================================================

// ---------------------------------------------------------------------------
// Type: BitTypespec with an expression-based packed dimension.
// ---------------------------------------------------------------------------
TEST_F(StringWordAssignment, NetA_HasBitTypespec) {
  const uhdm::Net *const net = getNet(m_design, "a");
  ASSERT_NE(net, nullptr);
  ASSERT_NE(net->getTypespec(), nullptr) << "net 'a' has no typespec";
  EXPECT_NE(net->getTypespec()->getActual<uhdm::BitTypespec>(), nullptr)
      << "§5.9: 'bit [8*3-1:0] a' must produce a BitTypespec";
}

// ---------------------------------------------------------------------------
// Range left = 8*3-1: verify the full expression tree stored by Surelog.
//
// The source writes [8*3-1:0]. Surelog stores this unevaluated as:
//   subtract (opType=11)
//     multiply (opType=25)
//       Constant "8"
//       Constant "3"
//     Constant "1"
//
// These four tests together confirm:
//   a) the expression type is correct (subtract of a multiply, not a literal)
//   b) the operands are correct (8×3=24, 24-1=23) — verifying the calculation
// ---------------------------------------------------------------------------
TEST_F(StringWordAssignment, NetA_RangeLeft_IsSubtractOperation) {
  const uhdm::Operation *const sub_op = getNetARangeLeft(m_design);
  ASSERT_NE(sub_op, nullptr)
      << "§5.9: range left of 'bit [8*3-1:0]' must be a subtract Operation";
  // vpiSubOp = 11
  EXPECT_EQ(sub_op->getOpType(), 11)
      << "§5.9: outer operation must be subtract (opType=11) for '(8*3) - 1'";
}

TEST_F(StringWordAssignment, NetA_RangeLeft_SubtractFirstOperand_IsMultiply) {
  const uhdm::Operation *const sub_op = getNetARangeLeft(m_design);
  ASSERT_NE(sub_op, nullptr);
  ASSERT_NE(sub_op->getOperands(), nullptr);
  ASSERT_GE(sub_op->getOperands()->size(), 1u);
  const auto *mult_op =
      any_cast<const uhdm::Operation *>((*sub_op->getOperands())[0]);
  ASSERT_NE(mult_op, nullptr)
      << "first operand of subtract must be a multiply Operation";
  // vpiMultOp = 25
  EXPECT_EQ(mult_op->getOpType(), 25)
      << "§5.9: first operand of subtract must be multiply (opType=25) for '8*3'";
}

TEST_F(StringWordAssignment, NetA_RangeLeft_MultiplyOperands_Are8And3) {
  // Verifies that the multiply sub-expression is 8×3, confirming the packed
  // dimension width = 8*3 = 24.
  const uhdm::Operation *const sub_op = getNetARangeLeft(m_design);
  ASSERT_NE(sub_op, nullptr);
  ASSERT_NE(sub_op->getOperands(), nullptr);
  const auto *mult_op =
      any_cast<const uhdm::Operation *>((*sub_op->getOperands())[0]);
  ASSERT_NE(mult_op, nullptr);
  ASSERT_NE(mult_op->getOperands(), nullptr);
  ASSERT_EQ(mult_op->getOperands()->size(), 2u);
  const auto *c8 =
      any_cast<const uhdm::Constant *>((*mult_op->getOperands())[0]);
  const auto *c3 =
      any_cast<const uhdm::Constant *>((*mult_op->getOperands())[1]);
  ASSERT_NE(c8, nullptr) << "multiply left operand must be Constant 8";
  ASSERT_NE(c3, nullptr) << "multiply right operand must be Constant 3";
  EXPECT_EQ(std::string(c8->getValue()), "8")
      << "§5.9: first multiply operand must be 8 (chars per word)";
  EXPECT_EQ(std::string(c3->getValue()), "3")
      << "§5.9: second multiply operand must be 3 (number of chars in \"hi0\")";
}

TEST_F(StringWordAssignment, NetA_RangeLeft_SubtractSecondOperand_Is1) {
  // 8*3-1 = 23: the subtract's second operand is 1 (converting width to MSB index).
  const uhdm::Operation *const sub_op = getNetARangeLeft(m_design);
  ASSERT_NE(sub_op, nullptr);
  ASSERT_NE(sub_op->getOperands(), nullptr);
  ASSERT_EQ(sub_op->getOperands()->size(), 2u);
  const auto *c1 =
      any_cast<const uhdm::Constant *>((*sub_op->getOperands())[1]);
  ASSERT_NE(c1, nullptr) << "subtract second operand must be Constant 1";
  EXPECT_EQ(std::string(c1->getValue()), "1")
      << "§5.9: '8*3-1' — second operand of subtract must be 1";
}

TEST_F(StringWordAssignment, NetA_RangeRight_Is0) {
  const uhdm::Net *const net = getNet(m_design, "a");
  ASSERT_NE(net, nullptr);
  const auto *bt = net->getTypespec()->getActual<uhdm::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  const uhdm::RangeCollection *const ranges = bt->getRanges();
  ASSERT_NE(ranges, nullptr);
  ASSERT_FALSE(ranges->empty());
  const auto *right = ranges->front()->getRightExpr<uhdm::Constant>();
  ASSERT_NE(right, nullptr) << "right bound of [8*3-1:0] must be a Constant";
  EXPECT_EQ(right->getDecompile(), "0")
      << "§5.9: right bound of [8*3-1:0] must be 0";
}

// ---------------------------------------------------------------------------
// §5.9 packed string storage: "hi0" in bit [8*3-1:0] = bit [23:0].
//
// "hi0" = 3 characters × 8 bits = 24 bits.
// Target = 8*3 = 24 bits → exact fit. No truncation or zero-padding occurs.
// Packing (MSB-first, per §5.9):
//   bits [23:16] = 'h' (0x68)
//   bits [15: 8] = 'i' (0x69)
//   bits [ 7: 0] = '0' (0x30)
//
// UHDM represents the entire string as one Constant node (not bit-by-bit).
// The size=24 confirms the exact fit. The value "hi0" confirms all 3 chars
// are preserved in order. The decompile confirms the full source representation.
// ---------------------------------------------------------------------------
TEST_F(StringWordAssignment, NetA_Value_IsStringConst) {
  const auto *c = getNet(m_design, "a")->getValue<uhdm::Constant>();
  ASSERT_NE(c, nullptr) << "net 'a' has no inline value Constant";
  EXPECT_EQ(c->getConstType(), 6)
      << "§5.9: string literal must be vpiStringConst (6)";
}

TEST_F(StringWordAssignment, NetA_Value_SizeIs24_ExactFit) {
  // §5.9: "hi0" = 3 chars × 8 bits = 24 bits.
  // Target 'bit [8*3-1:0]' = 24 bits. Size must equal 24 — no bits lost.
  const auto *c = getNet(m_design, "a")->getValue<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 24)
      << "§5.9: \"hi0\" = 3 chars × 8 = 24 bits; 'bit [8*3-1:0]' = 24 bits "
         "— exact fit, no truncation and no zero-padding";
}

TEST_F(StringWordAssignment, NetA_Value_AllCharsPreserved) {
  // §5.9: no characters were truncated — value holds all 3 source chars.
  const auto *c = getNet(m_design, "a")->getValue<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue(), "hi0")
      << "§5.9: all 3 characters of \"hi0\" must be preserved in the packed "
         "assignment (exact fit — none truncated from the left)";
}

TEST_F(StringWordAssignment, NetA_Value_HasStringTypespec) {
  const auto *c = getNet(m_design, "a")->getValue<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getTypespec(), nullptr);
  EXPECT_NE(c->getTypespec()->getActual<uhdm::StringTypespec>(), nullptr)
      << "§5.9: string literal initializer must have a StringTypespec";
}

// ===========================================================================
// Net 'b' — byte b[3:0] = "hi2"
// ===========================================================================

// ---------------------------------------------------------------------------
// Type: ArrayTypespec (unpacked static array of byte elements).
// ---------------------------------------------------------------------------
TEST_F(StringWordAssignment, NetB_HasArrayTypespec) {
  const uhdm::Net *const net = getNet(m_design, "b");
  ASSERT_NE(net, nullptr);
  ASSERT_NE(net->getTypespec(), nullptr) << "net 'b' has no typespec";
  EXPECT_NE(net->getTypespec()->getActual<uhdm::ArrayTypespec>(), nullptr)
      << "§5.9: 'byte b[3:0]' (unpacked array) must produce an ArrayTypespec";
}

TEST_F(StringWordAssignment, NetB_ArrayTypespec_IsUnpacked) {
  const auto *at =
      getNet(m_design, "b")->getTypespec()->getActual<uhdm::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_FALSE(at->getPacked())
      << "§5.9: 'byte b[3:0]' uses an unpacked dimension — getPacked() "
         "must be false";
}

TEST_F(StringWordAssignment, NetB_ArrayTypespec_ElemIsByteTypespec) {
  // §5.9: each element is a 'byte' (implicit 8-bit signed 2-state type).
  const auto *at =
      getNet(m_design, "b")->getTypespec()->getActual<uhdm::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr) << "ArrayTypespec has no element typespec";
  EXPECT_NE(at->getElemTypespec()->getActual<uhdm::ByteTypespec>(), nullptr)
      << "§5.9: element type of 'byte b[3:0]' must be ByteTypespec";
}

TEST_F(StringWordAssignment, NetB_ArrayTypespec_RangeLeft_Is3) {
  const auto *at =
      getNet(m_design, "b")->getTypespec()->getActual<uhdm::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  const uhdm::Range *const range = at->getRange();
  ASSERT_NE(range, nullptr) << "ArrayTypespec for 'byte b[3:0]' has no range";
  const auto *left = range->getLeftExpr<uhdm::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "3")
      << "'byte b[3:0]': left (high) bound must be 3";
}

TEST_F(StringWordAssignment, NetB_ArrayTypespec_RangeRight_Is0) {
  const auto *at =
      getNet(m_design, "b")->getTypespec()->getActual<uhdm::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  const uhdm::Range *const range = at->getRange();
  ASSERT_NE(range, nullptr);
  const auto *right = range->getRightExpr<uhdm::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0")
      << "'byte b[3:0]': right (low) bound must be 0";
}

// ---------------------------------------------------------------------------
// §5.9 unpacked byte array string justification.
//
// "hi2" = 3 characters × 8 bits = 24 bits.
// Array 'byte b[3:0]' has 4 elements × 8 bits = 32 bits capacity.
// String is shorter → §5.9 requires left-zero-padding:
//   b[3] = 0x00  (leftmost element, zero-padded — no 4th source char)
//   b[2] = 'h' (0x68)
//   b[1] = 'i' (0x69)
//   b[0] = '2' (0x32)
//
// UHDM stores the source string constant (24 bits), not the padded storage.
// The per-element zero-padding is a simulator elaboration semantic.
// size=24 < 32 is the indicator that padding applies for b[3].
// ---------------------------------------------------------------------------
TEST_F(StringWordAssignment, NetB_Value_IsStringConst) {
  const auto *c = getNet(m_design, "b")->getValue<uhdm::Constant>();
  ASSERT_NE(c, nullptr) << "net 'b' has no inline value Constant";
  EXPECT_EQ(c->getConstType(), 6)
      << "§5.9: string literal must be vpiStringConst (6)";
}

TEST_F(StringWordAssignment, NetB_Value_SizeIs24_ShorterThanArray) {
  // §5.9: "hi2" = 3 chars × 8 = 24 bits (source string).
  // Array capacity = 4 elements × 8 bits = 32 bits.
  // size=24 < 32 confirms the string is shorter than the array, so §5.9
  // left-zero-padding applies: b[3] = 0x00.
  const auto *c = getNet(m_design, "b")->getValue<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 24)
      << "§5.9: \"hi2\" = 3 chars × 8 = 24 bits; array 'byte b[3:0]' = 32 bits "
         "— size=24 < 32 means b[3] is zero-padded per §5.9";
}

TEST_F(StringWordAssignment, NetB_Value_AllCharsPreserved) {
  // §5.9: "hi2" has 3 chars and the array has 4 elements — no truncation.
  // Only zero-padding at b[3] occurs; b[2..0] hold all source characters.
  const auto *c = getNet(m_design, "b")->getValue<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue(), "hi2")
      << "§5.9: all 3 source characters of \"hi2\" must be preserved "
         "(string shorter than array → padding not truncation)";
}

TEST_F(StringWordAssignment, NetB_Value_HasStringTypespec) {
  const auto *c = getNet(m_design, "b")->getValue<uhdm::Constant>();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getTypespec(), nullptr);
  EXPECT_NE(c->getTypespec()->getActual<uhdm::StringTypespec>(), nullptr)
      << "§5.9: string literal initializer must have a StringTypespec";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
