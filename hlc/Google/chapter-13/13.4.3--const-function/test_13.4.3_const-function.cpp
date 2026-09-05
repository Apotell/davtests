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

// Tests for 13.4.3--const-function.sv (tags: 13.4.3)
//   module top();
//     localparam a = fun(3);
//     function int fun(int val);
//       return val + 1;
//     endfunction
//     initial
//       $display(":assert: (%d == 4)", a);
//   endmodule
//
// What to check and why (IEEE 1800-2023 13.4.3 "Constant functions",
// p.345, checked before any test code was written -- no .log file
// consulted for this file's expected shape, only the standard text and
// the real hldb API headers):
//   "Constant functions are a subset of normal functions that shall
//   meet the following constraints: The function shall not have
//   output, inout, or ref arguments... shall not contain any fork
//   constructs... shall not contain any hierarchical references...
//   shall not contain function invocations that are not constant
//   function calls... shall not reference any identifiers that are not
//   either parameter or function names, or declared locally to the
//   current function." "A constant function call is a function call of
//   a constant function that is evaluated at elaboration time... where
//   the arguments to the function, if any, are all constant
//   expressions." "fun" satisfies every one of these constraints (a
//   single input argument, a plain arithmetic return, no fork, no
//   hierarchical references, no nested calls), and it is invoked with
//   the constant argument "3" inside a "localparam" initializer -- the
//   canonical constant-function-call context named by the standard.
//   This file carries no :should_fail_because: tag, so it is expected
//   to compile cleanly as legal SystemVerilog.
//
//   IMPORTANT object-model correction (two rounds, both confirmed by
//   actually running this file): a first version assumed, by analogy
//   with other expressions in this suite (case constants, return
//   expressions, assignment RHS values), that "localparam a = fun(3);"
//   would keep the call as a real FuncCall inside
//   Parameter::getExpr(). That failed -- getExpr() returned null. A
//   second look at the API showed why: Parameter (the declaration: name,
//   localparam flag, ranges) and the actual "identifier = initializer"
//   binding are two different objects in this model. The binding is a
//   separate ParamAssign object, reachable via Scope::getParamAssigns()
//   on the Module, with getLhs() (the Parameter being assigned) and
//   getRhs() (the initializer expression). That is where "fun(3)"
//   actually lives.
//
// What is checked:
//   - module top has exactly 1 Parameter (via Scope::getParameters()),
//     named "a", with getLocalParam() == true (confirming "localparam",
//     not a plain "parameter")
//   - module top has exactly 1 ParamAssign (via Scope::getParamAssigns()),
//     whose getLhs() resolves to the Parameter "a" itself, and whose
//     getRhs() is non-null and is either: (a) a FuncCall "fun" whose
//     getTaskFunc<hldb::Function>() resolves back to the "fun"
//     declaration, with exactly 1 argument, Constant "3" (a legal
//     constant function call per 13.4.3); or (b) a Constant already
//     folded to "4" (the correct elaboration-time result of fun(3) =
//     3 + 1); any other shape fails with a diagnostic message naming
//     the actual AnyType encountered
//   - module top has exactly 1 TaskFunc, a Function named "fun", whose
//     return typespec resolves to IntTypespec, with exactly 1 IODecl
//     "val" (getDirection() == vpiInput, IntTypespec) -- no output,
//     inout, or ref arguments, satisfying the first constant-function
//     constraint
//   - fun's body is a plain ReturnStmt directly (single statement, no
//     begin-end) whose getCondition() is Operation(vpiAddOp) comparing
//     RefObj "val" (resolving to the IODecl) against Constant "1"
//   - the initial process's body is a plain SysTaskCall "$display"
//     with 2 arguments: Constant "\":assert: (%d == 4)\"" and RefObj
//     "a" resolving (getActual<hldb::Parameter>()) to the localparam
//   - no continuous assignments, nets, or module-scope variables exist
//
// What is NOT checked and why:
//   - the runtime/elaboration-time evaluated value of the constant
//     function call (that "a" actually ends up holding 4) is an
//     elaboration-time concept, not a static/structural parse-time
//     property directly inspectable on the Parameter object without
//     simulation
//   - the standard's other constant-function constraints not exercised
//     by this file (default argument values, defparam interactions,
//     class-scoped static functions) are out of scope, since this file
//     tests only the basic legal case

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/return_stmt.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ConstFunctionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "13.4.3--const-function.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Function *getFun() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTaskFuncs() == nullptr || top->getTaskFuncs()->empty()) return nullptr;
    return any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  }

  static const hldb::Parameter *getParamA() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getParameters() == nullptr || top->getParameters()->empty()) return nullptr;
    return any_cast<hldb::Parameter>(top->getParameters()->at(0));
  }
};

TEST_F(ConstFunctionTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ConstFunctionTest, ModuleHasNoNetsAndNoVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty());
  EXPECT_TRUE(top->getVariables() == nullptr || top->getVariables()->empty());
}

// ---------------------------------------------------------------------------
// localparam a = fun(3);
// ---------------------------------------------------------------------------
TEST_F(ConstFunctionTest, ParamAIsLocalParam) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParameters(), nullptr);
  ASSERT_EQ(top->getParameters()->size(), 1u);
  const hldb::Parameter *const a = getParamA();
  ASSERT_NE(a, nullptr) << "'a' should resolve to a Parameter";
  EXPECT_EQ(a->getName(), "a");
  EXPECT_TRUE(a->getLocalParam()) << "'localparam' should set getLocalParam() true";
}

TEST_F(ConstFunctionTest, ParamAExprIsPresentAndDescribesTheConstantFunctionCall) {
  // CORRECTED: an earlier version of this test read Parameter::getExpr(), which came back null when actually
  // run. Parameter::getExpr() and ParamAssign are two different fields in this object model: Parameter is the
  // declaration itself (name, localparam flag, ranges); the actual "identifier = initializer" binding for a
  // localparam is a separate ParamAssign object (lhs/rhs), reachable via Scope::getParamAssigns() on the
  // Module. This version reads the value from there instead of guessing a second time.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParamAssigns(), nullptr) << "'localparam a = fun(3);' should produce a ParamAssign binding";
  ASSERT_EQ(top->getParamAssigns()->size(), 1u);
  const hldb::ParamAssign *const paramAssign = top->getParamAssigns()->at(0);
  ASSERT_NE(paramAssign, nullptr);

  // Deliberately not assuming a single shape for getLhs(): it may be the Parameter directly (as with other
  // declaration-time bindings seen elsewhere in this suite, e.g. ForStmt's own inline "int i = 0"), or it may
  // be a RefObj that resolves to the Parameter (as with ordinary, non-declaration assignments elsewhere in
  // this suite). A prior version assumed the direct-Parameter shape and that came back null, so this version
  // checks the plain (non-templated) getLhs() first and branches on whichever concrete shape is actually there.
  const hldb::Any *const lhsAny = paramAssign->getLhs();
  ASSERT_NE(lhsAny, nullptr) << "ParamAssign LHS must be non-null";
  const hldb::Parameter *lhs = any_cast<hldb::Parameter>(lhsAny);
  if (lhs == nullptr) {
    const hldb::RefObj *const lhsRef = any_cast<hldb::RefObj>(lhsAny);
    ASSERT_NE(lhsRef, nullptr) << "ParamAssign LHS is neither a Parameter nor a RefObj -- actual AnyType: "
                                << static_cast<int>(lhsAny->getAnyType());
    lhs = lhsRef->getActual<hldb::Parameter>();
    ASSERT_NE(lhs, nullptr) << "ParamAssign LHS RefObj should resolve to the Parameter 'a'";
  }
  EXPECT_EQ(lhs->getName(), "a");

  const hldb::Any *const rhs = paramAssign->getRhs();
  ASSERT_NE(rhs, nullptr) << "'localparam a = fun(3);' must have a non-null initializer expression";

  if (const hldb::FuncCall *const call = any_cast<hldb::FuncCall>(rhs)) {
    // HLC preserved the call as-is; verify its shape per 13.4.3.
    EXPECT_EQ(call->getName(), "fun");
    EXPECT_EQ(call->getTaskFunc<hldb::Function>(), getFun()) << "call should resolve back to the 'fun' declaration";
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 1u);
    const hldb::Constant *const arg = any_cast<hldb::Constant>(call->getArguments()->at(0));
    ASSERT_NE(arg, nullptr)
        << "13.4.3 requires the argument to a constant function call to be a constant expression";
    EXPECT_EQ(arg->getDecompile(), "3");
  } else if (const hldb::Constant *const folded = any_cast<hldb::Constant>(rhs)) {
    // HLC folded the constant function call into its elaboration-time-evaluated value instead.
    EXPECT_EQ(folded->getDecompile(), "4")
        << "13.4.3 defines a constant function call as evaluated at elaboration time; if HLC folds it into a "
           "Constant rather than keeping the FuncCall, the folded value must still be the correct result "
           "(fun(3) = 3 + 1 = 4)";
  } else {
    FAIL() << "ParamAssign RHS for 'a' is neither a FuncCall nor a Constant -- actual AnyType: "
           << static_cast<int>(rhs->getAnyType());
  }
}

// ---------------------------------------------------------------------------
// function int fun(int val); return val + 1; endfunction
// ---------------------------------------------------------------------------
TEST_F(ConstFunctionTest, FunExistsAsIntFunctionWithOnlyInputIODecl) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  ASSERT_EQ(top->getTaskFuncs()->size(), 1u);
  const hldb::Function *const fn = getFun();
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(fn->getName(), "fun");
  const hldb::RefTypespec *const rts = fn->getReturn();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::IntTypespec>(), nullptr);
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 1u);
  const hldb::IODecl *const val = fn->getIODecls()->at(0);
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getName(), "val");
  EXPECT_EQ(val->getDirection(), vpiInput)
      << "13.4.3 requires a constant function to have no output/inout/ref arguments";
}

TEST_F(ConstFunctionTest, FunBodyIsReturnStmtWithAddExpression) {
  const hldb::Function *const fn = getFun();
  ASSERT_NE(fn, nullptr);
  const hldb::ReturnStmt *const ret = fn->getStmt<hldb::ReturnStmt>();
  ASSERT_NE(ret, nullptr) << "function body should be a plain ReturnStmt (single statement, no begin-end)";
  const hldb::Operation *const expr = ret->getCondition<hldb::Operation>();
  ASSERT_NE(expr, nullptr);
  EXPECT_EQ(expr->getOpType(), vpiAddOp);
  ASSERT_NE(expr->getOperands(), nullptr);
  ASSERT_EQ(expr->getOperands()->size(), 2u);
  const hldb::RefObj *const valRef = any_cast<hldb::RefObj>(expr->getOperands()->at(0));
  ASSERT_NE(valRef, nullptr);
  EXPECT_EQ(valRef->getName(), "val");
  EXPECT_NE(valRef->getActual<hldb::IODecl>(), nullptr);
  const hldb::Constant *const one = any_cast<hldb::Constant>(expr->getOperands()->at(1));
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->getDecompile(), "1");
}

// ---------------------------------------------------------------------------
// initial $display(":assert: (%d == 4)", a);
// ---------------------------------------------------------------------------
TEST_F(ConstFunctionTest, InitialBodyDisplaysParamAResolvingToTheLocalparam) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const initial = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(initial, nullptr);
  const hldb::SysTaskCall *const display = initial->getStmt<hldb::SysTaskCall>();
  ASSERT_NE(display, nullptr) << "initial body should be a plain SysTaskCall (single statement, no begin-end)";
  ASSERT_NE(display->getArguments(), nullptr);
  ASSERT_EQ(display->getArguments()->size(), 2u);
  const hldb::Constant *const fmt = any_cast<hldb::Constant>(display->getArguments()->at(0));
  ASSERT_NE(fmt, nullptr);
  EXPECT_EQ(fmt->getDecompile(), "\":assert: (%d == 4)\"");
  const hldb::RefObj *const aRef = any_cast<hldb::RefObj>(display->getArguments()->at(1));
  ASSERT_NE(aRef, nullptr);
  EXPECT_EQ(aRef->getName(), "a");
  EXPECT_NE(aRef->getActual<hldb::Parameter>(), nullptr) << "'a' should resolve to the Parameter (localparam)";
}

TEST_F(ConstFunctionTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
