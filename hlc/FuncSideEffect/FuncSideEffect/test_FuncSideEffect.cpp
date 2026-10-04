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

// Tests for FuncSideEffect/dut.sv:
//   module const_fold_func_top(
//       input wire [3:0] inp,
//       output wire [3:0] out1,
//       output reg [3:0] out2
//   );
//       function automatic [3:0] flip;
//           input [3:0] inp;
//           flip = ~inp;
//       endfunction
//
//     function automatic [3:0] pow_flip_b;
//       input [3:0] base, exp;
//       begin
//           out2[exp] = base & 1;
//           pow_flip_b = 1;
//           if (exp > 0)
//               pow_flip_b = base * pow_flip_b(flip(base), exp - 1);
//       end
//     endfunction
//
//       assign out1 = pow_flip_b(2, 2);
//   endmodule
//
// What to check and why (IEEE 1800-2023, checked before any test code was
// written -- no .log file consulted for this file's expected shape):
//
//   Sec 23.2.2.3 "Rules for ports": "output reg [3:0] out2" declares a
//   variable-type (non-net) output port -- per Sec 6.8, "reg" is a
//   variable data type, so "out2" must be modeled as a Variable in the
//   module's own getVariables(), not a Net, while "out1" ("output wire")
//   and "inp" ("input wire") remain Nets.
//
//   Sec 13.4.3 "Constant functions": "pow_flip_b" is a plain automatic
//   function (not declared/used exclusively as a constant function -- its
//   one use, "assign out1 = pow_flip_b(2, 2);", is inside a continuous
//   assignment, not a parameter/constant-expression context), so it is
//   free to have a side effect: "out2[exp] = base & 1;" assigns to
//   "out2", a variable declared OUTSIDE the function's own scope (a
//   module-level port variable), from inside the function body. This is
//   exactly the side-effect construct under test -- the assignment's LHS
//   RefObj/Select must resolve back to the module-scope "out2" Variable,
//   not to any function-local declaration.
//
//   Sec 13.4.1: "pow_flip_b = 1;" and the later "pow_flip_b = base *
//   pow_flip_b(flip(base), exp - 1);" both assign the function's return
//   value by name (RefObj "pow_flip_b" resolving back to the function
//   itself), with the second one being a genuine recursive self-call
//   nested inside a call to "flip".
//
//   Sec 12.4 "Conditional statements": "if (exp > 0) pow_flip_b = ...;"
//   (no else, no begin-end) should be a plain IfStmt whose getStmt() is
//   the single Assignment, not wrapped in a Begin.
//
// What is NOT checked and why:
//   - the runtime-computed numeric result of "pow_flip_b(2, 2)" (or of the
//     recursive/side-effecting evaluation in general) is a simulation-time
//     concept, not a static/structural compile-time property.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/if_stmt.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/var_select.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FuncSideEffectTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FuncSideEffect.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("const_fold_func_top", m_design->getAllModules());
  }

  static const hldb::Function *getFunc(std::string_view name) {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTaskFuncs() == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>(name, top->getTaskFuncs());
  }
};

TEST_F(FuncSideEffectTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

// input wire [3:0] inp, output wire [3:0] out1, output reg [3:0] out2
TEST_F(FuncSideEffectTest, Out2IsVariableWhileInpAndOut1AreNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("inp", top->getNets()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Net>("out1", top->getNets()), nullptr);

  ASSERT_NE(top->getVariables(), nullptr);
  const hldb::Variable *const out2 = hldb::findByName<hldb::Variable>("out2", top->getVariables());
  ASSERT_NE(out2, nullptr) << "'output reg [3:0] out2' should be a Variable, per IEEE 1800-2023 Sec 6.8/23.2.2.3";
}

// function automatic [3:0] flip; input [3:0] inp; flip = ~inp;
TEST_F(FuncSideEffectTest, FlipExistsWithOneInputAndBitwiseNegateBody) {
  const hldb::Function *const flip = getFunc("flip");
  ASSERT_NE(flip, nullptr);
  EXPECT_TRUE(flip->getAutomatic());
  ASSERT_NE(flip->getIODecls(), nullptr);
  ASSERT_EQ(flip->getIODecls()->size(), 1u);
  EXPECT_EQ(flip->getIODecls()->at(0)->getName(), "inp");
  EXPECT_EQ(flip->getIODecls()->at(0)->getDirection(), vpiInput);

  const hldb::Assignment *const assign = flip->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "flip");
  const hldb::Operation *const rhs = assign->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getOpType(), vpiBitNegOp);
}

// function automatic [3:0] pow_flip_b; input [3:0] base, exp;
TEST_F(FuncSideEffectTest, PowFlipBHasTwoInputArgumentsBaseAndExp) {
  const hldb::Function *const powFlipB = getFunc("pow_flip_b");
  ASSERT_NE(powFlipB, nullptr);
  EXPECT_TRUE(powFlipB->getAutomatic());
  ASSERT_NE(powFlipB->getIODecls(), nullptr);
  ASSERT_EQ(powFlipB->getIODecls()->size(), 2u);
  EXPECT_EQ(powFlipB->getIODecls()->at(0)->getName(), "base");
  EXPECT_EQ(powFlipB->getIODecls()->at(0)->getDirection(), vpiInput);
  EXPECT_EQ(powFlipB->getIODecls()->at(1)->getName(), "exp");
  EXPECT_EQ(powFlipB->getIODecls()->at(1)->getDirection(), vpiInput);
}

// out2[exp] = base & 1;  -- side effect on a module-scope variable
TEST_F(FuncSideEffectTest, PowFlipBBodyAssignsToModuleScopeOut2AsASideEffect) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const out2 = hldb::findByName<hldb::Variable>("out2", top->getVariables());
  ASSERT_NE(out2, nullptr);

  const hldb::Function *const powFlipB = getFunc("pow_flip_b");
  ASSERT_NE(powFlipB, nullptr);
  const hldb::Begin *const body = powFlipB->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "'begin ... end' function body should be a Begin";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 3u);

  const hldb::Assignment *const sideEffect = any_cast<hldb::Assignment>(body->getStmts()->at(0));
  ASSERT_NE(sideEffect, nullptr) << "'out2[exp] = base & 1;' should be an Assignment";
  const hldb::VarSelect *const lhs = sideEffect->getLhs<hldb::VarSelect>();
  ASSERT_NE(lhs, nullptr) << "'out2[exp]' should be a VarSelect";
  EXPECT_EQ(lhs->getName(), "out2");
  const hldb::RefObj *const expRef = lhs->getIndex<hldb::RefObj>();
  ASSERT_NE(expRef, nullptr);
  EXPECT_EQ(expRef->getName(), "exp");
  EXPECT_NE(expRef->getActual<hldb::IODecl>(), nullptr) << "'exp' should resolve to pow_flip_b's own IODecl";

  const hldb::Operation *const rhs = sideEffect->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "'base & 1' should be Operation(vpiBitAndOp)";
  EXPECT_EQ(rhs->getOpType(), vpiBitAndOp);
}

// pow_flip_b = 1; if (exp > 0) pow_flip_b = base * pow_flip_b(flip(base), exp - 1);
TEST_F(FuncSideEffectTest, PowFlipBRecursesThroughFlipInsideAnIfWithNoElse) {
  const hldb::Function *const powFlipB = getFunc("pow_flip_b");
  ASSERT_NE(powFlipB, nullptr);
  const hldb::Function *const flip = getFunc("flip");
  ASSERT_NE(flip, nullptr);
  const hldb::Begin *const body = powFlipB->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 3u);

  const hldb::Assignment *const initAssign = any_cast<hldb::Assignment>(body->getStmts()->at(1));
  ASSERT_NE(initAssign, nullptr) << "'pow_flip_b = 1;' should be an Assignment";
  const hldb::RefObj *const initLhs = initAssign->getLhs<hldb::RefObj>();
  ASSERT_NE(initLhs, nullptr);
  EXPECT_EQ(initLhs->getName(), "pow_flip_b");
  const hldb::Constant *const one = any_cast<hldb::Constant>(initAssign->getRhs());
  ASSERT_NE(one, nullptr);
  EXPECT_EQ(one->getDecompile(), "1");

  const hldb::IfStmt *const ifStmt = any_cast<hldb::IfStmt>(body->getStmts()->at(2));
  ASSERT_NE(ifStmt, nullptr) << "'if (exp > 0) ...;' should be an IfStmt";
  const hldb::Operation *const cond = ifStmt->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiGtOp);

  const hldb::Assignment *const recAssign = ifStmt->getStmt<hldb::Assignment>();
  ASSERT_NE(recAssign, nullptr) << "if-body 'pow_flip_b = base * ...;' should be a plain Assignment (no begin-end)";
  const hldb::Operation *const mul = recAssign->getRhs<hldb::Operation>();
  ASSERT_NE(mul, nullptr) << "'base * pow_flip_b(...)' should be Operation(vpiMultOp)";
  EXPECT_EQ(mul->getOpType(), vpiMultOp);
  ASSERT_NE(mul->getOperands(), nullptr);
  ASSERT_EQ(mul->getOperands()->size(), 2u);

  const hldb::FuncCall *const recCall = any_cast<hldb::FuncCall>(mul->getOperands()->at(1));
  ASSERT_NE(recCall, nullptr) << "'pow_flip_b(flip(base), exp - 1)' should be a recursive FuncCall";
  EXPECT_EQ(recCall->getName(), "pow_flip_b");
  EXPECT_EQ(recCall->getTaskFunc<hldb::Function>(), powFlipB) << "recursive call should resolve to itself";
  ASSERT_NE(recCall->getArguments(), nullptr);
  ASSERT_EQ(recCall->getArguments()->size(), 2u);
  const hldb::FuncCall *const flipCall = any_cast<hldb::FuncCall>(recCall->getArguments()->at(0));
  ASSERT_NE(flipCall, nullptr) << "'flip(base)' first argument should be a nested FuncCall";
  EXPECT_EQ(flipCall->getName(), "flip");
  EXPECT_EQ(flipCall->getTaskFunc<hldb::Function>(), flip);
}

// assign out1 = pow_flip_b(2, 2);
TEST_F(FuncSideEffectTest, Out1IsContinuouslyAssignedFromPowFlipBCallWithConstantArgs) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Function *const powFlipB = getFunc("pow_flip_b");
  ASSERT_NE(powFlipB, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = top->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "out1");

  const hldb::FuncCall *const call = ca->getRhs<hldb::FuncCall>();
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->getName(), "pow_flip_b");
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), powFlipB);
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 2u);
  const hldb::Constant *const arg0 = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(arg0, nullptr);
  EXPECT_EQ(arg0->getDecompile(), "2");
  const hldb::Constant *const arg1 = any_cast<hldb::Constant>(call->getArguments()->at(1));
  ASSERT_NE(arg1, nullptr);
  EXPECT_EQ(arg1->getDecompile(), "2");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
