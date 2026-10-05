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

// Tests for tests/LowMemPkg (tags: LowMemPkg)
//   command line: -DTOTO=#0 -mp 8 wddr_pkg.sv dut.sv -nobuiltin
//
//   wddr_pkg.sv (after a license block comment):
//     package wddr_pkg;
//     typedef logic my_logic;
//     endpackage
//
//   pack_incl.svh:
//     // toto
//     import wddr_pkg::*;
//
//   dut.sv:
//     `include "pack_incl.svh"
//     module top();
//        my_logic a;
//        logic b;
//     endmodule
//
// The construct under test is a package wildcard import placed in the
// compilation-unit scope through an `include file, and a module that uses
// a type made visible by that import.
//
// What is checked (IEEE 1800-2023):
//   - 26.2: package wddr_pkg exists and declares typedef my_logic whose
//     alias is "logic" (6.18 user-defined types).
//   - 22.4 / 3.12.1: "The contents of files included using one or more
//     `include directives become part of the compilation unit of the file
//     within which they are included" -- pack_incl.svh is recorded as an
//     include of dut.sv.
//   - 3.12.1 / 26.3: "import wddr_pkg::*;" lies outside any other scope, so
//     it is in dut.sv's compilation-unit scope (it is NOT an item of module
//     top). Name lookup in top searches "the portion of the compilation-unit
//     scope defined prior to the reference ... (including any identifiers
//     made available through package import declarations)", so "my_logic"
//     in top resolves to wddr_pkg::my_logic.
//   - 6.8: "my_logic a;" and "logic b;" are variable declarations (no net
//     type keyword), so a and b are Variables of top, not Nets; b is a
//     1-bit scalar logic. a's type is the typedef, whose alias is logic.
//   - no COMP_FAILED_TO_BIND for "my_logic" and no COMP_UNDEFINED_PACKAGE
//     for "wddr_pkg" (the package is compiled before the import).
//
// What is NOT checked and why:
//   - the -DTOTO / -mp options: tool command-line conventions; TOTO is
//     never referenced in the sources.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/import_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/ref_typespec.h>
#include <hldb/source_file.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>

namespace hlc {

class LowMemPkgTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LowMemPkg.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("wddr_pkg", m_design->getAllPackages());
  }
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
  static const hldb::Typedef *getMyLogic() {
    const hldb::Package *const pkg = getPkg();
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("my_logic", pkg->getTypedefs());
  }
};

TEST_F(LowMemPkgTest, PackageExists) { EXPECT_NE(getPkg(), nullptr); }

// 26.2 / 6.18: typedef logic my_logic;
TEST_F(LowMemPkgTest, PackageDeclaresTypedefMyLogicAliasingLogic) {
  const hldb::Typedef *const td = getMyLogic();
  ASSERT_NE(td, nullptr) << "typedef 'my_logic' not found in wddr_pkg";
  EXPECT_FALSE(td->getIsNettype());
  const hldb::RefTypespec *const alias = td->getAlias();
  ASSERT_NE(alias, nullptr);
  ASSERT_NE(alias->getActual(), nullptr);
  const hldb::LogicTypespec *const lt = alias->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "alias should be 'logic'";
  EXPECT_TRUE(lt->getRanges() == nullptr || lt->getRanges()->empty()) << "plain 'logic' has no packed range";
}

// 22.4 / 3.12.1: pack_incl.svh is included by dut.sv.
TEST_F(LowMemPkgTest, IncludeFileIsRecordedUnderDut) {
  ASSERT_NE(m_design->getSourceFiles(), nullptr);
  const hldb::SourceFile *const dut = hldb::findByName<hldb::SourceFile>("dut.sv", m_design->getSourceFiles());
  ASSERT_NE(dut, nullptr);
  ASSERT_NE(dut->getIncludes(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::SourceFile>("pack_incl.svh", dut->getIncludes()), nullptr);
}

// 3.12.1 / 26.3: the wildcard import lives in the compilation-unit scope.
TEST_F(LowMemPkgTest, WildcardImportIsInCompilationUnitScope) {
  const hldb::ImportTypespec *const imp = hldb::findByName<hldb::ImportTypespec>("wddr_pkg", m_design->getTypespecs());
  ASSERT_NE(imp, nullptr) << "'import wddr_pkg::*;' is outside any module/package (compilation-unit scope)";
  const hldb::Constant *const item = imp->getItem();
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(item->getDecompile(), "*");
}

TEST_F(LowMemPkgTest, ImportIsNotAnItemOfModuleTop) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(hldb::findByName<hldb::ImportTypespec>("wddr_pkg", top->getTypespecs()), nullptr)
      << "3.12.1: the import precedes 'module top' and is not inside it";
}

TEST_F(LowMemPkgTest, ModuleTopExists) { EXPECT_NE(getTop(), nullptr); }

// 6.8: no net-type keyword -> variables.
TEST_F(LowMemPkgTest, TopDeclaresVariablesAAndBNotNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("a", top->getVariables()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("b", top->getVariables()), nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty()) << "6.8: neither a nor b is a net";
}

// 3.12.1 / 26.3: my_logic resolves through the compilation-unit import.
TEST_F(LowMemPkgTest, VariableATypeResolvesToImportedTypedef) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rt = a->getTypespec();
  ASSERT_NE(rt, nullptr);
  EXPECT_EQ(rt->getName(), "my_logic");
  ASSERT_NE(rt->getActual(), nullptr) << "'my_logic' must resolve via 'import wddr_pkg::*;'";
  const hldb::TypedefTypespec *const tts = rt->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(tts, nullptr) << "a's type should be the typedef my_logic";
  ASSERT_NE(tts->getTypedef(), nullptr);
  EXPECT_EQ(tts->getTypedef(), getMyLogic()) << "must be wddr_pkg::my_logic";
}

// 6.8: logic b; is a 1-bit scalar logic variable.
TEST_F(LowMemPkgTest, VariableBIsScalarLogic) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  ASSERT_NE(b, nullptr);
  EXPECT_TRUE(b->getScalar());
  EXPECT_FALSE(b->getVector());
  ASSERT_NE(b->getTypespec(), nullptr);
  EXPECT_NE(b->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(LowMemPkgTest, NoBindingOrPackageErrors) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "my_logic"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_PACKAGE, "wddr_pkg"), nullptr);
  EXPECT_EQ(findError(ErrorDefinition::ELAB_UNDEFINED_PACKAGE, "wddr_pkg"), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
