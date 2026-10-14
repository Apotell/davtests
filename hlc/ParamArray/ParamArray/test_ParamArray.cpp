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

// Validates the HLDB model built for tests/ParamArray/dut.sv:
//
//   package otp_ctrl_pkg;
//     typedef struct packed {
//       logic [10:0] size;
//     } part_info_t;
//     parameter int NumPart = 7;
//   endpackage // otp_ctrl_pkg
//
//   package otp_ctrl_part_pkg;
//     import otp_ctrl_pkg::*;
//     localparam part_info_t PartInfo [NumPart] = '{
//       // CREATOR_SW_CFG
//       '{
//         size:       768
//       },
//       // LIFE_CYCLE
//       '{
//         size:       56
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
//   endmodule // otp_ctrl_lci
//
//   module top ();
//     import otp_ctrl_part_pkg::*;
//     otp_ctrl_lci #(
//       .Info(PartInfo[LifeCycleIdx])
//     ) u_otp_ctrl_lci ();
//   endmodule
//
// The point of the fixture is a module parameter of structure type that is
// overridden (IEEE 1800-2023 23.10.2.2) with one element of an unpacked
// array parameter of a package, selected by an enum name, and a local
// parameter computed from a member of that structure parameter. The
// regression this file exists to catch is HLC losing the override or the
// bindings in it. The fixture is also erroneous: PartInfo is declared with
// NumPart = 7 elements, but its assignment pattern lists 2. An array
// assignment pattern's "expressions shall match element for element"
// (10.9.1), so that is an error.
//
// What is checked, and why:
//   Package otp_ctrl_pkg
//     - exactly 1 Typedef, 'part_info_t', a packed StructTypespec (7.2.1)
//       with exactly 1 member, size, a LogicTypespec with the packed range
//       [10:0]
//     - exactly 1 parameter, NumPart, a local (6.20.4) signed int with the
//       ParamAssign RHS Constant "7"
//   Package otp_ctrl_part_pkg
//     - records exactly 1 import, the wildcard import of otp_ctrl_pkg
//     - exactly 1 parameter, PartInfo, a local parameter whose type is an
//       unpacked array (7.4.2) whose element type is part_info_t, resolved
//       through the import (26.3)
//     - unreduced, PartInfo's value is Operation vpiAssignmentPatternOp
//       (10.9.1) with exactly 2 items, each a structure pattern (10.9.2) with
//       the single member-keyed item 'size' of value Constant "768" / "56"
//     - exactly 1 Typedef, 'part_idx_e', an enum (6.19) with exactly 2 names
//       in source order, CreatorSwCfgIdx and LifeCycleIdx
//   Module otp_ctrl_lci
//     - '()' declares one null port (37.14 detail 10, "module M();")
//     - exactly 2 parameters. Info is declared in the parameter port list, so
//       it is not local (6.20.1), and is typed by part_info_t through the
//       header import (26.4); its default is Operation vpiCastOp (6.24.1) to
//       part_info_t of the Constant "0". NumLcOtpWords is a local signed int
//       whose value is Operation vpiRShiftOp over the RefObj path Info.size --
//       'Info' bound to the Parameter, 'size' bound to the member -- and
//       Constant "1"
//     - exactly 1 variable, cnt_d, of logic with the single packed range
//       [NumLcOtpWords-1:0]
//   Module top
//     - '()' declares one null port (37.14 detail 10)
//     - records exactly 1 import, the wildcard import of otp_ctrl_part_pkg
//     - exactly 1 instance, u_otp_ctrl_lci, of otp_ctrl_lci, which overrides
//       Info by name (23.10.2.2) with a bit-select whose prefix is bound to
//       otp_ctrl_part_pkg's PartInfo and whose index is bound to the EnumConst
//       LifeCycleIdx, both through top's wildcard import
//   Elaboration (23.3.1)
//     - otp_ctrl_lci is instantiated, so top is the only top-level instance,
//       and it holds the instance u_otp_ctrl_lci
//     - the element-count mismatch of PartInfo can only be measured once
//       NumPart is evaluated, so an elaborated design holds at least 1 error
//   Diagnostics
//     - there is no syntax error: zero syntax and zero fatal diagnostics
//
// Reduction and elaboration: PartInfo itself is erroneous, so no reduced
// value derived from it (Info, NumLcOtpWords, the width of cnt_d) is
// asserted; only the instance tree and the error count are checked under
// getElaborated().
//
// KNOWN COMPILER BUG (null port missing), not a defect in this test: HLC
// models a '()' port list with no port at all, although 37.14 detail 10
// names "module M();" as declaring a null port. EmptyPortListsDeclareOneNullPort
// is expected to fail until HLC is fixed; it is intentionally not skipped or
// relaxed.
//
// What is NOT checked, and why:
//   - Which diagnostic HLC gives for the element-count mismatch. The standard
//     makes it an error but names no specific diagnostic and no symbol to key
//     it on, so the error count is checked.
//   - The element count of PartInfo's '[NumPart]' dimension: it depends on
//     elaboration, and how HLC records a '[size]' dimension is not this
//     fixture's subject.
//   - How HLC records a parameter override of an unelaborated instance is a
//     model convention; it is read from the instance's module typespec.
//   - The name HLC gives a parameterized module definition: modules are
//     looked up by their definition name.
//   - How a RefTypespec refers to a typedef: it may resolve to the
//     TypedefTypespec or to the typespec it aliases. Both are that type
//     (6.18), so either is accepted. How HLC labels the key of a pattern
//     item is a model convention.
//   - Whether other packages (for example a built-in one) also appear in
//     Design::getAllPackages() is a tool convention; packages are looked up
//     by name.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.
//   - The comments in the fixture are not design objects (5.4).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
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

#include <string_view>
#include <vector>

namespace hlc {

class ParamArrayTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ParamArray.hlc"}); }
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

  static const hldb::TypespecMember *getSizeMember() {
    const hldb::Typedef *const td = getTypedef("otp_ctrl_pkg", "part_info_t");
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
    if (st == nullptr || st->getStruct() == nullptr) return nullptr;
    return hldb::findByName<hldb::TypespecMember>("size", st->getStruct()->getMembers());
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

  // Verifies 'scope' records exactly one import, the wildcard import of
  // 'pkg'.
  static void ExpectOnlyWildcardImport(const hldb::Instance *scope, std::string_view pkg) {
    ASSERT_NE(scope, nullptr);
    std::vector<const hldb::ImportTypespec *> imports;
    if (scope->getTypespecs() != nullptr) {
      for (const hldb::Typespec *const ts : *scope->getTypespecs()) {
        if (const hldb::ImportTypespec *const it = any_cast<hldb::ImportTypespec>(ts)) imports.emplace_back(it);
      }
    }
    ASSERT_EQ(imports.size(), 1u) << "exactly one import is written";
    EXPECT_EQ(imports[0]->getName(), pkg) << "26.3: the import names " << pkg;
    ASSERT_NE(imports[0]->getItem(), nullptr);
    EXPECT_EQ(imports[0]->getItem()->getDecompile(), "*") << "26.3: a wildcard import";
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
};

// ---------------------------------------------------------------------------
// package otp_ctrl_pkg;
// ---------------------------------------------------------------------------

TEST_F(ParamArrayTest, PartInfoTIsPackedStructWithSize10To0) {
  const hldb::Typedef *const td = getTypedef("otp_ctrl_pkg", "part_info_t");
  ASSERT_NE(td, nullptr) << "typedef 'part_info_t' not found in otp_ctrl_pkg";
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::StructTypespec *const st = td->getAlias()->getActual<hldb::StructTypespec>();
  ASSERT_NE(st, nullptr) << "6.18: 'part_info_t' names a structure type";
  ASSERT_NE(st->getStruct(), nullptr);
  EXPECT_TRUE(st->getStruct()->getPacked()) << "7.2.1: declared 'struct packed'";
  ASSERT_NE(st->getStruct()->getMembers(), nullptr);
  ASSERT_EQ(st->getStruct()->getMembers()->size(), 1u);
  const hldb::TypespecMember *const size = getSizeMember();
  ASSERT_NE(size, nullptr);
  ASSERT_NE(size->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = size->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "'size' is declared 'logic [10:0]'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  const hldb::Constant *const left = lt->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "10");
}

TEST_F(ParamArrayTest, NumPartIsLocalIntSeven) {
  const hldb::Package *const pkg = getPkg("otp_ctrl_pkg");
  ASSERT_NE(pkg, nullptr);
  const hldb::Parameter *const p = getParam(pkg, "NumPart");
  ASSERT_NE(p, nullptr) << "parameter 'NumPart' not found";
  EXPECT_TRUE(p->getLocalParam()) << "6.20.4: in a package, 'parameter' is a synonym for 'localparam'";
  ASSERT_NE(p->getTypespec(), nullptr);
  EXPECT_NE(p->getTypespec()->getActual<hldb::IntTypespec>(), nullptr) << "'NumPart' is declared 'int'";
  const hldb::ParamAssign *const pa = getParamAssign(pkg, "NumPart");
  ASSERT_NE(pa, nullptr);
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "7");
}

// ---------------------------------------------------------------------------
// package otp_ctrl_part_pkg;
// ---------------------------------------------------------------------------

TEST_F(ParamArrayTest, PartPkgImportsAllOfOtpCtrlPkg) {
  ExpectOnlyWildcardImport(getPkg("otp_ctrl_part_pkg"), "otp_ctrl_pkg");
}

TEST_F(ParamArrayTest, PartInfoIsLocalUnpackedArrayOfPartInfoT) {
  const hldb::Parameter *const p = getParam(getPkg("otp_ctrl_part_pkg"), "PartInfo");
  ASSERT_NE(p, nullptr) << "parameter 'PartInfo' not found";
  EXPECT_TRUE(p->getLocalParam()) << "declared 'localparam'";
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = p->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr) << "'[NumPart]' after the name declares an unpacked array";
  EXPECT_FALSE(at->getPacked()) << "7.4.2: a dimension after the name is unpacked";
  EXPECT_TRUE(namesTypedef(at->getElemTypespec(), getTypedef("otp_ctrl_pkg", "part_info_t")))
      << "26.3: the element type part_info_t is imported from otp_ctrl_pkg";
}

TEST_F(ParamArrayTest, PartInfoPatternListsTwoStructures) {
  const hldb::ParamAssign *const pa = getParamAssign(getPkg("otp_ctrl_part_pkg"), "PartInfo");
  ASSERT_NE(pa, nullptr) << "no ParamAssign for 'PartInfo'";
  const hldb::Operation *const pattern = pa->getRhs<hldb::Operation>();
  ASSERT_NE(pattern, nullptr) << "unreduced, the value is an Operation";
  EXPECT_EQ(pattern->getOpType(), vpiAssignmentPatternOp) << "10.9.1: an array assignment pattern";
  ASSERT_NE(pattern->getOperands(), nullptr);
  ASSERT_EQ(pattern->getOperands()->size(), 2u) << "the source writes two structure patterns";
  const char *const sizes[] = {"768", "56"};
  for (size_t i = 0; i < 2; ++i) {
    const hldb::Operation *const item = any_cast<hldb::Operation>(pattern->getOperands()->at(i));
    ASSERT_NE(item, nullptr) << "item " << i;
    EXPECT_EQ(item->getOpType(), vpiAssignmentPatternOp) << "10.9.2: item " << i << " is a structure pattern";
    ASSERT_NE(item->getOperands(), nullptr);
    ASSERT_EQ(item->getOperands()->size(), 1u) << "item " << i << " sets only 'size'";
    const hldb::TaggedPattern *const keyed = any_cast<hldb::TaggedPattern>(item->getOperands()->at(0));
    ASSERT_NE(keyed, nullptr) << "item " << i << " is a 'size: value' item";
    const hldb::Constant *const value = keyed->getPattern<hldb::Constant>();
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->getDecompile(), sizes[i]) << "item " << i;
  }
}

TEST_F(ParamArrayTest, PartIdxEHasTwoNames) {
  const hldb::Typedef *const td = getTypedef("otp_ctrl_part_pkg", "part_idx_e");
  ASSERT_NE(td, nullptr) << "typedef 'part_idx_e' not found";
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::EnumTypespec *const et = td->getAlias()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(et, nullptr) << "6.19: 'part_idx_e' names an enumerated type";
  ASSERT_NE(et->getEnum(), nullptr);
  ASSERT_NE(et->getEnum()->getEnumConsts(), nullptr);
  ASSERT_EQ(et->getEnum()->getEnumConsts()->size(), 2u);
  EXPECT_EQ(et->getEnum()->getEnumConsts()->at(0)->getName(), "CreatorSwCfgIdx");
  EXPECT_EQ(et->getEnum()->getEnumConsts()->at(1)->getName(), "LifeCycleIdx");
}

// ---------------------------------------------------------------------------
// module otp_ctrl_lci ... ;  module top ();
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (null port
// missing) in the file header.
TEST_F(ParamArrayTest, EmptyPortListsDeclareOneNullPort) {
  for (std::string_view name : {"otp_ctrl_lci", "top"}) {
    const hldb::Module *const m = getModule(name);
    ASSERT_NE(m, nullptr) << "module '" << name << "' not found";
    ASSERT_NE(m->getPorts(), nullptr) << "37.14 detail 10: '()' of " << name << " declares a null port";
    ASSERT_EQ(m->getPorts()->size(), 1u) << "37.14 detail 10: '()' declares exactly one null port";
    EXPECT_EQ(m->getPorts()->at(0)->getName(), "") << "37.14 detail 8: a null port has no name";
    EXPECT_EQ(m->getPorts()->at(0)->getLowConn(), nullptr) << "37.14 detail 10: no low connection";
  }
}

TEST_F(ParamArrayTest, InfoIsOverridablePartInfoTDefaultingToCastZero) {
  const hldb::Module *const m = getModule("otp_ctrl_lci");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 2u) << "'Info' and 'NumLcOtpWords'";
  const hldb::Parameter *const info = getParam(m, "Info");
  ASSERT_NE(info, nullptr) << "parameter 'Info' not found";
  EXPECT_FALSE(info->getLocalParam()) << "6.20.1: a parameter in the parameter port list can be overridden";
  EXPECT_TRUE(namesTypedef(info->getTypespec(), getTypedef("otp_ctrl_pkg", "part_info_t")))
      << "26.4: the header import makes part_info_t visible to the parameter port list";
  const hldb::ParamAssign *const pa = getParamAssign(m, "Info");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const cast = pa->getRhs<hldb::Operation>();
  ASSERT_NE(cast, nullptr) << "the default 'part_info_t'(0)' is an Operation";
  EXPECT_EQ(cast->getOpType(), vpiCastOp) << "6.24.1: a cast";
  EXPECT_TRUE(namesTypedef(cast->getTypespec(), getTypedef("otp_ctrl_pkg", "part_info_t")))
      << "6.24.1: the cast is to part_info_t";
  ASSERT_NE(cast->getOperands(), nullptr);
  ASSERT_EQ(cast->getOperands()->size(), 1u);
  const hldb::Constant *const zero = any_cast<hldb::Constant>(cast->getOperands()->at(0));
  ASSERT_NE(zero, nullptr);
  EXPECT_EQ(zero->getDecompile(), "0");
}

TEST_F(ParamArrayTest, NumLcOtpWordsShiftsInfoSizeRightByOne) {
  const hldb::Module *const m = getModule("otp_ctrl_lci");
  ASSERT_NE(m, nullptr);
  const hldb::Parameter *const p = getParam(m, "NumLcOtpWords");
  ASSERT_NE(p, nullptr) << "parameter 'NumLcOtpWords' not found";
  EXPECT_TRUE(p->getLocalParam()) << "declared 'localparam'";
  const hldb::ParamAssign *const pa = getParamAssign(m, "NumLcOtpWords");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *const shift = pa->getRhs<hldb::Operation>();
  ASSERT_NE(shift, nullptr) << "'Info.size >> 1' is an Operation";
  EXPECT_EQ(shift->getOpType(), vpiRShiftOp) << "11.4.10: '>>' is the logical right shift";
  ASSERT_NE(shift->getOperands(), nullptr);
  ASSERT_EQ(shift->getOperands()->size(), 2u);
  const hldb::RefObj *const path = any_cast<hldb::RefObj>(shift->getOperands()->at(0));
  ASSERT_NE(path, nullptr) << "'Info.size' should be a RefObj path";
  ASSERT_NE(path->getPathElems(), nullptr);
  ASSERT_EQ(path->getPathElems()->size(), 2u);
  const hldb::RefObj *const info = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
  ASSERT_NE(info, nullptr);
  EXPECT_EQ(info->getActual(), getParam(m, "Info")) << "'Info' is the module's parameter";
  const hldb::RefObj *const size = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
  ASSERT_NE(size, nullptr);
  ASSERT_NE(getSizeMember(), nullptr);
  EXPECT_EQ(size->getActual(), getSizeMember()) << "7.2: 'size' is the member of part_info_t";
  const hldb::Constant *const one = any_cast<hldb::Constant>(shift->getOperands()->at(1));
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->getDecompile(), "1");
}

TEST_F(ParamArrayTest, CntDIsLogicNumLcOtpWordsMinusOneDownTo0) {
  const hldb::Module *const m = getModule("otp_ctrl_lci");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getVariables(), nullptr);
  ASSERT_EQ(m->getVariables()->size(), 1u);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("cnt_d", m->getVariables());
  ASSERT_NE(v, nullptr);
  ASSERT_NE(v->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = v->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr);
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  const hldb::Operation *const left = lt->getRanges()->at(0)->getLeftExpr<hldb::Operation>();
  ASSERT_NE(left, nullptr) << "the left bound 'NumLcOtpWords-1' is an expression";
  EXPECT_EQ(left->getOpType(), vpiSubOp);
  ASSERT_NE(left->getOperands(), nullptr);
  ASSERT_EQ(left->getOperands()->size(), 2u);
  const hldb::RefObj *const words = any_cast<hldb::RefObj>(left->getOperands()->at(0));
  ASSERT_NE(words, nullptr);
  EXPECT_EQ(words->getActual(), getParam(m, "NumLcOtpWords"));
}

TEST_F(ParamArrayTest, TopImportsAllOfPartPkg) { ExpectOnlyWildcardImport(getModule("top"), "otp_ctrl_part_pkg"); }

TEST_F(ParamArrayTest, TopOverridesInfoWithPartInfoElement) {
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
  EXPECT_EQ(array->getName(), "PartInfo");
  EXPECT_EQ(array->getActual(), getParam(getPkg("otp_ctrl_part_pkg"), "PartInfo"))
      << "26.3: PartInfo is imported from otp_ctrl_part_pkg";
  const hldb::RefObj *const index = sel->getIndex<hldb::RefObj>();
  ASSERT_NE(index, nullptr);
  EXPECT_EQ(index->getName(), "LifeCycleIdx");
  const hldb::Typedef *const td = getTypedef("otp_ctrl_part_pkg", "part_idx_e");
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::EnumTypespec *const et = td->getAlias()->getActual<hldb::EnumTypespec>();
  ASSERT_NE(et, nullptr);
  ASSERT_NE(et->getEnum(), nullptr);
  EXPECT_EQ(index->getActual(), hldb::findByName<hldb::EnumConst>("LifeCycleIdx", et->getEnum()->getEnumConsts()))
      << "26.3: LifeCycleIdx is imported from otp_ctrl_part_pkg";
}

// ---------------------------------------------------------------------------
// Elaboration and diagnostics
// ---------------------------------------------------------------------------

TEST_F(ParamArrayTest, ElaboratedTopHoldsOtpCtrlLci) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_design->getTopModules(), nullptr);
    ASSERT_EQ(m_design->getTopModules()->size(), 1u) << "23.3.1: otp_ctrl_lci is instantiated, top is not";
    const hldb::Module *const top = m_design->getTopModules()->at(0);
    ASSERT_NE(top, nullptr);
    EXPECT_EQ(top->getDefName(), "top");
    EXPECT_NE(hldb::findByName<hldb::Module>("u_otp_ctrl_lci", top->getModules()), nullptr);
  }
}

TEST_F(ParamArrayTest, PatternElementCountMismatchIsReportedAfterElaboration) {
  if (m_design->getElaborated()) {
    ASSERT_NE(m_session->getErrorContainer(), nullptr);
    EXPECT_GE(m_session->getErrorContainer()->getErrorStats().nbError, 1)
        << "10.9.1: PartInfo has NumPart = 7 elements but its pattern lists 2";
  }
}

TEST_F(ParamArrayTest, NoSyntaxOrFatalDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "the file is syntactically well formed";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
