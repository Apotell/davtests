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

// Tests for tests/LogicTypespec/dut.sv:
//
//   package my_package;
//   typedef logic [1:0] typedef_logic_typespec;
//   endpackage: my_package
//
//   module logic_typespec ();
//
//   my_package::typedef_logic_typespec    [1:0][7:0] my_instance;
//
//   endmodule : logic_typespec
//
// -- rules under test ---------------------------------------------------
//
// IEEE 1800-2023 Sec 6.18 / 26.2: 'typedef logic [1:0] typedef_logic_typespec;'
//   declares a type name inside package 'my_package' aliasing an unsigned
//   2-bit logic vector (Sec 6.11.3, Sec 7.4.1).
// IEEE 1800-2023 Sec 26.3 "Referencing data in packages": the package scope
//   resolution form 'my_package::typedef_logic_typespec' references that
//   type name directly from the package.
// IEEE 1800-2023 Sec 7.4.1 "Packed arrays": packed arrays can be made of
//   other packed arrays; the typedef is a packed array of logic, so
//   '[1:0][7:0]' adds two more packed dimensions on top of it.
// IEEE 1800-2023 Sec 20.7 "Array query functions": the slowest-varying
//   dimension is the leftmost one, and "intermediate type definitions are
//   expanded first" -- so the full type of 'my_instance' is a packed array
//   indexed [1:0] (outermost), whose element is a packed array indexed
//   [7:0], whose element is 'typedef_logic_typespec' (itself [1:0] logic).
// IEEE 1800-2023 Sec 6.8: the declaration has no net-type keyword, so
//   'my_instance' is a Variable, never a Net.
// IEEE 1800-2023 Sec 26.2 / 23.2: the optional end labels ': my_package'
//   and ': logic_typespec' must match the declared names.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/package.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class LogicTypespecTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "LogicTypespec.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("my_package", m_design->getAllPackages());
  }

  static const hldb::Module *getMod() {
    return hldb::findByName<hldb::Module>("logic_typespec", m_design->getAllModules());
  }

  static const hldb::Variable *getMyInstance() {
    const hldb::Module *const m = getMod();
    if (m == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("my_instance", m->getVariables());
  }

  static const hldb::ArrayTypespec *getOuterArray() {
    const hldb::Variable *const v = getMyInstance();
    if (v == nullptr || v->getTypespec() == nullptr) return nullptr;
    return v->getTypespec()->getActual<hldb::ArrayTypespec>();
  }

  static void expectRange(const hldb::Range *r, std::string_view left, std::string_view right) {
    ASSERT_NE(r, nullptr);
    const hldb::Constant *const l = r->getLeftExpr<hldb::Constant>();
    const hldb::Constant *const rr = r->getRightExpr<hldb::Constant>();
    ASSERT_NE(l, nullptr);
    ASSERT_NE(rr, nullptr);
    EXPECT_EQ(l->getDecompile(), left);
    EXPECT_EQ(rr->getDecompile(), right);
  }
};

// ---------------------------------------------------------------------------
// Existence and end labels
// ---------------------------------------------------------------------------

TEST_F(LogicTypespecTest, PackageAndModuleExist) {
  EXPECT_NE(getPkg(), nullptr) << "package 'my_package' not found";
  EXPECT_NE(getMod(), nullptr) << "module 'logic_typespec' not found";
}

TEST_F(LogicTypespecTest, EndLabelsMatchNames) {
  const hldb::Package *const pkg = getPkg();
  const hldb::Module *const mod = getMod();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(mod, nullptr);
  EXPECT_EQ(pkg->getEndLabel(), std::string_view("my_package"));
  EXPECT_EQ(mod->getEndLabel(), std::string_view("logic_typespec"));
}

// ---------------------------------------------------------------------------
// Package typedef -- Sec 6.18
// ---------------------------------------------------------------------------

TEST_F(LogicTypespecTest, PackageTypedefAliasesUnsignedLogic1To0) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  const hldb::Typedef *const td = hldb::findByName<hldb::Typedef>("typedef_logic_typespec", pkg->getTypedefs());
  ASSERT_NE(td, nullptr) << "typedef 'typedef_logic_typespec' not found in 'my_package'";
  ASSERT_NE(td->getAlias(), nullptr);
  ASSERT_NE(td->getAlias()->getActual(), nullptr);
  ASSERT_EQ(td->getAlias()->getActual()->getAnyType(), hldb::AnyType::LogicTypespec);
  const hldb::LogicTypespec *const lts = td->getAlias()->getActual<hldb::LogicTypespec>();
  EXPECT_FALSE(lts->getSigned());
  ASSERT_NE(lts->getRanges(), nullptr);
  ASSERT_EQ(lts->getRanges()->size(), 1u);
  expectRange(lts->getRanges()->at(0), "1", "0");
}

TEST_F(LogicTypespecTest, PackageOwnsTypedefTypespec) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  const hldb::TypedefTypespec *const tts =
      hldb::findByName<hldb::TypedefTypespec>("typedef_logic_typespec", pkg->getTypespecs());
  ASSERT_NE(tts, nullptr);
  ASSERT_NE(tts->getTypedef(), nullptr);
  EXPECT_EQ(tts->getTypedef()->getName(), std::string_view("typedef_logic_typespec"));
}

// ---------------------------------------------------------------------------
// my_instance -- Sec 6.8, 7.4.1, 20.7
// ---------------------------------------------------------------------------

TEST_F(LogicTypespecTest, MyInstanceIsSoleVariableNotNet) {
  const hldb::Module *const mod = getMod();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getVariables(), nullptr);
  EXPECT_EQ(mod->getVariables()->size(), 1u);
  EXPECT_NE(getMyInstance(), nullptr) << "'my_instance' should be a Variable per Sec 6.8";
  EXPECT_EQ(hldb::findByName<hldb::Net>("my_instance", mod->getNets()), nullptr);
}

TEST_F(LogicTypespecTest, OuterDimensionIsPacked1To0) {
  const hldb::Variable *const v = getMyInstance();
  ASSERT_NE(v, nullptr);
  ASSERT_NE(v->getTypespec(), nullptr);
  ASSERT_NE(v->getTypespec()->getActual(), nullptr);
  ASSERT_EQ(v->getTypespec()->getActual()->getAnyType(), hldb::AnyType::ArrayTypespec);
  const hldb::ArrayTypespec *const outer = getOuterArray();
  EXPECT_TRUE(outer->getPacked()) << "'[1:0]' before the identifier is a packed dimension (Sec 7.4.1)";
  EXPECT_EQ(outer->getArrayType(), vpiStaticArray);
  expectRange(outer->getRange(), "1", "0");
}

TEST_F(LogicTypespecTest, MiddleDimensionIsPacked7To0) {
  const hldb::ArrayTypespec *const outer = getOuterArray();
  ASSERT_NE(outer, nullptr);
  ASSERT_NE(outer->getElemTypespec(), nullptr);
  ASSERT_NE(outer->getElemTypespec()->getActual(), nullptr);
  ASSERT_EQ(outer->getElemTypespec()->getActual()->getAnyType(), hldb::AnyType::ArrayTypespec)
      << "element of the [1:0] dimension is the [7:0] packed array (Sec 20.7)";
  const hldb::ArrayTypespec *const middle = outer->getElemTypespec()->getActual<hldb::ArrayTypespec>();
  EXPECT_TRUE(middle->getPacked());
  EXPECT_EQ(middle->getArrayType(), vpiStaticArray);
  expectRange(middle->getRange(), "7", "0");
}

TEST_F(LogicTypespecTest, InnermostElementIsPackageTypedef) {
  const hldb::ArrayTypespec *const outer = getOuterArray();
  ASSERT_NE(outer, nullptr);
  ASSERT_NE(outer->getElemTypespec(), nullptr);
  const hldb::ArrayTypespec *const middle = outer->getElemTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(middle, nullptr);
  ASSERT_NE(middle->getElemTypespec(), nullptr);
  ASSERT_NE(middle->getElemTypespec()->getActual(), nullptr);
  ASSERT_EQ(middle->getElemTypespec()->getActual()->getAnyType(), hldb::AnyType::TypedefTypespec);
  const hldb::TypedefTypespec *const tts = middle->getElemTypespec()->getActual<hldb::TypedefTypespec>();
  EXPECT_EQ(tts->getName(), std::string_view("typedef_logic_typespec"));
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  EXPECT_EQ(tts, hldb::findByName<hldb::TypedefTypespec>("typedef_logic_typespec", pkg->getTypespecs()))
      << "'my_package::typedef_logic_typespec' must resolve to the package's typedef (Sec 26.3)";
}

TEST_F(LogicTypespecTest, MyInstanceHasNoInitializer) {
  const hldb::Variable *const v = getMyInstance();
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getValue(), nullptr);
}

// ---------------------------------------------------------------------------
// Diagnostics -- the source is legal
// ---------------------------------------------------------------------------

TEST_F(LogicTypespecTest, CompilerReportsZeroErrors) {
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
