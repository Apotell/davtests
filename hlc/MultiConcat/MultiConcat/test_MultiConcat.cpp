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

// Tests for tests/MultiConcat/dut.sv (tags: MultiConcat)
//   package lc_ctrl_pkg;
//     parameter int NumTokens = 6;
//     parameter int TokenIdxWidth = 3;
//     typedef enum logic [TokenIdxWidth-1:0] {
//       ZeroTokenIdx = 3'h0, RawUnlockTokenIdx = 3'h1, TestUnlockTokenIdx = 3'h2,
//       TestExitTokenIdx = 3'h3, RmaTokenIdx = 3'h4, InvalidTokenIdx = 3'h5
//     } token_idx_e;
//     ...
//     `define TEST_UNLOCKED(idx) {2{ZeroTokenIdx}}, {3{TestExitTokenIdx}},
//         {(7-idx){InvalidTokenIdx, ZeroTokenIdx}}, {(2*idx+2){InvalidTokenIdx}}
//     `define TEST_LOCKED(idx) ZeroTokenIdx, InvalidTokenIdx,
//         {3{TestExitTokenIdx}}, {(7-idx){TestUnlockTokenIdx, InvalidTokenIdx}},
//         {(2*idx+2){InvalidTokenIdx}}
//     parameter token_idx_e [NumLcStates-1:0][NumLcStates-1:0]
//       TransTokenIdxMatrix = {
//         {21{InvalidTokenIdx}},                         // SCRAP   (1 item)
//         ZeroTokenIdx, {20{InvalidTokenIdx}},           // RMA     (2 items)
//         ZeroTokenIdx, {20{InvalidTokenIdx}},           // PROD_END(2 items)
//         ZeroTokenIdx, RmaTokenIdx, {19{InvalidTokenIdx}}, // PROD (3 items)
//         ZeroTokenIdx, RmaTokenIdx, {19{InvalidTokenIdx}}, // DEV  (3 items)
//         `TEST_UNLOCKED(7), `TEST_LOCKED(6), ... `TEST_UNLOCKED(0),
//         ZeroTokenIdx, {4{InvalidTokenIdx}},
//         {8{RawUnlockTokenIdx, InvalidTokenIdx}}         // RAW     (3 items)
//       };
//   endpackage : lc_ctrl_pkg
//
// What is checked (IEEE 1800-2023):
//   - 26.2 / 6.20.4: package lc_ctrl_pkg exists; "In these contexts [package]
//     the parameter keyword shall be a synonym for the localparam keyword",
//     so every parameter in it is a local parameter.
//   - 6.19: token_idx_e is an enum with 6 constants in source order and the
//     given 3-bit hex values.
//   - 11.4.12: the RHS of TransTokenIdxMatrix is a single concatenation
//     (vpiConcatOp). After macro expansion (22.5.1) it has
//       11 (SCRAP..DEV) + 8 x 4 (TEST_UNLOCKED) + 7 x 5 (TEST_LOCKED) + 3 (RAW)
//       = 81 top-level operands.
//   - 11.4.12.1: a replication "{N{...}}" is an Operation(vpiMultiConcatOp)
//     whose operands are the multiplier and the replicated concatenation.
//     First operand {21{InvalidTokenIdx}}: multiplier 21, inner concat of one
//     reference bound to enum constant InvalidTokenIdx. Last operand
//     {8{RawUnlockTokenIdx, InvalidTokenIdx}}: multiplier 8, inner concat of
//     two references in order.
//   - 11.4.12.1: "A replication operation may have a multiplier with a value
//     of zero ... Such a replication shall appear only within a concatenation
//     in which at least one of the operands ... has a positive size."
//     `TEST_UNLOCKED(7) yields {(7-7){...}} (zero multiplier) inside the
//     81-operand concatenation, which is legal -> no
//     COMP_ILLEGAL_REPLICATION_COUNT.
//   - 6.3 / 23.9: NumLcStates is used in the packed dimensions of
//     TransTokenIdxMatrix's type but is never declared in the package (or
//     imported) -> it must fail to bind (COMP_FAILED_TO_BIND "NumLcStates").
//   - 7.4.1: "token_idx_e [..][..]" puts both dimensions before the
//     identifier, so the parameter's type is a packed array.
//
// What is NOT checked and why:
//   - the exact HLDB shape of the non-constant multipliers (7-idx),
//     (2*idx+2): whether they are folded to constants is an implementation
//     choice; only the vpiMultiConcatOp shape is asserted.
//   - the evaluated width of the matrix: NumLcStates is undefined, so no
//     width can be computed.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class MultiConcatTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "MultiConcat.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("lc_ctrl_pkg", m_design->getAllPackages());
  }

  static const hldb::Enum *getTokenEnum() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    const hldb::Typedef *const td = hldb::findByName<hldb::Typedef>("token_idx_e", pkg->getTypedefs());
    if ((td == nullptr) || (td->getAlias() == nullptr)) return nullptr;
    const hldb::EnumTypespec *const ets = td->getAlias()->getActual<hldb::EnumTypespec>();
    if (ets == nullptr) return nullptr;
    return ets->getEnum();
  }

  static const hldb::Operation *getMatrixRhs() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    const hldb::ParamAssign *const pa =
        hldb::findByName<hldb::ParamAssign>("TransTokenIdxMatrix", pkg->getParamAssigns());
    if (pa == nullptr) return nullptr;
    return pa->getRhs<hldb::Operation>();
  }

  // Checks 'any' is a replication whose inner concatenation lists 'names'.
  static void expectReplication(const hldb::Any *any, std::string_view multiplier,
                                std::initializer_list<std::string_view> names) {
    const hldb::Operation *const rep = any_cast<hldb::Operation>(any);
    ASSERT_NE(rep, nullptr);
    EXPECT_EQ(rep->getOpType(), vpiMultiConcatOp);
    ASSERT_NE(rep->getOperands(), nullptr);
    ASSERT_EQ(rep->getOperands()->size(), 2u);
    const hldb::Constant *const mult = any_cast<hldb::Constant>(rep->getOperands()->at(0));
    ASSERT_NE(mult, nullptr);
    EXPECT_EQ(mult->getDecompile(), multiplier);
    const hldb::Operation *const inner = any_cast<hldb::Operation>(rep->getOperands()->at(1));
    ASSERT_NE(inner, nullptr);
    EXPECT_EQ(inner->getOpType(), vpiConcatOp);
    ASSERT_NE(inner->getOperands(), nullptr);
    ASSERT_EQ(inner->getOperands()->size(), names.size());
    size_t i = 0;
    for (std::string_view name : names) {
      const hldb::RefObj *const ref = any_cast<hldb::RefObj>(inner->getOperands()->at(i++));
      ASSERT_NE(ref, nullptr) << name;
      EXPECT_EQ(ref->getName(), name);
      ASSERT_NE(ref->getActual(), nullptr) << name;
      EXPECT_EQ(ref->getActual()->getAnyType(), hldb::AnyType::EnumConst) << name;
    }
  }
};

// ===========================================================================
// package + parameters
// ===========================================================================

TEST_F(MultiConcatTest, PackageExists) { EXPECT_NE(getPkg(), nullptr); }

TEST_F(MultiConcatTest, PackageParametersAreLocal) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getParameters(), nullptr);
  for (std::string_view name :
       {"NumTokens", "TokenIdxWidth", "TxWidth", "LC_TX_DEFAULT", "RmaSeedWidth", "LC_FLASH_RMA_SEED_DEFAULT",
        "LcKeymgrDivWidth", "FsmStateWidth", "TransTokenIdxMatrix"}) {
    const hldb::Parameter *const p = hldb::findByName<hldb::Parameter>(name, pkg->getParameters());
    ASSERT_NE(p, nullptr) << name;
    EXPECT_TRUE(p->getLocalParam()) << name << ": 6.20.4 parameter in a package is a localparam";
  }
}

// ===========================================================================
// 6.19: token_idx_e
// ===========================================================================

TEST_F(MultiConcatTest, TokenIdxEnumConstants) {
  const hldb::Enum *const en = getTokenEnum();
  ASSERT_NE(en, nullptr);
  ASSERT_NE(en->getEnumConsts(), nullptr);
  ASSERT_EQ(en->getEnumConsts()->size(), 6u);
  const char *const names[] = {"ZeroTokenIdx",     "RawUnlockTokenIdx", "TestUnlockTokenIdx",
                               "TestExitTokenIdx", "RmaTokenIdx",       "InvalidTokenIdx"};
  const char *const values[] = {"3'h0", "3'h1", "3'h2", "3'h3", "3'h4", "3'h5"};
  for (size_t i = 0; i < 6; ++i) {
    const hldb::EnumConst *const ec = en->getEnumConsts()->at(i);
    EXPECT_EQ(ec->getName(), names[i]);
    const hldb::Constant *const c = ec->getValue<hldb::Constant>();
    ASSERT_NE(c, nullptr) << names[i];
    EXPECT_EQ(c->getDecompile(), values[i]);
  }
}

// ===========================================================================
// 11.4.12: TransTokenIdxMatrix RHS
// ===========================================================================

TEST_F(MultiConcatTest, MatrixRhsIsConcatenationOf81Operands) {
  const hldb::Operation *const rhs = getMatrixRhs();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getOpType(), vpiConcatOp);
  ASSERT_NE(rhs->getOperands(), nullptr);
  EXPECT_EQ(rhs->getOperands()->size(), 81u) << "11 + 8*4 + 7*5 + 3 after macro expansion";
}

TEST_F(MultiConcatTest, FirstOperandIsReplicationOfInvalidToken) {
  const hldb::Operation *const rhs = getMatrixRhs();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_FALSE(rhs->getOperands()->empty());
  expectReplication(rhs->getOperands()->at(0), "21", {"InvalidTokenIdx"});
}

TEST_F(MultiConcatTest, SecondOperandIsPlainEnumReference) {
  const hldb::Operation *const rhs = getMatrixRhs();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_GE(rhs->getOperands()->size(), 2u);
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(rhs->getOperands()->at(1));
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "ZeroTokenIdx");
  ASSERT_NE(ref->getActual(), nullptr);
  EXPECT_EQ(ref->getActual()->getAnyType(), hldb::AnyType::EnumConst);
}

TEST_F(MultiConcatTest, LastOperandIsReplicationOfTwoItemConcat) {
  const hldb::Operation *const rhs = getMatrixRhs();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_FALSE(rhs->getOperands()->empty());
  expectReplication(rhs->getOperands()->back(), "8", {"RawUnlockTokenIdx", "InvalidTokenIdx"});
}

TEST_F(MultiConcatTest, MacroExpandedOperandsAreReplications) {
  // Operands 11..14 come from `TEST_UNLOCKED(7): four replications.
  const hldb::Operation *const rhs = getMatrixRhs();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_GE(rhs->getOperands()->size(), 15u);
  for (size_t i = 11; i < 15; ++i) {
    const hldb::Operation *const op = any_cast<hldb::Operation>(rhs->getOperands()->at(i));
    ASSERT_NE(op, nullptr) << "operand " << i;
    EXPECT_EQ(op->getOpType(), vpiMultiConcatOp) << "operand " << i;
  }
  expectReplication(rhs->getOperands()->at(11), "2", {"ZeroTokenIdx"});
  expectReplication(rhs->getOperands()->at(12), "3", {"TestExitTokenIdx"});
}

TEST_F(MultiConcatTest, ZeroReplicationInsideConcatIsLegal) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_ILLEGAL_REPLICATION_COUNT), nullptr)
      << "11.4.12.1: zero multiplier within a concatenation with positive-size operands is legal";
}

// ===========================================================================
// NumLcStates / packed type
// ===========================================================================

TEST_F(MultiConcatTest, UndeclaredNumLcStatesFailsToBind) {
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "NumLcStates"), nullptr)
      << "6.3: NumLcStates is never declared in lc_ctrl_pkg";
}

TEST_F(MultiConcatTest, MatrixTypeIsPackedArray) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  const hldb::Parameter *const p = hldb::findByName<hldb::Parameter>("TransTokenIdxMatrix", pkg->getParameters());
  ASSERT_NE(p, nullptr);
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = p->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_TRUE(at->getPacked()) << "7.4.1: dimensions before the identifier are packed";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
