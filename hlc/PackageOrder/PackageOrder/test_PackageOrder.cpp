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

// Validates the HLDB model built for tests/PackageOrder/dut.sv:
//
//   package B;
//     typedef logic C;
//   endpackage;
//
//   package D;
//     import B::*;
//   endpackage;
//
//   package A;
//     import D::*;
//   endpackage; // A
//
// The point of the fixture is a chain of package imports written in
// dependency order: D imports B, and A imports D. IEEE 1800-2023 26.3 requires
// "The compilation of a package shall precede the compilation of scopes in
// which the package is imported", and the source satisfies that: B comes
// before D, and D before A. The names are deliberately not alphabetical, so
// the regression this file exists to catch is HLC compiling packages in an
// order other than the one the source gives (for example sorted by name, A
// first) and then failing to find B or D when it reaches the import.
//
// Each 'endpackage' is followed by ';'. That ';' is legal: at
// compilation-unit scope, description ::= { attribute_instance } package_item
// (A.1.2), and package_item can be the empty item ';'
// (package_or_generate_item_declaration, 26.2 Syntax 26-1).
//
// What is checked, and why:
//   The three packages (26.2)
//     - B, D and A all exist as distinct packages
//     - none has a ': label' after endpackage; '// A' is a comment
//   Package B
//     - exactly 1 Typedef, 'C', whose alias is a LogicTypespec with no
//       packed range: 'logic' with no dimension is a single bit (6.18, 6.11)
//     - no parameters and no variables
//   Package D
//     - records exactly 1 import, the wildcard import of B: an
//       ImportTypespec named "B" whose item is "*"
//     - declares nothing of its own: no typedefs, parameters or variables.
//       A wildcard import makes B's names potentially visible but declares
//       nothing in D (26.3), and nothing in D references C
//   Package A
//     - records exactly 1 import, the wildcard import of D
//     - declares nothing of its own. A wildcard import of D does not make
//       B's C visible in A (imports are not chained unless D exports them,
//       26.6), and nothing in A references C anyway
//   Diagnostics
//     - B and D are found when imported: no COMP_UNDEFINED_PACKAGE or
//       ELAB_UNDEFINED_PACKAGE for either
//     - the file is legal, stray ';' included: zero fatal, syntax and error
//       diagnostics
//
// Reduction and elaboration: there is nothing to reduce and no hierarchy to
// elaborate, so no check is gated on getElaborated().
//
// What is NOT checked, and why:
//   - The order of the packages in Design::getAllPackages(). The standard
//     requires that each package be compiled before it is imported, not that
//     the model list packages in any particular order, so the packages are
//     looked up by name. Whether other packages (for example a built-in
//     one) also appear in that collection is a tool convention, so the
//     package count is not asserted either.
//   - Whether HLC models the stray ';' items in any way. They are empty
//     items; the absence of syntax errors is the evidence that they parse.
//   - Source line numbers encode nothing about SystemVerilog semantics and
//     change whenever the fixture is reformatted, so none is asserted.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/import_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/package.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef.h>

#include <string_view>
#include <vector>

namespace hlc {

class PackageOrderTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PackageOrder.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg(std::string_view name) {
    return hldb::findByName<hldb::Package>(name, m_design->getAllPackages());
  }

  // The imports a package records, in the order the model lists them.
  static std::vector<const hldb::ImportTypespec *> getImports(std::string_view pkgName) {
    std::vector<const hldb::ImportTypespec *> imports;
    const hldb::Package *const pkg = getPkg(pkgName);
    if (pkg == nullptr || pkg->getTypespecs() == nullptr) return imports;
    for (const hldb::Typespec *const ts : *pkg->getTypespecs()) {
      if (const hldb::ImportTypespec *const it = any_cast<hldb::ImportTypespec>(ts)) imports.emplace_back(it);
    }
    return imports;
  }

  // Verifies 'pkgName' records exactly one import, the wildcard import of
  // 'imported'.
  static void ExpectOnlyWildcardImportOf(std::string_view pkgName, std::string_view imported) {
    const std::vector<const hldb::ImportTypespec *> imports = getImports(pkgName);
    ASSERT_EQ(imports.size(), 1u) << "package " << pkgName << " writes exactly one import";
    EXPECT_EQ(imports[0]->getName(), imported) << "26.3: package " << pkgName << " imports from " << imported;
    const hldb::Constant *const item = imports[0]->getItem();
    ASSERT_NE(item, nullptr) << "the import names what it imports";
    EXPECT_EQ(item->getDecompile(), "*") << "26.3: 'import " << imported << "::*;' is a wildcard import";
  }

  // Verifies 'pkgName' declares no typedef, parameter or variable of its
  // own.
  static void ExpectDeclaresNothing(std::string_view pkgName) {
    const hldb::Package *const pkg = getPkg(pkgName);
    ASSERT_NE(pkg, nullptr) << "package '" << pkgName << "' not found";
    EXPECT_TRUE(pkg->getTypedefs() == nullptr || pkg->getTypedefs()->empty())
        << "26.3: an import declares nothing in " << pkgName;
    EXPECT_TRUE(pkg->getParameters() == nullptr || pkg->getParameters()->empty())
        << "package " << pkgName << " declares no parameter";
    EXPECT_TRUE(pkg->getVariables() == nullptr || pkg->getVariables()->empty())
        << "package " << pkgName << " declares no variable";
  }
};

// ---------------------------------------------------------------------------
// package B; package D; package A;
// ---------------------------------------------------------------------------

TEST_F(PackageOrderTest, AllThreePackagesExistAndAreDistinct) {
  const hldb::Package *const b = getPkg("B");
  const hldb::Package *const d = getPkg("D");
  const hldb::Package *const a = getPkg("A");
  ASSERT_NE(b, nullptr) << "package 'B' not found";
  ASSERT_NE(d, nullptr) << "package 'D' not found";
  ASSERT_NE(a, nullptr) << "package 'A' not found";
  EXPECT_NE(b, d);
  EXPECT_NE(d, a);
  EXPECT_NE(b, a);
}

TEST_F(PackageOrderTest, NoPackageHasEndLabel) {
  for (std::string_view name : {"B", "D", "A"}) {
    const hldb::Package *const pkg = getPkg(name);
    ASSERT_NE(pkg, nullptr) << "package '" << name << "' not found";
    EXPECT_EQ(pkg->getEndLabel(), "") << "'endpackage;' carries no ': " << name << "' label";
  }
}

// ---------------------------------------------------------------------------
// package B; typedef logic C; endpackage;
// ---------------------------------------------------------------------------

TEST_F(PackageOrderTest, PackageBDeclaresOneTypedefC) {
  const hldb::Package *const b = getPkg("B");
  ASSERT_NE(b, nullptr);
  ASSERT_NE(b->getTypedefs(), nullptr);
  ASSERT_EQ(b->getTypedefs()->size(), 1u) << "'typedef logic C;' is B's only typedef";
  EXPECT_EQ(b->getTypedefs()->at(0)->getName(), "C");
  EXPECT_TRUE(b->getParameters() == nullptr || b->getParameters()->empty()) << "B declares no parameter";
  EXPECT_TRUE(b->getVariables() == nullptr || b->getVariables()->empty()) << "B declares no variable";
}

TEST_F(PackageOrderTest, TypedefCIsSingleBitLogic) {
  const hldb::Package *const b = getPkg("B");
  ASSERT_NE(b, nullptr);
  const hldb::Typedef *const c = hldb::findByName<hldb::Typedef>("C", b->getTypedefs());
  ASSERT_NE(c, nullptr) << "typedef 'C' not found in B";
  ASSERT_NE(c->getAlias(), nullptr);
  const hldb::LogicTypespec *const lt = c->getAlias()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "6.18: 'C' names the type 'logic'";
  EXPECT_TRUE(lt->getRanges() == nullptr || lt->getRanges()->empty())
      << "'logic' with no packed dimension is a single bit";
}

// ---------------------------------------------------------------------------
// package D; import B::*; endpackage;
// ---------------------------------------------------------------------------

TEST_F(PackageOrderTest, PackageDImportsAllOfB) { ExpectOnlyWildcardImportOf("D", "B"); }

TEST_F(PackageOrderTest, PackageDDeclaresNothingOfItsOwn) { ExpectDeclaresNothing("D"); }

// ---------------------------------------------------------------------------
// package A; import D::*; endpackage;
// ---------------------------------------------------------------------------

TEST_F(PackageOrderTest, PackageAImportsAllOfD) { ExpectOnlyWildcardImportOf("A", "D"); }

TEST_F(PackageOrderTest, PackageADeclaresNothingOfItsOwn) { ExpectDeclaresNothing("A"); }

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

TEST_F(PackageOrderTest, ImportedPackagesAreFound) {
  for (std::string_view name : {"B", "D"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_UNDEFINED_PACKAGE, name), nullptr)
        << "26.3: package " << name << " is compiled before the package that imports it";
    EXPECT_EQ(findError(ErrorDefinition::ELAB_UNDEFINED_PACKAGE, name), nullptr)
        << "26.3: package " << name << " is compiled before the package that imports it";
  }
}

TEST_F(PackageOrderTest, NoFatalSyntaxOrErrorDiagnostics) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0) << "A.1.2: the ';' after each endpackage is a legal empty package_item";
  EXPECT_EQ(stats.nbError, 0) << "the file is legal SystemVerilog";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
