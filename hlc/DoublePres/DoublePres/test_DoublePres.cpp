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

// Tests for dut.sv (tags: DoublePres)
//   module top(input clk_i, output b);
//     dut #(.CLKFBOUT_PHASE(12.50/3.0)) d (.a(clk_i), .b(b));
//   endmodule
//
//   module dut #(parameter real CLKFBOUT_PHASE = 1142.500) (input a, output b);
//     parameter err = CLKFBOUT_PHASE / 0;
//     parameter f = 9;
//     parameter r = 5.7;
//     parameter average_delay = (r + f)/2;
//     parameter A1 = 0.1 * 0.5;
//     parameter A2 = 0.1 - 0.5;
//     parameter A3 = - 0.6;
//     ...
//     parameter A9  = 10.3 % 2.1;
//     parameter A11 = 2 ** 8;
//     parameter A13 = A8 ? A12 : A11;
//     parameter A14 = 20 % 0;
//     function incr_d; integer incr_d; incr_d = 10.1; incr_d++; return incr_d; endfunction
//     parameter A15 = incr_d();
//     if (A1 == 0.05) begin GOOD good1(); end
//     ...
//   endmodule
//
// This file exercises double-precision (IEEE 1800-2023 5.7.2/6.12 "real")
// arithmetic in constant_expression contexts, checked before any test code
// was written:
//   - 6.20.2: "parameter real CLKFBOUT_PHASE = 1142.500" gives an explicit
//     'real' type to the parameter (unlike 6.20.2--parameter_real.sv, which
//     leaves the type implicit); its default ParamAssign rhs is a Constant
//     with constType vpiRealConst.
//   - 5.7.2: real literals ('0.1', '2.1', '12.50', ...) are real-typed
//     Constants (vpiRealConst); integer literals ('0', '2', '8', '20', ...)
//     stay integer-typed (vpiIntConst / vpiUIntConst-ish decimal Constants,
//     checked here only by decompile, not constType, since plain unsized
//     decimal integer constants are not the focus of this file).
//   - 11.4.4 division/modulus: 'CLKFBOUT_PHASE / 0' and '20 % 0' are both
//     legal constant_expressions structurally (an Operation(vpiDivOp) /
//     Operation(vpiModOp)); IEEE 1800-2023 11.4.4 leaves the *evaluated*
//     result of integer division/modulus by zero undefined ('x') and real
//     division by zero as +/-infinity, both being evaluation-time facts, not
//     something a parse/db-time (-d db -d ast, no elaboration) test can
//     observe -- so only the operator shape is asserted here, never a
//     computed value.
//   - 11.4.5/11.4.6/11.4.7: real operands to '%' are legal per 1800-2023
//     Table 11-2 (a real operand makes '%' a real modulus), so 'A9 = 10.3 %
//     2.1' is a plain Operation(vpiModOp) like any other, not an error.
//   - 6.24.1 "real to integer": 'integer incr_d; incr_d = 10.1;' assigns a
//     real Constant to an integer Variable; conversion to the nearest
//     integer happens at evaluation time (10.1 rounds to 10), not
//     structurally, so the Assignment's own rhs Constant stays "10.1"
//     (vpiRealConst) -- the structural shape (Assignment, real Constant
//     rhs) is what is checked, not the runtime-rounded value.
//   - 6.20.2/11.2.1: 'top' overrides CLKFBOUT_PHASE via a positional
//     parameter-value assignment '#(.CLKFBOUT_PHASE(12.50/3.0))' (by-name,
//     despite the parens -- '.NAME(value)' syntax), rhs an
//     Operation(vpiDivOp) of two real Constants.
//
// What is checked:
//   - module 'top' exists and instantiates 'dut' as RefInstance "d",
//     whose ModuleTypespec resolves to "dut"
//   - "d"'s ParamAssign overriding CLKFBOUT_PHASE: by-name connection,
//     marked as overriding the default, rhs Operation(vpiDivOp) over two
//     real Constants "12.50" and "3.0"
//   - module 'dut' exists with parameter CLKFBOUT_PHASE typed 'real'
//     (getTypespec resolves to RealTypespec), not a localparam, default
//     ParamAssign rhs Constant with constType vpiRealConst
//   - 'err = CLKFBOUT_PHASE / 0': Operation(vpiDivOp), lhs RefObj
//     "CLKFBOUT_PHASE" (resolving to the Parameter), rhs Constant "0"
//   - 'average_delay = (r + f)/2': Operation(vpiDivOp) whose lhs is a
//     nested Operation(vpiAddOp) over RefObj "r" and RefObj "f", rhs
//     Constant "2"
//   - 'A1 = 0.1 * 0.5': Operation(vpiMultOp) over two real Constants
//   - 'A2 = 0.1 - 0.5': Operation(vpiSubOp) over two real Constants
//   - 'A3 = - 0.6': Operation(vpiMinusOp), 1 operand, real Constant "0.6"
//   - 'A9 = 10.3 % 2.1': Operation(vpiModOp) over two real Constants
//   - 'A11 = 2 ** 8': Operation(vpiPowerOp) over two integer Constants
//   - 'A13 = A8 ? A12 : A11': Operation(vpiConditionOp), 3 operands, each a
//     RefObj resolving to the corresponding sibling Parameter
//   - 'A14 = 20 % 0': Operation(vpiModOp) over two integer Constants
//     "20"/"0" -- structural shape only (see 11.4.4 note above)
//   - function 'incr_d' exists in dut's task/func collection, body Begin
//     with 3 statements: Assignment (lhs Variable "incr_d", rhs real
//     Constant "10.1"), post-increment Operation(vpiPostIncOp) on RefObj
//     "incr_d", and a ReturnStmt whose getCondition() is RefObj "incr_d"
//   - 'A15 = incr_d()': ParamAssign rhs is a FuncCall named "incr_d" with
//     no arguments, whose getTaskFunc() resolves back to that same
//     Function
//
// What is NOT checked and why:
//   - the generate-if blocks ("if (A1 == 0.05) begin GOOD good1(); end",
//     ... good2..good15) and the undefined "GOOD" module they instantiate
//     are out of scope: at -d db -d ast (parse/db only, no elaboration),
//     the generate-if condition is not evaluated and "GOOD" is never
//     required to bind -- exercising real-number constant folding at
//     elaboration time is a different (elaboration-phase) concern
//   - the actual numeric VALUE each parameter evaluates to (e.g. that A1
//     ends up 0.05, or that incr_d() ends up 11) is an evaluation-time
//     fact, not a static/structural compile-time property observable from
//     the AST/db built here

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/real_typespec.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/return_stmt.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class DoublePresTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "DoublePres.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  }

  static const hldb::Module *getDut() {
    return hldb::findByDefName<hldb::Module>("dut", m_design->getAllModules());
  }

  static const hldb::RefInstance *getInstD() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>("d", top->getRefInstances());
  }

  static const hldb::Parameter *getDutParam(std::string_view name) {
    const hldb::Module *const dut = getDut();
    if (dut == nullptr) return nullptr;
    return hldb::findByName<hldb::Parameter>(name, dut->getParameters());
  }

  static const hldb::ParamAssign *getDutParamAssign(std::string_view name) {
    const hldb::Module *const dut = getDut();
    if (dut == nullptr) return nullptr;
    return hldb::findByName(name, dut->getParamAssigns());
  }

  static const hldb::Constant *getDutParamRhsConstant(std::string_view name) {
    const hldb::ParamAssign *const pa = getDutParamAssign(name);
    if (pa == nullptr) return nullptr;
    return pa->getRhs<hldb::Constant>();
  }

  static const hldb::Operation *getDutParamRhsOperation(std::string_view name) {
    const hldb::ParamAssign *const pa = getDutParamAssign(name);
    if (pa == nullptr) return nullptr;
    return pa->getRhs<hldb::Operation>();
  }

  static const hldb::Function *getIncrD() {
    const hldb::Module *const dut = getDut();
    if (dut == nullptr || dut->getTaskFuncs() == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("incr_d", dut->getTaskFuncs());
  }
};

// ===========================================================================
// top / dut existence
// ===========================================================================

TEST_F(DoublePresTest, TopAndDutModulesExist) {
  EXPECT_NE(getTop(), nullptr);
  EXPECT_NE(getDut(), nullptr);
}

// ===========================================================================
// top: 'dut #(.CLKFBOUT_PHASE(12.50/3.0)) d (.a(clk_i), .b(b));'
// ===========================================================================

TEST_F(DoublePresTest, TopInstantiatesDutAsD) {
  const hldb::RefInstance *const d = getInstD();
  ASSERT_NE(d, nullptr) << "'d' RefInstance not found in top";
  ASSERT_NE(d->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = d->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr) << "d's typespec is not ModuleTypespec";
  EXPECT_EQ(mt->getName(), std::string_view("dut"));
}

TEST_F(DoublePresTest, TopOverridesClkfboutPhaseWithDivOfTwoRealConstants) {
  const hldb::RefInstance *const d = getInstD();
  ASSERT_NE(d, nullptr);
  const hldb::RefTypespec *const rt = d->getTypespec();
  ASSERT_NE(rt, nullptr);
  const hldb::Typespec *const ts = rt->getActual();
  ASSERT_NE(ts, nullptr);
  const hldb::ParamAssignCollection *const paramAssigns = hldb::getParamAssigns(ts);
  ASSERT_NE(paramAssigns, nullptr);
  const hldb::ParamAssign *const pa = hldb::findByName("CLKFBOUT_PHASE", paramAssigns);
  ASSERT_NE(pa, nullptr) << "'.CLKFBOUT_PHASE(12.50/3.0)' override not found on d";
  EXPECT_TRUE(pa->getConnByName()) << "'.CLKFBOUT_PHASE(...)' is a by-name parameter connection";
  EXPECT_TRUE(pa->getOverridden()) << "an explicit instance-level override must be marked as overriding the default";

  const hldb::Operation *const op = pa->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "'12.50/3.0' should be an Operation(vpiDivOp)";
  EXPECT_EQ(op->getOpType(), vpiDivOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const hldb::Constant *const lhs = any_cast<hldb::Constant>(op->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getConstType(), vpiRealConst);
  EXPECT_EQ(lhs->getDecompile(), "12.50");
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(op->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getConstType(), vpiRealConst);
  EXPECT_EQ(rhs->getDecompile(), "3.0");
}

// ===========================================================================
// dut: 'parameter real CLKFBOUT_PHASE = 1142.500'
// ===========================================================================

TEST_F(DoublePresTest, ClkfboutPhaseIsRealNonLocalParameter) {
  const hldb::Parameter *const p = getDutParam("CLKFBOUT_PHASE");
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->getLocalParam()) << "'parameter real CLKFBOUT_PHASE' is not a localparam";
  ASSERT_NE(p->getTypespec(), nullptr) << "'real' is an explicit type here (unlike an implicit-real parameter)";
  EXPECT_NE(p->getTypespec()->getActual<hldb::RealTypespec>(), nullptr)
      << "6.20.2: explicit 'real' type should resolve to RealTypespec";
}

TEST_F(DoublePresTest, ClkfboutPhaseDefaultIsRealConstant) {
  const hldb::Constant *const c = getDutParamRhsConstant("CLKFBOUT_PHASE");
  ASSERT_NE(c, nullptr) << "default ParamAssign rhs for CLKFBOUT_PHASE not found";
  EXPECT_EQ(c->getConstType(), vpiRealConst);
  EXPECT_EQ(c->getDecompile(), "1142.500");
}

// ===========================================================================
// 'err = CLKFBOUT_PHASE / 0'
// ===========================================================================

TEST_F(DoublePresTest, ErrIsClkfboutPhaseDividedByZero) {
  const hldb::Operation *const op = getDutParamRhsOperation("err");
  ASSERT_NE(op, nullptr) << "'CLKFBOUT_PHASE / 0' should be an Operation(vpiDivOp)";
  EXPECT_EQ(op->getOpType(), vpiDivOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(op->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "CLKFBOUT_PHASE");
  EXPECT_NE(lhs->getActual<hldb::Parameter>(), nullptr);
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(op->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "0");
}

// ===========================================================================
// 'average_delay = (r + f)/2'
// ===========================================================================

TEST_F(DoublePresTest, AverageDelayIsRPlusFDividedByTwo) {
  const hldb::Operation *const div = getDutParamRhsOperation("average_delay");
  ASSERT_NE(div, nullptr) << "'(r + f)/2' should be an Operation(vpiDivOp)";
  EXPECT_EQ(div->getOpType(), vpiDivOp);
  ASSERT_NE(div->getOperands(), nullptr);
  ASSERT_EQ(div->getOperands()->size(), 2u);

  const hldb::Operation *const add = any_cast<hldb::Operation>(div->getOperands()->at(0));
  ASSERT_NE(add, nullptr) << "'(r + f)' should be a nested Operation(vpiAddOp)";
  EXPECT_EQ(add->getOpType(), vpiAddOp);
  ASSERT_NE(add->getOperands(), nullptr);
  ASSERT_EQ(add->getOperands()->size(), 2u);
  const hldb::RefObj *const r = any_cast<hldb::RefObj>(add->getOperands()->at(0));
  ASSERT_NE(r, nullptr);
  EXPECT_EQ(r->getName(), "r");
  const hldb::RefObj *const f = any_cast<hldb::RefObj>(add->getOperands()->at(1));
  ASSERT_NE(f, nullptr);
  EXPECT_EQ(f->getName(), "f");

  const hldb::Constant *const two = any_cast<hldb::Constant>(div->getOperands()->at(1));
  ASSERT_NE(two, nullptr);
  EXPECT_EQ(two->getDecompile(), "2");
}

// ===========================================================================
// A1 = 0.1 * 0.5   |   A2 = 0.1 - 0.5   |   A3 = - 0.6
// ===========================================================================

TEST_F(DoublePresTest, A1IsRealMultiplication) {
  const hldb::Operation *const op = getDutParamRhsOperation("A1");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiMultOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const hldb::Constant *const lhs = any_cast<hldb::Constant>(op->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getConstType(), vpiRealConst);
  EXPECT_EQ(lhs->getDecompile(), "0.1");
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(op->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getConstType(), vpiRealConst);
  EXPECT_EQ(rhs->getDecompile(), "0.5");
}

TEST_F(DoublePresTest, A2IsRealSubtraction) {
  const hldb::Operation *const op = getDutParamRhsOperation("A2");
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiSubOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::Constant>(op->getOperands()->at(0))->getDecompile(), "0.1");
  EXPECT_EQ(any_cast<hldb::Constant>(op->getOperands()->at(1))->getDecompile(), "0.5");
}

TEST_F(DoublePresTest, A3IsUnaryMinusOfRealConstant) {
  const hldb::Operation *const op = getDutParamRhsOperation("A3");
  ASSERT_NE(op, nullptr) << "'- 0.6' should be an Operation(vpiMinusOp)";
  EXPECT_EQ(op->getOpType(), vpiMinusOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 1u);
  const hldb::Constant *const operand = any_cast<hldb::Constant>(op->getOperands()->at(0));
  ASSERT_NE(operand, nullptr);
  EXPECT_EQ(operand->getConstType(), vpiRealConst);
  EXPECT_EQ(operand->getDecompile(), "0.6");
}

// ===========================================================================
// A9 = 10.3 % 2.1 (real modulus, IEEE 1800-2023 Table 11-2)
// ===========================================================================

TEST_F(DoublePresTest, A9IsRealModulus) {
  const hldb::Operation *const op = getDutParamRhsOperation("A9");
  ASSERT_NE(op, nullptr) << "'10.3 % 2.1' should be an Operation(vpiModOp)";
  EXPECT_EQ(op->getOpType(), vpiModOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const hldb::Constant *const lhs = any_cast<hldb::Constant>(op->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getConstType(), vpiRealConst);
  EXPECT_EQ(lhs->getDecompile(), "10.3");
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(op->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getConstType(), vpiRealConst);
  EXPECT_EQ(rhs->getDecompile(), "2.1");
}

// ===========================================================================
// A11 = 2 ** 8
// ===========================================================================

TEST_F(DoublePresTest, A11IsPowerOperation) {
  const hldb::Operation *const op = getDutParamRhsOperation("A11");
  ASSERT_NE(op, nullptr) << "'2 ** 8' should be an Operation(vpiPowerOp)";
  EXPECT_EQ(op->getOpType(), vpiPowerOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::Constant>(op->getOperands()->at(0))->getDecompile(), "2");
  EXPECT_EQ(any_cast<hldb::Constant>(op->getOperands()->at(1))->getDecompile(), "8");
}

// ===========================================================================
// A13 = A8 ? A12 : A11
// ===========================================================================

TEST_F(DoublePresTest, A13IsConditionalOverSiblingParameters) {
  const hldb::Operation *const op = getDutParamRhsOperation("A13");
  ASSERT_NE(op, nullptr) << "'A8 ? A12 : A11' should be an Operation(vpiConditionOp)";
  EXPECT_EQ(op->getOpType(), vpiConditionOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 3u);

  const hldb::RefObj *const cond = any_cast<hldb::RefObj>(op->getOperands()->at(0));
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getName(), "A8");
  EXPECT_NE(cond->getActual<hldb::Parameter>(), nullptr);

  const hldb::RefObj *const thenVal = any_cast<hldb::RefObj>(op->getOperands()->at(1));
  ASSERT_NE(thenVal, nullptr);
  EXPECT_EQ(thenVal->getName(), "A12");
  EXPECT_NE(thenVal->getActual<hldb::Parameter>(), nullptr);

  const hldb::RefObj *const elseVal = any_cast<hldb::RefObj>(op->getOperands()->at(2));
  ASSERT_NE(elseVal, nullptr);
  EXPECT_EQ(elseVal->getName(), "A11");
  EXPECT_NE(elseVal->getActual<hldb::Parameter>(), nullptr);
}

// ===========================================================================
// A14 = 20 % 0 (structural shape only -- see 11.4.4 note above)
// ===========================================================================

TEST_F(DoublePresTest, A14IsIntegerModulusByZero) {
  const hldb::Operation *const op = getDutParamRhsOperation("A14");
  ASSERT_NE(op, nullptr) << "'20 % 0' should be an Operation(vpiModOp)";
  EXPECT_EQ(op->getOpType(), vpiModOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::Constant>(op->getOperands()->at(0))->getDecompile(), "20");
  EXPECT_EQ(any_cast<hldb::Constant>(op->getOperands()->at(1))->getDecompile(), "0");
}

// ===========================================================================
// function incr_d; integer incr_d; incr_d = 10.1; incr_d++; return incr_d;
// endfunction
// ===========================================================================

TEST_F(DoublePresTest, IncrDFunctionExists) {
  const hldb::Function *const fn = getIncrD();
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(fn->getName(), "incr_d");
}

TEST_F(DoublePresTest, IncrDBodyAssignsRealConstantThenIncrementsThenReturns) {
  const hldb::Function *const fn = getIncrD();
  ASSERT_NE(fn, nullptr);
  const hldb::Begin *const body = fn->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "function body should be a Begin";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 3u);

  // 'incr_d = 10.1;' -- 6.24.1 real-to-integer conversion happens at
  // evaluation time, so the Assignment's rhs Constant stays real-typed here.
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(body->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "'incr_d' here is the function's own (re-declared) return Variable";
  EXPECT_EQ(lhs->getName(), "incr_d");
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getConstType(), vpiRealConst);
  EXPECT_EQ(rhs->getDecompile(), "10.1");

  // 'incr_d++;'
  const hldb::Operation *const inc = any_cast<hldb::Operation>(body->getStmts()->at(1));
  ASSERT_NE(inc, nullptr);
  EXPECT_EQ(inc->getOpType(), vpiPostIncOp);
  ASSERT_NE(inc->getOperands(), nullptr);
  ASSERT_EQ(inc->getOperands()->size(), 1u);
  const hldb::RefObj *const incOperand = any_cast<hldb::RefObj>(inc->getOperands()->at(0));
  ASSERT_NE(incOperand, nullptr);
  EXPECT_EQ(incOperand->getName(), "incr_d");

  // 'return incr_d;'
  const hldb::ReturnStmt *const ret = any_cast<hldb::ReturnStmt>(body->getStmts()->at(2));
  ASSERT_NE(ret, nullptr);
  const hldb::RefObj *const retVal = ret->getCondition<hldb::RefObj>();
  ASSERT_NE(retVal, nullptr);
  EXPECT_EQ(retVal->getName(), "incr_d");
}

// ===========================================================================
// A15 = incr_d()
// ===========================================================================

TEST_F(DoublePresTest, A15CallsIncrDWithNoArguments) {
  const hldb::ParamAssign *const pa = getDutParamAssign("A15");
  ASSERT_NE(pa, nullptr);
  const hldb::FuncCall *const call = pa->getRhs<hldb::FuncCall>();
  ASSERT_NE(call, nullptr) << "'incr_d()' should be a FuncCall";
  EXPECT_EQ(call->getName(), "incr_d");
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), getIncrD());
  EXPECT_TRUE(call->getArguments() == nullptr || call->getArguments()->empty())
      << "'incr_d()' is called with no arguments";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
