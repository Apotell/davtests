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

// Tests for dut.sv (tags: LargeConst)
//   module dut (/*output logic[80:0] a, output logic[80:0] b*/);
//      assign a =    147573952589676412928;
//      assign b = 'sd147573952589676412928;
//      assign c =     18446744073709551615; // 2^64
//      assign d =     18446744073709551616; // 2^64 + 1
//      assign e =    -18446744073709551616; // 2^64 + 1
//   endmodule
//
// Note: the source comments are off by one -- 18446744073709551615 is
// 2^64 - 1, 18446744073709551616 is 2^64, and 147573952589676412928 is
// 2^67. The tests use the true values.
//
// What is checked (IEEE 1800-2023):
//   - module 'dut' exists with an empty port list and 5 continuous
//     assignments (10.3.2)
//   - 6.10: a, b, c, d, e are never declared and each appears on the LHS of
//     a continuous assignment, so each is an implicit scalar net of the
//     default net type (wire) belonging to 'dut'; the LHS references must
//     bind to those nets
//   - 5.7.1: every RHS literal is an unsized decimal integer literal
//     (vpiDecConst); the decompiled text keeps every digit (no truncation
//     of the >64-bit value)
//   - 5.7.1: "Simple decimal numbers without the size and the base format
//     shall be treated as signed integers"; 'sd is explicitly signed
//   - 5.7.1: "An unsized number that requires more than 32 bits shall have
//     at least the minimum width needed to properly represent the value,
//     including a sign bit if the number is signed":
//       2^67      (signed)  -> >= 69 bits
//       2^64 - 1  (signed)  -> >= 65 bits
//       2^64      (signed)  -> >= 66 bits
//   - 5.7.1: "A plus or minus operator preceding the size constant is a
//     unary plus or minus operator" -- the RHS of 'e' is a unary minus
//     operation applied to the literal 2^64
//
// What is NOT checked and why:
//   - the exact string format HLDB uses for Constant::getValue(): it is an
//     HLDB encoding, not something IEEE 1800 defines.
//   - the exact width chosen for each unsized literal: 5.7.1 only demands
//     "at least" the minimum width, so a lower bound is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bit_typespec.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/integer_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/long_int_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class LargeConstTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LargeConst.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getDut() { return hldb::findByName<hldb::Module>("dut", m_design->getAllModules()); }

  // Continuous assignment whose LHS is named 'lhsName', or nullptr.
  static const hldb::ContAssign *getAssign(std::string_view lhsName) {
    const hldb::Module *const dut = getDut();
    if ((dut == nullptr) || (dut->getContAssigns() == nullptr)) return nullptr;
    for (const hldb::ContAssign *const ca : *dut->getContAssigns()) {
      if (const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>()) {
        if (lhs->getName() == lhsName) return ca;
      }
    }
    return nullptr;
  }

  // Returns 1 for signed, 0 for unsigned, -1 if the typespec is missing or
  // of an unexpected kind.
  static int32_t signedness(const hldb::Constant *c) {
    if ((c->getTypespec() == nullptr) || (c->getTypespec()->getActual() == nullptr)) return -1;
    const hldb::Typespec *const ts = c->getTypespec()->getActual();
    if (const hldb::LogicTypespec *const t = any_cast<hldb::LogicTypespec>(ts)) return t->getSigned() ? 1 : 0;
    if (const hldb::BitTypespec *const t = any_cast<hldb::BitTypespec>(ts)) return t->getSigned() ? 1 : 0;
    if (const hldb::IntTypespec *const t = any_cast<hldb::IntTypespec>(ts)) return t->getSigned() ? 1 : 0;
    if (const hldb::IntegerTypespec *const t = any_cast<hldb::IntegerTypespec>(ts)) return t->getSigned() ? 1 : 0;
    if (const hldb::LongIntTypespec *const t = any_cast<hldb::LongIntTypespec>(ts)) return t->getSigned() ? 1 : 0;
    return -1;
  }

  static void checkLiteral(const hldb::Constant *c, std::string_view text, int32_t minBits) {
    EXPECT_EQ(c->getConstType(), vpiDecConst) << text;
    EXPECT_EQ(c->getDecompile(), text);
    EXPECT_GE(c->getSize(), minBits) << "5.7.1: unsized literal " << text << " needs at least " << minBits
                                     << " bits including the sign bit";
    EXPECT_EQ(signedness(c), 1) << "5.7.1: " << text << " is a signed literal";
  }
};

TEST_F(LargeConstTest, ModuleDutExistsWithFiveContAssigns) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  EXPECT_EQ(dut->getPorts(), nullptr) << "the port list is empty (contents are commented out)";
  ASSERT_NE(dut->getContAssigns(), nullptr);
  EXPECT_EQ(dut->getContAssigns()->size(), 5u);
  for (std::string_view name : {"a", "b", "c", "d", "e"}) {
    EXPECT_NE(getAssign(name), nullptr) << "assign " << name;
  }
}

// ===========================================================================
// 6.10: implicit scalar nets of default net type
// ===========================================================================

TEST_F(LargeConstTest, LhsIdentifiersAreImplicitWireNets) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  for (std::string_view name : {"a", "b", "c", "d", "e"}) {
    const hldb::Net *const n = hldb::findByName<hldb::Net>(name, dut->getNets());
    ASSERT_NE(n, nullptr) << "6.10: '" << name << "' on the LHS of a continuous assignment is an implicit net";
    EXPECT_TRUE(n->getImplicitDecl()) << name;
    EXPECT_EQ(n->getNetType(), vpiWire) << name;
    EXPECT_TRUE(n->getScalar()) << "6.10: implicit net '" << name << "' is scalar";
  }
}

TEST_F(LargeConstTest, LhsReferencesBindToImplicitNets) {
  for (std::string_view name : {"a", "b", "c", "d", "e"}) {
    const hldb::ContAssign *const ca = getAssign(name);
    ASSERT_NE(ca, nullptr) << name;
    const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr);
    ASSERT_NE(lhs->getActual(), nullptr) << "6.10: LHS '" << name << "' must resolve to its implicit net";
    EXPECT_EQ(lhs->getActual()->getAnyType(), hldb::AnyType::Net) << name;
  }
}

// ===========================================================================
// 5.7.1: large unsized decimal literals
// ===========================================================================

TEST_F(LargeConstTest, ATwoPow67) {
  const hldb::ContAssign *const ca = getAssign("a");
  ASSERT_NE(ca, nullptr);
  const hldb::Constant *const c = ca->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  checkLiteral(c, "147573952589676412928", 69);
}

TEST_F(LargeConstTest, BSignedBasedTwoPow67) {
  const hldb::ContAssign *const ca = getAssign("b");
  ASSERT_NE(ca, nullptr);
  const hldb::Constant *const c = ca->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  checkLiteral(c, "'sd147573952589676412928", 69);
}

TEST_F(LargeConstTest, CTwoPow64Minus1) {
  const hldb::ContAssign *const ca = getAssign("c");
  ASSERT_NE(ca, nullptr);
  const hldb::Constant *const c = ca->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  checkLiteral(c, "18446744073709551615", 65);
}

TEST_F(LargeConstTest, DTwoPow64) {
  const hldb::ContAssign *const ca = getAssign("d");
  ASSERT_NE(ca, nullptr);
  const hldb::Constant *const c = ca->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr);
  checkLiteral(c, "18446744073709551616", 66);
}

TEST_F(LargeConstTest, EUnaryMinusTwoPow64) {
  const hldb::ContAssign *const ca = getAssign("e");
  ASSERT_NE(ca, nullptr);
  const hldb::Operation *const op = ca->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "5.7.1: a leading minus is a unary minus operator";
  EXPECT_EQ(op->getOpType(), vpiMinusOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 1u);
  const hldb::Constant *const c = any_cast<hldb::Constant>(op->getOperands()->at(0));
  ASSERT_NE(c, nullptr);
  checkLiteral(c, "18446744073709551616", 66);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
