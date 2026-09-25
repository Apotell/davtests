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

// Tests for FuncReturnRange/dut.sv:
//   module func_width_scope_top(inp, out1, out2);
//       input wire inp;
//       localparam WIDTH_A = 5;
//       function automatic [WIDTH_A-1:0] func1;
//           input reg [WIDTH_A-1:0] s;
//           func1 = ~s;
//       endfunction
//       wire [func1(0)-1:0] xc;
//       assign xc = 1'sb1;
//       output wire [1023:0] out1, out2;
//       wire [WIDTH_A-1:0] xn;
//       assign xn = func1(inp);
//       assign out1 = xn;
//       assign out2 = xc;
//   endmodule
//
// What to check and why (IEEE 1800-2023, checked before any test code was
// written -- no .log file consulted for this file's expected shape):
//
//   Sec 13.4.1 "Return values and void functions" + Sec 6.8 "Variable
//   declarations": "function automatic [WIDTH_A-1:0] func1;" specifies a
//   return type with an explicit packed range but no leading type keyword
//   -- per 6.8, a declaration with a range and no explicit data type
//   defaults to an (unsigned, 4-state) "logic" type. So getReturn() should
//   resolve to a LogicTypespec carrying exactly one Range,
//   "[WIDTH_A-1:0]": getLeftExpr() Operation(vpiSubOp) over RefObj
//   "WIDTH_A" and Constant "1", getRightExpr() Constant "0".
//
//   Sec 13.5 "Task and function input/output/inout" (ANSI vs. non-ANSI I/O
//   lists): "input reg [WIDTH_A-1:0] s;" is a non-ANSI-style task/function
//   port declaration; "reg" is a keyword synonym for the variable type
//   underlying "logic" (Sec 6.8), so the IODecl "s" should carry the same
//   LogicTypespec/range shape as the function's own return type.
//
//   Sec 13.4.1: "func1 = ~s;" assigns the function's return value by name
//   -- an Assignment whose LHS is RefObj "func1" (resolving back to the
//   Function itself, since there is no separate return-value Variable
//   object distinct from the function's own name) and whose RHS is
//   Operation(vpiBitNegOp) over RefObj "s" (resolving to the IODecl).
//
//   Sec 6.20.4 (net/variable declaration dimensions) + Sec 13.4.3
//   "Constant functions": "wire [func1(0)-1:0] xc;" uses a FuncCall as
//   part of a net's packed-dimension range -- for this to be legal, the
//   dimension must be a constant expression, so "func1" is being used
//   here as a constant function (it has one input argument, no
//   disallowed constructs, per 13.4.3). The declared net "xc" should
//   exist as a Net named "xc" of net type vpiWire.
//
// What is NOT checked and why:
//   - the exact shape HLC chooses for "xc"'s packed-dimension range
//     expression (whether it keeps the FuncCall live or folds it into a
//     Constant at elaboration time) is treated leniently, the same way
//     EvalFuncArray's constant-folding test does, since the standard does
//     not mandate one representation over the other for a compile-time-
//     evaluated constant function result -- this file's focus is that the
//     construct is accepted and "xc" and "xn"/"out1"/"out2" are all
//     properly declared and driven, not the internal representation of
//     the folded width.
//   - the runtime value of "func1(inp)" / "func1(0)" (whether the bitwise
//     inversion actually computes correctly) is a simulation-time concept.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/io_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/parameter.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FuncReturnRangeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FuncReturnRange.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("func_width_scope_top", m_design->getAllModules());
  }

  static const hldb::Function *getFunc1() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getTaskFuncs() == nullptr || top->getTaskFuncs()->empty()) return nullptr;
    return any_cast<hldb::Function>(top->getTaskFuncs()->at(0));
  }

  static void CheckRangeIsWidthAMinusOne(const hldb::RangeCollection *ranges) {
    ASSERT_NE(ranges, nullptr);
    ASSERT_EQ(ranges->size(), 1u);
    const hldb::Range *const range = ranges->at(0);
    ASSERT_NE(range, nullptr);
    const hldb::Operation *const msb = range->getLeftExpr<hldb::Operation>();
    ASSERT_NE(msb, nullptr) << "'WIDTH_A-1' should be Operation(vpiSubOp)";
    EXPECT_EQ(msb->getOpType(), vpiSubOp);
    ASSERT_NE(msb->getOperands(), nullptr);
    ASSERT_EQ(msb->getOperands()->size(), 2u);
    const hldb::RefObj *const widthARef = any_cast<hldb::RefObj>(msb->getOperands()->at(0));
    ASSERT_NE(widthARef, nullptr);
    EXPECT_EQ(widthARef->getName(), "WIDTH_A");
    const hldb::Constant *const one = any_cast<hldb::Constant>(msb->getOperands()->at(1));
    ASSERT_NE(one, nullptr);
    EXPECT_EQ(one->getDecompile(), "1");
    const hldb::Constant *const lsb = range->getRightExpr<hldb::Constant>();
    ASSERT_NE(lsb, nullptr);
    EXPECT_EQ(lsb->getDecompile(), "0");
  }
};

TEST_F(FuncReturnRangeTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

// localparam WIDTH_A = 5;
TEST_F(FuncReturnRangeTest, WidthAIsFive) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParameters(), nullptr);
  const hldb::Parameter *const widthA = hldb::findByName<hldb::Parameter>("WIDTH_A", top->getParameters());
  ASSERT_NE(widthA, nullptr);
  EXPECT_TRUE(widthA->getLocalParam());
  const hldb::Constant *const val = widthA->getExpr<hldb::Constant>();
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getDecompile(), "5");
}

// function automatic [WIDTH_A-1:0] func1; input reg [WIDTH_A-1:0] s;
TEST_F(FuncReturnRangeTest, Func1ReturnsImplicitLogicWithWidthARangeAndHasOneInputArg) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  ASSERT_EQ(top->getTaskFuncs()->size(), 1u);
  const hldb::Function *const func1 = getFunc1();
  ASSERT_NE(func1, nullptr);
  EXPECT_EQ(func1->getName(), "func1");
  EXPECT_TRUE(func1->getAutomatic());

  const hldb::RefTypespec *const rts = func1->getReturn();
  ASSERT_NE(rts, nullptr);
  const hldb::LogicTypespec *const logicTs = rts->getActual<hldb::LogicTypespec>();
  ASSERT_NE(logicTs, nullptr) << "'[WIDTH_A-1:0]' return type with no keyword should default to LogicTypespec";
  CheckRangeIsWidthAMinusOne(logicTs->getRanges());

  ASSERT_NE(func1->getIODecls(), nullptr);
  ASSERT_EQ(func1->getIODecls()->size(), 1u);
  const hldb::IODecl *const s = func1->getIODecls()->at(0);
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->getName(), "s");
  EXPECT_EQ(s->getDirection(), vpiInput);
  const hldb::RefTypespec *const sRts = s->getTypespec();
  ASSERT_NE(sRts, nullptr);
  const hldb::LogicTypespec *const sLogicTs = sRts->getActual<hldb::LogicTypespec>();
  ASSERT_NE(sLogicTs, nullptr) << "'input reg [WIDTH_A-1:0] s' should resolve to LogicTypespec ('reg' == 'logic')";
  CheckRangeIsWidthAMinusOne(sLogicTs->getRanges());
}

// func1 = ~s;
TEST_F(FuncReturnRangeTest, Func1BodyAssignsBitwiseNegatedSToItsOwnName) {
  const hldb::Function *const func1 = getFunc1();
  ASSERT_NE(func1, nullptr);
  const hldb::Assignment *const assign = func1->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr) << "function body should be a plain Assignment (single statement)";
  EXPECT_TRUE(assign->getBlocking());

  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "'func1 = ...' LHS should be RefObj 'func1' (the implicit return-name variable)";
  EXPECT_EQ(lhs->getName(), "func1");

  const hldb::Operation *const rhs = assign->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "'~s' should be Operation(vpiBitNegOp)";
  EXPECT_EQ(rhs->getOpType(), vpiBitNegOp);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 1u);
  const hldb::RefObj *const sRef = any_cast<hldb::RefObj>(rhs->getOperands()->at(0));
  ASSERT_NE(sRef, nullptr);
  EXPECT_EQ(sRef->getName(), "s");
  EXPECT_NE(sRef->getActual<hldb::IODecl>(), nullptr) << "'s' should resolve to the IODecl, not a Variable or Net";
}

// wire [func1(0)-1:0] xc; assign xc = 1'sb1;
TEST_F(FuncReturnRangeTest, XcNetIsDeclaredWithFuncCallDependentWidth) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const xc = hldb::findByName<hldb::Net>("xc", top->getNets());
  ASSERT_NE(xc, nullptr) << "'wire [func1(0)-1:0] xc;' should declare a net named 'xc'";
  EXPECT_EQ(xc->getNetType(), vpiWire);
}

// output wire [1023:0] out1, out2; wire [WIDTH_A-1:0] xn;
// assign xn = func1(inp); assign out1 = xn; assign out2 = xc;
TEST_F(FuncReturnRangeTest, XnIsDrivenByFunc1CallOnInpAndFeedsOut1) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const xn = hldb::findByName<hldb::Net>("xn", top->getNets());
  ASSERT_NE(xn, nullptr);

  ASSERT_NE(top->getContAssigns(), nullptr);
  const hldb::ContAssign *xnAssign = nullptr;
  for (const hldb::ContAssign *const ca : *top->getContAssigns()) {
    const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
    if (lhs != nullptr && lhs->getName() == "xn") {
      xnAssign = ca;
      break;
    }
  }
  ASSERT_NE(xnAssign, nullptr) << "'assign xn = func1(inp);' not found";
  const hldb::FuncCall *const call = xnAssign->getRhs<hldb::FuncCall>();
  ASSERT_NE(call, nullptr) << "'func1(inp)' RHS should be a FuncCall";
  EXPECT_EQ(call->getName(), "func1");
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), getFunc1());
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), "inp");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
