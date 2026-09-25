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

// Tests for FuncParam/dut.sv:
//   module prim_lfsr;
//      parameter int unsigned LfsrDw = 12;
//      function automatic logic [LfsrDw-1:0] compute();
//         logic [LfsrDw-1:0] next_state;
//         return next_state;
//      endfunction
//   endmodule
//
//   module aes_prng;
//      prim_lfsr #(.LfsrDw(16)) u_lfsr16();
//      prim_lfsr #(.LfsrDw(18)) u_lfsr18();
//   endmodule
//
// What to check and why (IEEE 1800-2023, checked before any test code was
// written -- no .log file consulted for this file's expected shape, only
// the standard text and the real hldb API headers):
//
//   Sec 6.20.2 "Parameter declaration syntax": "int unsigned" is a signed
//   32-bit integer type restricted to unsigned, so "LfsrDw" should be a
//   plain scalar Parameter whose default value is Constant "12".
//
//   Sec 13.4 / Sec 6.8: the function's return type "logic [LfsrDw-1:0]" and
//   the local variable "next_state" both use the module-scope parameter
//   "LfsrDw" inside a packed-dimension range expression -- this is the
//   "function called/related to a parameter" corner case: a module
//   parameter feeding a function's own return-type width, not the other
//   direction (a function called from within a parameter expression, which
//   is exercised by FuncParam2/EvalFunc instead). Per Sec 6.8, "logic" with
//   no explicit net keyword is a variable data type, and the range
//   "[LfsrDw-1:0]" should be modeled as a Range on the LogicTypespec whose
//   getLeftExpr() is Operation(vpiSubOp) over RefObj "LfsrDw" and Constant
//   "1", and whose getRightExpr() is Constant "0". Both the return
//   typespec and next_state's typespec should reference this same
//   parameter-derived width.
//
//   Sec 23.3.2/23.10 "Module instantiation": "prim_lfsr #(.LfsrDw(16))
//   u_lfsr16();" overrides the "LfsrDw" parameter by name for this specific
//   instance -- this should produce a per-instance ParamAssign whose
//   getConnByName() is true, LHS resolving to the "LfsrDw" Parameter and
//   RHS Constant "16" (respectively "18" for u_lfsr18), without disturbing
//   prim_lfsr's own default-value Parameter declaration.
//
// What is NOT checked and why:
//   - the runtime-elaborated bit width of next_state / the function's
//     return value for each of the two prim_lfsr instances is a
//     simulation/elaboration-time concept, not a static/structural
//     compile-time property of the un-elaborated parse/typespec tree this
//     test inspects.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/return_stmt.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FuncParamTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FuncParam.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getPrimLfsr() {
    return hldb::findByName<hldb::Module>("prim_lfsr", m_design->getAllModules());
  }

  static const hldb::Module *getAesPrng() {
    return hldb::findByName<hldb::Module>("aes_prng", m_design->getAllModules());
  }

  static const hldb::Function *getCompute() {
    const hldb::Module *const mod = getPrimLfsr();
    if (mod == nullptr || mod->getTaskFuncs() == nullptr || mod->getTaskFuncs()->empty()) return nullptr;
    return any_cast<hldb::Function>(mod->getTaskFuncs()->at(0));
  }

  static void CheckRangeIsLfsrDwMinusOne(const hldb::RangeCollection *ranges) {
    ASSERT_NE(ranges, nullptr);
    ASSERT_EQ(ranges->size(), 1u);
    const hldb::Range *const range = ranges->at(0);
    ASSERT_NE(range, nullptr);
    const hldb::Operation *const msb = range->getLeftExpr<hldb::Operation>();
    ASSERT_NE(msb, nullptr) << "'LfsrDw-1' should be Operation(vpiSubOp)";
    EXPECT_EQ(msb->getOpType(), vpiSubOp);
    ASSERT_NE(msb->getOperands(), nullptr);
    ASSERT_EQ(msb->getOperands()->size(), 2u);
    const hldb::RefObj *const lfsrDwRef = any_cast<hldb::RefObj>(msb->getOperands()->at(0));
    ASSERT_NE(lfsrDwRef, nullptr);
    EXPECT_EQ(lfsrDwRef->getName(), "LfsrDw");
    EXPECT_NE(lfsrDwRef->getActual<hldb::Parameter>(), nullptr) << "'LfsrDw' should resolve to the Parameter decl";
    const hldb::Constant *const one = any_cast<hldb::Constant>(msb->getOperands()->at(1));
    ASSERT_NE(one, nullptr);
    EXPECT_EQ(one->getDecompile(), "1");

    const hldb::Constant *const lsb = range->getRightExpr<hldb::Constant>();
    ASSERT_NE(lsb, nullptr);
    EXPECT_EQ(lsb->getDecompile(), "0");
  }
};

TEST_F(FuncParamTest, ModulesExist) {
  EXPECT_NE(getPrimLfsr(), nullptr);
  EXPECT_NE(getAesPrng(), nullptr);
}

// parameter int unsigned LfsrDw = 12;
TEST_F(FuncParamTest, LfsrDwParameterDefaultsToTwelve) {
  const hldb::Module *const mod = getPrimLfsr();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getParameters(), nullptr);
  const hldb::Parameter *const lfsrDw = hldb::findByName<hldb::Parameter>("LfsrDw", mod->getParameters());
  ASSERT_NE(lfsrDw, nullptr);
  EXPECT_FALSE(lfsrDw->getLocalParam());
  const hldb::Constant *const defaultVal = lfsrDw->getExpr<hldb::Constant>();
  ASSERT_NE(defaultVal, nullptr);
  EXPECT_EQ(defaultVal->getDecompile(), "12");
}

// function automatic logic [LfsrDw-1:0] compute();
TEST_F(FuncParamTest, ComputeExistsAsAutomaticFunctionWithNoArguments) {
  const hldb::Module *const mod = getPrimLfsr();
  ASSERT_NE(mod, nullptr);
  ASSERT_NE(mod->getTaskFuncs(), nullptr);
  ASSERT_EQ(mod->getTaskFuncs()->size(), 1u);
  const hldb::Function *const compute = getCompute();
  ASSERT_NE(compute, nullptr);
  EXPECT_EQ(compute->getName(), "compute");
  EXPECT_TRUE(compute->getAutomatic());
  EXPECT_TRUE(compute->getIODecls() == nullptr || compute->getIODecls()->empty());
}

TEST_F(FuncParamTest, ComputeReturnTypeUsesLfsrDwParameterWidth) {
  const hldb::Function *const compute = getCompute();
  ASSERT_NE(compute, nullptr);
  const hldb::RefTypespec *const rts = compute->getReturn();
  ASSERT_NE(rts, nullptr);
  const hldb::LogicTypespec *const logicTs = rts->getActual<hldb::LogicTypespec>();
  ASSERT_NE(logicTs, nullptr) << "'logic [LfsrDw-1:0]' return type should resolve to a LogicTypespec";
  EXPECT_FALSE(logicTs->getSigned());
  CheckRangeIsLfsrDwMinusOne(logicTs->getRanges());
}

// logic [LfsrDw-1:0] next_state; return next_state;
TEST_F(FuncParamTest, ComputeBodyDeclaresNextStateWithSameParameterizedWidthAndReturnsIt) {
  const hldb::Function *const compute = getCompute();
  ASSERT_NE(compute, nullptr);
  const hldb::Begin *const body = compute->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "function body should be a Begin (decl + return)";
  ASSERT_NE(body->getVariables(), nullptr);
  ASSERT_EQ(body->getVariables()->size(), 1u);
  const hldb::Variable *const nextState = hldb::findByName<hldb::Variable>("next_state", body->getVariables());
  ASSERT_NE(nextState, nullptr);

  const hldb::RefTypespec *const rts = nextState->getTypespec();
  ASSERT_NE(rts, nullptr);
  const hldb::LogicTypespec *const logicTs = rts->getActual<hldb::LogicTypespec>();
  ASSERT_NE(logicTs, nullptr) << "'logic [LfsrDw-1:0] next_state' should resolve to a LogicTypespec";
  CheckRangeIsLfsrDwMinusOne(logicTs->getRanges());

  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 2u);
  const hldb::ReturnStmt *const ret = any_cast<hldb::ReturnStmt>(body->getStmts()->at(1));
  ASSERT_NE(ret, nullptr);
  const hldb::RefObj *const retExpr = ret->getCondition<hldb::RefObj>();
  ASSERT_NE(retExpr, nullptr);
  EXPECT_EQ(retExpr->getName(), "next_state");
  EXPECT_EQ(retExpr->getActual(), nextState);
}

// prim_lfsr #(.LfsrDw(16)) u_lfsr16();
// prim_lfsr #(.LfsrDw(18)) u_lfsr18();
TEST_F(FuncParamTest, AesPrngInstantiatesPrimLfsrTwiceWithDifferentParamOverrides) {
  const hldb::Module *const aesPrng = getAesPrng();
  ASSERT_NE(aesPrng, nullptr);
  ASSERT_NE(aesPrng->getModules(), nullptr);
  ASSERT_EQ(aesPrng->getModules()->size(), 2u);

  const hldb::Module *const u16 = hldb::findByName<hldb::Module>("u_lfsr16", aesPrng->getModules());
  ASSERT_NE(u16, nullptr);
  EXPECT_EQ(u16->getDefName(), "prim_lfsr");
  const hldb::Module *const u18 = hldb::findByName<hldb::Module>("u_lfsr18", aesPrng->getModules());
  ASSERT_NE(u18, nullptr);
  EXPECT_EQ(u18->getDefName(), "prim_lfsr");

  ASSERT_NE(u16->getParamAssigns(), nullptr);
  const hldb::ParamAssign *const pa16 = hldb::findByName("LfsrDw", u16->getParamAssigns());
  ASSERT_NE(pa16, nullptr) << "'.LfsrDw(16)' should produce a named ParamAssign on u_lfsr16";
  EXPECT_TRUE(pa16->getConnByName());
  const hldb::Constant *const rhs16 = any_cast<hldb::Constant>(pa16->getRhs());
  ASSERT_NE(rhs16, nullptr);
  EXPECT_EQ(rhs16->getDecompile(), "16");

  ASSERT_NE(u18->getParamAssigns(), nullptr);
  const hldb::ParamAssign *const pa18 = hldb::findByName("LfsrDw", u18->getParamAssigns());
  ASSERT_NE(pa18, nullptr) << "'.LfsrDw(18)' should produce a named ParamAssign on u_lfsr18";
  EXPECT_TRUE(pa18->getConnByName());
  const hldb::Constant *const rhs18 = any_cast<hldb::Constant>(pa18->getRhs());
  ASSERT_NE(rhs18, nullptr);
  EXPECT_EQ(rhs18->getDecompile(), "18");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
