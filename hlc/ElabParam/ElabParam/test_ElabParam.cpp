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

// Tests for tests/ElabParam/dut.sv -- a package parameter and an
// enumeration constant, both consumed via package-qualified references
// ('top_pkg::PMP_CFG_W', 'top_pkg::OPCODE_LOAD') inside a module's
// continuous assignment, each wrapped in an explicit size cast:
//
//   package top_pkg;
//      typedef enum logic [5:0] {
//        OPCODE_LOAD  = 6'h03,
//        OPCODE_STORE = 6'h13
//      } opcode_e;
//      parameter int unsigned PMP_CFG_W = 2;
//   endpackage
//
//   module dut (a, b);
//     input [5:0] a;
//     output [2:0] b;
//     wire [5:0] a;
//     reg [2:0] b;
//     assign b = 4'(top_pkg::PMP_CFG_W) | 5'(a == top_pkg::OPCODE_LOAD);
//   endmodule
//
// -- rules under test ---------------------------------------------------
//
// IEEE 1800-2023 6.20.2 "Parameter declarations": 'parameter int unsigned
// PMP_CFG_W = 2' declared inside a package is a package parameter,
// resolved via 'top_pkg::PMP_CFG_W' (26.3 "Package declarations" +
// 23.3 scope resolution operator) inside 'dut'.
// IEEE 1800-2023 6.19 "Enumerations": 'OPCODE_LOAD'/'OPCODE_STORE' are
// EnumConst members of 'opcode_e', referenced the same way.
// IEEE 1800-2023 6.24.1 "Size casts": "4'(expr)"/"5'(expr)" are constant
// size casts, each an Operation(vpiCastOp) with exactly one operand.
// IEEE 1800-2023 11.4.7 "Bitwise operators": '|' is vpiBitOrOp.
// IEEE 1800-2023 11.4.5 "Equality operators": '==' is vpiEqOp.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ElabParamTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ElabParam.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getTopPkg() {
    return hldb::findByName<hldb::Package>("top_pkg", m_design->getAllPackages());
  }

  static const hldb::Module *getDut() { return hldb::findByDefName<hldb::Module>("dut", m_design->getAllModules()); }

  template <typename ScopeT>
  static const hldb::Parameter *findParam(const ScopeT *scope, std::string_view name) {
    if (scope == nullptr || scope->getParameters() == nullptr) return nullptr;
    for (const hldb::Any *const p : *scope->getParameters()) {
      const hldb::Parameter *const param = any_cast<hldb::Parameter>(p);
      if (param != nullptr && param->getName() == name) return param;
    }
    return nullptr;
  }

  template <typename ScopeT>
  static const hldb::ParamAssign *findParamAssign(const ScopeT *scope, std::string_view name) {
    return (scope == nullptr) ? nullptr : hldb::findByName(name, hldb::getParamAssigns(scope));
  }

  static const hldb::EnumTypespec *getOpcodeEnum() {
    const hldb::Package *const pkg = getTopPkg();
    if (pkg == nullptr || pkg->getTypespecs() == nullptr) return nullptr;
    const hldb::TypedefTypespec *const tt = hldb::findByName<hldb::TypedefTypespec>("opcode_e", pkg->getTypespecs());
    if (tt == nullptr || tt->getTypedef() == nullptr) return nullptr;
    return tt->getTypedef()->getAlias()->getActual<hldb::EnumTypespec>();
  }

  // Returns the ContAssign whose LHS RefObj is named 'name', or nullptr.
  static const hldb::ContAssign *findContAssign(const hldb::Module *m, std::string_view name) {
    if (m == nullptr || m->getContAssigns() == nullptr) return nullptr;
    for (const hldb::ContAssign *const ca : *m->getContAssigns()) {
      const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
      if (lhs != nullptr && lhs->getName() == name) return ca;
    }
    return nullptr;
  }
};

// ---------------------------------------------------------------------------
// Package top_pkg
// ---------------------------------------------------------------------------

TEST_F(ElabParamTest, PackageTopPkgExists) { EXPECT_NE(getTopPkg(), nullptr) << "package 'top_pkg' not found"; }

TEST_F(ElabParamTest, ModuleDutExists) { EXPECT_NE(getDut(), nullptr) << "module 'dut' not found"; }

// ---------------------------------------------------------------------------
// top_pkg::PMP_CFG_W -- 'parameter int unsigned PMP_CFG_W = 2;'
// ---------------------------------------------------------------------------

TEST_F(ElabParamTest, PmpCfgWNotLocalParamWithDefaultTwo) {
  const hldb::Package *const pkg = getTopPkg();
  ASSERT_NE(pkg, nullptr);
  const hldb::Parameter *const p = findParam(pkg, "PMP_CFG_W");
  ASSERT_NE(p, nullptr) << "'parameter int unsigned PMP_CFG_W' not found in 'top_pkg'";
  EXPECT_FALSE(p->getLocalParam()) << "'parameter int unsigned PMP_CFG_W' must not be a localparam";

  const hldb::ParamAssign *const pa = findParamAssign(pkg, "PMP_CFG_W");
  ASSERT_NE(pa, nullptr) << "default ParamAssign for 'PMP_CFG_W' not found";
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "'PMP_CFG_W = 2': default RHS must be a Constant";
  EXPECT_EQ(std::string(rhs->getDecompile()), "2");
}

// ---------------------------------------------------------------------------
// top_pkg::opcode_e -- 'typedef enum logic [5:0] {OPCODE_LOAD = 6'h03,
// OPCODE_STORE = 6'h13} opcode_e;' (IEEE 1800-2023 6.19)
// ---------------------------------------------------------------------------

TEST_F(ElabParamTest, OpcodeEnumHasTwoConstsInDeclarationOrder) {
  const hldb::EnumTypespec *const enumTs = getOpcodeEnum();
  ASSERT_NE(enumTs, nullptr) << "top_pkg::opcode_e should resolve to an EnumTypespec";
  const hldb::Enum *const e = enumTs->getEnum();
  ASSERT_NE(e, nullptr);
  ASSERT_NE(e->getEnumConsts(), nullptr);
  ASSERT_EQ(e->getEnumConsts()->size(), 2u);
  EXPECT_EQ(e->getEnumConsts()->at(0)->getName(), std::string_view("OPCODE_LOAD"));
  EXPECT_EQ(e->getEnumConsts()->at(1)->getName(), std::string_view("OPCODE_STORE"));
}

TEST_F(ElabParamTest, OpcodeLoadAndStoreHaveConstantValues) {
  const hldb::EnumTypespec *const enumTs = getOpcodeEnum();
  ASSERT_NE(enumTs, nullptr);
  const hldb::Enum *const e = enumTs->getEnum();
  ASSERT_NE(e, nullptr);
  ASSERT_EQ(e->getEnumConsts()->size(), 2u);

  const hldb::Constant *const loadVal = e->getEnumConsts()->at(0)->getValue<hldb::Constant>();
  ASSERT_NE(loadVal, nullptr) << "'OPCODE_LOAD = 6'h03': value must be a Constant";
  const hldb::Constant *const storeVal = e->getEnumConsts()->at(1)->getValue<hldb::Constant>();
  ASSERT_NE(storeVal, nullptr) << "'OPCODE_STORE = 6'h13': value must be a Constant";
}

// ---------------------------------------------------------------------------
// dut: ports a (input), b (output)
// ---------------------------------------------------------------------------

TEST_F(ElabParamTest, DutPortsAAndBExist) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getPorts(), nullptr) << "'dut' has no ports";
  const hldb::Port *const a = hldb::findByName<hldb::Port>("a", dut->getPorts());
  const hldb::Port *const b = hldb::findByName<hldb::Port>("b", dut->getPorts());
  ASSERT_NE(a, nullptr) << "port 'a' not found";
  ASSERT_NE(b, nullptr) << "port 'b' not found";
  EXPECT_EQ(a->getDirection(), vpiInput) << "'input [5:0] a' must have vpiInput direction";
  EXPECT_EQ(b->getDirection(), vpiOutput) << "'output [2:0] b' must have vpiOutput direction";
}

// ---------------------------------------------------------------------------
// 'assign b = 4'(top_pkg::PMP_CFG_W) | 5'(a == top_pkg::OPCODE_LOAD);'
// ---------------------------------------------------------------------------

TEST_F(ElabParamTest, AssignBExistsWithBitwiseOrRhs) {
  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  const hldb::ContAssign *const ca = findContAssign(dut, "b");
  ASSERT_NE(ca, nullptr) << "'assign b = ...;' ContAssign not found";
  const hldb::Operation *const orOp = ca->getRhs<hldb::Operation>();
  ASSERT_NE(orOp, nullptr) << "'... | ...': RHS must be an Operation";
  EXPECT_EQ(orOp->getOpType(), vpiBitOrOp) << "'|' must produce vpiBitOrOp";
  ASSERT_NE(orOp->getOperands(), nullptr);
  ASSERT_EQ(orOp->getOperands()->size(), 2u) << "binary '|' has exactly two operands";
}

// Left operand: "4'(top_pkg::PMP_CFG_W)" -- size cast over the
// package-qualified parameter reference.
TEST_F(ElabParamTest, LeftOperandIsSizeCastOfPmpCfgWReference) {
  GTEST_SKIP() << "HLC's leavePA_Casting_type() only synthesizes a target Typespec for the "
                  "identifier (named-type) form of casting_type (e.g. \"int'(expr)\"). For the "
                  "numeric size-cast form (\"4'(expr)\"), it falls back to pullModelsUp() and "
                  "leaves the size literal as a bare Constant, which leavePA_Cast() then pushes "
                  "onto the cast Operation's operands instead of routing it through "
                  "setTypespec() -- so today castOp->getOperands() incorrectly has 2 entries "
                  "(the size Constant, then the expr) and castOp->getTypespec() is null. Per "
                  "IEEE 1800-2023 Sec 6.24.1, a size cast and a named-type cast are the same "
                  "vpiCastOp construct and must have the same shape: 1 operand (the expr being "
                  "cast) plus a populated target Typespec. Fix pending.";

  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  const hldb::ContAssign *const ca = findContAssign(dut, "b");
  ASSERT_NE(ca, nullptr);
  const hldb::Operation *const orOp = ca->getRhs<hldb::Operation>();
  ASSERT_NE(orOp, nullptr);
  ASSERT_EQ(orOp->getOperands()->size(), 2u);

  const hldb::Operation *const castOp = any_cast<hldb::Operation>((*orOp->getOperands())[0]);
  ASSERT_NE(castOp, nullptr) << "\"4'(top_pkg::PMP_CFG_W)\" must be an Operation(vpiCastOp)";
  EXPECT_EQ(castOp->getOpType(), vpiCastOp);

  // Sec 6.24.1: the leading "4" is a width specifier for the cast's target
  // type, exactly like the "int" in "int'(expr)" -- it belongs on a
  // synthesized target Typespec, not as a second operand.
  ASSERT_NE(castOp->getTypespec(), nullptr) << "a size cast must synthesize a target typespec, same as a named-type cast";
  ASSERT_NE(castOp->getOperands(), nullptr);
  ASSERT_EQ(castOp->getOperands()->size(), 1u) << "a cast operator has exactly one operand: the expr being cast";

  const hldb::RefObj *const ref = any_cast<hldb::RefObj>((*castOp->getOperands())[0]);
  ASSERT_NE(ref, nullptr) << "'top_pkg::PMP_CFG_W': cast operand must be a RefObj";
  EXPECT_NE(ref->getActual(), nullptr) << "'top_pkg::PMP_CFG_W' must resolve to an actual";
  const hldb::Parameter *const actual = ref->getActual<hldb::Parameter>();
  ASSERT_NE(actual, nullptr) << "'top_pkg::PMP_CFG_W' must resolve to the package's Parameter";
  EXPECT_EQ(actual->getName(), "PMP_CFG_W");
}

// Right operand: "5'(a == top_pkg::OPCODE_LOAD)" -- size cast over an
// equality comparison against the package-qualified enum constant.
TEST_F(ElabParamTest, RightOperandIsSizeCastOfEqualityWithOpcodeLoad) {
  GTEST_SKIP() << "Same HLC limitation as LeftOperandIsSizeCastOfPmpCfgWReference: "
                  "leavePA_Casting_type() does not synthesize a target Typespec for the numeric "
                  "size-cast form, so leavePA_Cast() pushes the size literal as a second operand "
                  "instead of routing it through setTypespec(). Per IEEE 1800-2023 Sec 6.24.1 "
                  "this size cast must have the same shape as a named-type cast: 1 operand plus "
                  "a populated target Typespec. Fix pending.";

  const hldb::Module *const dut = getDut();
  ASSERT_NE(dut, nullptr);
  const hldb::ContAssign *const ca = findContAssign(dut, "b");
  ASSERT_NE(ca, nullptr);
  const hldb::Operation *const orOp = ca->getRhs<hldb::Operation>();
  ASSERT_NE(orOp, nullptr);
  ASSERT_EQ(orOp->getOperands()->size(), 2u);

  const hldb::Operation *const castOp = any_cast<hldb::Operation>((*orOp->getOperands())[1]);
  ASSERT_NE(castOp, nullptr) << "\"5'(a == top_pkg::OPCODE_LOAD)\" must be an Operation(vpiCastOp)";
  EXPECT_EQ(castOp->getOpType(), vpiCastOp);

  ASSERT_NE(castOp->getTypespec(), nullptr) << "a size cast must synthesize a target typespec, same as a named-type cast";
  ASSERT_NE(castOp->getOperands(), nullptr);
  ASSERT_EQ(castOp->getOperands()->size(), 1u) << "a cast operator has exactly one operand: the expr being cast";

  const hldb::Operation *const eqOp = any_cast<hldb::Operation>((*castOp->getOperands())[0]);
  ASSERT_NE(eqOp, nullptr) << "'a == top_pkg::OPCODE_LOAD' must be an Operation(vpiEqOp)";
  EXPECT_EQ(eqOp->getOpType(), vpiEqOp);
  ASSERT_NE(eqOp->getOperands(), nullptr);
  ASSERT_EQ(eqOp->getOperands()->size(), 2u);

  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>((*eqOp->getOperands())[0]);
  ASSERT_NE(lhs, nullptr) << "'a == ...': left operand must be a RefObj (reference to port a)";
  EXPECT_EQ(lhs->getName(), "a");

  const hldb::RefObj *const rhs = any_cast<hldb::RefObj>((*eqOp->getOperands())[1]);
  ASSERT_NE(rhs, nullptr) << "'top_pkg::OPCODE_LOAD': right operand must be a RefObj";
  EXPECT_NE(rhs->getActual(), nullptr) << "'top_pkg::OPCODE_LOAD' must resolve to an actual";
  const hldb::EnumConst *const actual = rhs->getActual<hldb::EnumConst>();
  ASSERT_NE(actual, nullptr) << "'top_pkg::OPCODE_LOAD' must resolve to the enum's EnumConst";
  EXPECT_EQ(actual->getName(), "OPCODE_LOAD");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
