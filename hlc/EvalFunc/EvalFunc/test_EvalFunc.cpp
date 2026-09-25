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

// Tests for EvalFunc.hlc (tests/EvalFunc/dut.sv):
//   package prim_util_pkg;
//     function automatic integer _clog2(integer value); ... endfunction
//     function automatic integer vbits(integer value);
//       return (value == 1) ? 1 : prim_util_pkg::_clog2(value);
//     endfunction
//   endpackage
//
//   module top ();
//     function automatic integer vbits(integer value);
//       return (value == 1) ? 1 : $clog2(value);
//     endfunction
//     function integer foo; input integer value; return value; endfunction
//     function integer log2; input integer value; ... endfunction
//     function integer log2_2; input integer value; ... endfunction
//
//     localparam RATIO = 30;
//     localparam log2RATIO1 = log2(RATIO);
//     localparam log2RATIO2 = vbits(RATIO);
//     localparam log2RATIO3 = log2_2(RATIO);
//     localparam log2RATIO4 = prim_util_pkg::vbits(RATIO);
//   endmodule
//
// IEEE 1800-2023 Sec 13.4.3 "Constant functions": each of "log2", "vbits"
// (module-local), "log2_2", and "prim_util_pkg::vbits" is invoked with the
// constant argument "RATIO" inside a "localparam" initializer -- the
// canonical constant-function-call context. None of the four take output,
// inout, or ref arguments (all take a single input), so all four calls are
// legal constant function calls. This file has no :should_fail_because:
// tag and is expected to compile cleanly.
//
// Per the pattern already established in
// hlc/Google/chapter-13/13.4.3--const-function/test_13.4.3_const-function.cpp,
// "localparam X = f(...)" produces a Scope::getParamAssigns() binding whose
// getRhs() is either the FuncCall as-parsed, or -- if HLC folds constant
// function calls at elaboration time -- the already-evaluated Constant.
// Both shapes are accepted below; only the resulting FuncCall's target
// name/argument or the folded value is asserted, per the "assert the
// standard, not the implementation" rule.  No .log file was consulted.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/package.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/return_stmt.h>
#include <hldb/vpi_user.h>

namespace hlc {

class EvalFuncTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "EvalFunc.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
  static const hldb::Package *getPkg() {
    return hldb::findByName<hldb::Package>("prim_util_pkg", m_design->getAllPackages());
  }

  template <typename ScopeT>
  static const hldb::Function *findFunc(const ScopeT *scope, std::string_view name) {
    return (scope == nullptr) ? nullptr : hldb::findByName<hldb::Function>(name, scope->getTaskFuncs());
  }

  static const hldb::ParamAssign *findParamAssign(const hldb::Module *m, std::string_view name) {
    return (m == nullptr) ? nullptr : hldb::findByName(name, m->getParamAssigns());
  }
};

TEST_F(EvalFuncTest, ModuleTopExists) { ASSERT_NE(getTop(), nullptr); }

TEST_F(EvalFuncTest, PackagePrimUtilPkgExists) { ASSERT_NE(getPkg(), nullptr); }

TEST_F(EvalFuncTest, PackageHasTwoFunctions) {
  const hldb::Package *const pkg = getPkg();
  ASSERT_NE(pkg, nullptr);
  ASSERT_NE(pkg->getTaskFuncs(), nullptr);
  EXPECT_EQ(pkg->getTaskFuncs()->size(), 2u);
  EXPECT_NE(findFunc(pkg, "_clog2"), nullptr) << "'_clog2' should be reachable via the package scope";
}

TEST_F(EvalFuncTest, ModuleTopHasFourLocalFunctions) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  EXPECT_EQ(top->getTaskFuncs()->size(), 4u);
  EXPECT_NE(findFunc(top, "vbits"), nullptr);
  EXPECT_NE(findFunc(top, "foo"), nullptr);
  EXPECT_NE(findFunc(top, "log2"), nullptr);
  EXPECT_NE(findFunc(top, "log2_2"), nullptr);
}

// function integer foo; input integer value; return value; endfunction
TEST_F(EvalFuncTest, FooIsIdentityFunctionOfItsInput) {
  const hldb::Function *const foo = findFunc(getTop(), "foo");
  ASSERT_NE(foo, nullptr);
  ASSERT_NE(foo->getIODecls(), nullptr);
  ASSERT_EQ(foo->getIODecls()->size(), 1u);
  const hldb::IODecl *const value = foo->getIODecls()->at(0);
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(value->getName(), "value");
  EXPECT_EQ(value->getDirection(), vpiInput)
      << "13.4.3 requires a constant function to have no output/inout/ref arguments";

  const hldb::ReturnStmt *const ret = foo->getStmt<hldb::ReturnStmt>();
  ASSERT_NE(ret, nullptr) << "'foo' body should be a plain ReturnStmt (single statement, no begin-end)";
  const hldb::RefObj *const valRef = ret->getCondition<hldb::RefObj>();
  ASSERT_NE(valRef, nullptr) << "'return value;' should reference the IODecl 'value' directly";
  EXPECT_EQ(valRef->getName(), "value");
  EXPECT_NE(valRef->getActual<hldb::IODecl>(), nullptr);
}

// localparam RATIO = 30;
TEST_F(EvalFuncTest, LocalparamRatioIs30) {
  const hldb::ParamAssign *const pa = findParamAssign(getTop(), "RATIO");
  ASSERT_NE(pa, nullptr) << "'localparam RATIO = 30;' should produce a ParamAssign binding";
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(pa->getRhs());
  ASSERT_NE(rhs, nullptr) << "'30' is a plain constant literal, not a function call";
  EXPECT_EQ(rhs->getDecompile(), "30");
}

// Shared shape check for "localparam log2RATIOn = <call>(RATIO);": either the
// FuncCall survives as-is (name + one Constant "30" argument resolving back
// to the declared function), or HLC folds it to a Constant "5" (log2(30) via
// integer division halving: 30->15->7->3->1, 5 steps -- the ceiling-ish
// bit-count of 30, which every one of the four helper functions computes the
// same way).
static void ExpectConstantFunctionCallOrFoldedFive(const hldb::Any *rhs, std::string_view expectedCalleeName) {
  ASSERT_NE(rhs, nullptr) << "'localparam log2RATIOx = " << expectedCalleeName << "(RATIO);' must have a non-null "
                           << "initializer expression";
  if (const hldb::FuncCall *const call = any_cast<hldb::FuncCall>(rhs)) {
    EXPECT_EQ(call->getName(), expectedCalleeName);
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 1u);
    const hldb::RefObj *const argRef = any_cast<hldb::RefObj>(call->getArguments()->at(0));
    const hldb::Constant *const argConst = any_cast<hldb::Constant>(call->getArguments()->at(0));
    EXPECT_TRUE(argRef != nullptr || argConst != nullptr)
        << "13.4.3 requires the argument to a constant function call to be a constant expression";
  } else if (const hldb::Constant *const folded = any_cast<hldb::Constant>(rhs)) {
    EXPECT_EQ(folded->getDecompile(), "5")
        << "13.4.3 defines a constant function call as evaluated at elaboration time; " << expectedCalleeName
        << "(30) must fold to 5 (30->15->7->3->1, 5 halving steps)";
  } else {
    FAIL() << "ParamAssign RHS for a log2RATIO localparam is neither a FuncCall nor a Constant -- actual AnyType: "
           << static_cast<int>(rhs->getAnyType());
  }
}

TEST_F(EvalFuncTest, Log2Ratio1CallsLog2) {
  const hldb::ParamAssign *const pa = findParamAssign(getTop(), "log2RATIO1");
  ASSERT_NE(pa, nullptr);
  ExpectConstantFunctionCallOrFoldedFive(pa->getRhs(), "log2");
}

TEST_F(EvalFuncTest, Log2Ratio2CallsVbits) {
  const hldb::ParamAssign *const pa = findParamAssign(getTop(), "log2RATIO2");
  ASSERT_NE(pa, nullptr);
  ExpectConstantFunctionCallOrFoldedFive(pa->getRhs(), "vbits");
}

TEST_F(EvalFuncTest, Log2Ratio3CallsLog2_2) {
  const hldb::ParamAssign *const pa = findParamAssign(getTop(), "log2RATIO3");
  ASSERT_NE(pa, nullptr);
  ExpectConstantFunctionCallOrFoldedFive(pa->getRhs(), "log2_2");
}

// localparam log2RATIO4 = prim_util_pkg::vbits(RATIO);  -- package-scoped
// (class/package-scope) constant function call.
TEST_F(EvalFuncTest, Log2Ratio4CallsPackageScopedVbits) {
  const hldb::ParamAssign *const pa = findParamAssign(getTop(), "log2RATIO4");
  ASSERT_NE(pa, nullptr);
  const hldb::RefObj *const rhs = pa->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getPathElems(), nullptr);
  ASSERT_EQ(rhs->getPathElems()->size(), 2u);
  const hldb::RefObj *const pkgRO = any_cast<hldb::RefObj>(rhs->getPathElems()->front());
  ASSERT_NE(pkgRO, nullptr);
  EXPECT_EQ(pkgRO->getName(), std::string_view("prim_util_pkg"));
  const hldb::Package *const pkg = pkgRO->getActual<hldb::Package>();
  ASSERT_NE(pkg, nullptr);
  EXPECT_EQ(pkg->getName(), std::string_view("prim_util_pkg"));
  const hldb::MethodFuncCall *const call = any_cast<hldb::MethodFuncCall>(rhs->getPathElems()->back());
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), std::string_view("vbits"));
  const hldb::Function *const func = call->getTaskFunc<hldb::Function>();
  ASSERT_NE(func, nullptr);
  EXPECT_EQ(func->getName(), std::string_view("vbits"));
}

TEST_F(EvalFuncTest, ModuleTopHasNoNetsVariablesOrContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty());
  EXPECT_TRUE(top->getVariables() == nullptr || top->getVariables()->empty());
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
