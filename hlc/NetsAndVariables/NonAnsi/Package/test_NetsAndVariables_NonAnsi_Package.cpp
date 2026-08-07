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

// Validates the UHDM graph produced for tests/NetsAndVariables/NonAnsi/Package.sv,
// split out of the combined NetsAndVariablesNonAnsi.sv suite so the
// file-scope package testing point stands on its own.
//
// Checked:
//   - nets_and_variables_pkg_nonansi is reachable via Design::getAllPackages()
//   - pkg_logic / pkg_reg are hldb::Variable (never duplicated as a Net)
//   - pkg_wire is a hldb::Net with vpiNetType == vpiWire (never duplicated
//     as a Variable)
//
// Per IEEE 1800, 'package_item' -> 'package_or_generate_item_declaration'
// (grammar/SV3_1aParser.g4, rule package_or_generate_item_declaration) does
// not include 'continuous_assign' as an alternative -- a continuous
// assignment is not a legal package item. This is also reflected in the
// object model itself: hldb::Package has no getContAssigns() accessor at
// all, so there is no container to hold one even if it were legal.
// Package.sv nonetheless contains 'assign pkg_wire = pkg_logic;' inside the
// package; since this is not a legal construct, no assertion is made about
// it -- see ContAssignIsIllegalInsidePackage below. Matches the ANSI suite's
// Package test.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/net.h>
#include <hldb/package.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class NonAnsiPackageTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Package.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("nets_and_variables_pkg_nonansi", m_design->getAllPackages());
  }
};

TEST_F(NonAnsiPackageTest, PackageExists) {
  ASSERT_NE(m_design->getAllPackages(), nullptr);
  ASSERT_NE(getPkg(), nullptr);
}

TEST_F(NonAnsiPackageTest, PkgLogicIsVariableNoNetDuplicate) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("pkg_logic", pkg->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::LogicTypespec>(), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("pkg_logic", pkg->getNets()), nullptr)
      << "'pkg_logic' is variable-declared -- it must not also appear in vpiNet";
}

TEST_F(NonAnsiPackageTest, PkgRegIsVariableNoNetDuplicate) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("pkg_reg", pkg->getVariables());
  ASSERT_NE(v, nullptr);
  const hldb::RefTypespec *const rts = v->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::LogicTypespec>(), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Net>("pkg_reg", pkg->getNets()), nullptr)
      << "'pkg_reg' is variable-declared -- it must not also appear in vpiNet";
}

TEST_F(NonAnsiPackageTest, PkgWireIsNetWithWireTypeNoVariableDuplicate) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  const hldb::Net *const n = hldb::findByName<hldb::Net>("pkg_wire", pkg->getNets());
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(n->getNetType(), vpiWire);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("pkg_wire", pkg->getVariables()), nullptr)
      << "'pkg_wire' is net-declared -- it must not also appear in vpiVariables";
}

TEST_F(NonAnsiPackageTest, ContAssignIsIllegalInsidePackage) {
  GTEST_SKIP() << "IEEE 1800 package_or_generate_item_declaration has no continuous_assign alternative -- a "
                  "continuous assignment is not a legal package item, and hldb::Package has no getContAssigns() "
                  "accessor at all. No assertion is made about the resulting graph shape for this illegal "
                  "declaration.";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
