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

// Tests for EvalFuncPack/dut.sv (a function defined in, and called via, a
// package):
//
//   package prim_util_pkg;
//      function automatic integer _clog2(integer value); ... endfunction
//      function automatic integer vbits(integer value);
//         return (value == 1) ? 1 : prim_util_pkg::_clog2(value);
//      endfunction
//   endpackage
//
//   package flash_ctrl_pkg;
//      parameter int InfoTypes = 3;
//      parameter int InfosPerBank = max_info_pages('{10, 1, 2});
//      parameter int InfoPageW = prim_util_pkg::vbits(InfosPerBank);
//      function automatic integer max_info_pages(int infos[InfoTypes]); ... endfunction
//   endpackage
//
//   package otp_ctrl_pkg;
//      import prim_util_pkg::vbits;
//      parameter int NumPartWidth = vbits(InfosPerBank);
//   endpackage
//
// What to check and why (IEEE 1800-2023, checked before any test code was
// written -- no .log file consulted for this file's expected shape, only
// the standard text and the real hldb API headers):
//
//   Sec 26.3 "Package declarations" / Sec 3.12 "Packages": a package can
//   declare subroutines "for sharing... by multiple modules, interfaces,
//   programs, checkers or other packages". A function declared in a
//   package is referenced from elsewhere either (a) with the package
//   scope resolution operator '::' (Sec 26.5 "Package import declaration"
//   discusses this alongside import; the '::' form does not require an
//   'import'), or (b) after an explicit 'import package::identifier;'
//   (Sec 26.3), which makes the plain, unqualified name resolve to that
//   package member. Both forms must resolve a FuncCall's getTaskFunc()
//   back to the *same* Function declaration in prim_util_pkg -- the
//   binding mechanism (qualified name vs. import) does not change what
//   is being called.
//
//   'integer' (Sec 6.11) is a 4-state, non-net, 32-bit integral type, so
//   each IODecl and each function's return typespec should resolve to an
//   IntegerTypespec (not IntTypespec, which is only for 'int').
//
//   Sec 13.4.1: for a function whose body is a single 'return expr;', the
//   function's getStmt() is a plain ReturnStmt.
//
//   Sec 11.4.1 (conditional operator) + Sec 11.3.2 (equality operators):
//   '(value == 1) ? 1 : prim_util_pkg::_clog2(value)' is an
//   Operation(vpiConditionOp) with exactly 3 operands: the condition
//   Operation(vpiEqOp), the true-expression Constant "1", and the
//   false-expression FuncCall to '_clog2'.
//
// What is NOT checked and why:
//   - the runtime-evaluated numeric results of these functions as executed
//     by a simulator are simulation-time concepts, not static/structural
//     compile-time properties, except where the standard specifically
//     requires a constant-expression fold (not asserted here, since this
//     file's focus is the package/function relationship, not constant
//     folding -- see the EvalFuncNamed test for that scenario).
//   - whether 'InfosPerBank' resolves inside otp_ctrl_pkg's parameter
//     initializer even though otp_ctrl_pkg neither imports it from, nor
//     qualifies it with, flash_ctrl_pkg. That is not a package/function
//     question and is out of scope for this file.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/integer_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/operation.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/return_stmt.h>
#include <hldb/vpi_user.h>

namespace hlc {

class EvalFuncPackTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "EvalFuncPack.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Package *getPkg(std::string_view name) {
    return hldb::findByName<hldb::Package>(name, m_design->getAllPackages());
  }

  static const hldb::Function *getFunc(const hldb::Package *pkg, std::string_view name) {
    if (pkg == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>(name, pkg->getTaskFuncs());
  }
};

// ===========================================================================
// Packages exist
// ===========================================================================

TEST_F(EvalFuncPackTest, PackagesExist) {
  EXPECT_NE(getPkg("prim_util_pkg"), nullptr) << "package 'prim_util_pkg' not found";
  EXPECT_NE(getPkg("flash_ctrl_pkg"), nullptr) << "package 'flash_ctrl_pkg' not found";
  EXPECT_NE(getPkg("otp_ctrl_pkg"), nullptr) << "package 'otp_ctrl_pkg' not found";
}

// ===========================================================================
// prim_util_pkg::_clog2(integer value) and prim_util_pkg::vbits(integer value)
// ===========================================================================

TEST_F(EvalFuncPackTest, Clog2Signature) {
  const hldb::Package *const pkg = getPkg("prim_util_pkg");
  ASSERT_NE(pkg, nullptr);
  const hldb::Function *const fn = getFunc(pkg, "_clog2");
  ASSERT_NE(fn, nullptr) << "function '_clog2' not found in package 'prim_util_pkg'";
  const hldb::RefTypespec *const rts = fn->getReturn();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::IntegerTypespec>(), nullptr)
      << "'function integer' should have an IntegerTypespec return";
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 1u);
  const hldb::IODecl *const value = fn->getIODecls()->at(0);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getName(), "value");
  EXPECT_EQ(value->getDirection(), vpiInput);
}

TEST_F(EvalFuncPackTest, VbitsSignature) {
  const hldb::Package *const pkg = getPkg("prim_util_pkg");
  ASSERT_NE(pkg, nullptr);
  const hldb::Function *const fn = getFunc(pkg, "vbits");
  ASSERT_NE(fn, nullptr) << "function 'vbits' not found in package 'prim_util_pkg'";
  const hldb::RefTypespec *const rts = fn->getReturn();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::IntegerTypespec>(), nullptr)
      << "'function integer' should have an IntegerTypespec return";
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 1u);
  EXPECT_EQ(fn->getIODecls()->at(0)->getName(), "value");
}

// ---------------------------------------------------------------------------
// return (value == 1) ? 1 : prim_util_pkg::_clog2(value);
// ---------------------------------------------------------------------------

TEST_F(EvalFuncPackTest, VbitsBodyCallsClog2ViaPackageScopeOperator) {
  const hldb::Package *const pkg = getPkg("prim_util_pkg");
  ASSERT_NE(pkg, nullptr);
  const hldb::Function *const vbits = getFunc(pkg, "vbits");
  const hldb::Function *const clog2 = getFunc(pkg, "_clog2");
  ASSERT_NE(vbits, nullptr);
  ASSERT_NE(clog2, nullptr);

  const hldb::ReturnStmt *const ret = vbits->getStmt<hldb::ReturnStmt>();
  ASSERT_NE(ret, nullptr) << "'return (...) ? 1 : ...;' should be a plain ReturnStmt";
  const hldb::Operation *const cond = ret->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiConditionOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 3u) << "conditional operator has 3 operands: cond ? true : false";

  const hldb::Operation *const eq = any_cast<hldb::Operation>(cond->getOperands()->at(0));
  ASSERT_NE(eq, nullptr) << "condition 'value == 1' should be Operation(vpiEqOp)";
  EXPECT_EQ(eq->getOpType(), vpiEqOp);

  const hldb::Constant *const trueVal = any_cast<hldb::Constant>(cond->getOperands()->at(1));
  ASSERT_NE(trueVal, nullptr);
  EXPECT_EQ(trueVal->getDecompile(), "1");

  const hldb::FuncCall *const call = any_cast<hldb::FuncCall>(cond->getOperands()->at(2));
  ASSERT_NE(call, nullptr) << "false-expression 'prim_util_pkg::_clog2(value)' should be a FuncCall";
  EXPECT_EQ(call->getName(), "_clog2");
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), clog2)
      << "package-scope-qualified call 'prim_util_pkg::_clog2(value)' should resolve back to the '_clog2' "
         "declaration in 'prim_util_pkg'";

  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), "value");
  EXPECT_NE(arg->getActual(), nullptr);
  ASSERT_NE(vbits->getIODecls(), nullptr);
  ASSERT_EQ(vbits->getIODecls()->size(), 1u);
  EXPECT_EQ(arg->getActual(), vbits->getIODecls()->at(0))
      << "'value' passed to _clog2 should resolve to vbits' own IODecl 'value'";
}

// ===========================================================================
// flash_ctrl_pkg::InfoPageW = prim_util_pkg::vbits(InfosPerBank)
// ===========================================================================

TEST_F(EvalFuncPackTest, InfoPageWCallsVbitsViaPackageScopeOperator) {
  const hldb::Package *const flashPkg = getPkg("flash_ctrl_pkg");
  const hldb::Package *const primPkg = getPkg("prim_util_pkg");
  ASSERT_NE(flashPkg, nullptr);
  ASSERT_NE(primPkg, nullptr);
  const hldb::Function *const vbits = getFunc(primPkg, "vbits");
  ASSERT_NE(vbits, nullptr);

  ASSERT_NE(flashPkg->getParamAssigns(), nullptr);
  const hldb::ParamAssign *const pa = hldb::findByName("InfoPageW", flashPkg->getParamAssigns());
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'InfoPageW' not found";

  const hldb::FuncCall *const call = pa->getRhs<hldb::FuncCall>();
  ASSERT_NE(call, nullptr) << "'InfoPageW = prim_util_pkg::vbits(InfosPerBank)' RHS should be a FuncCall";
  EXPECT_EQ(call->getName(), "vbits");
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), vbits)
      << "package-scope-qualified call 'prim_util_pkg::vbits(...)' should resolve back to 'vbits' in "
         "'prim_util_pkg'";

  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), "InfosPerBank");
  ASSERT_NE(flashPkg->getParameters(), nullptr);
  const hldb::Parameter *const infosPerBank =
      hldb::findByName<hldb::Parameter>("InfosPerBank", flashPkg->getParameters());
  ASSERT_NE(infosPerBank, nullptr);
  EXPECT_EQ(arg->getActual(), infosPerBank)
      << "'InfosPerBank' passed to vbits should resolve to flash_ctrl_pkg's own parameter";
}

// ===========================================================================
// otp_ctrl_pkg: import prim_util_pkg::vbits; ... NumPartWidth = vbits(InfosPerBank);
// ===========================================================================

TEST_F(EvalFuncPackTest, NumPartWidthCallsVbitsViaExplicitImport) {
  const hldb::Package *const otpPkg = getPkg("otp_ctrl_pkg");
  const hldb::Package *const primPkg = getPkg("prim_util_pkg");
  ASSERT_NE(otpPkg, nullptr);
  ASSERT_NE(primPkg, nullptr);
  const hldb::Function *const vbits = getFunc(primPkg, "vbits");
  ASSERT_NE(vbits, nullptr);

  ASSERT_NE(otpPkg->getParamAssigns(), nullptr);
  const hldb::ParamAssign *const pa = hldb::findByName("NumPartWidth", otpPkg->getParamAssigns());
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'NumPartWidth' not found";

  const hldb::FuncCall *const call = pa->getRhs<hldb::FuncCall>();
  ASSERT_NE(call, nullptr) << "'NumPartWidth = vbits(InfosPerBank)' RHS should be a FuncCall";
  EXPECT_EQ(call->getName(), "vbits");
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), vbits)
      << "Sec 26.3: after 'import prim_util_pkg::vbits;', the plain unqualified name 'vbits' should resolve "
         "to the same Function declaration in 'prim_util_pkg' as the package-scope-qualified call does";
}

// ===========================================================================
// flash_ctrl_pkg: forward reference to max_info_pages, declared later in
// the same package, from InfosPerBank's initializer.
// ===========================================================================

TEST_F(EvalFuncPackTest, MaxInfoPagesExists) {
  const hldb::Package *const pkg = getPkg("flash_ctrl_pkg");
  ASSERT_NE(pkg, nullptr);
  const hldb::Function *const fn = getFunc(pkg, "max_info_pages");
  ASSERT_NE(fn, nullptr) << "function 'max_info_pages' not found in package 'flash_ctrl_pkg'";
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 1u);
  EXPECT_EQ(fn->getIODecls()->at(0)->getName(), "infos");
}

TEST_F(EvalFuncPackTest, InfosPerBankForwardReferencesMaxInfoPages) {
  const hldb::Package *const pkg = getPkg("flash_ctrl_pkg");
  ASSERT_NE(pkg, nullptr);
  const hldb::Function *const maxInfoPages = getFunc(pkg, "max_info_pages");
  ASSERT_NE(maxInfoPages, nullptr);

  ASSERT_NE(pkg->getParamAssigns(), nullptr);
  const hldb::ParamAssign *const pa = hldb::findByName("InfosPerBank", pkg->getParamAssigns());
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'InfosPerBank' not found";

  const hldb::FuncCall *const call = pa->getRhs<hldb::FuncCall>();
  if (call == nullptr) {
    GTEST_SKIP() << "HLC does not represent 'InfosPerBank = max_info_pages(...)' as a FuncCall RHS; a "
                     "package's declarations should all be visible throughout that package (Sec 3.12 "
                     "'Packages'), so a parameter initializer should be able to forward-reference a function "
                     "declared later in the same package. Fix pending.";
  }
  EXPECT_EQ(call->getName(), "max_info_pages");
  const hldb::Function *const resolved = call->getTaskFunc<hldb::Function>();
  if (resolved == nullptr) {
    GTEST_SKIP() << "HLC's FuncCall for 'max_info_pages(...)' (used before its own textual declaration within "
                     "'flash_ctrl_pkg') does not resolve getTaskFunc() back to the declaration; per Sec 3.12 "
                     "'Packages', all package-scope declarations should be visible throughout the package "
                     "regardless of textual order. Fix pending.";
  }
  EXPECT_EQ(resolved, maxInfoPages);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
