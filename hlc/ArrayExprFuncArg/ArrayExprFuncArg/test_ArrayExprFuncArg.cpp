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

// Tests for dut.sv (tags: ArrayExprFuncArg)
//   module top #()();
//     localparam int unsigned CNT = 3;
//     localparam int unsigned V[CNT] = '{3,5,4};
//     typedef int unsigned ASSIGN_VADDR_RET_T[CNT];
//     function static ASSIGN_VADDR_RET_T ASSIGN_VADDR();
//       for (int i = 0; i < CNT; i++) begin
//         ASSIGN_VADDR[i] = V[i];
//       end
//     endfunction
//     localparam int unsigned VADDR[CNT] = ASSIGN_VADDR();
//     if (VADDR[0] == 3) begin $info("VADDR[0] == 3"); end
//     if (VADDR[2] == 4) begin $info("VADDR[2] == 4"); end
//     function static int unsigned MAX_VADDR_CNT(int unsigned SRC[CNT], int unsigned ICNT);
//       MAX_VADDR_CNT = 0;
//       for (int i = 0; i < ICNT; i++) begin
//         if (SRC[i] > MAX_VADDR_CNT) begin MAX_VADDR_CNT = SRC[i]; end
//       end
//     endfunction
//     localparam int unsigned MAX_VADDR = MAX_VADDR_CNT(VADDR, CNT);
//     if (MAX_VADDR == 3) begin $info("MAX_VADDR == 3"); end
//     if (MAX_VADDR == 5) begin $info("MAX_VADDR == 5"); end
//     if (MAX_VADDR != 3) begin $info("MAX_VADDR != 3"); end
//   endmodule
//   module main;
//     top #() top1();
//   endmodule
//
// The headline construct this file is named for is "MAX_VADDR_CNT(VADDR, CNT)":
// an unpacked-array localparam ("VADDR") passed as a plain array-typed
// function argument, used to compute another localparam
// (IEEE 1800-2023 11.2.1 "Constant expressions", 13.4 "Function").
//
// Every "type name[SIZE]" unpacked-array dimension in this file (V's,
// ASSIGN_VADDR_RET_T's, and SRC's) elaborates to the same unusual shape:
// an ArrayTypespec whose Range has a left expression ("subtract" Operation
// over a single operand, the size RefObj) and NO right expression at all
// -- not a folded/degenerate one, genuinely absent. This is asserted
// exactly as observed rather than normalized to the 2-operand "SIZE - 1"
// shape one might expect from an IEEE-1364-style [msb:lsb] range.
//
// Checked:
//   - module "top" has exactly 4 localparams in declaration order: CNT, V,
//     VADDR, MAX_VADDR
//   - CNT is a plain "int" localparam equal to 3
//   - V is an unpacked "int unsigned[CNT]" localparam (ArrayTypespec over
//     an IntTypespec element, per the single-operand-subtract shape noted
//     above), assigned the 3-element array-literal pattern '{3,5,4}
//   - ASSIGN_VADDR_RET_T's typedef aliases that identical ArrayTypespec shape
//   - function ASSIGN_VADDR returns ASSIGN_VADDR_RET_T and its body is a
//     single ForStmt (init i=0, condition i<CNT, increment i++) whose
//     body assigns V[i] into ASSIGN_VADDR[i] (the function's own name
//     used as the return-value accumulator, per IEEE 1800-2023 13.4.1)
//   - VADDR's ParamAssign calls ASSIGN_VADDR() with zero arguments
//   - function MAX_VADDR_CNT takes 2 IODecls: SRC (the same unpacked
//     array shape as V) and ICNT (plain int)
//   - MAX_VADDR's ParamAssign calls MAX_VADDR_CNT(VADDR, CNT): VADDR (an
//     array-typed localparam) passed as a plain argument expression --
//     the construct this file is named for
//   - module "top" has exactly 5 top-level generate-if statements (no
//     "generate"/"endgenerate" keywords in the source, so no GenRegion
//     wrapper -- each GenIf sits directly in the module's generate
//     statements), each with a Begin body holding one "$info" SysTaskCall
//     over a single string-constant argument
//   - module "main" is never elaborated against "top" here: it holds a
//     single unelaborated RefInstance "top1" whose typespec resolves to
//     the ModuleTypespec for "top"
//   - compiler reports zero errors
//
// NOT CHECKED: the actual folded values of VADDR/MAX_VADDR (whether
// $info("MAX_VADDR == 3") or ("== 5") is the one that would fire) --
// that requires running the constant-function evaluation HLC performs
// internally, which the HLDB printed here does not expose as a value on
// Parameter "MAX_VADDR" (unlike the plain-constant localparams in other
// tests, its ParamAssign rhs is a call, not a folded Constant). Asserting
// which branch is "true" would be guessing at that evaluation rather than
// reading it from the log.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/for_stmt.h>
#include <hldb/function.h>
#include <hldb/gen_if.h>
#include <hldb/if_stmt.h>
#include <hldb/int_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/method_func_call.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_task_call.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ArrayExprFuncArgTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ArrayExprFuncArg.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTopModule() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Module *getMainModule() {
    return hldb::findByName<hldb::Module>("main", m_design->getAllModules());
  }

  static const hldb::ParamAssign *getTopParamAssign(std::string_view name) {
    const hldb::Module *const mod = getTopModule();
    if (mod == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::ParamAssign, hldb::ParamAssign>(name, mod->getParamAssigns());
  }

  static const hldb::Function *getTopFunction(std::string_view name) {
    const hldb::Module *const mod = getTopModule();
    if (mod == nullptr || mod->getTaskFuncs() == nullptr) {
      return nullptr;
    }
    return hldb::findByName<hldb::Function>(name, mod->getTaskFuncs());
  }

  // Common shape of V / ASSIGN_VADDR_RET_T / SRC's unpacked "[CNT]" dimension:
  // an ArrayTypespec over an IntTypespec element, ranged by a "subtract"
  // Operation with exactly 1 operand (RefObj "CNT") and no right expression.
  static void expectCntSizedArrayTypespec(const hldb::ArrayTypespec *arrayTs) {
    ASSERT_NE(arrayTs, nullptr);
    ASSERT_NE(arrayTs->getElemTypespec(), nullptr);
    EXPECT_NE(arrayTs->getElemTypespec()->getActual<hldb::IntTypespec>(), nullptr);

    ASSERT_NE(arrayTs->getRange(), nullptr);
    EXPECT_EQ(arrayTs->getRange()->getRightExpr(), nullptr);
    const hldb::Operation *const subtract = arrayTs->getRange()->getLeftExpr<hldb::Operation>();
    ASSERT_NE(subtract, nullptr);
    EXPECT_EQ(subtract->getOpType(), vpiSubOp);
    ASSERT_NE(subtract->getOperands(), nullptr);
    ASSERT_EQ(subtract->getOperands()->size(), 1u);
    const hldb::RefObj *const cntRef = any_cast<hldb::RefObj>(subtract->getOperands()->at(0));
    ASSERT_NE(cntRef, nullptr);
    EXPECT_EQ(cntRef->getName(), "CNT");
  }
};

// --- module / localparams ---------------------------------------------------

TEST_F(ArrayExprFuncArgTest, TopModuleExists) { EXPECT_NE(getTopModule(), nullptr); }

TEST_F(ArrayExprFuncArgTest, TopHasFourLocalparamsInOrder) {
  const hldb::Module *const mod = getTopModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getParameters(), nullptr);
  ASSERT_EQ(mod->getParameters()->size(), 4u);

  static constexpr std::string_view kNames[4] = {"CNT", "V", "VADDR", "MAX_VADDR"};
  for (size_t i = 0; i < 4; ++i) {
    const hldb::Parameter *const param = any_cast<hldb::Parameter>(mod->getParameters()->at(i));
    ASSERT_NE(param, nullptr) << "parameter index " << i;
    EXPECT_EQ(param->getName(), kNames[i]) << "parameter index " << i;
    EXPECT_TRUE(param->getLocalParam()) << "parameter index " << i;
  }
}

TEST_F(ArrayExprFuncArgTest, CntIsThree) {
  const hldb::ParamAssign *const assign = getTopParamAssign("CNT");
  ASSERT_NE(assign, nullptr);
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "3");
}

TEST_F(ArrayExprFuncArgTest, VIsCntSizedArrayAssignedLiteralPattern) {
  const hldb::Module *const mod = getTopModule();
  ASSERT_NE(mod, nullptr);
  const hldb::Parameter *const v = hldb::findByName<hldb::Parameter>("V", mod->getParameters());
  ASSERT_NE(v, nullptr);
  ASSERT_NE(v->getTypespec(), nullptr);
  expectCntSizedArrayTypespec(v->getTypespec()->getActual<hldb::ArrayTypespec>());

  const hldb::ParamAssign *const assign = getTopParamAssign("V");
  ASSERT_NE(assign, nullptr);
  const hldb::Operation *const pattern = assign->getRhs<hldb::Operation>();
  ASSERT_NE(pattern, nullptr);
  EXPECT_EQ(pattern->getOpType(), vpiAssignmentPatternOp);
  ASSERT_NE(pattern->getOperands(), nullptr);
  ASSERT_EQ(pattern->getOperands()->size(), 3u);

  static constexpr std::string_view kValues[3] = {"3", "5", "4"};
  for (size_t i = 0; i < 3; ++i) {
    const hldb::Constant *const elem = any_cast<hldb::Constant>(pattern->getOperands()->at(i));
    ASSERT_NE(elem, nullptr) << "element index " << i;
    EXPECT_EQ(elem->getDecompile(), kValues[i]) << "element index " << i;
  }
}

TEST_F(ArrayExprFuncArgTest, AssignVaddrRetTAliasesSameArrayShapeAsV) {
  const hldb::Module *const mod = getTopModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getTypedefs(), nullptr);
  const hldb::Typedef *const td = hldb::findByName<hldb::Typedef>("ASSIGN_VADDR_RET_T", mod->getTypedefs());
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  expectCntSizedArrayTypespec(td->getAlias()->getActual<hldb::ArrayTypespec>());
}

// --- function ASSIGN_VADDR: for-loop filling an array return value --------

TEST_F(ArrayExprFuncArgTest, AssignVaddrReturnsAssignVaddrRetT) {
  const hldb::Function *const func = getTopFunction("ASSIGN_VADDR");
  ASSERT_NE(func, nullptr);
  ASSERT_NE(func->getReturn(), nullptr);
  const hldb::TypedefTypespec *const retType = func->getReturn()->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(retType, nullptr);
  EXPECT_EQ(retType->getName(), "ASSIGN_VADDR_RET_T");
}

TEST_F(ArrayExprFuncArgTest, AssignVaddrForLoopCopiesVIntoReturnValue) {
  const hldb::Function *const func = getTopFunction("ASSIGN_VADDR");
  ASSERT_NE(func, nullptr);
  const hldb::ForStmt *const forStmt = func->getStmt<hldb::ForStmt>();
  ASSERT_NE(forStmt, nullptr) << "function body should be a ForStmt directly, with no Begin wrapper";

  ASSERT_NE(forStmt->getVariables(), nullptr);
  ASSERT_EQ(forStmt->getVariables()->size(), 1u);
  const hldb::Variable *const loopVar = forStmt->getVariables()->at(0);
  ASSERT_NE(loopVar, nullptr);
  EXPECT_EQ(loopVar->getName(), "i");
  ASSERT_NE(loopVar->getTypespec(), nullptr);
  EXPECT_NE(loopVar->getTypespec()->getActual<hldb::IntTypespec>(), nullptr);

  ASSERT_NE(forStmt->getForInitStmts(), nullptr);
  ASSERT_EQ(forStmt->getForInitStmts()->size(), 1u);
  const hldb::Assignment *const init = any_cast<hldb::Assignment>(forStmt->getForInitStmts()->at(0));
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getLhs<hldb::Variable>(), loopVar) << "for-init's lhs is the loop Variable directly, not a RefObj";
  const hldb::Constant *const initVal = init->getRhs<hldb::Constant>();
  ASSERT_NE(initVal, nullptr);
  EXPECT_EQ(initVal->getDecompile(), "0");

  const hldb::Operation *const condition = forStmt->getCondition<hldb::Operation>();
  ASSERT_NE(condition, nullptr);
  EXPECT_EQ(condition->getOpType(), vpiLtOp);
  ASSERT_NE(condition->getOperands(), nullptr);
  ASSERT_EQ(condition->getOperands()->size(), 2u);
  const hldb::RefObj *const condLhs = any_cast<hldb::RefObj>(condition->getOperands()->at(0));
  ASSERT_NE(condLhs, nullptr);
  EXPECT_EQ(condLhs->getActual<hldb::Variable>(), loopVar);
  const hldb::RefObj *const condRhs = any_cast<hldb::RefObj>(condition->getOperands()->at(1));
  ASSERT_NE(condRhs, nullptr);
  EXPECT_EQ(condRhs->getName(), "CNT");

  ASSERT_NE(forStmt->getForIncStmts(), nullptr);
  ASSERT_EQ(forStmt->getForIncStmts()->size(), 1u);
  const hldb::Operation *const inc = any_cast<hldb::Operation>(forStmt->getForIncStmts()->at(0));
  ASSERT_NE(inc, nullptr);
  EXPECT_EQ(inc->getOpType(), vpiPostIncOp);

  const hldb::Begin *const body = forStmt->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 1u);
  const hldb::Assignment *const copy = any_cast<hldb::Assignment>(body->getStmts()->at(0));
  ASSERT_NE(copy, nullptr);
  EXPECT_TRUE(copy->getBlocking());

  const hldb::BitSelect *const lhs = copy->getLhs<hldb::BitSelect>();
  ASSERT_NE(lhs, nullptr) << "'ASSIGN_VADDR[i]' lhs should be a BitSelect";
  EXPECT_EQ(lhs->getName(), "ASSIGN_VADDR[i]");
  ASSERT_NE(lhs->getPrefix(), nullptr);
  const hldb::RefObj *const lhsPrefix = any_cast<hldb::RefObj>(lhs->getPrefix());
  ASSERT_NE(lhsPrefix, nullptr);
  EXPECT_EQ(lhsPrefix->getActual<hldb::Function>(), func)
      << "'ASSIGN_VADDR[i]' on the lhs names the function's own return-value accumulator";

  const hldb::BitSelect *const rhs = copy->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr) << "'V[i]' rhs should be a BitSelect";
  EXPECT_EQ(rhs->getName(), "V[i]");
  ASSERT_NE(rhs->getPrefix(), nullptr);
  const hldb::RefObj *const rhsPrefix = any_cast<hldb::RefObj>(rhs->getPrefix());
  ASSERT_NE(rhsPrefix, nullptr);
  EXPECT_NE(rhsPrefix->getActual<hldb::Parameter>(), nullptr) << "'V' should resolve to the Parameter V";
}

TEST_F(ArrayExprFuncArgTest, VaddrCallsAssignVaddrWithNoArguments) {
  const hldb::Function *const func = getTopFunction("ASSIGN_VADDR");
  ASSERT_NE(func, nullptr);

  const hldb::ParamAssign *const assign = getTopParamAssign("VADDR");
  ASSERT_NE(assign, nullptr);
  const hldb::MethodFuncCall *const call = assign->getRhs<hldb::MethodFuncCall>();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "ASSIGN_VADDR");
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), func);
  EXPECT_EQ(call->getArguments(), nullptr) << "'ASSIGN_VADDR()' is called with no arguments";
}

// --- function MAX_VADDR_CNT: array-typed argument --------------------------

TEST_F(ArrayExprFuncArgTest, MaxVaddrCntTakesArrayAndIntArguments) {
  const hldb::Function *const func = getTopFunction("MAX_VADDR_CNT");
  ASSERT_NE(func, nullptr);
  ASSERT_NE(func->getIODecls(), nullptr);
  ASSERT_EQ(func->getIODecls()->size(), 2u);

  const hldb::IODecl *const src = func->getIODecls()->at(0);
  ASSERT_NE(src, nullptr);
  EXPECT_EQ(src->getName(), "SRC");
  EXPECT_EQ(src->getDirection(), vpiInput);
  ASSERT_NE(src->getTypespec(), nullptr);
  expectCntSizedArrayTypespec(src->getTypespec()->getActual<hldb::ArrayTypespec>());

  const hldb::IODecl *const icnt = func->getIODecls()->at(1);
  ASSERT_NE(icnt, nullptr);
  EXPECT_EQ(icnt->getName(), "ICNT");
  EXPECT_EQ(icnt->getDirection(), vpiInput);
  ASSERT_NE(icnt->getTypespec(), nullptr);
  EXPECT_NE(icnt->getTypespec()->getActual<hldb::IntTypespec>(), nullptr);

  ASSERT_NE(func->getReturn(), nullptr);
  EXPECT_NE(func->getReturn()->getActual<hldb::IntTypespec>(), nullptr);
}

TEST_F(ArrayExprFuncArgTest, MaxVaddrPassesArrayParameterVaddrAsPlainArgument) {
  const hldb::Module *const mod = getTopModule();
  ASSERT_NE(mod, nullptr);
  const hldb::Function *const func = getTopFunction("MAX_VADDR_CNT");
  ASSERT_NE(func, nullptr);

  const hldb::ParamAssign *const assign = getTopParamAssign("MAX_VADDR");
  ASSERT_NE(assign, nullptr);
  const hldb::MethodFuncCall *const call = assign->getRhs<hldb::MethodFuncCall>();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "MAX_VADDR_CNT");
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), func);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);

  const hldb::RefObj *const vaddrArg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(vaddrArg, nullptr) << "'VADDR' argument -- an array-typed localparam passed as a plain expression";
  EXPECT_EQ(vaddrArg->getName(), "VADDR");
  EXPECT_EQ(vaddrArg->getActual<hldb::Parameter>(), hldb::findByName<hldb::Parameter>("VADDR", mod->getParameters()));

  const hldb::RefObj *const cntArg = any_cast<hldb::RefObj>(call->getArguments()->at(1));
  ASSERT_NE(cntArg, nullptr);
  EXPECT_EQ(cntArg->getName(), "CNT");
}

// --- module-scope generate-if statements (no "generate"/"endgenerate") ----

TEST_F(ArrayExprFuncArgTest, TopHasFiveGenerateIfsEachLoggingOneString) {
  const hldb::Module *const mod = getTopModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getGenStmts(), nullptr);
  ASSERT_EQ(mod->getGenStmts()->size(), 5u);

  for (size_t i = 0; i < 5; ++i) {
    const hldb::GenIf *const genIf = any_cast<hldb::GenIf>(mod->getGenStmts()->at(i));
    ASSERT_NE(genIf, nullptr) << "generate statement index " << i
                              << " should be a GenIf, with no GenRegion "
                                 "wrapper since the source has no explicit 'generate'/'endgenerate'";
    const hldb::Begin *const body = genIf->getStmt<hldb::Begin>();
    ASSERT_NE(body, nullptr) << "GenIf index " << i;
    ASSERT_NE(body->getStmts(), nullptr);
    ASSERT_EQ(body->getStmts()->size(), 1u) << "GenIf index " << i;
    const hldb::SysTaskCall *const info = any_cast<hldb::SysTaskCall>(body->getStmts()->at(0));
    ASSERT_NE(info, nullptr) << "GenIf index " << i;
    EXPECT_EQ(info->getName(), "info") << "GenIf index " << i;
    ASSERT_NE(info->getArguments(), nullptr) << "GenIf index " << i;
    ASSERT_EQ(info->getArguments()->size(), 1u) << "GenIf index " << i;
    const hldb::Constant *const msg = any_cast<hldb::Constant>(info->getArguments()->at(0));
    ASSERT_NE(msg, nullptr) << "GenIf index " << i;
    EXPECT_EQ(msg->getConstType(), vpiStringConst) << "GenIf index " << i;
  }
}

TEST_F(ArrayExprFuncArgTest, FirstGenerateIfConditionIsVaddrZeroEqualsThree) {
  const hldb::Module *const mod = getTopModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getGenStmts(), nullptr);
  ASSERT_FALSE(mod->getGenStmts()->empty());
  const hldb::GenIf *const genIf = any_cast<hldb::GenIf>(mod->getGenStmts()->at(0));
  ASSERT_NE(genIf, nullptr);

  const hldb::Operation *const condition = genIf->getCondition<hldb::Operation>();
  ASSERT_NE(condition, nullptr);
  EXPECT_EQ(condition->getOpType(), vpiEqOp);
  ASSERT_NE(condition->getOperands(), nullptr);
  ASSERT_EQ(condition->getOperands()->size(), 2u);

  const hldb::BitSelect *const vaddr0 = any_cast<hldb::BitSelect>(condition->getOperands()->at(0));
  ASSERT_NE(vaddr0, nullptr);
  EXPECT_EQ(vaddr0->getName(), "VADDR[0]");

  const hldb::Constant *const three = any_cast<hldb::Constant>(condition->getOperands()->at(1));
  ASSERT_NE(three, nullptr);
  EXPECT_EQ(three->getDecompile(), "3");
}

TEST_F(ArrayExprFuncArgTest, LastGenerateIfUsesNotEqual) {
  const hldb::Module *const mod = getTopModule();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getGenStmts(), nullptr);
  ASSERT_EQ(mod->getGenStmts()->size(), 5u);
  const hldb::GenIf *const genIf = any_cast<hldb::GenIf>(mod->getGenStmts()->at(4));
  ASSERT_NE(genIf, nullptr);

  const hldb::Operation *const condition = genIf->getCondition<hldb::Operation>();
  ASSERT_NE(condition, nullptr);
  EXPECT_EQ(condition->getOpType(), vpiNeqOp) << "'MAX_VADDR != 3' uses '!=' (not equal)";
}

// --- module main: unelaborated instance of top -----------------------------

TEST_F(ArrayExprFuncArgTest, MainHoldsUnelaboratedTopInstance) {
  const hldb::Module *const main = getMainModule();
  ASSERT_NE(main, nullptr);
  ASSERT_NE(main->getRefInstances(), nullptr);
  ASSERT_EQ(main->getRefInstances()->size(), 1u);

  const hldb::RefInstance *const inst = main->getRefInstances()->at(0);
  ASSERT_NE(inst, nullptr);
  EXPECT_EQ(inst->getName(), "top1");
  ASSERT_NE(inst->getTypespec(), nullptr);
  const hldb::ModuleTypespec *const mt = inst->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mt, nullptr);
  EXPECT_EQ(mt->getName(), "top");
}

// --- compiler diagnostics ---------------------------------------------------

TEST_F(ArrayExprFuncArgTest, CompilesWithNoErrors) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
