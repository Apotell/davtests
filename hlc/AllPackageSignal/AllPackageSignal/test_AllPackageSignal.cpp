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

// Tests for dut.sv (tags: AllPackageSignal)
//   package pkg_b;
//     parameter int ParameterIntEqual4 = 4;
//   endpackage : pkg_b
//
//   package pkg_a;
//     parameter int ParameterIntInPkgA = 4;
//     `ASSERT_STATIC_IN_PACKAGE(ThisNameDoesNotMatter1, 32 == $bits(pkg_b::ParameterIntEqual4))
//     `ASSERT_STATIC_IN_PACKAGE(ThisNameDoesNotMatter2, $bits(ParameterIntInPkgA) == 32)
//     `ASSERT_STATIC_IN_PACKAGE(ThisNameDoesNotMatter3, $bits(ParameterIntInPkgA) == $bits(pkg_b::ParameterIntEqual4))
//   endpackage : pkg_a
//
// Each `ASSERT_STATIC_IN_PACKAGE(name, prop) macro expands to a package-level
// "function automatic bit assert_static_in_package_<name>()". The design has
// 2 packages, both with one local parameter of type "int" (pkg_b's
// ParameterIntEqual4, pkg_a's ParameterIntInPkgA), and pkg_a additionally
// has the 3 expanded functions. The first function's body references
// "pkg_b::ParameterIntEqual4" with an explicit package-qualified path: this
// test walks that reference end to end to confirm cross-package name
// resolution binds the RefObj to pkg_b's actual Parameter.
//
// Checked:
//   - design has exactly 2 packages, pkg_b then pkg_a (source order)
//   - pkg_b has exactly 1 parameter, "ParameterIntEqual4": a localparam
//     with an actual IntTypespec, bound to a single ParamAssign whose rhs
//     is the constant 4
//   - pkg_a has exactly 1 parameter, "ParameterIntInPkgA": same shape,
//     also bound to constant 4
//   - pkg_a has exactly 3 task/funcs (the 3 macro expansions), in source
//     order: assert_static_in_package_ThisNameDoesNotMatter{1,2,3}, each
//     an automatic Function with an actual BitTypespec return type
//   - function 1's body resolves "$bits(pkg_b::ParameterIntEqual4)": the
//     RefObj's 2 path elements are RefObj "pkg_b" (actual: the pkg_b
//     Package) and RefObj "ParameterIntEqual4" (actual: pkg_b's own
//     Parameter object, not a copy or an unresolved symbol)
//   - compiler reports zero errors
//
// NOT CHECKED: the dut.sv comment on ThisNameDoesNotMatter3 claims that
// assertion "does fail", but HLC is a compiler/elaborator with no
// simulation or static-elaboration capability for package-scope functions,
// so that claim is about runtime behavior this test has no way to observe.
// All 3 functions compile and bind identically as far as HLC is concerned
// (0 errors for the whole design).

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/begin.h>
#include <hldb/bit_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/int_typespec.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class AllPackageSignalTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "AllPackageSignal.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPackage(std::string_view name) {
    return hldb::findByName<hldb::Package>(name, m_design->getAllPackages());
  }

  static void expectIntParamEqualsFour(const hldb::Package *pkg, std::string_view paramName) {
    ASSERT_NE(pkg, nullptr);
    ASSERT_NE(pkg->getParameters(), nullptr);
    ASSERT_EQ(pkg->getParameters()->size(), 1u);
    const hldb::Parameter *const param = any_cast<hldb::Parameter>(pkg->getParameters()->at(0));
    ASSERT_NE(param, nullptr);
    EXPECT_EQ(param->getName(), paramName);
    EXPECT_TRUE(param->getLocalParam());
    ASSERT_NE(param->getTypespec(), nullptr);
    EXPECT_NE(param->getTypespec()->getActual<hldb::IntTypespec>(), nullptr);

    ASSERT_NE(pkg->getParamAssigns(), nullptr);
    ASSERT_EQ(pkg->getParamAssigns()->size(), 1u);
    const hldb::ParamAssign *const assign = pkg->getParamAssigns()->at(0);
    ASSERT_NE(assign, nullptr);
    const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
    ASSERT_NE(rhs, nullptr);
    EXPECT_EQ(rhs->getConstType(), vpiUIntConst);
    EXPECT_EQ(rhs->getDecompile(), "4");
  }
};

// --- packages / parameters ---------------------------------------------------

TEST_F(AllPackageSignalTest, DesignHasPkgBThenPkgA) {
  ASSERT_NE(m_design->getAllPackages(), nullptr);
  ASSERT_EQ(m_design->getAllPackages()->size(), 2u);
  EXPECT_EQ(m_design->getAllPackages()->at(0)->getName(), "pkg_b");
  EXPECT_EQ(m_design->getAllPackages()->at(1)->getName(), "pkg_a");
}

TEST_F(AllPackageSignalTest, PkgBHasParameterIntEqual4) {
  expectIntParamEqualsFour(getPackage("pkg_b"), "ParameterIntEqual4");
}

TEST_F(AllPackageSignalTest, PkgAHasParameterIntInPkgA) {
  expectIntParamEqualsFour(getPackage("pkg_a"), "ParameterIntInPkgA");
}

// --- macro-expanded functions in pkg_a --------------------------------------

TEST_F(AllPackageSignalTest, PkgAHasThreeAutomaticBitFunctionsInOrder) {
  const hldb::Package *const pkgA = getPackage("pkg_a");
  ASSERT_NE(pkgA, nullptr);
  ASSERT_NE(pkgA->getTaskFuncs(), nullptr);
  ASSERT_EQ(pkgA->getTaskFuncs()->size(), 3u);

  static constexpr std::string_view kExpectedNames[3] = {
      "assert_static_in_package_ThisNameDoesNotMatter1",
      "assert_static_in_package_ThisNameDoesNotMatter2",
      "assert_static_in_package_ThisNameDoesNotMatter3",
  };
  for (size_t i = 0; i < 3; ++i) {
    const hldb::Function *const func = any_cast<hldb::Function>(pkgA->getTaskFuncs()->at(i));
    ASSERT_NE(func, nullptr) << "task/func " << i << " should be a Function";
    EXPECT_EQ(func->getName(), kExpectedNames[i]);
    EXPECT_TRUE(func->getAutomatic());
    ASSERT_NE(func->getReturn(), nullptr);
    EXPECT_NE(func->getReturn()->getActual<hldb::BitTypespec>(), nullptr) << "macro declares 'function ... bit ...'";
  }
}

// --- cross-package reference resolution -------------------------------------

TEST_F(AllPackageSignalTest, FirstFunctionResolvesPkgBParameterAcrossPackages) {
  const hldb::Package *const pkgA = getPackage("pkg_a");
  const hldb::Package *const pkgB = getPackage("pkg_b");
  ASSERT_NE(pkgA, nullptr);
  ASSERT_NE(pkgB, nullptr);
  ASSERT_NE(pkgA->getTaskFuncs(), nullptr);
  ASSERT_FALSE(pkgA->getTaskFuncs()->empty());
  const hldb::Function *const func = any_cast<hldb::Function>(pkgA->getTaskFuncs()->at(0));
  ASSERT_NE(func, nullptr);

  // function body: bit unused_bit[((32 == $bits(pkg_b::ParameterIntEqual4)) ? 1 : -1)];
  const hldb::Begin *const body = func->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getVariables(), nullptr);
  ASSERT_FALSE(body->getVariables()->empty());
  const hldb::Variable *const unusedBit = hldb::findByName<hldb::Variable>("unused_bit", body->getVariables());
  ASSERT_NE(unusedBit, nullptr);
  ASSERT_NE(unusedBit->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const arrayTypespec = unusedBit->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(arrayTypespec, nullptr);
  const hldb::Range *const range = arrayTypespec->getRange();
  ASSERT_NE(range, nullptr);

  const hldb::Operation *const subtract = range->getLeftExpr<hldb::Operation>();
  ASSERT_NE(subtract, nullptr);
  ASSERT_EQ(subtract->getOpType(), vpiSubOp);
  ASSERT_NE(subtract->getOperands(), nullptr);
  ASSERT_FALSE(subtract->getOperands()->empty());

  const hldb::Operation *const condition = any_cast<hldb::Operation>(subtract->getOperands()->at(0));
  ASSERT_NE(condition, nullptr);
  ASSERT_EQ(condition->getOpType(), vpiConditionOp);
  ASSERT_NE(condition->getOperands(), nullptr);
  ASSERT_EQ(condition->getOperands()->size(), 3u);

  const hldb::Operation *const equal = any_cast<hldb::Operation>(condition->getOperands()->at(0));
  ASSERT_NE(equal, nullptr);
  ASSERT_EQ(equal->getOpType(), vpiEqOp);
  ASSERT_NE(equal->getOperands(), nullptr);
  ASSERT_EQ(equal->getOperands()->size(), 2u);

  const hldb::SysFuncCall *const bitsCall = any_cast<hldb::SysFuncCall>(equal->getOperands()->at(1));
  ASSERT_NE(bitsCall, nullptr);
  EXPECT_EQ(bitsCall->getName(), "$bits");
  ASSERT_NE(bitsCall->getArguments(), nullptr);
  ASSERT_EQ(bitsCall->getArguments()->size(), 1u);

  const hldb::RefObj *const qualifiedRef = any_cast<hldb::RefObj>(bitsCall->getArguments()->at(0));
  ASSERT_NE(qualifiedRef, nullptr);
  EXPECT_EQ(qualifiedRef->getName(), "pkg_b::ParameterIntEqual4");
  ASSERT_NE(qualifiedRef->getPathElems(), nullptr);
  ASSERT_EQ(qualifiedRef->getPathElems()->size(), 2u);

  const hldb::RefObj *const pkgPathElem = any_cast<hldb::RefObj>(qualifiedRef->getPathElems()->at(0));
  ASSERT_NE(pkgPathElem, nullptr);
  EXPECT_EQ(pkgPathElem->getName(), "pkg_b");
  EXPECT_EQ(pkgPathElem->getActual<hldb::Package>(), pkgB) << "'pkg_b' should resolve to pkg_b itself";

  const hldb::RefObj *const paramPathElem = any_cast<hldb::RefObj>(qualifiedRef->getPathElems()->at(1));
  ASSERT_NE(paramPathElem, nullptr);
  EXPECT_EQ(paramPathElem->getName(), "ParameterIntEqual4");
  ASSERT_NE(pkgB->getParameters(), nullptr);
  ASSERT_FALSE(pkgB->getParameters()->empty());
  EXPECT_EQ(paramPathElem->getActual<hldb::Parameter>(), any_cast<hldb::Parameter>(pkgB->getParameters()->at(0)))
      << "'ParameterIntEqual4' should resolve to pkg_b's own Parameter object";
}

// --- compiler diagnostics ---------------------------------------------------

TEST_F(AllPackageSignalTest, CompilesWithNoErrors) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
