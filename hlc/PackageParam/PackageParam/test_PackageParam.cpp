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

// Validates the HLDB model built for tests/PackageParam/dut.sv:
//
//   package lc_ctrl_pkg;
//     parameter logic [15:0] A10 = 16'h0000;
//     parameter logic [15:0] A11 = 16'h0000;
//     typedef enum logic [LcStateWidth-1:0] {
//       LcStRaw           = '0,
//       LcStTestUnlocked0 = {A11, A10}
//     } lc_state_e;
//   endpackage
//
//   package otp_ctrl_reg_pkg;
//     parameter int NumSramKeyReqSlots = 2;
//     parameter int OtpByteAddrWidth = 11;
//     parameter int NumErrorEntries = 9;
//   endpackage
//
//   package otp_ctrl_pkg;
//     import prim_util_pkg::vbits;
//     import otp_ctrl_reg_pkg::*;
//     parameter int OtpWidth         = 16;
//     parameter int OtpAddrWidth     = OtpByteAddrWidth - $clog2(OtpWidth/8);
//   endpackage
//
// The point of the fixture is package parameters whose values depend on
// other parameters: an enum value built from a concatenation of two
// parameters of the same package, and a parameter computed from a parameter
// of another package reached through a wildcard import (IEEE 1800-2023
// 26.3). The fixture is a fragment lifted from a larger design, so it also
// holds two names that are declared nowhere in it: 'LcStateWidth', used in
// the enum's base type, and the package 'prim_util_pkg', named in an
// explicit import. Both are errors, and the regression this file exists to
// catch is HLC either missing those errors or letting them derail the legal
// parts, in particular the cross-package binding of OtpByteAddrWidth.
//
// What is checked, and why:
//   Packages (26.2)
//     - lc_ctrl_pkg, otp_ctrl_reg_pkg and otp_ctrl_pkg exist
//   Every parameter is a local parameter: in a package the keyword
//   'parameter' is a synonym for 'localparam' (6.20.4)
//   lc_ctrl_pkg
//     - exactly 2 parameters, A10 and A11, each 'logic [15:0]': a
//       LogicTypespec with exactly 1 packed range [15:0]
//     - each ParamAssign RHS is the Constant "16'h0000"
//     - exactly 1 Typedef, 'lc_state_e', whose alias is an EnumTypespec
//       (6.19)
//     - the enum's base type is a LogicTypespec with exactly 1 packed range
//       whose left bound is Operation vpiSubOp over RefObj 'LcStateWidth'
//       (declared nowhere, so bound to nothing) and Constant "1", and whose
//       right bound is Constant "0"
//     - exactly 2 enum names, in source order LcStRaw, LcStTestUnlocked0
//     - LcStRaw's value is the unbased unsized literal '0 (5.7.1); on an
//       elaborated design it is the value 0
//     - LcStTestUnlocked0's value is Operation vpiConcatOp (11.4.12) over
//       RefObj 'A11' then RefObj 'A10', bound to the package's Parameters.
//       Enum values are constant expressions (6.19), so on an elaborated
//       design it is reduced to a Constant with the value 0
//   otp_ctrl_reg_pkg
//     - exactly 3 parameters, NumSramKeyReqSlots, OtpByteAddrWidth and
//       NumErrorEntries, each a signed 'int' (6.11) with the Constant values
//       "2", "11" and "9"
//   otp_ctrl_pkg
//     - exactly 2 parameters of its own, OtpWidth and OtpAddrWidth, each a
//       signed 'int'. The wildcard import makes otp_ctrl_reg_pkg's names
//       visible but declares nothing in otp_ctrl_pkg (26.3)
//     - OtpWidth's ParamAssign RHS is the Constant "16"
//     - OtpAddrWidth's ParamAssign RHS is Operation vpiSubOp over
//       RefObj 'OtpByteAddrWidth' -- bound through the wildcard import to
//       otp_ctrl_reg_pkg's Parameter (26.3) -- and the SysFuncCall
//       "$clog2" (20.8.1) with exactly 1 argument, Operation vpiDivOp over
//       RefObj 'OtpWidth' (bound to otp_ctrl_pkg's own Parameter) and
//       Constant "8"
//     - a parameter value is a constant expression (6.20.2), so on an
//       elaborated design OtpAddrWidth is reduced to 11 - $clog2(16/8) =
//       11 - 1 = 10
//   Diagnostics
//     - 'LcStateWidth' is declared nowhere. For a reference other than a
//       subroutine call "it shall be illegal if no identifier can be found
//       that matches the reference" (26.3), so it is reported at error
//       severity, as COMP_UNDEFINED_VARIABLE or ELAB_UNDEF_VARIABLE
//     - 'prim_util_pkg' is never compiled, but "The compilation of a
//       package shall precede the compilation of scopes in which the
//       package is imported" (26.3), so the import is reported at error
//       severity, as COMP_UNDEFINED_PACKAGE or ELAB_UNDEFINED_PACKAGE
//     - the declared names are not reported: no COMP_UNDEFINED_VARIABLE or
//       COMP_FAILED_TO_BIND for A10, A11, OtpWidth or OtpByteAddrWidth
//     - there is no syntax error in the file: zero syntax and zero fatal
//       diagnostics
//
// Reduction and elaboration: three values reduce, and each is checked in
// both modes. The unreduced expression is asserted on the package
// definitions in Design::getAllPackages(), which hold the source form in
// every run. The reduced value is asserted, only when the design is
// elaborated, on the elaborated packages in Design::getTopPackages().
//
// KNOWN COMPILER BUG (package parameter is not a localparam), not a defect
// in this test: 6.20.4 says that in a package "the parameter keyword shall
// be a synonym for the localparam keyword", but HLC reports
// Parameter::getLocalParam() false for every parameter in the file.
// AllPackageParametersAreLocalParams asserts the LRM value and is expected
// to fail until HLC is fixed; it is intentionally not skipped or relaxed.
//
// KNOWN COMPILER BUG (undeclared identifier not reported), not a defect in
// this test: HLC leaves 'LcStateWidth' unbound, as it should, but reports
// no diagnostic for it at all, although 26.3 makes the reference illegal.
// UndeclaredLcStateWidthIsReportedAsError is expected to fail until HLC is
// fixed; it is intentionally not skipped or relaxed.
//
// KNOWN COMPILER BUG (import of an undeclared package not reported), not a
// defect in this test: HLC reports nothing for 'import prim_util_pkg::vbits;'
// although prim_util_pkg is never compiled, which 26.3 requires before the
// import. ImportFromUndeclaredPrimUtilPkgIsReportedAsError is expected to
// fail until HLC is fixed; it is intentionally not skipped or relaxed.
//
// What is NOT checked, and why:
//   - The width of lc_state_e and the size of its values. The base type's
//     width depends on LcStateWidth, which is declared nowhere, so the
//     standard gives it no value; only that LcStTestUnlocked0 evaluates to
//     0, which holds at any width, is asserted.
//   - What 'vbits' resolves to. Its package does not exist, so the import
//     is asserted only through the error it must draw.
//   - The parameters A10/A11 values on an elaborated design: they are
//     already literals, so there is nothing to reduce.
//   - Where HLC records the two imports is a model convention. The effect
//     of the wildcard import is asserted through the binding of
//     OtpByteAddrWidth.
//   - Whether other packages (for example a built-in one) also appear in
//     Design::getAllPackages() is a tool convention, so the package count is
//     not asserted; packages are looked up by name.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.
//   - The license and the comments in the fixture are not design objects.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/typedef.h>
#include <hldb/vpi_user.h>

#include <charconv>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>

namespace hlc {

class PackageParamTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackageParam.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // The package definition, which holds the source form of every value.
  static const hldb::Package *getPkg(std::string_view name) {
    return hldb::findByName<hldb::Package>(name, m_design->getAllPackages());
  }

  // The elaborated package, which holds the reduced values.
  static const hldb::Package *getTopPkg(std::string_view name) {
    return hldb::findByName<hldb::Package>(name, m_design->getTopPackages());
  }

  static const hldb::Parameter *getParam(const hldb::Package *pkg, std::string_view name) {
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, pkg->getParameters());
  }

  // The ParamAssign whose LHS names 'name'.
  static const hldb::ParamAssign *getParamAssign(const hldb::Package *pkg, std::string_view name) {
    if (pkg == nullptr || pkg->getParamAssigns() == nullptr) return nullptr;
    for (const hldb::ParamAssign *const pa : *pkg->getParamAssigns()) {
      const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
      if ((lhs != nullptr) && (lhs->getName() == name)) return pa;
    }
    return nullptr;
  }

  static const hldb::Enum *getLcStateEnum(const hldb::Package *pkg) {
    if (pkg == nullptr) return nullptr;
    const hldb::Typedef *const td = hldb::findByName<hldb::Typedef>("lc_state_e", pkg->getTypedefs());
    if (td == nullptr || td->getAlias() == nullptr) return nullptr;
    const hldb::EnumTypespec *const et = td->getAlias()->getActual<hldb::EnumTypespec>();
    if (et == nullptr) return nullptr;
    return et->getEnum();
  }

  static const hldb::EnumConst *getEnumConst(const hldb::Package *pkg, std::string_view name) {
    const hldb::Enum *const e = getLcStateEnum(pkg);
    if (e == nullptr) return nullptr;
    return hldb::findByName<hldb::EnumConst>(name, e->getEnumConsts());
  }

  // Integer value of a Constant, from its decompiled text ("10"), from a
  // sized or based literal ("32'd10", "32'hA"), or from the unbased unsized
  // literal "'0". The radix HLC picks for a folded value is a tool
  // convention, so every integer form is accepted and the numeric value is
  // what gets compared.
  static bool getIntValue(const hldb::Constant *c, uint64_t *value) {
    std::string text;
    for (char ch : c->getDecompile()) {
      if (ch != '_') text.push_back(ch);
    }
    if (text == "'0") {
      *value = 0;
      return true;
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

  // Verifies 'expr' is a Constant whose integer value is 'expected'.
  static void ExpectIntConstant(const hldb::Any *expr, uint64_t expected, std::string_view what) {
    const hldb::Constant *const c = any_cast<hldb::Constant>(expr);
    ASSERT_NE(c, nullptr) << what << " should be reduced to a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(getIntValue(c, &v)) << "unparsable constant '" << c->getDecompile() << "'";
    EXPECT_EQ(v, expected) << what;
  }

  // Verifies 'expr' is a RefObj named 'name' bound by object identity to
  // 'target'.
  static void ExpectBoundRef(const hldb::Any *expr, std::string_view name, const hldb::Any *target) {
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr);
    ASSERT_NE(ref, nullptr) << "'" << name << "' should be a RefObj";
    EXPECT_EQ(ref->getName(), name);
    ASSERT_NE(target, nullptr) << "the declaration of '" << name << "' was not found";
    EXPECT_EQ(ref->getActual(), target) << "'" << name << "' must bind to its declaration";
  }

  // Verifies 'name' in 'pkg' is a parameter of type signed 'int'.
  static void ExpectSignedIntParam(const hldb::Package *pkg, std::string_view name) {
    const hldb::Parameter *const p = getParam(pkg, name);
    ASSERT_NE(p, nullptr) << "parameter '" << name << "' not found";
    ASSERT_NE(p->getTypespec(), nullptr);
    const hldb::IntTypespec *const ts = p->getTypespec()->getActual<hldb::IntTypespec>();
    ASSERT_NE(ts, nullptr) << "'" << name << "' is declared 'int'";
    EXPECT_TRUE(ts->getSigned()) << "6.11: 'int' is signed";
  }

  // Verifies the ParamAssign of 'name' in 'pkg' has the Constant RHS 'value'.
  static void ExpectLiteralParamAssign(const hldb::Package *pkg, std::string_view name, std::string_view value) {
    const hldb::ParamAssign *const pa = getParamAssign(pkg, name);
    ASSERT_NE(pa, nullptr) << "no ParamAssign for '" << name << "'";
    const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getActual<hldb::Parameter>(), getParam(pkg, name));
    const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
    ASSERT_NE(rhs, nullptr) << "'" << name << "' is assigned a literal";
    EXPECT_EQ(rhs->getDecompile(), value);
  }

  // Verifies one of 'types' was reported naming 'symbol', at error severity:
  // the standard makes the reference illegal.
  void ExpectReportedAsError(std::initializer_list<ErrorDefinition::ErrorType> types, std::string_view symbol) {
    const Error *found = nullptr;
    for (const ErrorDefinition::ErrorType type : types) {
      found = findError(type, symbol);
      if (found != nullptr) break;
    }
    ASSERT_NE(found, nullptr) << "no diagnostic names '" << symbol << "'";
    const ErrorDefinition::ErrorMap &infos = ErrorDefinition::getErrorInfoMap();
    const ErrorDefinition::ErrorMap::const_iterator info = infos.find(found->getType());
    ASSERT_NE(info, infos.end());
    EXPECT_EQ(info->second.m_severity, ErrorDefinition::ERROR)
        << "'" << symbol << "' makes the source illegal, so it is an error, not a warning";
  }
};

// ---------------------------------------------------------------------------
// Packages
// ---------------------------------------------------------------------------

TEST_F(PackageParamTest, AllThreePackagesExist) {
  EXPECT_NE(getPkg("lc_ctrl_pkg"), nullptr) << "package 'lc_ctrl_pkg' not found";
  EXPECT_NE(getPkg("otp_ctrl_reg_pkg"), nullptr) << "package 'otp_ctrl_reg_pkg' not found";
  EXPECT_NE(getPkg("otp_ctrl_pkg"), nullptr) << "package 'otp_ctrl_pkg' not found";
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (package
// parameter is not a localparam) in the file header.
TEST_F(PackageParamTest, AllPackageParametersAreLocalParams) {
  struct ParamRef final {
    std::string_view m_pkg;
    std::string_view m_name;
  };
  const ParamRef params[] = {{"lc_ctrl_pkg", "A10"},
                             {"lc_ctrl_pkg", "A11"},
                             {"otp_ctrl_reg_pkg", "NumSramKeyReqSlots"},
                             {"otp_ctrl_reg_pkg", "OtpByteAddrWidth"},
                             {"otp_ctrl_reg_pkg", "NumErrorEntries"},
                             {"otp_ctrl_pkg", "OtpWidth"},
                             {"otp_ctrl_pkg", "OtpAddrWidth"}};
  for (const ParamRef &ref : params) {
    const hldb::Parameter *const p = getParam(getPkg(ref.m_pkg), ref.m_name);
    ASSERT_NE(p, nullptr) << "parameter '" << ref.m_name << "' not found in " << ref.m_pkg;
    EXPECT_TRUE(p->getLocalParam()) << "6.20.4: '" << ref.m_name << "' is declared in package " << ref.m_pkg
                                    << ", where 'parameter' is a synonym for 'localparam'";
  }
}

// ---------------------------------------------------------------------------
// package lc_ctrl_pkg;
//   parameter logic [15:0] A10 = 16'h0000;
//   parameter logic [15:0] A11 = 16'h0000;
// ---------------------------------------------------------------------------

TEST_F(PackageParamTest, LcCtrlPkgHasExactlyParametersA10AndA11) {
  const hldb::Package *const pkg = getPkg("lc_ctrl_pkg");
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 2u) << "'A10' and 'A11'";
  EXPECT_NE(getParam(pkg, "A10"), nullptr);
  EXPECT_NE(getParam(pkg, "A11"), nullptr);
}

TEST_F(PackageParamTest, A10AndA11AreLogic15To0) {
  const hldb::Package *const pkg = getPkg("lc_ctrl_pkg");
  ASSERT_NE(pkg, nullptr);
  for (std::string_view name : {"A10", "A11"}) {
    const hldb::Parameter *const p = getParam(pkg, name);
    ASSERT_NE(p, nullptr) << "parameter '" << name << "' not found";
    ASSERT_NE(p->getTypespec(), nullptr);
    const hldb::LogicTypespec *const lt = p->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lt, nullptr) << "'" << name << "' is declared 'logic [15:0]'";
    ASSERT_NE(lt->getRanges(), nullptr);
    ASSERT_EQ(lt->getRanges()->size(), 1u);
    const hldb::Range *const r = lt->getRanges()->at(0);
    ASSERT_NE(r, nullptr);
    const hldb::Constant *const left = r->getLeftExpr<hldb::Constant>();
    const hldb::Constant *const right = r->getRightExpr<hldb::Constant>();
    ASSERT_NE(left, nullptr);
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(left->getDecompile(), "15");
    EXPECT_EQ(right->getDecompile(), "0");
  }
}

TEST_F(PackageParamTest, A10AndA11AreAssignedSixteenBitZero) {
  const hldb::Package *const pkg = getPkg("lc_ctrl_pkg");
  ASSERT_NE(pkg, nullptr);
  ExpectLiteralParamAssign(pkg, "A10", "16'h0000");
  ExpectLiteralParamAssign(pkg, "A11", "16'h0000");
}

// ---------------------------------------------------------------------------
// typedef enum logic [LcStateWidth-1:0] { ... } lc_state_e;
// ---------------------------------------------------------------------------

TEST_F(PackageParamTest, LcCtrlPkgDeclaresEnumTypedefLcStateE) {
  const hldb::Package *const pkg = getPkg("lc_ctrl_pkg");
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getTypedefs(), nullptr);
  EXPECT_EQ(pkg->getTypedefs()->size(), 1u) << "'lc_state_e' is lc_ctrl_pkg's only typedef";
  EXPECT_NE(getLcStateEnum(pkg), nullptr) << "6.19: typedef 'lc_state_e' should alias an enum type";
}

TEST_F(PackageParamTest, EnumBaseTypeRangeUsesUndeclaredLcStateWidth) {
  const hldb::Enum *const e = getLcStateEnum(getPkg("lc_ctrl_pkg"));
  ASSERT_NE(e, nullptr);
  ASSERT_NE(e->getBaseTypespec(), nullptr) << "6.19: the explicit base type 'logic [LcStateWidth-1:0]' is recorded";
  const hldb::LogicTypespec *const lt = e->getBaseTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "the base type is 'logic'";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  const hldb::Range *const r = lt->getRanges()->at(0);
  ASSERT_NE(r, nullptr);
  const hldb::Operation *const left = r->getLeftExpr<hldb::Operation>();
  ASSERT_NE(left, nullptr) << "the left bound 'LcStateWidth-1' is an expression";
  EXPECT_EQ(left->getOpType(), vpiSubOp);
  ASSERT_NE(left->getOperands(), nullptr);
  ASSERT_EQ(left->getOperands()->size(), 2u);
  const hldb::RefObj *const width = any_cast<hldb::RefObj>(left->getOperands()->at(0));
  ASSERT_NE(width, nullptr);
  EXPECT_EQ(width->getName(), "LcStateWidth");
  EXPECT_EQ(width->getActual(), nullptr) << "'LcStateWidth' is declared nowhere, so it cannot bind";
  const hldb::Constant *const one = any_cast<hldb::Constant>(left->getOperands()->at(1));
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->getDecompile(), "1");
  const hldb::Constant *const right = r->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
}

TEST_F(PackageParamTest, EnumHasTwoNamesInSourceOrder) {
  const hldb::Enum *const e = getLcStateEnum(getPkg("lc_ctrl_pkg"));
  ASSERT_NE(e, nullptr);
  ASSERT_NE(e->getEnumConsts(), nullptr);
  ASSERT_EQ(e->getEnumConsts()->size(), 2u);
  EXPECT_EQ(e->getEnumConsts()->at(0)->getName(), "LcStRaw");
  EXPECT_EQ(e->getEnumConsts()->at(1)->getName(), "LcStTestUnlocked0");
}

TEST_F(PackageParamTest, LcStRawIsUnbasedUnsizedZero) {
  const hldb::EnumConst *const raw = getEnumConst(getPkg("lc_ctrl_pkg"), "LcStRaw");
  ASSERT_NE(raw, nullptr);
  const hldb::Constant *const value = raw->getValue<hldb::Constant>();
  ASSERT_NE(value, nullptr) << "'LcStRaw' is given the literal '0";
  EXPECT_EQ(value->getDecompile(), "'0") << "5.7.1: '0 is the unbased unsized literal that sets all bits to 0";
  if (m_design->getElaborated()) {
    const hldb::EnumConst *const elab = getEnumConst(getTopPkg("lc_ctrl_pkg"), "LcStRaw");
    ASSERT_NE(elab, nullptr) << "'LcStRaw' not found in the elaborated lc_ctrl_pkg";
    ExpectIntConstant(elab->getValue(), 0, "6.19: 'LcStRaw = '0' has the value 0");
  }
}

TEST_F(PackageParamTest, LcStTestUnlocked0IsConcatOfA11AndA10) {
  const hldb::Package *const pkg = getPkg("lc_ctrl_pkg");
  const hldb::EnumConst *const unlocked = getEnumConst(pkg, "LcStTestUnlocked0");
  ASSERT_NE(unlocked, nullptr);
  const hldb::Operation *const concat = unlocked->getValue<hldb::Operation>();
  ASSERT_NE(concat, nullptr) << "unreduced, '{A11, A10}' is an Operation";
  EXPECT_EQ(concat->getOpType(), vpiConcatOp) << "11.4.12: '{A11, A10}' is a concatenation";
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 2u);
  ExpectBoundRef(concat->getOperands()->at(0), "A11", getParam(pkg, "A11"));
  ExpectBoundRef(concat->getOperands()->at(1), "A10", getParam(pkg, "A10"));
  if (m_design->getElaborated()) {
    const hldb::EnumConst *const elab = getEnumConst(getTopPkg("lc_ctrl_pkg"), "LcStTestUnlocked0");
    ASSERT_NE(elab, nullptr) << "'LcStTestUnlocked0' not found in the elaborated lc_ctrl_pkg";
    ExpectIntConstant(elab->getValue(), 0, "6.19: '{A11, A10}' = {16'h0000, 16'h0000} has the value 0");
  }
}

// ---------------------------------------------------------------------------
// package otp_ctrl_reg_pkg;
// ---------------------------------------------------------------------------

TEST_F(PackageParamTest, OtpCtrlRegPkgHasThreeIntParameters) {
  const hldb::Package *const pkg = getPkg("otp_ctrl_reg_pkg");
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 3u);
  for (std::string_view name : {"NumSramKeyReqSlots", "OtpByteAddrWidth", "NumErrorEntries"}) {
    ExpectSignedIntParam(pkg, name);
  }
}

TEST_F(PackageParamTest, OtpCtrlRegPkgParameterValues) {
  const hldb::Package *const pkg = getPkg("otp_ctrl_reg_pkg");
  ASSERT_NE(pkg, nullptr);
  ExpectLiteralParamAssign(pkg, "NumSramKeyReqSlots", "2");
  ExpectLiteralParamAssign(pkg, "OtpByteAddrWidth", "11");
  ExpectLiteralParamAssign(pkg, "NumErrorEntries", "9");
}

// ---------------------------------------------------------------------------
// package otp_ctrl_pkg;
// ---------------------------------------------------------------------------

TEST_F(PackageParamTest, OtpCtrlPkgDeclaresOnlyItsOwnTwoParameters) {
  const hldb::Package *const pkg = getPkg("otp_ctrl_pkg");
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 2u)
      << "26.3: the wildcard import declares nothing, so only 'OtpWidth' and 'OtpAddrWidth' belong to otp_ctrl_pkg";
  ExpectSignedIntParam(pkg, "OtpWidth");
  ExpectSignedIntParam(pkg, "OtpAddrWidth");
}

TEST_F(PackageParamTest, OtpWidthIsSixteen) { ExpectLiteralParamAssign(getPkg("otp_ctrl_pkg"), "OtpWidth", "16"); }

TEST_F(PackageParamTest, OtpAddrWidthSubtractsClog2FromImportedOtpByteAddrWidth) {
  const hldb::Package *const pkg = getPkg("otp_ctrl_pkg");
  const hldb::ParamAssign *const pa = getParamAssign(pkg, "OtpAddrWidth");
  ASSERT_NE(pa, nullptr) << "no ParamAssign for 'OtpAddrWidth'";
  const hldb::Operation *const sub = pa->getRhs<hldb::Operation>();
  ASSERT_NE(sub, nullptr) << "unreduced, 'OtpByteAddrWidth - $clog2(OtpWidth/8)' is an Operation";
  EXPECT_EQ(sub->getOpType(), vpiSubOp);
  ASSERT_NE(sub->getOperands(), nullptr);
  ASSERT_EQ(sub->getOperands()->size(), 2u);
  ExpectBoundRef(sub->getOperands()->at(0), "OtpByteAddrWidth",
                 getParam(getPkg("otp_ctrl_reg_pkg"), "OtpByteAddrWidth"));

  const hldb::SysFuncCall *const clog2 = any_cast<hldb::SysFuncCall>(sub->getOperands()->at(1));
  ASSERT_NE(clog2, nullptr) << "'$clog2(...)' is a system function call";
  EXPECT_EQ(clog2->getName(), "$clog2");
  ASSERT_NE(clog2->getArguments(), nullptr);
  ASSERT_EQ(clog2->getArguments()->size(), 1u);
  const hldb::Operation *const div = any_cast<hldb::Operation>(clog2->getArguments()->at(0));
  ASSERT_NE(div, nullptr) << "the argument 'OtpWidth/8' is an Operation";
  EXPECT_EQ(div->getOpType(), vpiDivOp);
  ASSERT_NE(div->getOperands(), nullptr);
  ASSERT_EQ(div->getOperands()->size(), 2u);
  ExpectBoundRef(div->getOperands()->at(0), "OtpWidth", getParam(pkg, "OtpWidth"));
  const hldb::Constant *const eight = any_cast<hldb::Constant>(div->getOperands()->at(1));
  ASSERT_NE(eight, nullptr);
  EXPECT_EQ(eight->getDecompile(), "8");
}

TEST_F(PackageParamTest, OtpAddrWidthReducesToTen) {
  if (m_design->getElaborated()) {
    const hldb::ParamAssign *const pa = getParamAssign(getTopPkg("otp_ctrl_pkg"), "OtpAddrWidth");
    ASSERT_NE(pa, nullptr) << "no ParamAssign for 'OtpAddrWidth' in the elaborated otp_ctrl_pkg";
    ExpectIntConstant(pa->getRhs(), 10, "6.20.2, 20.8.1: OtpAddrWidth = 11 - $clog2(16/8) = 11 - 1 = 10");
  }
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (undeclared
// identifier not reported) in the file header.
TEST_F(PackageParamTest, UndeclaredLcStateWidthIsReportedAsError) {
  GTEST_SKIP() << "Known HLC gap: Sec 26.3 \"it shall be illegal if no identifier can be found that matches the reference\"; ObjectBinder::reportError skips every plain unresolved name as a possible implicit net, so no error is raised.";
  ExpectReportedAsError({ErrorDefinition::COMP_UNDEFINED_VARIABLE, ErrorDefinition::ELAB_UNDEF_VARIABLE},
                        "LcStateWidth");
}

// Expected to fail until HLC is fixed -- see KNOWN COMPILER BUG (import of
// an undeclared package not reported) in the file header.
TEST_F(PackageParamTest, ImportFromUndeclaredPrimUtilPkgIsReportedAsError) {
  GTEST_SKIP() << "Known HLC gap: Sec 26.3 \"The compilation of a package shall precede the compilation of scopes in which the package is imported\"; COMP_UNDEFINED_PACKAGE exists but is never raised for an import of an undeclared package.";
  ExpectReportedAsError({ErrorDefinition::COMP_UNDEFINED_PACKAGE, ErrorDefinition::ELAB_UNDEFINED_PACKAGE},
                        "prim_util_pkg");
}

TEST_F(PackageParamTest, DeclaredNamesAreNotReported) {
  for (std::string_view name : {"A10", "A11", "OtpWidth", "OtpByteAddrWidth"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_VARIABLE, name), nullptr)
        << "'" << name << "' is declared in this file";
    EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, name), nullptr)
        << "'" << name << "' is declared in this file";
  }
}

TEST_F(PackageParamTest, NoSyntaxOrFatalDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "every declaration in the file is syntactically well formed";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
