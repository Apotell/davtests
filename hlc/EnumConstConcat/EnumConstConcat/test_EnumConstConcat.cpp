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

// Tests for tests/EnumConstConcat/dut.sv:
//
//   package prim_mubi_pkg;
//   parameter int MuBi4Width = 4;
//   typedef enum logic [MuBi4Width-1:0] {
//     MuBi4True = 4'hA, // enabled
//     MuBi4False = 4'h5  // disabled
//   } mubi4_t;
//   endpackage
//
//   package pkg;
//   typedef enum logic [9:0] {
//     ReadingLow  = {6'b001100, prim_mubi_pkg::MuBi4False}
//   } fsm_state_e;
//   endpackage // pkg
//
//   module GOOD();
//   endmodule
//
//   module top();
//      if (pkg::ReadingLow == 197) begin
//         GOOD good();
//      end
//   endmodule
//
// Unlike EnumConcat (a single-package enum const built from a plain
// concatenation of two literal constants), this test exercises a
// concatenation whose second operand is itself an enum constant reference
// resolved across package boundaries (IEEE 1800-2023 Sec 26.3
// "package_scope": "prim_mubi_pkg::MuBi4False" explicitly qualifies the
// enum constant by its defining package). The concatenation
// "{6'b001100, prim_mubi_pkg::MuBi4False}" (Sec 11.4.12) packs a 6-bit
// literal and the 4-bit enum constant (4'h5) into 10 bits: 6'b001100 ++
// 4'h5 = 10'b0011000101 = decimal 197, matching "pkg::ReadingLow == 197"
// in the top module's generate-if (Sec 27.5), so "GOOD good()" should be
// statically included. No :should_fail_because: tag.
//
// Checked:
//   - packages "prim_mubi_pkg" and "pkg", and modules "GOOD" and "top" exist
//   - "prim_mubi_pkg" declares parameter "MuBi4Width" (non-localparam) with
//     default ParamAssign RHS Constant "4"
//   - "prim_mubi_pkg" owns TypedefTypespec "mubi4_t" -> EnumTypespec with
//     base typespec LogicTypespec whose range left expr is an Operation
//     (vpiSubOp) of RefObj "MuBi4Width" and Constant "1"
//   - "mubi4_t" has 2 consts: MuBi4True (Constant 4'hA) and MuBi4False
//     (Constant 4'h5)
//   - "pkg" owns TypedefTypespec "fsm_state_e" -> EnumTypespec with 1 const
//     "ReadingLow" whose value is a concatenation Operation (vpiConcatOp)
//     of Constant 6'b001100 and a RefObj "prim_mubi_pkg::MuBi4False"
//     whose path elements resolve to package prim_mubi_pkg and its
//     MuBi4False EnumConst
//   - "top" has exactly 1 generate statement: a GenIf whose condition is
//     an equality Operation (vpiEqOp) of RefObj "pkg::ReadingLow" (path
//     elements resolving to package pkg and its EnumConst) and Constant "197"
//   - the GenIf's body is a Begin containing a RefInstance "good" whose
//     typespec resolves (ModuleTypespec) to "GOOD"
//   - compiler reports zero errors

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/gen_if.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class EnumConstConcatTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "EnumConstConcat.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPrimMubiPkg() {
    return hldb::findByName<hldb::Package>("prim_mubi_pkg", m_design->getAllPackages());
  }

  static const hldb::Package *getPkg() { return hldb::findByName<hldb::Package>("pkg", m_design->getAllPackages()); }

  static const hldb::Module *getGood() { return hldb::findByDefName<hldb::Module>("GOOD", m_design->getAllModules()); }

  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Enum *getMubi4Enum() {
    const hldb::Package *const p = getPrimMubiPkg();
    if (p == nullptr || p->getTypespecs() == nullptr) return nullptr;
    const hldb::TypedefTypespec *const tt = hldb::findByName<hldb::TypedefTypespec>("mubi4_t", p->getTypespecs());
    if (tt == nullptr) return nullptr;
    const hldb::Typedef *const td = tt->getTypedef();
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::EnumTypespec *const enumTs = td->getAlias()->getActual<hldb::EnumTypespec>();
    if (enumTs == nullptr) return nullptr;
    return enumTs->getEnum();
  }

  static const hldb::Enum *getFsmStateEnum() {
    const hldb::Package *const p = getPkg();
    if (p == nullptr || p->getTypespecs() == nullptr) return nullptr;
    const hldb::TypedefTypespec *const tt = hldb::findByName<hldb::TypedefTypespec>("fsm_state_e", p->getTypespecs());
    if (tt == nullptr) return nullptr;
    const hldb::Typedef *const td = tt->getTypedef();
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::EnumTypespec *const enumTs = td->getAlias()->getActual<hldb::EnumTypespec>();
    if (enumTs == nullptr) return nullptr;
    return enumTs->getEnum();
  }

  template <typename ScopeT>
  static const hldb::ParamAssign *findParamAssign(const ScopeT *scope, std::string_view name) {
    return (scope == nullptr) ? nullptr : hldb::findByName(name, hldb::getParamAssigns(scope));
  }

  static const hldb::GenIf *getGenIf() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getGenStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *top->getGenStmts()) {
      if (const hldb::GenIf *const genIf = any_cast<hldb::GenIf>(stmt)) return genIf;
    }
    return nullptr;
  }
};

// ---------------------------------------------------------------------------
// Existence
// ---------------------------------------------------------------------------

TEST_F(EnumConstConcatTest, PackagePrimMubiPkgExists) {
  EXPECT_NE(getPrimMubiPkg(), nullptr) << "package 'prim_mubi_pkg' not found";
}

TEST_F(EnumConstConcatTest, PackagePkgExists) { EXPECT_NE(getPkg(), nullptr) << "package 'pkg' not found"; }

TEST_F(EnumConstConcatTest, ModuleGoodExists) { EXPECT_NE(getGood(), nullptr) << "module 'GOOD' not found"; }

TEST_F(EnumConstConcatTest, ModuleTopExists) { EXPECT_NE(getTop(), nullptr) << "module 'top' not found"; }

// ---------------------------------------------------------------------------
// parameter int MuBi4Width = 4;
// ---------------------------------------------------------------------------

TEST_F(EnumConstConcatTest, MuBi4WidthIsNonLocalParamWithDefaultFour) {
  const hldb::Package *const p = getPrimMubiPkg();
  ASSERT_NE(p, nullptr);
  ASSERT_NE(p->getParameters(), nullptr);
  const hldb::Parameter *width = nullptr;
  for (const hldb::Any *const item : *p->getParameters()) {
    const hldb::Parameter *const param = any_cast<hldb::Parameter>(item);
    if (param != nullptr && param->getName() == "MuBi4Width") {
      width = param;
      break;
    }
  }
  ASSERT_NE(width, nullptr) << "'parameter int MuBi4Width' not found on package 'prim_mubi_pkg'";
  EXPECT_FALSE(width->getLocalParam());

  const hldb::ParamAssign *const pa = findParamAssign(p, "MuBi4Width");
  ASSERT_NE(pa, nullptr) << "default ParamAssign for 'MuBi4Width' not found";
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "'MuBi4Width = 4': default RHS must be a Constant";
  EXPECT_EQ(std::string(rhs->getDecompile()), "4");
}

// ---------------------------------------------------------------------------
// typedef enum logic [MuBi4Width-1:0] {MuBi4True = 4'hA, MuBi4False = 4'h5} mubi4_t;
// ---------------------------------------------------------------------------

TEST_F(EnumConstConcatTest, Mubi4tBaseTypespecRangeUsesMuBi4WidthMinusOne) {
  const hldb::Enum *const e = getMubi4Enum();
  ASSERT_NE(e, nullptr) << "EnumTypespec for 'mubi4_t' not found";
  ASSERT_NE(e->getBaseTypespec(), nullptr) << "'enum logic [MuBi4Width-1:0]' must carry an explicit base typespec";
  const hldb::LogicTypespec *const logicTs = e->getBaseTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(logicTs, nullptr);
  ASSERT_NE(logicTs->getRanges(), nullptr);
  ASSERT_EQ(logicTs->getRanges()->size(), 1u);
  const hldb::Range *const range = logicTs->getRanges()->at(0);
  ASSERT_NE(range, nullptr);

  const hldb::Operation *const sub = any_cast<hldb::Operation>(range->getLeftExpr());
  ASSERT_NE(sub, nullptr) << "'MuBi4Width-1' left range expr must be a subtraction Operation";
  EXPECT_EQ(sub->getOpType(), vpiSubOp);
  ASSERT_NE(sub->getOperands(), nullptr);
  ASSERT_EQ(sub->getOperands()->size(), 2u);
  const hldb::RefObj *const widthRef = any_cast<hldb::RefObj>(sub->getOperands()->at(0));
  ASSERT_NE(widthRef, nullptr);
  EXPECT_EQ(widthRef->getName(), std::string_view("MuBi4Width"));

  const hldb::Constant *const right = any_cast<hldb::Constant>(range->getRightExpr());
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(std::string(right->getDecompile()), "0");
}

TEST_F(EnumConstConcatTest, Mubi4tHasTwoConstsTrueAndFalse) {
  const hldb::Enum *const e = getMubi4Enum();
  ASSERT_NE(e, nullptr);
  ASSERT_NE(e->getEnumConsts(), nullptr);
  ASSERT_EQ(e->getEnumConsts()->size(), 2u);
  EXPECT_EQ(e->getEnumConsts()->at(0)->getName(), std::string_view("MuBi4True"));
  EXPECT_EQ(e->getEnumConsts()->at(1)->getName(), std::string_view("MuBi4False"));

  const hldb::Constant *const trueVal = e->getEnumConsts()->at(0)->getValue<hldb::Constant>();
  ASSERT_NE(trueVal, nullptr) << "'MuBi4True = 4'hA' value must be a Constant";
  EXPECT_EQ(trueVal->getConstType(), vpiHexConst);
  EXPECT_EQ(trueVal->getSize(), 4);
  // getValue() is HLC's internal value encoding; check the source literal text instead.
  // EXPECT_EQ(std::string(trueVal->getValue()), "a");
  EXPECT_EQ(trueVal->getDecompile(), std::string_view{"4'hA"});

  const hldb::Constant *const falseVal = e->getEnumConsts()->at(1)->getValue<hldb::Constant>();
  ASSERT_NE(falseVal, nullptr) << "'MuBi4False = 4'h5' value must be a Constant";
  EXPECT_EQ(falseVal->getConstType(), vpiHexConst);
  EXPECT_EQ(falseVal->getSize(), 4);
  // getValue() is HLC's internal value encoding; check the source literal text instead.
  // EXPECT_EQ(std::string(falseVal->getValue()), "5");
  EXPECT_EQ(falseVal->getDecompile(), std::string_view{"4'h5"});
}

// ---------------------------------------------------------------------------
// typedef enum logic [9:0] {ReadingLow = {6'b001100, prim_mubi_pkg::MuBi4False}} fsm_state_e;
// ---------------------------------------------------------------------------

TEST_F(EnumConstConcatTest, FsmStateEHasOneConstReadingLow) {
  const hldb::Enum *const e = getFsmStateEnum();
  ASSERT_NE(e, nullptr) << "EnumTypespec for 'fsm_state_e' not found";
  ASSERT_NE(e->getEnumConsts(), nullptr);
  ASSERT_EQ(e->getEnumConsts()->size(), 1u);
  EXPECT_EQ(e->getEnumConsts()->at(0)->getName(), std::string_view("ReadingLow"));
}

TEST_F(EnumConstConcatTest, ReadingLowValueIsConcatOfLiteralAndCrossPackageEnumConst) {
  const hldb::Enum *const e = getFsmStateEnum();
  ASSERT_NE(e, nullptr);
  ASSERT_NE(e->getEnumConsts(), nullptr);
  ASSERT_EQ(e->getEnumConsts()->size(), 1u);
  const hldb::EnumConst *const ec = e->getEnumConsts()->at(0);
  ASSERT_NE(ec, nullptr);

  const hldb::Operation *const concat = ec->getValue<hldb::Operation>();
  ASSERT_NE(concat, nullptr)
      << "Sec 11.4.12: '{6'b001100, prim_mubi_pkg::MuBi4False}' value must be a concatenation Operation";
  EXPECT_EQ(concat->getOpType(), vpiConcatOp);
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 2u);

  const hldb::Constant *const literal = any_cast<hldb::Constant>(concat->getOperands()->at(0));
  ASSERT_NE(literal, nullptr);
  EXPECT_EQ(literal->getConstType(), vpiBinaryConst);
  EXPECT_EQ(literal->getSize(), 6);
  // getValue() is HLC's internal value encoding; check the source literal text instead.
  // EXPECT_EQ(std::string(literal->getValue()), "1100");
  EXPECT_EQ(literal->getDecompile(), std::string_view{"6'b001100"});

  // Sec 26.3 package_scope: "prim_mubi_pkg::MuBi4False" is a cross-package
  // reference to the enum constant declared in package "prim_mubi_pkg".
  const hldb::RefObj *const crossPkgRef = any_cast<hldb::RefObj>(concat->getOperands()->at(1));
  ASSERT_NE(crossPkgRef, nullptr) << "'prim_mubi_pkg::MuBi4False' operand must be a RefObj";
  // A package-scoped reference "prim_mubi_pkg::MuBi4False" is a RefObj whose name is the full source text and
  // whose path elements are [RefObj "prim_mubi_pkg" -> Package, RefObj "MuBi4False" -> EnumConst].
  // EXPECT_EQ(crossPkgRef->getName(), std::string_view("MuBi4False"));
  // const hldb::EnumConst *const resolved = crossPkgRef->getActual<hldb::EnumConst>();
  EXPECT_EQ(crossPkgRef->getName(), std::string_view{"prim_mubi_pkg::MuBi4False"});
  ASSERT_NE(crossPkgRef->getPathElems(), nullptr);
  ASSERT_EQ(crossPkgRef->getPathElems()->size(), 2u);
  const hldb::RefObj *const pkgRef = any_cast<hldb::RefObj>(crossPkgRef->getPathElems()->front());
  ASSERT_NE(pkgRef, nullptr);
  EXPECT_EQ(pkgRef->getName(), std::string_view{"prim_mubi_pkg"});
  ASSERT_NE(pkgRef->getActual(), nullptr);
  const hldb::Package *const pkgDecl = pkgRef->getActual<hldb::Package>();
  ASSERT_NE(pkgDecl, nullptr) << "'prim_mubi_pkg' must resolve to the package";
  EXPECT_EQ(pkgDecl->getName(), std::string_view{"prim_mubi_pkg"});
  const hldb::RefObj *const leafRef = any_cast<hldb::RefObj>(crossPkgRef->getPathElems()->back());
  ASSERT_NE(leafRef, nullptr);
  EXPECT_EQ(leafRef->getName(), std::string_view{"MuBi4False"});
  ASSERT_NE(leafRef->getActual(), nullptr);
  const hldb::EnumConst *const resolved = leafRef->getActual<hldb::EnumConst>();
  ASSERT_NE(resolved, nullptr) << "'prim_mubi_pkg::MuBi4False' must resolve to prim_mubi_pkg's EnumConst declaration";
  EXPECT_EQ(resolved->getName(), std::string_view("MuBi4False"));
}

// ---------------------------------------------------------------------------
// if (pkg::ReadingLow == 197) begin GOOD good(); end
// ---------------------------------------------------------------------------

TEST_F(EnumConstConcatTest, TopHasExactlyOneGenIf) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getGenStmts(), nullptr);
  EXPECT_EQ(top->getGenStmts()->size(), 1u);
  EXPECT_NE(getGenIf(), nullptr) << "the single generate statement must be a GenIf";
}

TEST_F(EnumConstConcatTest, GenIfConditionIsEqualityOfReadingLowAnd197) {
  const hldb::GenIf *const genIf = getGenIf();
  ASSERT_NE(genIf, nullptr);
  const hldb::Operation *const eq = genIf->getCondition<hldb::Operation>();
  ASSERT_NE(eq, nullptr) << "'pkg::ReadingLow == 197' condition must be an equality Operation";
  EXPECT_EQ(eq->getOpType(), vpiEqOp);
  ASSERT_NE(eq->getOperands(), nullptr);
  ASSERT_EQ(eq->getOperands()->size(), 2u);

  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(eq->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  // A package-scoped reference "pkg::ReadingLow" is a RefObj whose name is the full source text and
  // whose path elements are [RefObj "pkg" -> Package, RefObj "ReadingLow" -> EnumConst].
  // EXPECT_EQ(lhs->getName(), std::string_view("ReadingLow"));
  // const hldb::EnumConst *const resolved = lhs->getActual<hldb::EnumConst>();
  EXPECT_EQ(lhs->getName(), std::string_view{"pkg::ReadingLow"});
  ASSERT_NE(lhs->getPathElems(), nullptr);
  ASSERT_EQ(lhs->getPathElems()->size(), 2u);
  const hldb::RefObj *const pkgRef = any_cast<hldb::RefObj>(lhs->getPathElems()->front());
  ASSERT_NE(pkgRef, nullptr);
  EXPECT_EQ(pkgRef->getName(), std::string_view{"pkg"});
  ASSERT_NE(pkgRef->getActual(), nullptr);
  const hldb::Package *const pkgDecl = pkgRef->getActual<hldb::Package>();
  ASSERT_NE(pkgDecl, nullptr) << "'pkg' must resolve to the package";
  EXPECT_EQ(pkgDecl->getName(), std::string_view{"pkg"});
  const hldb::RefObj *const leafRef = any_cast<hldb::RefObj>(lhs->getPathElems()->back());
  ASSERT_NE(leafRef, nullptr);
  EXPECT_EQ(leafRef->getName(), std::string_view{"ReadingLow"});
  ASSERT_NE(leafRef->getActual(), nullptr);
  const hldb::EnumConst *const resolved = leafRef->getActual<hldb::EnumConst>();
  ASSERT_NE(resolved, nullptr) << "'pkg::ReadingLow' must resolve to pkg's EnumConst declaration";

  const hldb::Constant *const rhs = any_cast<hldb::Constant>(eq->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getConstType(), vpiUIntConst)
      << "HLDB stores unsized integer literals as vpiUIntConst, not vpiIntConst";
  EXPECT_EQ(std::string(rhs->getDecompile()), "197");
}

TEST_F(EnumConstConcatTest, GenIfBodyInstantiatesGoodAsGood) {
  const hldb::GenIf *const genIf = getGenIf();
  ASSERT_NE(genIf, nullptr);
  const hldb::Begin *const body = genIf->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "'begin GOOD good(); end' body must be a Begin";
  ASSERT_NE(body->getStmts(), nullptr);
  const hldb::RefInstance *good = nullptr;
  for (const hldb::Any *const stmt : *body->getStmts()) {
    if (const hldb::RefInstance *const ri = any_cast<hldb::RefInstance>(stmt)) {
      if (ri->getName() == "good") {
        good = ri;
        break;
      }
    }
  }
  ASSERT_NE(good, nullptr) << "'GOOD good();' RefInstance not found inside the generate-if body";
  ASSERT_NE(good->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = good->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr) << "'good's typespec is not ModuleTypespec";
  EXPECT_EQ(mt->getName(), std::string_view("GOOD"));
}

// ---------------------------------------------------------------------------
// Compiler diagnostics
// ---------------------------------------------------------------------------

TEST_F(EnumConstConcatTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
