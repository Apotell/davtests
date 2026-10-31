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

// Validates the HLDB model built for tests/ParamArraySelect/dut.sv:
//
//   package otp_ctrl_pkg;
//     typedef struct packed {
//       logic [10:0] size;
//       logic [10:0] offset;
//     } part_info_t;
//     parameter int NumPart = 2;
//   endpackage // otp_ctrl_pkg
//
//   package otp_ctrl_part_pkg;
//     import otp_ctrl_pkg::*;
//     localparam part_info_t PartInfo [NumPart] = '{
//       // CREATOR_SW_CFG
//       '{
//         size:       768,
//         offset:     100
//       },
//       // LIFE_CYCLE
//       '{
//         size:       56,
//         offset:     200
//       }
//     };
//     typedef enum {
//       CreatorSwCfgIdx,
//       LifeCycleIdx
//     } part_idx_e;
//   endpackage
//
//   module otp_ctrl_lci
//     import otp_ctrl_pkg::*;
//   #(
//     // Lifecycle partition information
//     parameter part_info_t Info = part_info_t'(0)
//     )  ();
//    localparam int NumLcOtpWords = Info.size >> 1;
//    logic [NumLcOtpWords-1:0] cnt_d;
//     if (NumLcOtpWords != 28) begin
//        BAD bad();
//     end
//   endmodule // otp_ctrl_lci
//
//   module top ();
//     import otp_ctrl_part_pkg::*;
//     otp_ctrl_lci #(
//       .Info(PartInfo[LifeCycleIdx])
//     ) u_otp_ctrl_lci ();
//     parameter int OtpByteAddrWidth = 7;
//     logic [PartInfo[LifeCycleIdx].size:0] aaa;
//    for (genvar k = 0; k < NumPart; k++) begin : gen_part_sel
//       localparam logic [OtpByteAddrWidth:0] PartEnd = (OtpByteAddrWidth+1)'(PartInfo[k].offset) +
//                                                       (OtpByteAddrWidth+1)'(PartInfo[k].size);
//       localparam logic [OtpByteAddrWidth-1:0] DigestOffset = OtpByteAddrWidth'(PartEnd -
//                                                                                4);
//     end
//   endmodule
//
// The point of the fixture is parameter values selected out of an unpacked
// array parameter of structures from a package: a module parameter
// overridden (IEEE 1800-2023 23.10.2.2) with one element, a variable range
// taken from a member of an element, and local parameters of a loop generate
// block computed from members with size casts (6.24.1). The design checks
// itself: the module BAD, declared nowhere, is instantiated only if
// NumLcOtpWords is not 28. The regression this file exists to catch is HLC
// mis-selecting an element or member of PartInfo during elaboration.
//
// What is checked, and why:
//   Packages
//     - otp_ctrl_pkg declares part_info_t, a packed StructTypespec (7.2.1)
//       with exactly 2 members, size then offset, each logic [10:0], and the
//       local int parameter NumPart = 2
//     - otp_ctrl_part_pkg records the wildcard import of otp_ctrl_pkg;
//       PartInfo is a local parameter, an unpacked array (7.4.2) of
//       part_info_t whose unreduced value is an array pattern (10.9.1) of 2
//       structure patterns (10.9.2), each with the member-keyed items size
//       and offset: (768, 100) and (56, 200); part_idx_e is an enum with the
//       names CreatorSwCfgIdx and LifeCycleIdx
//   Module otp_ctrl_lci
//     - Info is overridable (6.20.1) and typed part_info_t through the
//       header import (26.4); NumLcOtpWords is a local int
//     - exactly 1 generate construct, an if-generate (27.5) whose condition
//       is Operation vpiNeqOp over RefObj 'NumLcOtpWords' and Constant "28"
//   Module top
//     - exactly 1 instance, u_otp_ctrl_lci, overriding Info by name with a
//       bit-select whose prefix is bound to PartInfo and whose index is bound
//       to the EnumConst LifeCycleIdx, both through the wildcard import
//     - OtpByteAddrWidth is a parameter of the module body; the module has no
//       parameter port list, so it is not local (6.20.1)
//     - exactly 1 generate construct, the loop generate gen_part_sel (27.4)
//   Elaboration (all values follow from 6.20.2: a parameter value is a
//   constant expression)
//     - top is the only top-level instance (23.3.1); u_otp_ctrl_lci gets
//       Info = PartInfo[1] = '{size: 56, offset: 200}, so NumLcOtpWords =
//       56 >> 1 = 28. The condition 28 != 28 is false, so no generate block
//       is instantiated in u_otp_ctrl_lci and BAD is never instantiated
//     - aaa's range is [PartInfo[1].size:0] = [56:0]
//     - gen_part_sel is instantiated twice (NumPart = 2), as gen_part_sel[0]
//       and gen_part_sel[1]. A size cast returns "the value that a packed
//       array type with a single [n-1:0] index would hold after being
//       assigned the expression" (6.24.1), so with n = 8:
//         k = 0: PartEnd = 8'(100) + 8'(768) = 100 + 0 = 100;
//                DigestOffset = 7'(100 - 4) = 96
//         k = 1: PartEnd = 8'(200) + 8'(56) = 256, which is 0 in the
//                8-bit PartEnd; DigestOffset = 7'(0 - 4): the subtraction is
//                32 bits wide, 0 - 4 = 32'hFFFFFFFC, whose low 7 bits are 124
//   Diagnostics
//     - there is no syntax error: zero syntax and zero fatal diagnostics
//     - 'OtpByteAddrWidth'(...)' is a size cast whose size is a parameter.
//       6.24.1 gives "P'(x - 2)" as an example, and casting_type includes
//       constant_primary (A.2.2.1), so HLC must not report OtpByteAddrWidth
//       as an unsupported type
//
// Reduction and elaboration: every value above is checked only when the
// design is elaborated, on Design::getTopModules(); the unreduced forms are
// asserted on the definitions in every run.
//
// KNOWN COMPILER BUG (null port missing), not a defect in this test: HLC
// models a '()' port list with no port at all, although 37.14 detail 10
// names "module M();" as declaring a null port.
// EmptyPortListsDeclareOneNullPort is expected to fail until HLC is fixed;
// it is intentionally not skipped or relaxed.
//
// KNOWN COMPILER BUG (parameter size cast read as a type), not a defect in
// this test: HLC reports [ERR:DL2510] Unsupported typespec "OtpByteAddrWidth"
// for 'OtpByteAddrWidth'(PartEnd - 4)', treating the parameter as a type
// name, and then two [ERR:DL2516] "'vpiStmt' property value has invalid
// objects" errors on the gen_part_sel block that holds the cast.
// ParameterCastSizeIsNotReportedAsAType is expected to fail until HLC is
// fixed; it is intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - The total error count. BAD is declared nowhere, and its instance is in
//     a generate block that is not selected (27.5). The standard does not say
//     whether naming an undefined module there is an error, so the count is
//     not asserted; the size-cast defect is pinned by its own error instead.
//   - How HLC records a parameter override of an unelaborated instance is a
//     model convention; it is read from the instance's module typespec.
//   - The name HLC gives a parameterized module definition: modules are
//     looked up by their definition name.
//   - How a RefTypespec refers to a typedef: it may resolve to the
//     TypedefTypespec or to the typespec it aliases. Both are that type
//     (6.18), so either is accepted.
//   - The width of cnt_d (28 bits). It follows from NumLcOtpWords, which is
//     checked directly.
//   - Whether other packages (for example a built-in one) also appear in
//     Design::getAllPackages() is a tool convention; packages are looked up
//     by name.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.
//   - The comments in the fixture are not design objects (5.4).

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/gen_for.h>
#include <hldb/gen_if.h>
#include <hldb/gen_scope.h>
#include <hldb/gen_scope_array.h>
#include <hldb/import_typespec.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/tagged_pattern.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace hlc {

class ParamArraySelectTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamArraySelect.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg(std::string_view name) {
    return hldb::findByName<hldb::Package>(name, m_design->getAllPackages());
  }

  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Typedef *getTypedef(std::string_view pkg, std::string_view name) {
    const hldb::Package *const p = getPkg(pkg);
    if (p == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>(name, p->getTypedefs());
  }

  static const hldb::Parameter *getParam(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, scope->getParameters());
  }

  // The ParamAssign in 'scope' whose LHS names 'name'.
  static const hldb::ParamAssign *getParamAssign(const hldb::Scope *scope, std::string_view name) {
    if (scope == nullptr || scope->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *scope->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == name)) return pa;
      const hldb::Parameter *const p = pa->getLhs<hldb::Parameter>();
      if ((p != nullptr) && (p->getName() == name)) return pa;
    }
    return nullptr;
  }

  // Whether 'rts' names the type of 'td' (6.18).
  static bool namesTypedef(const hldb::RefTypespec *rts, const hldb::Typedef *td) {
    if (rts == nullptr || td == nullptr || td->getAlias() == nullptr) return false;
    const hldb::Typespec *const actual = rts->getActual();
    if (actual == nullptr) return false;
    const hldb::TypedefTypespec *const viaTypedef = any_cast<hldb::TypedefTypespec>(actual);
    return ((viaTypedef != nullptr) && (viaTypedef->getTypedef() == td)) || (actual == td->getAlias()->getActual());
  }

  // The override expression for 'param' that the unelaborated instance
  // 'inst' writes.
  static const hldb::Any *getOverride(const hldb::RefInstance *inst, std::string_view param) {
    if (inst == nullptr || inst->getTypespec() == nullptr) return nullptr;
    const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
    if (mt == nullptr || mt->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *mt->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == param)) return pa->getRhs();
    }
    return nullptr;
  }

  // Integer value of a Constant, from its decompiled text or from a sized or
  // based literal. The radix HLC picks for a folded value is a tool
  // convention, so every integer form is accepted.
  static bool getIntValue(const hldb::Constant *c, uint64_t *value) {
    std::string text;
    for (char ch : c->getDecompile()) {
      if ((ch != '_') && (ch != ' ')) text.push_back(ch);
    }
    int base = 10;
    const std::string::size_type tick = text.find('\'');
    if (tick != std::string::npos) {
      std::string::size_type pos = tick + 1;
      if ((pos < text.size()) && ((text[pos] == 's') || (text[pos] == 'S'))) ++pos;
      if (pos >= text.size()) return false;
      switch (text[pos]) {
        case 'h':
        case 'H': base = 16; break;
        case 'd':
        case 'D': base = 10; break;
        case 'o':
        case 'O': base = 8; break;
        case 'b':
        case 'B': base = 2; break;
        default: return false;
      }
      text = text.substr(pos + 1);
    }
    if (text.empty()) return false;
    const char *const last = text.data() + text.size();
    const std::from_chars_result res = std::from_chars(text.data(), last, *value, base);
    return (res.ec == std::errc()) && (res.ptr == last);
  }

  // Verifies the reduced value of parameter 'name' in the elaborated scope
  // 'scope' is 'expected'.
  static void ExpectElabParam(const hldb::Scope *scope, std::string_view name, uint64_t expected,
                              std::string_view why) {
    const hldb::ParamAssign *const pa = getParamAssign(scope, name);
    ASSERT_NE(pa, nullptr) << "no ParamAssign for '" << name << "' in the elaborated scope";
    const hldb::Constant *const c = pa->getRhs<hldb::Constant>();
    ASSERT_NE(c, nullptr) << "6.20.2: '" << name << "' should be reduced to a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(c, &v)) << "unparsable constant '" << c->getDecompile() << "'";
    EXPECT_EQ(v, expected) << why;
  }

  static const hldb::Module *getElabTop() {
    return hldb::findByDefName<hldb::Module>("top", m_design->getTopModules());
  }
};

// ---------------------------------------------------------------------------
// Packages
// ---------------------------------------------------------------------------

TEST_F(ParamArraySelectTest, PartInfoTHasMembersSizeThenOffset) {
  const hldb::Typedef *const td = getTypedef("otp_ctrl_pkg", "part_info_t");
  ASSERT_NE(td, nullptr) << "typedef 'part_info_t' not found in otp_ctrl_pkg";
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr) << "6.18: 'part_info_t' names a structure type";
  ASSERT_NE(st->getStruct(), nullptr);
  EXPECT_TRUE(st->getStruct()->getPacked()) << "7.2.1: declared 'struct packed'";
  ASSERT_NE(st->getStruct()->getMembers(), nullptr);
  ASSERT_EQ(st->getStruct()->getMembers()->size(), 2u);
  EXPECT_EQ(st->getStruct()->getMembers()->at(0)->getName(), "size");
  EXPECT_EQ(st->getStruct()->getMembers()->at(1)->getName(), "offset");
}

TEST_F(ParamArraySelectTest, NumPartIsLocalTwo) {
  const hldb::Package *const pkg = getPkg("otp_ctrl_pkg");
  ASSERT_NE(pkg, nullptr);
  const hldb::Parameter *const p = getParam(pkg, "NumPart");
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(p->getLocalParam()) << "6.20.4: in a package, 'parameter' is a synonym for 'localparam'";
  const hldb::ParamAssign *const pa = getParamAssign(pkg, "NumPart");
  ASSERT_NE(pa, nullptr);
  ASSERT_NE(pa->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(pa->getRhs<hldb::Constant>()->getDecompile(), "2");
}

TEST_F(ParamArraySelectTest, PartInfoPatternListsTwoKeyedStructures) {
  const hldb::Package *const pkg = getPkg("otp_ctrl_part_pkg");
  ASSERT_NE(pkg, nullptr);
  const hldb::Parameter *const p = getParam(pkg, "PartInfo");
  ASSERT_NE(p, nullptr) << "parameter 'PartInfo' not found";
  EXPECT_TRUE(p->getLocalParam()) << "declared 'localparam'";
  const hldb::ParamAssign *const pa = getParamAssign(pkg, "PartInfo");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const pattern = pa->getRhs<hldb::Operation>();
  ASSERT_NE(pattern, nullptr) << "unreduced, the value is an Operation";
  EXPECT_EQ(pattern->getOpType(), vpiAssignmentPatternOp) << "10.9.1: an array assignment pattern";
  ASSERT_NE(pattern->getOperands(), nullptr);
  ASSERT_EQ(pattern->getOperands()->size(), 2u) << "10.9.1: one item per element of the NumPart = 2 array";
  const char *const values[2][2] = {{"768", "100"}, {"56", "200"}};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::Operation *const item = any_cast<hldb::Operation>(pattern->getOperands()->at(i));
    ASSERT_NE(item, nullptr) << "item " << i;
    EXPECT_EQ(item->getOpType(), vpiAssignmentPatternOp) << "10.9.2: item " << i << " is a structure pattern";
    ASSERT_NE(item->getOperands(), nullptr);
    ASSERT_EQ(item->getOperands()->size(), 2u) << "item " << i << ": size, then offset";
    for (size_t j = 0; j < 2; ++j) {
      const hldb::TaggedPattern *const keyed = any_cast<hldb::TaggedPattern>(item->getOperands()->at(j));
      ASSERT_NE(keyed, nullptr) << "item " << i << ", value " << j << " is member-keyed";
      ASSERT_NE(keyed->getPattern<hldb::Constant>(), nullptr);
      EXPECT_EQ(keyed->getPattern<hldb::Constant>()->getDecompile(), values[i][j]) << "item " << i << ", value " << j;
    }
  }
}

// ---------------------------------------------------------------------------
// module otp_ctrl_lci
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (null port
// missing) in the file header.
TEST_F(ParamArraySelectTest, EmptyPortListsDeclareOneNullPort) {
  for (std::string_view name : {"otp_ctrl_lci", "top"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << "module '" << name << "' not found";
    ASSERT_NE(m->getPorts(), nullptr) << "37.14 detail 10: '()' of " << name << " declares a null port";
    ASSERT_EQ(m->getPorts()->size(), 1u) << "37.14 detail 10: '()' declares exactly one null port";
    EXPECT_EQ(m->getPorts()->at(0)->getName(), "") << "37.14 detail 8: a null port has no name";
    EXPECT_EQ(m->getPorts()->at(0)->getLowConn(), nullptr) << "37.14 detail 10: no low connection";
  }
}

TEST_F(ParamArraySelectTest, LciHasOverridableInfoAndLocalNumLcOtpWords) {
  const hldb::Module *const m = getModule("otp_ctrl_lci");
  ASSERT_NE(m, nullptr) << "module 'otp_ctrl_lci' not found";
  const hldb::Parameter *const info = getParam(m, "Info");
  ASSERT_NE(info, nullptr);
  EXPECT_FALSE(info->getLocalParam()) << "6.20.1: a parameter in the parameter port list can be overridden";
  EXPECT_TRUE(namesTypedef(info->getTypespec(), getTypedef("otp_ctrl_pkg", "part_info_t")))
      << "26.4: the header import makes part_info_t visible to the parameter port list";
  const hldb::Parameter *const words = getParam(m, "NumLcOtpWords");
  ASSERT_NE(words, nullptr);
  EXPECT_TRUE(words->getLocalParam()) << "declared 'localparam'";
}

TEST_F(ParamArraySelectTest, LciIfGenerateTestsNumLcOtpWordsAgainst28) {
  const hldb::Module *const m = getModule("otp_ctrl_lci");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getGenStmts(), nullptr);
  ASSERT_EQ(m->getGenStmts()->size(), 1u) << "the module body holds one generate construct";
  const hldb::GenIf *const genIf = any_cast<hldb::GenIf>(m->getGenStmts()->at(0));
  ASSERT_NE(genIf, nullptr) << "27.5: 'if (NumLcOtpWords != 28) begin ... end' is an if-generate";
  const hldb::Operation *const neq = genIf->getCondition<hldb::Operation>();
  ASSERT_NE(neq, nullptr);
  EXPECT_EQ(neq->getOpType(), vpiNeqOp);
  ASSERT_NE(neq->getOperands(), nullptr);
  ASSERT_EQ(neq->getOperands()->size(), 2u);
  const hldb::RefObj *const words = any_cast<hldb::RefObj>(neq->getOperands()->at(0));
  ASSERT_NE(words, nullptr);
  EXPECT_EQ(words->getActual(), getParam(m, "NumLcOtpWords"));
  const hldb::Constant *const c = any_cast<hldb::Constant>(neq->getOperands()->at(1));
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getDecompile(), "28");
}

// ---------------------------------------------------------------------------
// module top
// ---------------------------------------------------------------------------

TEST_F(ParamArraySelectTest, TopOverridesInfoWithPartInfoElement) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  ASSERT_EQ(top->getRefInstances()->size(), 1u);
  const hldb::RefInstance *const inst = top->getRefInstances()->at(0);
  ASSERT_NE(inst, nullptr);
  EXPECT_EQ(inst->getName(), "u_otp_ctrl_lci");
  const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(getOverride(inst, "Info"));
  ASSERT_NE(sel, nullptr) << "23.10.2.2: '.Info(PartInfo[LifeCycleIdx])' overrides Info with an element select";
  const hldb::RefObj *const array = sel->getPrefix<hldb::RefObj>();
  ASSERT_NE(array, nullptr);
  EXPECT_EQ(array->getActual(), getParam(getPkg("otp_ctrl_part_pkg"), "PartInfo"))
      << "26.3: PartInfo is imported from otp_ctrl_part_pkg";
  const hldb::RefObj *const index = sel->getIndex<hldb::RefObj>();
  ASSERT_NE(index, nullptr);
  EXPECT_EQ(index->getName(), "LifeCycleIdx");
  ASSERT_NE(index->getActual<hldb::EnumConst>(), nullptr) << "26.3: LifeCycleIdx is an enum name of otp_ctrl_part_pkg";
}

TEST_F(ParamArraySelectTest, TopOtpByteAddrWidthIsOverridableSeven) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  const hldb::Parameter *const p = getParam(top, "OtpByteAddrWidth");
  ASSERT_NE(p, nullptr) << "parameter 'OtpByteAddrWidth' not found";
  EXPECT_FALSE(p->getLocalParam()) << "6.20.1: top has no parameter port list, so a body 'parameter' is overridable";
  const hldb::ParamAssign *const pa = getParamAssign(top, "OtpByteAddrWidth");
  ASSERT_NE(pa, nullptr);
  ASSERT_NE(pa->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(pa->getRhs<hldb::Constant>()->getDecompile(), "7");
}

TEST_F(ParamArraySelectTest, TopHasOneLoopGenerateGenPartSel) {
  const hldb::Module *const top = getModule("top");
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getGenStmts(), nullptr);
  ASSERT_EQ(top->getGenStmts()->size(), 1u) << "the module body holds one generate construct";
  EXPECT_NE(any_cast<hldb::GenFor>(top->getGenStmts()->at(0)), nullptr)
      << "27.4: 'for (genvar k = 0; ...)' is a loop generate construct";
}

// ---------------------------------------------------------------------------
// Elaboration
// ---------------------------------------------------------------------------

TEST_F(ParamArraySelectTest, ElaboratedLciGetsNumLcOtpWords28AndNoBadBlock) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: otp_ctrl_lci is instantiated, top is not";
    const hldb::Module *const top = getElabTop();
    ASSERT_NE(top, nullptr);
    const hldb::Module *const lci = hldb::findByName<hldb::Module>("u_otp_ctrl_lci", top->getModules());
    ASSERT_NE(lci, nullptr) << "the instance u_otp_ctrl_lci is elaborated";
    ExpectElabParam(lci, "NumLcOtpWords", 28, "Info = PartInfo[LifeCycleIdx] = '{56, 200}, and 56 >> 1 = 28");
    size_t scopes = 0;
    if (lci->getGenScopeArrays() != nullptr) {
      for (const hldb::GenScopeArray *const gsa : *lci->getGenScopeArrays()) {
        if (gsa->getGenScopes() != nullptr) scopes += gsa->getGenScopes()->size();
      }
    }
    EXPECT_EQ(scopes, 0u) << "27.5: 28 != 28 is false, so the block instantiating BAD is not selected";
  }
}

TEST_F(ParamArraySelectTest, ElaboratedAaaIs56DownTo0) {
  if (m_design->getElaborated()) {
    const hldb::Module *const top = getElabTop();
    ASSERT_NE(top, nullptr);
    const hldb::Variable *const aaa = hldb::findByName<hldb::Variable>("aaa", top->getVariables());
    ASSERT_NE(aaa, nullptr) << "variable 'aaa' not found in the elaborated top";
    ASSERT_NE(aaa->getTypespec(), nullptr);
    const hldb::LogicTypespec *const lt = aaa->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr);
    ASSERT_NE(lt->getRanges(), nullptr);
    ASSERT_EQ(lt->getRanges()->size(), 1u);
    const hldb::Constant *const left = lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
    ASSERT_NE(left, nullptr) << "a range bound is a constant expression, reduced on an elaborated design";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(left, &v));
    EXPECT_EQ(v, 56u) << "PartInfo[LifeCycleIdx].size is 56";
  }
}

TEST_F(ParamArraySelectTest, ElaboratedGenPartSelComputesPartEndAndDigestOffset) {
  if (m_design->getElaborated()) {
    const hldb::Module *const top = getElabTop();
    ASSERT_NE(top, nullptr);
    const hldb::GenScopeArray *const gen =
        hldb::findByName<hldb::GenScopeArray>("gen_part_sel", top->getGenScopeArrays());
    ASSERT_NE(gen, nullptr) << "27.4: the loop generate gen_part_sel is instantiated";
    ASSERT_NE(gen->getGenScopes(), nullptr);
    ASSERT_EQ(gen->getGenScopes()->size(), 2u) << "27.4: k runs over 0 and 1 (NumPart = 2)";
    const uint64_t partEnd[] = {100, 0};
    const uint64_t digestOffset[] = {96, 124};
    for (size_t k = 0; k < 2; ++k) {
      const hldb::GenScope *const scope = gen->getGenScopes()->at(k);
      ASSERT_NE(scope, nullptr);
      ExpectElabParam(scope, "PartEnd", partEnd[k],
                      "6.24.1: 8'(offset) + 8'(size), held in 8 bits, k = " + std::to_string(k));
      ExpectElabParam(scope, "DigestOffset", digestOffset[k], "6.24.1: 7'(PartEnd - 4), k = " + std::to_string(k));
    }
  }
}

TEST_F(ParamArraySelectTest, NoSyntaxOrFatalDiagnostics) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0) << "the file is syntactically well formed";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
