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

// Tests for dut.sv (tags: MultiIndexBind)
//   module PreDecodeStage();
//   typedef enum logic [3:0] { AC_ADD = 4'b0000 } IntALU_Code;
//   typedef struct packed { IntALU_Code aluCode; } IntMicroOpOperand;
//   typedef union packed { IntMicroOpOperand intOp; } MicroOpOperand;
//   typedef struct packed { MicroOpOperand operand; } OpInfo;
//
//   OpInfo [1:0][1:0] microOps;
//
//   assign o = microOps[i][1].operand.intOp.aluCode;
//   endmodule
//
// What is checked (IEEE 1800-2023):
//   - 6.19: 'IntALU_Code' is an enum with base type logic [3:0] and a single
//     constant AC_ADD whose value is 0.
//   - 7.2.1 / 7.3.1: 'IntMicroOpOperand' and 'OpInfo' are packed structs,
//     'MicroOpOperand' is a packed union, each with a single member
//     (aluCode / operand / intOp respectively).
//   - 7.4.1: 'OpInfo [1:0][1:0] microOps;' is a variable (6.8: no net-type
//     keyword) whose type is a two-dimensional packed array of the packed
//     struct 'OpInfo'.
//   - 7.4.5 / 7.2 / 7.3: the RHS 'microOps[i][1].operand.intOp.aluCode'
//     selects one element with two indices ('[i]' then '[1]') and then
//     walks three member selects. Each member name binds to the member of
//     the struct/union type of the preceding prefix: 'operand' -> member of
//     OpInfo, 'intOp' -> member of MicroOpOperand, 'aluCode' -> member of
//     IntMicroOpOperand. 'microOps' binds to the variable.
//   - 6.10: 'o' is undeclared and appears on the LHS of a continuous
//     assignment, so "an implicit scalar net of default net type shall be
//     assumed": a scalar implicit 'wire' net 'o' exists and the LHS binds
//     to it.
//   - 6.10 / 23.9: 'i' is undeclared and is used in an expression that is
//     NOT a continuous-assignment LHS or port connection, so no implicit net
//     is created for it and it cannot be resolved: a COMP_FAILED_TO_BIND
//     error must be reported for 'i'.
//
// What is NOT checked and why:
//   - the concrete HLDB class used for the non-constant select '[i]'
//     (BitSelect vs VarSelect): both are valid single-element selects; only
//     the prefix chain and index identity are checked.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/enum.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/union.h>
#include <hldb/union_typespec.h>
#include <hldb/var_select.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string>

namespace hlc {

class MultiIndexBindTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "MultiIndexBind.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getMod() {
    return hldb::findByName<hldb::Module>("PreDecodeStage", m_design->getAllModules());
  }

  static const hldb::Typespec *getAliasActual(std::string_view name) {
    const hldb::Module *const m = getMod();
    if (m == nullptr) return nullptr;
    const hldb::Typedef *const td = hldb::findByName<hldb::Typedef>(name, m->getTypedefs());
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    return td->getAlias()->getActual();
  }

  static const hldb::Struct *getStruct(std::string_view name) {
    const hldb::StructTypespec *const sts = any_cast<hldb::StructTypespec>(getAliasActual(name));
    return (sts == nullptr) ? nullptr : sts->getStruct();
  }

  static const hldb::Union *getUnion(std::string_view name) {
    const hldb::UnionTypespec *const uts = any_cast<hldb::UnionTypespec>(getAliasActual(name));
    return (uts == nullptr) ? nullptr : uts->getUnion();
  }

  static const hldb::ContAssign *getAssign() {
    const hldb::Module *const m = getMod();
    if (m == nullptr || m->getContAssigns() == nullptr || m->getContAssigns()->empty()) return nullptr;
    return m->getContAssigns()->at(0);
  }

  static const hldb::RefObj *getRhs() {
    const hldb::ContAssign *const ca = getAssign();
    return (ca == nullptr) ? nullptr : any_cast<hldb::RefObj>(ca->getRhs());
  }

  static const hldb::Expr *selectIndex(const hldb::Any *sel) {
    if (const hldb::BitSelect *const bs = any_cast<hldb::BitSelect>(sel)) return bs->getIndex();
    if (const hldb::VarSelect *const vs = any_cast<hldb::VarSelect>(sel)) return vs->getIndex();
    return nullptr;
  }

  static const hldb::Expr *selectPrefix(const hldb::Any *sel) {
    if (const hldb::BitSelect *const bs = any_cast<hldb::BitSelect>(sel)) return bs->getPrefix();
    if (const hldb::VarSelect *const vs = any_cast<hldb::VarSelect>(sel)) return vs->getPrefix();
    return nullptr;
  }

  // Checks path element 'index' is a RefObj 'name' bound to the single
  // member of 'owner'.
  static void checkMember(size_t index, std::string_view name, const hldb::Any *owner) {
    const hldb::RefObj *const rhs = getRhs();
    ASSERT_NE(rhs, nullptr);
    ASSERT_NE(rhs->getPathElems(), nullptr);
    ASSERT_EQ(rhs->getPathElems()->size(), 4u);
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(rhs->getPathElems()->at(index));
    ASSERT_NE(ref, nullptr);
    EXPECT_EQ(ref->getName(), name);
    ASSERT_NE(ref->getActual(), nullptr) << "member select '" << name << "' must bind";
    const hldb::TypespecMember *const tm = any_cast<hldb::TypespecMember>(ref->getActual());
    ASSERT_NE(tm, nullptr);
    EXPECT_EQ(tm->getName(), name);
    ASSERT_NE(owner, nullptr);
    EXPECT_EQ(tm->getParent(), owner) << "'" << name << "' must be the member of the prefix's struct/union type";
  }
};

TEST_F(MultiIndexBindTest, ModuleExists) { EXPECT_NE(getMod(), nullptr); }

// ===========================================================================
// 6.19 enum
// ===========================================================================

TEST_F(MultiIndexBindTest, IntALUCodeIsLogic4EnumWithAcAdd) {
  const hldb::EnumTypespec *const ets = any_cast<hldb::EnumTypespec>(getAliasActual("IntALU_Code"));
  ASSERT_NE(ets, nullptr) << "'IntALU_Code' is an enum typedef";
  const hldb::Enum *const e = ets->getEnum();
  ASSERT_NE(e, nullptr);
  ASSERT_NE(e->getBaseTypespec(), nullptr);
  const hldb::LogicTypespec *const base = any_cast<hldb::LogicTypespec>(e->getBaseTypespec()->getActual());
  ASSERT_NE(base, nullptr) << "6.19: explicit base type logic [3:0]";
  ASSERT_NE(base->getRanges(), nullptr);
  EXPECT_EQ(base->getRanges()->size(), 1u);
  ASSERT_NE(e->getEnumConsts(), nullptr);
  ASSERT_EQ(e->getEnumConsts()->size(), 1u);
  const hldb::EnumConst *const c = e->getEnumConsts()->at(0);
  EXPECT_EQ(c->getName(), "AC_ADD");
}

// ===========================================================================
// 7.2.1 / 7.3.1 packed struct / union nesting
// ===========================================================================

TEST_F(MultiIndexBindTest, NestedPackedAggregates) {
  const hldb::Struct *const intOpS = getStruct("IntMicroOpOperand");
  const hldb::Union *const opU = getUnion("MicroOpOperand");
  const hldb::Struct *const infoS = getStruct("OpInfo");
  ASSERT_NE(intOpS, nullptr);
  ASSERT_NE(opU, nullptr) << "'MicroOpOperand' is a union typedef";
  ASSERT_NE(infoS, nullptr);
  EXPECT_TRUE(intOpS->getPacked());
  EXPECT_TRUE(opU->getPacked());
  EXPECT_TRUE(infoS->getPacked());
  ASSERT_NE(intOpS->getMembers(), nullptr);
  ASSERT_NE(opU->getMembers(), nullptr);
  ASSERT_NE(infoS->getMembers(), nullptr);
  ASSERT_EQ(intOpS->getMembers()->size(), 1u);
  ASSERT_EQ(opU->getMembers()->size(), 1u);
  ASSERT_EQ(infoS->getMembers()->size(), 1u);
  EXPECT_EQ(intOpS->getMembers()->at(0)->getName(), "aluCode");
  EXPECT_EQ(opU->getMembers()->at(0)->getName(), "intOp");
  EXPECT_EQ(infoS->getMembers()->at(0)->getName(), "operand");
}

// ===========================================================================
// 7.4.1: OpInfo [1:0][1:0] microOps
// ===========================================================================

TEST_F(MultiIndexBindTest, MicroOpsIsTwoDimPackedArrayOfOpInfo) {
  const hldb::Module *const m = getMod();
  ASSERT_NE(m, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("microOps", m->getVariables());
  ASSERT_NE(v, nullptr) << "6.8: 'microOps' is a variable";
  ASSERT_NE(v->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const outer = any_cast<hldb::ArrayTypespec>(v->getTypespec()->getActual());
  ASSERT_NE(outer, nullptr);
  EXPECT_TRUE(outer->getPacked()) << "7.4.1: dimensions before the name are packed";
  ASSERT_NE(outer->getElemTypespec(), nullptr);
  const hldb::ArrayTypespec *const inner = any_cast<hldb::ArrayTypespec>(outer->getElemTypespec()->getActual());
  ASSERT_NE(inner, nullptr) << "second packed dimension";
  EXPECT_TRUE(inner->getPacked());
  ASSERT_NE(inner->getElemTypespec(), nullptr);
  const hldb::TypedefTypespec *const elem = any_cast<hldb::TypedefTypespec>(inner->getElemTypespec()->getActual());
  ASSERT_NE(elem, nullptr) << "element type is typedef 'OpInfo'";
  EXPECT_EQ(elem->getName(), "OpInfo");
}

// ===========================================================================
// RHS: microOps[i][1].operand.intOp.aluCode
// ===========================================================================

TEST_F(MultiIndexBindTest, RhsIsFourElementPath) {
  const hldb::RefObj *const rhs = getRhs();
  ASSERT_NE(rhs, nullptr) << "RHS should be a RefObj path";
  ASSERT_NE(rhs->getPathElems(), nullptr);
  EXPECT_EQ(rhs->getPathElems()->size(), 4u) << "'microOps[i][1]' then 'operand', 'intOp', 'aluCode'";
}

TEST_F(MultiIndexBindTest, RhsHeadIsTwoIndexSelectOnMicroOps) {
  const hldb::RefObj *const rhs = getRhs();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getPathElems(), nullptr);
  ASSERT_FALSE(rhs->getPathElems()->empty());
  const hldb::Any *const outerSel = rhs->getPathElems()->at(0);
  const hldb::Constant *const idx1 = any_cast<hldb::Constant>(selectIndex(outerSel));
  ASSERT_NE(idx1, nullptr) << "outer select is '[1]'";
  EXPECT_EQ(idx1->getDecompile(), "1");

  const hldb::Expr *const innerSel = selectPrefix(outerSel);
  ASSERT_NE(innerSel, nullptr);
  const hldb::RefObj *const idxI = any_cast<hldb::RefObj>(selectIndex(innerSel));
  ASSERT_NE(idxI, nullptr) << "inner select is '[i]'";
  EXPECT_EQ(idxI->getName(), "i");

  const hldb::RefObj *const base = any_cast<hldb::RefObj>(selectPrefix(innerSel));
  ASSERT_NE(base, nullptr);
  EXPECT_EQ(base->getName(), "microOps");
  ASSERT_NE(base->getActual(), nullptr);
  EXPECT_EQ(base->getActual()->getAnyType(), hldb::AnyType::Variable);
}

TEST_F(MultiIndexBindTest, OperandBindsToOpInfoMember) { checkMember(1, "operand", getStruct("OpInfo")); }

TEST_F(MultiIndexBindTest, IntOpBindsToMicroOpOperandMember) { checkMember(2, "intOp", getUnion("MicroOpOperand")); }

TEST_F(MultiIndexBindTest, AluCodeBindsToIntMicroOpOperandMember) {
  checkMember(3, "aluCode", getStruct("IntMicroOpOperand"));
}

// ===========================================================================
// 6.10: implicit net 'o'; undeclared 'i' fails to bind
// ===========================================================================

TEST_F(MultiIndexBindTest, OIsImplicitScalarWire) {
  const hldb::Module *const m = getMod();
  ASSERT_NE(m, nullptr);
  const hldb::Net *const o = hldb::findByName<hldb::Net>("o", m->getNets());
  ASSERT_NE(o, nullptr) << "6.10: undeclared LHS of a continuous assignment is an implicit net";
  EXPECT_TRUE(o->getImplicitDecl());
  EXPECT_EQ(o->getNetType(), vpiWire);
  EXPECT_TRUE(o->getScalar()) << "6.10: implicit scalar net";
}

TEST_F(MultiIndexBindTest, LhsBindsToImplicitNet) {
  const hldb::ContAssign *const ca = getAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(ca->getLhs());
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "o");
  ASSERT_NE(lhs->getActual(), nullptr) << "6.10: 'o' must resolve to its implicit net";
  EXPECT_EQ(lhs->getActual()->getAnyType(), hldb::AnyType::Net);
}

TEST_F(MultiIndexBindTest, UndeclaredIndexIFailsToBind) {
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "i"), nullptr)
      << "6.10: implicit nets are only created for continuous-assignment LHS / port connections; 'i' is undeclared";
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "microOps"), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
