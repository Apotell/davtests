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

// Tests for sha3_pkg.sv, kmac_core.sv, kmac.sv (tags: Kmac)
//   package sha3_pkg;
//     parameter int MsgWidth = 64;
//     typedef enum logic [2:0] {
//       L128 = 3'b 000, L224 = 3'b 001, L256 = 3'b 010,
//       L384 = 3'b 011, L512 = 3'b 100
//     } keccak_strength_e;
//     parameter int unsigned KeccakRate [5] = '{
//       1344/MsgWidth, 1152/MsgWidth, 1088/MsgWidth, 832/MsgWidth, 576/MsgWidth };
//     parameter int unsigned KeccakEntries = 1600/MsgWidth;
//     parameter int unsigned KeccakCountW = $clog2(KeccakEntries+1);
//   endpackage : sha3_pkg
//
//   module kmac_core#(
//     parameter  bit EnMasking = 0,
//     localparam int Share = (EnMasking) ? 2 : 1
//   ) (
//     input                             kmac_en_i,
//     input sha3_pkg::keccak_strength_e strength_i
//   );
//     import sha3_pkg::KeccakCountW;
//     import sha3_pkg::KeccakRate;
//     import sha3_pkg::L128;  ... L512;
//    logic [sha3_pkg::KeccakCountW-1:0] block_addr_limit;
//     always_comb begin
//       unique case (strength_i)
//         L128: block_addr_limit = KeccakCountW'(KeccakRate[L128]);
//         ...   (L224, L256, L384, L512 alike)
//         default: block_addr_limit = '0;
//       endcase
//     end
//   endmodule
//
//   module kmac#( <same parameter ports> ) ( <same ports> );
//     kmac_core core (.*);
//   endmodule
//
// What is checked (IEEE 1800-2023):
//   - sha3_pkg: 6.20.1 "All param_assignments appearing within a ...
//     package ... shall become localparam declarations" (MsgWidth,
//     KeccakRate, KeccakEntries, KeccakCountW); keccak_strength_e is an
//     enum (6.19) with base type logic [2:0] and 5 constants in order;
//     KeccakRate is an unpacked fixed-size array [5] (7.4.2) initialized by
//     a 5-element assignment pattern (10.9.1) whose elements are N/MsgWidth
//   - kmac_core / kmac parameter ports: EnMasking is an overridable
//     parameter of type bit, Share is a localparam (6.20.4: local
//     parameters may be declared in a parameter_port_list) whose value is
//     the conditional operator (11.4.11) on EnMasking
//   - ANSI input ports with no port kind default to nets of the default net
//     type (23.2.2.3); strength_i's data type is the package-scoped
//     sha3_pkg::keccak_strength_e (26.3)
//   - 26.3: each "import sha3_pkg::X;" is an explicit import of a distinct
//     item; importing several items from the same package is legal and
//     must NOT be reported as a redefinition of "sha3_pkg"
//   - block_addr_limit's packed range uses the class-scoped
//     sha3_pkg::KeccakCountW, resolving to the package parameter
//   - always_comb (9.2.2.2) holding a unique case (12.5.3): exact case type
//     with unique qualifier, condition strength_i, 6 items (5 + default),
//     item expressions bind to the imported enum constants
//   - 6.24.1: "KeccakCountW'(...)" is a size cast whose casting_type is a
//     constant_primary (the imported parameter); it is legal and must not
//     be reported as an unsupported typespec. Per 37.59 detail 3 it is a
//     unary vpiCastOp whose sole operand is KeccakRate[Lxxx], a bit-select
//     (7.4.6) of the imported array indexed by the imported enum constant
//   - default item assigns the unbased unsized literal '0 (5.7.1)
//   - kmac: "kmac_core core (.*);" is an instance of kmac_core using a
//     wildcard named port connection (23.3.2.4), which "is semantically
//     equivalent to an implicit .name port connection for every port
//     declared in the instantiated module" -> kmac_en_i and strength_i
//
// What is NOT checked and why:
//   - evaluated values (KeccakCountW == 5, KeccakRate contents, Share == 1):
//     this .hlc does not elaborate.
//   - Module::getName(): HLC decorates parameterized definition names;
//     definitions are looked up by getDefName().

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/bit_typespec.h>
#include <hldb/case_item.h>
#include <hldb/case_stmt.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/typedef.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>
#include <vector>

namespace hlc {

class KmacTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Kmac.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("sha3_pkg", m_design->getAllPackages());
  }

  static const hldb::Module *getModuleDef(std::string_view defName) {
    if (m_design->getAllModules() == nullptr) return nullptr;
    for (const hldb::Module *const m : *m_design->getAllModules()) {
      if (m->getDefName() == defName) return m;
    }
    return nullptr;
  }

  static const hldb::CaseStmt *getCase() {
    const hldb::Module *const core = getModuleDef("kmac_core");
    if ((core == nullptr) || (core->getProcesses() == nullptr) || (core->getProcesses()->size() != 1)) return nullptr;
    const hldb::Always *const alw = any_cast<hldb::Always>(core->getProcesses()->at(0));
    if (alw == nullptr) return nullptr;
    const hldb::Begin *const blk = alw->getStmt<hldb::Begin>();
    if ((blk == nullptr) || (blk->getStmts() == nullptr) || (blk->getStmts()->size() != 1)) return nullptr;
    return any_cast<hldb::CaseStmt>(blk->getStmts()->at(0));
  }

  static void checkParamPorts(const hldb::Module *m) {
    const hldb::Parameter *const en = hldb::findByName<hldb::Parameter>("EnMasking", m->getParameters());
    const hldb::Parameter *const share = hldb::findByName<hldb::Parameter>("Share", m->getParameters());
    ASSERT_NE(en, nullptr);
    ASSERT_NE(share, nullptr);
    EXPECT_FALSE(en->getLocalParam()) << "EnMasking is declared 'parameter'";
    EXPECT_TRUE(share->getLocalParam()) << "6.20.4: Share is declared 'localparam' in the parameter_port_list";
    ASSERT_NE(en->getTypespec(), nullptr);
    EXPECT_NE(en->getTypespec()->getActual<hldb::BitTypespec>(), nullptr) << "EnMasking is 'bit'";

    const hldb::ParamAssign *const pa = hldb::findByName<hldb::ParamAssign>("Share", m->getParamAssigns());
    ASSERT_NE(pa, nullptr);
    const hldb::Operation *const cond = pa->getRhs<hldb::Operation>();
    ASSERT_NE(cond, nullptr);
    EXPECT_EQ(cond->getOpType(), vpiConditionOp);
    ASSERT_NE(cond->getOperands(), nullptr);
    ASSERT_EQ(cond->getOperands()->size(), 3u);
    const hldb::RefObj *const sel = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
    ASSERT_NE(sel, nullptr);
    EXPECT_EQ(sel->getName(), "EnMasking");
    EXPECT_EQ(sel->getActual(), en);
  }

  static void checkPorts(const hldb::Module *m) {
    ASSERT_NE(m->getPorts(), nullptr);
    ASSERT_EQ(m->getPorts()->size(), 2u);
    const std::vector<std::string_view> names = {"kmac_en_i", "strength_i"};
    for (size_t i = 0; i < names.size(); ++i) {
      const hldb::Port *const p = m->getPorts()->at(i);
      ASSERT_NE(p, nullptr);
      EXPECT_EQ(p->getName(), names[i]);
      EXPECT_EQ(p->getDirection(), vpiInput);
      const hldb::Net *const n = hldb::findByName<hldb::Net>(names[i], m->getNets());
      ASSERT_NE(n, nullptr) << "23.2.2.3: input port '" << names[i] << "' defaults to a net";
      EXPECT_EQ(n->getNetType(), vpiWire);
    }
    const hldb::Port *const s = m->getPorts()->at(1);
    ASSERT_NE(s, nullptr);
    ASSERT_NE(s->getTypespec(), nullptr);
    EXPECT_EQ(s->getTypespec()->getName(), "sha3_pkg::keccak_strength_e");
    ASSERT_NE(s->getTypespec()->getActual(), nullptr);
    EXPECT_EQ(s->getTypespec()->getActual()->getName(), "keccak_strength_e");
  }
};

// ===========================================================================
// sha3_pkg
// ===========================================================================

TEST_F(KmacTest, PackageParametersAreLocal) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  EXPECT_EQ(pkg->getEndLabel(), "sha3_pkg");
  for (std::string_view name : {"MsgWidth", "KeccakRate", "KeccakEntries", "KeccakCountW"}) {
    const hldb::Parameter *const p = hldb::findByName<hldb::Parameter>(name, pkg->getParameters());
    ASSERT_NE(p, nullptr) << name;
    EXPECT_TRUE(p->getLocalParam()) << "6.20.1: package parameter '" << name << "' becomes localparam";
  }
}

TEST_F(KmacTest, KeccakStrengthEnum) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  const hldb::Typedef *const td = hldb::findByName<hldb::Typedef>("keccak_strength_e", pkg->getTypedefs());
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::EnumTypespec *const et = td->getAlias()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(et, nullptr);
  const hldb::Enum *const e = et->getEnum();
  ASSERT_NE(e, nullptr);
  ASSERT_NE(e->getBaseTypespec(), nullptr);
  EXPECT_NE(e->getBaseTypespec()->getActual<hldb::LogicTypespec>(), nullptr) << "base type is logic [2:0]";
  ASSERT_NE(e->getEnumConsts(), nullptr);
  ASSERT_EQ(e->getEnumConsts()->size(), 5u);
  const std::vector<std::string_view> names = {"L128", "L224", "L256", "L384", "L512"};
  const std::vector<std::string_view> texts = {"3'b 000", "3'b 001", "3'b 010", "3'b 011", "3'b 100"};
  for (size_t i = 0; i < names.size(); ++i) {
    const hldb::EnumConst *const c = e->getEnumConsts()->at(i);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->getName(), names[i]);
    const hldb::Constant *const v = c->getValue<hldb::Constant>();
    ASSERT_NE(v, nullptr) << names[i];
    EXPECT_EQ(v->getConstType(), vpiBinaryConst);
    EXPECT_EQ(v->getSize(), 3);
    EXPECT_EQ(v->getDecompile(), texts[i]);
  }
}

TEST_F(KmacTest, KeccakRateIsUnpackedArrayWithFiveElementPattern) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  const hldb::Parameter *const p = hldb::findByName<hldb::Parameter>("KeccakRate", pkg->getParameters());
  ASSERT_NE(p, nullptr);
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = p->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr) << "7.4.2: 'KeccakRate [5]' is an unpacked array";
  EXPECT_EQ(at->getArrayType(), vpiStaticArray);
  EXPECT_FALSE(at->getPacked());

  const hldb::ParamAssign *const pa = hldb::findByName<hldb::ParamAssign>("KeccakRate", pkg->getParamAssigns());
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const pat = pa->getRhs<hldb::Operation>();
  ASSERT_NE(pat, nullptr);
  EXPECT_EQ(pat->getOpType(), vpiAssignmentPatternOp);
  ASSERT_NE(pat->getOperands(), nullptr);
  ASSERT_EQ(pat->getOperands()->size(), 5u);
  const std::vector<std::string_view> nums = {"1344", "1152", "1088", "832", "576"};
  for (size_t i = 0; i < nums.size(); ++i) {
    const hldb::Operation *const div = any_cast<hldb::Operation>(pat->getOperands()->at(i));
    ASSERT_NE(div, nullptr) << i;
    EXPECT_EQ(div->getOpType(), vpiDivOp);
    ASSERT_NE(div->getOperands(), nullptr);
    ASSERT_EQ(div->getOperands()->size(), 2u);
    const hldb::Constant *const n = any_cast<hldb::Constant>(div->getOperands()->at(0));
    ASSERT_NE(n, nullptr);
    EXPECT_EQ(n->getDecompile(), nums[i]);
    const hldb::RefObj *const w = any_cast<hldb::RefObj>(div->getOperands()->at(1));
    ASSERT_NE(w, nullptr);
    EXPECT_EQ(w->getName(), "MsgWidth");
  }
}

// ===========================================================================
// kmac_core
// ===========================================================================

TEST_F(KmacTest, KmacCoreParameterPorts) {
  const hldb::Module *const m = getModuleDef("kmac_core");
  ASSERT_NE(m, nullptr);
  checkParamPorts(m);
}

TEST_F(KmacTest, KmacCorePortsAreInputNets) {
  const hldb::Module *const m = getModuleDef("kmac_core");
  ASSERT_NE(m, nullptr);
  checkPorts(m);
}

TEST_F(KmacTest, MultipleExplicitImportsFromSamePackageAreLegal) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_MULTIPLY_DEFINED_TYPEDEF, "sha3_pkg"), nullptr)
      << "26.3: several 'import sha3_pkg::<item>;' declarations import distinct items and redefine nothing";
}

TEST_F(KmacTest, BlockAddrLimitRangeUsesPackageScopedParameter) {
  const hldb::Module *const m = getModuleDef("kmac_core");
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(m, nullptr);
  ASSERT_NE(pkg, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("block_addr_limit", m->getVariables());
  ASSERT_NE(v, nullptr);
  ASSERT_NE(v->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = v->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr);
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  const hldb::Range *const r = lt->getRanges()->at(0);
  ASSERT_NE(r, nullptr);
  const hldb::Operation *const left = r->getLeftExpr<hldb::Operation>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getOpType(), vpiSubOp);
  ASSERT_NE(left->getOperands(), nullptr);
  ASSERT_EQ(left->getOperands()->size(), 2u);
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(left->getOperands()->at(0));
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getActual(), hldb::findByName<hldb::Parameter>("KeccakCountW", pkg->getParameters()));
}

TEST_F(KmacTest, AlwaysCombUniqueCase) {
  const hldb::Module *const m = getModuleDef("kmac_core");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getProcesses(), nullptr);
  ASSERT_EQ(m->getProcesses()->size(), 1u);
  const hldb::Always *const alw = any_cast<hldb::Always>(m->getProcesses()->at(0));
  ASSERT_NE(alw, nullptr);
  EXPECT_EQ(alw->getAlwaysType(), vpiAlwaysComb);

  const hldb::CaseStmt *const cs = getCase();
  ASSERT_NE(cs, nullptr) << "always_comb begin unique case ... endcase end";
  EXPECT_EQ(cs->getCaseType(), vpiCaseExact);
  EXPECT_EQ(cs->getQualifier(), vpiUniqueQualifier) << "12.5.3";
  const hldb::RefObj *const cond = cs->getCondition<hldb::RefObj>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getName(), "strength_i");
  ASSERT_NE(cond->getActual(), nullptr);
  EXPECT_EQ(cond->getActual()->getAnyType(), hldb::AnyType::Net);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  EXPECT_EQ(cs->getCaseItems()->size(), 6u);
}

TEST_F(KmacTest, CaseItemsMatchImportedEnumConstants) {
  const hldb::CaseStmt *const cs = getCase();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_EQ(cs->getCaseItems()->size(), 6u);
  const std::vector<std::string_view> names = {"L128", "L224", "L256", "L384", "L512"};
  for (size_t i = 0; i < names.size(); ++i) {
    const hldb::CaseItem *const ci = cs->getCaseItems()->at(i);
    ASSERT_NE(ci, nullptr);
    ASSERT_NE(ci->getExprs(), nullptr);
    ASSERT_EQ(ci->getExprs()->size(), 1u);
    const hldb::RefObj *const e = any_cast<hldb::RefObj>(ci->getExprs()->at(0));
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->getName(), names[i]);
    ASSERT_NE(e->getActual(), nullptr) << names[i];
    EXPECT_EQ(e->getActual()->getAnyType(), hldb::AnyType::EnumConst) << "26.3: imported enum constant";
  }
  const hldb::CaseItem *const def = cs->getCaseItems()->at(5);
  ASSERT_NE(def, nullptr);
  EXPECT_TRUE((def->getExprs() == nullptr) || def->getExprs()->empty()) << "the last item is 'default'";
  const hldb::Assignment *const a = def->getStmt<hldb::Assignment>();
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const zero = a->getRhs<hldb::Constant>();
  ASSERT_NE(zero, nullptr);
  EXPECT_EQ(zero->getDecompile(), "'0");
}

TEST_F(KmacTest, SizeCastByImportedParameterIsSupported) {
  for (uint32_t line = 26; line <= 30; ++line) {
    EXPECT_EQ(findError(ErrorDefinition::HLDB_UNSUPPORTED_TYPESPEC, "KeccakCountW", line, 32), nullptr)
        << "6.24.1: a constant_primary casting_type (KeccakCountW') is a legal size cast (line " << line << ")";
  }
}

TEST_F(KmacTest, CaseItemBodiesAreUnarySizeCastsOfBitSelect) {
  const hldb::CaseStmt *const cs = getCase();
  ASSERT_NE(cs, nullptr);
  ASSERT_NE(cs->getCaseItems(), nullptr);
  ASSERT_EQ(cs->getCaseItems()->size(), 6u);
  const std::vector<std::string_view> names = {"L128", "L224", "L256", "L384", "L512"};
  for (size_t i = 0; i < names.size(); ++i) {
    const hldb::CaseItem *const ci = cs->getCaseItems()->at(i);
    ASSERT_NE(ci, nullptr);
    const hldb::Assignment *const a = ci->getStmt<hldb::Assignment>();
    ASSERT_NE(a, nullptr);
    EXPECT_TRUE(a->getBlocking());
    const hldb::RefObj *const lhs = a->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getName(), "block_addr_limit");
    const hldb::Operation *const cast = a->getRhs<hldb::Operation>();
    ASSERT_NE(cast, nullptr) << names[i];
    EXPECT_EQ(cast->getOpType(), vpiCastOp);
    ASSERT_NE(cast->getOperands(), nullptr);
    ASSERT_EQ(cast->getOperands()->size(), 1u) << "37.59 detail 3: a cast is a unary operation";
    const hldb::BitSelect *const bs = any_cast<hldb::BitSelect>(cast->getOperands()->at(0));
    ASSERT_NE(bs, nullptr) << "KeccakRate[" << names[i] << "]";
    const hldb::RefObj *const prefix = bs->getPrefix<hldb::RefObj>();
    ASSERT_NE(prefix, nullptr);
    EXPECT_EQ(prefix->getName(), "KeccakRate");
    ASSERT_NE(prefix->getActual(), nullptr);
    EXPECT_EQ(prefix->getActual()->getAnyType(), hldb::AnyType::Parameter);
    const hldb::RefObj *const idx = bs->getIndex<hldb::RefObj>();
    ASSERT_NE(idx, nullptr);
    EXPECT_EQ(idx->getName(), names[i]);
  }
}

// ===========================================================================
// kmac
// ===========================================================================

TEST_F(KmacTest, KmacParameterPortsAndPorts) {
  const hldb::Module *const m = getModuleDef("kmac");
  ASSERT_NE(m, nullptr);
  checkParamPorts(m);
  checkPorts(m);
}

TEST_F(KmacTest, KmacInstantiatesKmacCore) {
  const hldb::Module *const m = getModuleDef("kmac");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getRefInstances(), nullptr);
  ASSERT_EQ(m->getRefInstances()->size(), 1u);
  const hldb::RefInstance *const core = m->getRefInstances()->at(0);
  ASSERT_NE(core, nullptr);
  EXPECT_EQ(core->getName(), "core");
  ASSERT_NE(core->getTypespec(), nullptr);
  EXPECT_EQ(core->getTypespec()->getName(), "kmac_core");
}

TEST_F(KmacTest, WildcardConnectionCoversEveryPort) {
  const hldb::Module *const m = getModuleDef("kmac");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getRefInstances(), nullptr);
  ASSERT_EQ(m->getRefInstances()->size(), 1u);
  const hldb::RefInstance *const core = m->getRefInstances()->at(0);
  ASSERT_NE(core, nullptr);
  ASSERT_NE(core->getPorts(), nullptr);
  ASSERT_EQ(core->getPorts()->size(), 2u)
      << "23.3.2.4: .* is equivalent to an implicit .name connection for every port of kmac_core";
  const std::vector<std::string_view> names = {"kmac_en_i", "strength_i"};
  for (size_t i = 0; i < names.size(); ++i) {
    const hldb::Port *const p = any_cast<hldb::Port>(core->getPorts()->at(i));
    ASSERT_NE(p, nullptr);
    const hldb::RefObj *const lo = p->getLowConn<hldb::RefObj>();
    const hldb::RefObj *const hi = p->getHighConn<hldb::RefObj>();
    ASSERT_NE(lo, nullptr);
    ASSERT_NE(hi, nullptr);
    EXPECT_EQ(lo->getName(), names[i]);
    EXPECT_EQ(hi->getName(), names[i]);
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
