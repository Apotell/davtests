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

// Validates the HLDB model built for tests/PackageConst/dut.sv:
//
//   package spi_device_reg_pkg;
//      parameter int unsigned NumCmdInfo = 24;
//   endpackage // spi_device_reg_pkg
//
//   package spi_device_pkg;
//      typedef enum int unsigned {
//         CmdInfoReserveEnd = spi_device_reg_pkg::NumCmdInfo
//      } cmd_info_index_e;
//   endpackage // spi_device_pkg
//
// The point of the fixture is a constant declared in one package being used,
// through the scope resolution operator and with no import (IEEE 1800-2023
// 26.3), as the explicit value of an enum name in a different package.
//
// What is checked, and why:
//   Both packages exist in Design::getAllPackages() as distinct objects.
//   Neither 'endpackage' has a ': label' (the trailing '// name' is only a
//   comment), so both getEndLabel() are empty.
//   Package spi_device_reg_pkg
//     - exactly 1 parameter 'NumCmdInfo' and exactly 1 ParamAssign
//     - 6.20.4: inside a package the keyword 'parameter' is a synonym for
//       'localparam', so getLocalParam() is true
//     - type 'int unsigned' -> IntTypespec, unsigned (6.11)
//     - the ParamAssign binds its LHS to the Parameter and its RHS is
//       Constant "24"
//   Package spi_device_pkg
//     - declares no parameters of its own
//     - owns exactly 1 TypedefTypespec, 'cmd_info_index_e', whose alias is
//       an EnumTypespec (6.19)
//     - the enum's explicit base type 'int unsigned' -> IntTypespec,
//       unsigned (6.19: an explicit base type is recorded)
//     - exactly 1 enum name, 'CmdInfoReserveEnd'
//     - its value expression is the scoped reference to NumCmdInfo: a
//       RefObj path whose prefix is bound to spi_device_reg_pkg and whose
//       last element is bound by object identity to the Parameter in the
//       OTHER package (26.3)
//   Elaboration (6.19: enum values are constant expressions, evaluated at
//   elaboration time)
//     - on an elaborated design the enum name's value is reduced to 24;
//       otherwise the unreduced RefObj bound to NumCmdInfo is asserted
//   Diagnostics
//     - the file is legal: zero fatal, syntax and error diagnostics, and no
//       COMP_FAILED_TO_BIND for 'NumCmdInfo'
//
// What is NOT checked, and why:
//   - The exact spelling HLC gives the scoped reference ("NumCmdInfo" versus
//     "spi_device_reg_pkg::NumCmdInfo") is a tool convention; either is
//     accepted, and the binding is checked by object identity.
//   - HLC represents a package-scoped name as a RefObj path (the package,
//     then the named item). That shape is a model convention; the test
//     follows it and asserts both bindings.
//   - Whether other packages (for example a built-in one) also appear in
//     Design::getAllPackages() is a tool convention, so the package count is
//     not asserted; both user packages are looked up by name instead.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/int_typespec.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>

namespace hlc {

class PackageConstTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackageConst.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getRegPkg() {
    return hldb::findByName<hldb::Package>("spi_device_reg_pkg", m_design->getAllPackages());
  }

  static const hldb::Package *getDevicePkg() {
    return hldb::findByName<hldb::Package>("spi_device_pkg", m_design->getAllPackages());
  }

  static const hldb::Parameter *getNumCmdInfo() {
    const hldb::Package *const pkg = getRegPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>("NumCmdInfo", pkg->getParameters());
  }

  static const hldb::TypedefTypespec *getTypedef() {
    const hldb::Package *const pkg = getDevicePkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::TypedefTypespec>("cmd_info_index_e", pkg->getTypespecs());
  }

  static const hldb::EnumTypespec *getEnum() {
    const hldb::TypedefTypespec *const td = getTypedef();
    if (td == nullptr || td->getTypedef() == nullptr || td->getTypedef()->getAlias() == nullptr) return nullptr;
    return td->getTypedef()->getAlias()->getActual<hldb::EnumTypespec>();
  }

  // The enum declaration the typespec refers to. It owns the base type and
  // the enum constants.
  static const hldb::Enum *getEnumDecl() {
    const hldb::EnumTypespec *const et = getEnum();
    if (et == nullptr) return nullptr;
    return et->getEnum();
  }

  static const hldb::EnumConst *getReserveEnd() {
    const hldb::Enum *const decl = getEnumDecl();
    if (decl == nullptr) return nullptr;
    return hldb::findByName<hldb::EnumConst>("CmdInfoReserveEnd", decl->getEnumConsts());
  }

  // Integer value of a Constant, from its decompiled text ("24") or from a
  // sized/based literal ("32'd24", "32'h18"). The radix HLC picks for a
  // folded value is a tool convention, so every IEEE 1800 integer form is
  // accepted and the numeric value is what gets compared.
  static bool parseConstantValue(const hldb::Constant *c, uint64_t *value) {
    std::string text;
    for (char ch : c->getDecompile()) {
      if (ch != '_') text.push_back(ch);
    }
    if (text.empty()) text = std::string(c->getValue());
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
};

// ---------------------------------------------------------------------------
// Packages
// ---------------------------------------------------------------------------

TEST_F(PackageConstTest, BothPackagesExistAndAreDistinct) {
  const hldb::Package *const reg = getRegPkg();
  const hldb::Package *const dev = getDevicePkg();
  ASSERT_NE(reg, nullptr) << "package 'spi_device_reg_pkg' not found";
  ASSERT_NE(dev, nullptr) << "package 'spi_device_pkg' not found";
  EXPECT_NE(reg, dev);
  EXPECT_EQ(reg->getName(), "spi_device_reg_pkg");
  EXPECT_EQ(dev->getName(), "spi_device_pkg");
}

TEST_F(PackageConstTest, NeitherPackageHasEndLabel) {
  const hldb::Package *const reg = getRegPkg();
  const hldb::Package *const dev = getDevicePkg();
  ASSERT_NE(reg, nullptr);
  ASSERT_NE(dev, nullptr);
  EXPECT_EQ(reg->getEndLabel(), "") << "'// spi_device_reg_pkg' after endpackage is a comment, not a label";
  EXPECT_EQ(dev->getEndLabel(), "") << "'// spi_device_pkg' after endpackage is a comment, not a label";
}

// ---------------------------------------------------------------------------
// parameter int unsigned NumCmdInfo = 24;
// ---------------------------------------------------------------------------

TEST_F(PackageConstTest, RegPkgHasExactlyOneParameterNumCmdInfo) {
  const hldb::Package *const pkg = getRegPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getParameters(), nullptr);
  EXPECT_EQ(pkg->getParameters()->size(), 1u);
  const hldb::Parameter *const p = getNumCmdInfo();
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getName(), "NumCmdInfo");
}

TEST_F(PackageConstTest, NumCmdInfoIsLocalParam) {
  const hldb::Parameter *const p = getNumCmdInfo();
  ASSERT_NE(p, nullptr);
  EXPECT_TRUE(p->getLocalParam()) << "6.20.4: in a package, 'parameter' is a synonym for 'localparam'";
}

TEST_F(PackageConstTest, NumCmdInfoIsUnsignedInt) {
  const hldb::Parameter *const p = getNumCmdInfo();
  ASSERT_NE(p, nullptr);
  ASSERT_NE(p->getTypespec(), nullptr);
  const hldb::IntTypespec *const ts = p->getTypespec()->getActual<hldb::IntTypespec>();
  ASSERT_NE(ts, nullptr) << "'int unsigned' should resolve to IntTypespec";
  EXPECT_FALSE(ts->getSigned()) << "'int unsigned' overrides int's default signedness";
}

TEST_F(PackageConstTest, NumCmdInfoParamAssignIsTwentyFour) {
  const hldb::Package *const pkg = getRegPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getParamAssigns(), nullptr);
  ASSERT_EQ(pkg->getParamAssigns()->size(), 1u);
  const hldb::ParamAssign *const pa = pkg->getParamAssigns()->at(0);
  ASSERT_NE(pa, nullptr);
  const hldb::RefObj *const lhs = pa->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "NumCmdInfo");
  EXPECT_EQ(lhs->getActual<hldb::Parameter>(), getNumCmdInfo());
  const hldb::Constant *const rhs = pa->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "24");
}

// ---------------------------------------------------------------------------
// typedef enum int unsigned { ... } cmd_info_index_e;
// ---------------------------------------------------------------------------

TEST_F(PackageConstTest, DevicePkgDeclaresNoParameters) {
  const hldb::Package *const pkg = getDevicePkg();
  ASSERT_NE(pkg, nullptr);
  EXPECT_TRUE(pkg->getParameters() == nullptr || pkg->getParameters()->empty())
      << "spi_device_pkg only references NumCmdInfo; it must not get its own copy";
}

TEST_F(PackageConstTest, DevicePkgOwnsOneTypedefCmdInfoIndexE) {
  const hldb::Package *const pkg = getDevicePkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getTypespecs(), nullptr);
  size_t typedefCount = 0;
  for (const hldb::Typespec *const ts : *pkg->getTypespecs()) {
    if (any_cast<hldb::TypedefTypespec>(ts) != nullptr) ++typedefCount;
  }
  EXPECT_EQ(typedefCount, 1u) << "spi_device_pkg declares exactly one typedef";
  const hldb::TypedefTypespec *const td = getTypedef();
  ASSERT_NE(td, nullptr);
  EXPECT_EQ(td->getName(), "cmd_info_index_e");
}

TEST_F(PackageConstTest, TypedefAliasIsEnumTypespec) {
  EXPECT_NE(getEnum(), nullptr) << "typedef 'cmd_info_index_e' should alias an EnumTypespec";
}

TEST_F(PackageConstTest, EnumBaseTypeIsUnsignedInt) {
  const hldb::Enum *const decl = getEnumDecl();
  ASSERT_NE(decl, nullptr);
  const hldb::RefTypespec *const base = decl->getBaseTypespec();
  ASSERT_NE(base, nullptr) << "6.19: the explicit base type 'int unsigned' must be recorded";
  const hldb::IntTypespec *const ts = base->getActual<hldb::IntTypespec>();
  ASSERT_NE(ts, nullptr);
  EXPECT_FALSE(ts->getSigned()) << "the enum base type is 'int unsigned'";
}

TEST_F(PackageConstTest, EnumHasSingleNameCmdInfoReserveEnd) {
  const hldb::Enum *const decl = getEnumDecl();
  ASSERT_NE(decl, nullptr);
  ASSERT_NE(decl->getEnumConsts(), nullptr);
  ASSERT_EQ(decl->getEnumConsts()->size(), 1u);
  EXPECT_EQ(decl->getEnumConsts()->at(0)->getName(), "CmdInfoReserveEnd");
}

// ---------------------------------------------------------------------------
// CmdInfoReserveEnd = spi_device_reg_pkg::NumCmdInfo
// ---------------------------------------------------------------------------

TEST_F(PackageConstTest, EnumValueIsCrossPackageConstant) {
  const hldb::EnumConst *const ec = getReserveEnd();
  ASSERT_NE(ec, nullptr);
  ASSERT_NE(ec->getValue(), nullptr) << "'CmdInfoReserveEnd' has an explicit value in the source";
  if (m_design->getElaborated()) {
    // 6.19: enum values are constant expressions, fixed at elaboration.
    const hldb::Constant *const value = ec->getValue<hldb::Constant>();
    ASSERT_NE(value, nullptr) << "on an elaborated design the enum value should be reduced to a Constant";
    uint64_t v = 0;
    ASSERT_TRUE(parseConstantValue(value, &v)) << "unparsable constant '" << value->getDecompile() << "'";
    EXPECT_EQ(v, 24u) << "CmdInfoReserveEnd = spi_device_reg_pkg::NumCmdInfo = 24";
  } else {
    const hldb::RefObj *const path = ec->getValue<hldb::RefObj>();
    ASSERT_NE(path, nullptr) << "unreduced, the value is a package-scoped RefObj path to NumCmdInfo";
    ASSERT_NE(path->getPathElems(), nullptr);
    ASSERT_EQ(path->getPathElems()->size(), 2u) << "the package, then the parameter";
    const hldb::RefObj *const scope = any_cast<hldb::RefObj>(path->getPathElems()->at(0));
    ASSERT_NE(scope, nullptr);
    EXPECT_EQ(scope->getName(), "spi_device_reg_pkg");
    ASSERT_NE(getRegPkg(), nullptr);
    EXPECT_EQ(scope->getActual<hldb::Package>(), getRegPkg())
        << "26.3: the scope prefix names package spi_device_reg_pkg";
    const hldb::RefObj *const ref = any_cast<hldb::RefObj>(path->getPathElems()->at(1));
    ASSERT_NE(ref, nullptr);
    EXPECT_TRUE(ref->getName() == "NumCmdInfo" || ref->getName() == "spi_device_reg_pkg::NumCmdInfo")
        << "unexpected reference name '" << ref->getName() << "'";
    ASSERT_NE(getNumCmdInfo(), nullptr);
    EXPECT_EQ(ref->getActual<hldb::Parameter>(), getNumCmdInfo())
        << "26.3: the scoped name must bind to NumCmdInfo declared in spi_device_reg_pkg";
  }
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

TEST_F(PackageConstTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "NumCmdInfo"), nullptr)
      << "the package-scoped reference must bind (IEEE 1800-2023 26.3)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
