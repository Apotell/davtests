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
// byte arrays per IEEE 1800-2017 Sec 5.9.
//
// Key Sec 5.9 rules under test:
//   1. A string assigned to a packed integral type is treated as a sequence of
//      8-bit ASCII characters packed left-to-right (MSB = leftmost char).
//      If the string has fewer bits than the target, it is zero-extended on
//      the left. If more, leftmost characters are truncated.
//   2. When assigned to an unpacked array of bytes, each element holds one
//      character. If the string is shorter than the array, leftmost elements
//      are filled with 0. If longer, leftmost characters are truncated.
//   3. The bit-width of a string constant = 8 x number of characters.
//
// SV source:
//   bit [8 * 3 - 1 : 0] a = "hi0";   // packed 24-bit variable
//   byte      b[3 : 0]   = "hi2";    // unpacked array of 4 bytes
//
// -- Variable 'a' ----
//   Declaration:  bit [8*3-1:0]  -- expression-based packed dimension.
//   UHDM type:    BitTypespec with range left = subtract(multiply(8,3), 1).
//                 The expression is stored unevaluated as an Operation tree;
//                 Surelog does not fold it to the literal constant 23.
//   Value:        Constant { constType: string (6), size: 24, value: "hi0" }
//
//   Sec 5.9 packing: "hi0" = 3 chars x 8 = 24 bits.
//                 Target = 8x3 = 24 bits -> exact fit, no truncation or padding.
//                 Bit layout: bits[23:16]='h'(0x68), [15:8]='i'(0x69), [7:0]='0'(0x30).
//
// -- Variable 'b' ----
//   Declaration:  byte b[3:0]  -- unpacked array of 4 byte elements.
//   UHDM type:    ArrayTypespec { static, unpacked, range [3:0],
//                                 elemTypespec -> ByteTypespec }
//   Value:        Constant { constType: string (6), size: 24, value: "hi2" }
//
//   Sec 5.9 justification: "hi2" = 3 chars x 8 = 24 bits; array = 4 x 8 = 32 bits.
//                 The string is shorter than the array -> left-zero-padding applies:
//                   b[3] = 0x00  (leftmost element, zero-padded)
//                   b[2] = 'h' (0x68)
//                   b[1] = 'i' (0x69)
//                   b[0] = '2' (0x32)
//                 UHDM stores the source string (24 bits), not the padded storage.
//                 size=24 < 32 confirms the padding case -- it is a simulator semantic.
//
// All tests PASS. No Surelog bugs for this SV file.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/bit_typespec.h>
#include <hldb/byte_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/variable.h>

#include <string>

namespace hlc {

class StringWordAssignment : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.9-string-word-assignment.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("top", d->getAllModules());
}

static const hldb::Variable *getVariable(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getVariables()) return nullptr;
  return hldb::findByName<hldb::Variable>(name, m->getVariables());
}

// Helper: get the BitTypespec range's left Operation for var 'a'.
static const hldb::Operation *getVariableARangeLeft(const hldb::Design *d) {
  const hldb::Variable *var = getVariable(d, "a");
  if (!var || !var->getTypespec()) return nullptr;
  const auto *bt = var->getTypespec()->getActual<hldb::BitTypespec>();
  if (!bt || !bt->getRanges() || bt->getRanges()->empty()) return nullptr;
  return bt->getRanges()->front()->getLeftExpr<hldb::Operation>();
}

// ----
// Module structure
// ----
TEST_F(StringWordAssignment, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'top' not found"; }

TEST_F(StringWordAssignment, TwoVariablesExist) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getVariables(), nullptr);
  EXPECT_EQ(m->getVariables()->size(), 2u) << "expected 2 variables: a (bit [8*3-1:0]) and b (byte [3:0])";
}

// Neither `bit` nor `byte` is a net-type keyword, so per IEEE 1800-2023 Sec
// 6.7/6.8 neither a nor b must appear in the module's net collection.
TEST_F(StringWordAssignment, ModuleHasNoNets) {
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getNets() || m->getNets()->empty()) << "bit/byte declarations must not appear as Nets";
}

TEST_F(StringWordAssignment, NoInitialBlock) {
  // Initializers are inline in the declarations -- no separate initial block.
  const hldb::Module *const m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_TRUE(!m->getProcesses() || m->getProcesses()->empty())
      << "module has no initial/always blocks; initializers are inline";
}

// ===========================================================================
// Variable 'a' -- bit [8 * 3 - 1 : 0] a = "hi0"
// ===========================================================================

// ----
// Type: BitTypespec with an expression-based packed dimension.
// ----
TEST_F(StringWordAssignment, VariableA_HasBitTypespec) {
  const hldb::Variable *const var = getVariable(m_design, "a");
  ASSERT_NE(var, nullptr);
  ASSERT_NE(var->getTypespec(), nullptr) << "var 'a' has no typespec";
  EXPECT_NE(var->getTypespec()->getActual<hldb::BitTypespec>(), nullptr)
      << "Sec 5.9: 'bit [8*3-1:0] a' must produce a BitTypespec";
}

// ----
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
//   b) the operands are correct (8x3=24, 24-1=23) -- verifying the calculation
// ----
TEST_F(StringWordAssignment, VariableA_RangeLeft_IsSubtractOperation) {
  const hldb::Operation *const sub_op = getVariableARangeLeft(m_design);
  ASSERT_NE(sub_op, nullptr) << "Sec 5.9: range left of 'bit [8*3-1:0]' must be a subtract Operation";
  // vpiSubOp = 11
  EXPECT_EQ(sub_op->getOpType(), 11) << "Sec 5.9: outer operation must be subtract (opType=11) for '(8*3) - 1'";
}

TEST_F(StringWordAssignment, VariableA_RangeLeft_SubtractFirstOperand_IsMultiply) {
  const hldb::Operation *const sub_op = getVariableARangeLeft(m_design);
  ASSERT_NE(sub_op, nullptr);
  ASSERT_NE(sub_op->getOperands(), nullptr);
  ASSERT_GE(sub_op->getOperands()->size(), 1u);
  const auto *mult_op = any_cast<const hldb::Operation *>((*sub_op->getOperands())[0]);
  ASSERT_NE(mult_op, nullptr) << "first operand of subtract must be a multiply Operation";
  // vpiMultOp = 25
  EXPECT_EQ(mult_op->getOpType(), 25) << "Sec 5.9: first operand of subtract must be multiply (opType=25) for '8*3'";
}

TEST_F(StringWordAssignment, VariableA_RangeLeft_MultiplyOperands_Are8And3) {
  // Verifies that the multiply sub-expression is 8x3, confirming the packed
  // dimension width = 8*3 = 24.
  const hldb::Operation *const sub_op = getVariableARangeLeft(m_design);
  ASSERT_NE(sub_op, nullptr);
  ASSERT_NE(sub_op->getOperands(), nullptr);
  const auto *mult_op = any_cast<const hldb::Operation *>((*sub_op->getOperands())[0]);
  ASSERT_NE(mult_op, nullptr);
  ASSERT_NE(mult_op->getOperands(), nullptr);
  ASSERT_EQ(mult_op->getOperands()->size(), 2u);
  const auto *c8 = any_cast<const hldb::Constant *>((*mult_op->getOperands())[0]);
  const auto *c3 = any_cast<const hldb::Constant *>((*mult_op->getOperands())[1]);
  ASSERT_NE(c8, nullptr) << "multiply left operand must be Constant 8";
  ASSERT_NE(c3, nullptr) << "multiply right operand must be Constant 3";
  EXPECT_EQ(std::string(c8->getValue()), "8") << "Sec 5.9: first multiply operand must be 8 (chars per word)";
  EXPECT_EQ(std::string(c3->getValue()), "3") << "Sec 5.9: second multiply operand must be 3 (number of chars in \"hi0\")";
}

TEST_F(StringWordAssignment, VariableA_RangeLeft_SubtractSecondOperand_Is1) {
  // 8*3-1 = 23: the subtract's second operand is 1 (converting width to MSB index).
  const hldb::Operation *const sub_op = getVariableARangeLeft(m_design);
  ASSERT_NE(sub_op, nullptr);
  ASSERT_NE(sub_op->getOperands(), nullptr);
  ASSERT_EQ(sub_op->getOperands()->size(), 2u);
  const auto *c1 = any_cast<const hldb::Constant *>((*sub_op->getOperands())[1]);
  ASSERT_NE(c1, nullptr) << "subtract second operand must be Constant 1";
  EXPECT_EQ(std::string(c1->getValue()), "1") << "Sec 5.9: '8*3-1' -- second operand of subtract must be 1";
}

TEST_F(StringWordAssignment, VariableA_RangeRight_Is0) {
  const hldb::Variable *const var = getVariable(m_design, "a");
  ASSERT_NE(var, nullptr);
  const auto *bt = var->getTypespec()->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  const hldb::RangeCollection *const ranges = bt->getRanges();
  ASSERT_NE(ranges, nullptr);
  ASSERT_FALSE(ranges->empty());
  const auto *right = ranges->front()->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr) << "right bound of [8*3-1:0] must be a Constant";
  EXPECT_EQ(right->getDecompile(), "0") << "Sec 5.9: right bound of [8*3-1:0] must be 0";
}

// ----
// Sec 5.9 packed string storage: "hi0" in bit [8*3-1:0] = bit [23:0].
//
// "hi0" = 3 characters x 8 bits = 24 bits.
// Target = 8*3 = 24 bits -> exact fit. No truncation or zero-padding occurs.
// Packing (MSB-first, per Sec 5.9):
//   bits [23:16] = 'h' (0x68)
//   bits [15: 8] = 'i' (0x69)
//   bits [ 7: 0] = '0' (0x30)
//
// UHDM represents the entire string as one Constant node (not bit-by-bit).
// The size=24 confirms the exact fit. The value "hi0" confirms all 3 chars
// are preserved in order. The decompile confirms the full source representation.
// ----
TEST_F(StringWordAssignment, VariableA_Value_IsStringConst) {
  const auto *c = getVariable(m_design, "a")->getValue<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "var 'a' has no inline value Constant";
  EXPECT_EQ(c->getConstType(), 6) << "Sec 5.9: string literal must be vpiStringConst (6)";
}

TEST_F(StringWordAssignment, VariableA_Value_SizeIs24_ExactFit) {
  // Sec 5.9: "hi0" = 3 chars x 8 bits = 24 bits.
  // Target 'bit [8*3-1:0]' = 24 bits. Size must equal 24 -- no bits lost.
  const auto *c = getVariable(m_design, "a")->getValue<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 24) << "Sec 5.9: \"hi0\" = 3 chars x 8 = 24 bits; 'bit [8*3-1:0]' = 24 bits "
                                 "-- exact fit, no truncation and no zero-padding";
}

TEST_F(StringWordAssignment, VariableA_Value_AllCharsPreserved) {
  // Sec 5.9: no characters were truncated -- value holds all 3 source chars.
  const auto *c = getVariable(m_design, "a")->getValue<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue(), "hi0") << "Sec 5.9: all 3 characters of \"hi0\" must be preserved in the packed "
                                     "assignment (exact fit -- none truncated from the left)";
}

TEST_F(StringWordAssignment, VariableA_Value_HasStringTypespec) {
  const auto *c = getVariable(m_design, "a")->getValue<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getTypespec(), nullptr);
  EXPECT_NE(c->getTypespec()->getActual<hldb::StringTypespec>(), nullptr)
      << "Sec 5.9: string literal initializer must have a StringTypespec";
}

// ===========================================================================
// Variable 'b' -- byte b[3:0] = "hi2"
// ===========================================================================

// ----
// Type: ArrayTypespec (unpacked static array of byte elements).
// ----
TEST_F(StringWordAssignment, VariableB_HasArrayTypespec) {
  const hldb::Variable *const var = getVariable(m_design, "b");
  ASSERT_NE(var, nullptr);
  ASSERT_NE(var->getTypespec(), nullptr) << "var 'b' has no typespec";
  EXPECT_NE(var->getTypespec()->getActual<hldb::ArrayTypespec>(), nullptr)
      << "Sec 5.9: 'byte b[3:0]' (unpacked array) must produce an ArrayTypespec";
}

TEST_F(StringWordAssignment, VariableB_ArrayTypespec_IsUnpacked) {
  const auto *at = getVariable(m_design, "b")->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_FALSE(at->getPacked()) << "Sec 5.9: 'byte b[3:0]' uses an unpacked dimension -- getPacked() "
                                   "must be false";
}

TEST_F(StringWordAssignment, VariableB_ArrayTypespec_ElemIsByteTypespec) {
  // Sec 5.9: each element is a 'byte' (implicit 8-bit signed 2-state type).
  const auto *at = getVariable(m_design, "b")->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr) << "ArrayTypespec has no element typespec";
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::ByteTypespec>(), nullptr)
      << "Sec 5.9: element type of 'byte b[3:0]' must be ByteTypespec";
}

TEST_F(StringWordAssignment, VariableB_ArrayTypespec_RangeLeft_Is3) {
  const auto *at = getVariable(m_design, "b")->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  const hldb::Range *const range = at->getRange();
  ASSERT_NE(range, nullptr) << "ArrayTypespec for 'byte b[3:0]' has no range";
  const auto *left = range->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "3") << "'byte b[3:0]': left (high) bound must be 3";
}

TEST_F(StringWordAssignment, VariableB_ArrayTypespec_RangeRight_Is0) {
  const auto *at = getVariable(m_design, "b")->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  const hldb::Range *const range = at->getRange();
  ASSERT_NE(range, nullptr);
  const auto *right = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0") << "'byte b[3:0]': right (low) bound must be 0";
}

// ----
// Sec 5.9 unpacked byte array string justification.
//
// "hi2" = 3 characters x 8 bits = 24 bits.
// Array 'byte b[3:0]' has 4 elements x 8 bits = 32 bits capacity.
// String is shorter -> Sec 5.9 requires left-zero-padding:
//   b[3] = 0x00  (leftmost element, zero-padded -- no 4th source char)
//   b[2] = 'h' (0x68)
//   b[1] = 'i' (0x69)
//   b[0] = '2' (0x32)
//
// UHDM stores the source string constant (24 bits), not the padded storage.
// The per-element zero-padding is a simulator elaboration semantic.
// size=24 < 32 is the indicator that padding applies for b[3].
// ----
TEST_F(StringWordAssignment, VariableB_Value_IsStringConst) {
  const auto *c = getVariable(m_design, "b")->getValue<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "var 'b' has no inline value Constant";
  EXPECT_EQ(c->getConstType(), 6) << "Sec 5.9: string literal must be vpiStringConst (6)";
}

TEST_F(StringWordAssignment, VariableB_Value_SizeIs24_ShorterThanArray) {
  // Sec 5.9: "hi2" = 3 chars x 8 = 24 bits (source string).
  // Array capacity = 4 elements x 8 bits = 32 bits.
  // size=24 < 32 confirms the string is shorter than the array, so Sec 5.9
  // left-zero-padding applies: b[3] = 0x00.
  const auto *c = getVariable(m_design, "b")->getValue<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 24) << "Sec 5.9: \"hi2\" = 3 chars x 8 = 24 bits; array 'byte b[3:0]' = 32 bits "
                                 "-- size=24 < 32 means b[3] is zero-padded per Sec 5.9";
}

TEST_F(StringWordAssignment, VariableB_Value_AllCharsPreserved) {
  // Sec 5.9: "hi2" has 3 chars and the array has 4 elements -- no truncation.
  // Only zero-padding at b[3] occurs; b[2..0] hold all source characters.
  const auto *c = getVariable(m_design, "b")->getValue<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getValue(), "hi2") << "Sec 5.9: all 3 source characters of \"hi2\" must be preserved "
                                     "(string shorter than array -> padding not truncation)";
}

TEST_F(StringWordAssignment, VariableB_Value_HasStringTypespec) {
  const auto *c = getVariable(m_design, "b")->getValue<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getTypespec(), nullptr);
  EXPECT_NE(c->getTypespec()->getActual<hldb::StringTypespec>(), nullptr)
      << "Sec 5.9: string literal initializer must have a StringTypespec";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
