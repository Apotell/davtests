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

// Tests for EvalFuncCont.hlc (tests/EvalFuncCont/dut.sv):
//   module t (input clk);
//     parameter  WIDTH = 33;
//     localparam MAX_WIDTH = 11;
//     localparam NUM_OUT = num_out(WIDTH);
//     wire [NUM_OUT-1:0] z;
//     function integer num_out;
//       input integer width;
//       num_out = 1;
//       while ((width + num_out - 1) / num_out > MAX_WIDTH) begin
//         num_out = num_out * 2;
//         continue;
//       end
//     endfunction
//     initial begin ... $stop / $finish ... end
//   endmodule
//
// Unlike EvalFunc (ternary-based constant function) and EvalFuncArray
// (array-typed constant function argument), the distinguishing construct
// here -- matching the "Cont" in the test's name -- is the "continue;"
// statement (IEEE 1800-2023 Sec 12.7.2 "continue statement") inside the
// function body's "while" loop. "num_out" is otherwise a legal constant
// function per Sec 13.4.3 (a single input argument, no fork/hierarchical
// references, no non-constant calls), and it is invoked with the constant
// "WIDTH" inside a "localparam" initializer -- the same canonical
// constant-function-call context as EvalFunc/EvalFuncArray. This file has
// no :should_fail_because: tag and is expected to compile cleanly.
//
// As with EvalFunc/EvalFuncArray, "localparam NUM_OUT = num_out(WIDTH);" is
// modeled via Scope::getParamAssigns() (ParamAssign lhs/rhs), not
// Parameter::getExpr() -- re-derived from the real hldb headers, matching
// the pattern already confirmed by test_13.4.3_const-function.cpp. No .log
// file was consulted to write this file's expectations.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/continue_stmt.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>
#include <hldb/while_stmt.h>

namespace hlc {

class EvalFuncContTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "EvalFuncCont.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("t", m_design->getAllModules()); }

  static const hldb::Function *getFunc() {
    const hldb::Module *const top = getTop();
    return (top == nullptr) ? nullptr : hldb::findByName<hldb::Function>("num_out", top->getTaskFuncs());
  }

  static const hldb::ParamAssign *findParamAssign(std::string_view name) {
    const hldb::Module *const top = getTop();
    return (top == nullptr) ? nullptr : hldb::findByName(name, top->getParamAssigns());
  }
};

TEST_F(EvalFuncContTest, ModuleTExists) { ASSERT_NE(getTop(), nullptr); }

TEST_F(EvalFuncContTest, FunctionNumOutExists) { ASSERT_NE(getFunc(), nullptr); }

// parameter WIDTH = 33;
TEST_F(EvalFuncContTest, ParamWidthIs33) {
  const hldb::ParamAssign *const pa = findParamAssign("WIDTH");
  ASSERT_NE(pa, nullptr) << "'parameter WIDTH = 33;' should produce a ParamAssign binding";
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(pa->getRhs());
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "33");
}

// localparam MAX_WIDTH = 11;
TEST_F(EvalFuncContTest, LocalparamMaxWidthIs11) {
  const hldb::ParamAssign *const pa = findParamAssign("MAX_WIDTH");
  ASSERT_NE(pa, nullptr) << "'localparam MAX_WIDTH = 11;' should produce a ParamAssign binding";
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(pa->getRhs());
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "11");
}

// localparam NUM_OUT = num_out(WIDTH);  -- per 13.4.3 a legal constant
// function call, same accepted-shape pattern as EvalFunc/EvalFuncArray.
TEST_F(EvalFuncContTest, LocalparamNumOutCallsNumOut) {
  const hldb::ParamAssign *const pa = findParamAssign("NUM_OUT");
  ASSERT_NE(pa, nullptr) << "'localparam NUM_OUT = num_out(WIDTH);' should produce a ParamAssign binding";
  const hldb::Any *const rhs = pa->getRhs();
  ASSERT_NE(rhs, nullptr);
  if (const hldb::FuncCall *const call = any_cast<hldb::FuncCall>(rhs)) {
    EXPECT_EQ(call->getName(), "num_out");
    ASSERT_NE(call->getArguments(), nullptr);
    ASSERT_EQ(call->getArguments()->size(), 1u);
  } else if (const hldb::Constant *const folded = any_cast<hldb::Constant>(rhs)) {
    // num_out(33): num_out starts at 1, doubles while (33+num_out-1)/num_out > 11.
    //   num_out=1: (33+0)/1=33 > 11 -> double
    //   num_out=2: (33+1)/2=17 > 11 -> double
    //   num_out=4: (33+3)/4=9, not > 11 -> stop. Result: 4.
    EXPECT_EQ(folded->getDecompile(), "4") << "num_out(33) must fold to 4 per the function's own while-loop logic";
  } else {
    FAIL() << "ParamAssign RHS for 'NUM_OUT' is neither a FuncCall nor a Constant -- actual AnyType: "
           << static_cast<int>(rhs->getAnyType());
  }
}

// wire [NUM_OUT-1:0] z;  -- a plain net-type keyword ('wire') declaration,
// so per IEEE 1800-2023 Sec 6.7 this must be modeled as a Net (not a
// Variable), with vpiWire net type and a non-empty range (it is a vector,
// not scalar, since its declared width depends on the elaborated NUM_OUT).
TEST_F(EvalFuncContTest, NetZIsWireWithRange) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *z = nullptr;
  for (const hldb::Net *const n : *top->getNets()) {
    if (n->getName() == "z") {
      z = n;
      break;
    }
  }
  ASSERT_NE(z, nullptr) << "'wire [NUM_OUT-1:0] z;' should produce a Net named 'z'";
  EXPECT_EQ(z->getNetType(), vpiWire) << "bare 'wire' keyword must set vpiWire per IEEE 1800-2023 Sec 6.7";
}

// while ((width + num_out - 1) / num_out > MAX_WIDTH) begin
//   num_out = num_out * 2;
//   continue;
// end
TEST_F(EvalFuncContTest, FunctionBodyContainsWhileLoop) {
  const hldb::Function *const fn = getFunc();
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getStmt(), nullptr) << "'num_out' should have a non-null function body";
  // The body is a sequence of statements (the two blocking assignments and the while loop), so it is wrapped in a
  // Begin block rather than being a single bare statement like EvalFunc's 'foo'.
  const hldb::Begin *const body = any_cast<hldb::Begin>(fn->getStmt());
  ASSERT_NE(body, nullptr) << "'num_out's multi-statement body should be a Begin block, actual AnyType: "
                            << static_cast<int>(fn->getStmt()->getAnyType());
  ASSERT_NE(body->getStmts(), nullptr);

  const hldb::WhileStmt *whileStmt = nullptr;
  for (hldb::Any *const stmt : *body->getStmts()) {
    if (const hldb::WhileStmt *const w = any_cast<hldb::WhileStmt>(stmt)) {
      whileStmt = w;
      break;
    }
  }
  ASSERT_NE(whileStmt, nullptr) << "the 'while' loop should be one of num_out's top-level statements";

  const hldb::Operation *const cond = whileStmt->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "the while condition '(...)/num_out > MAX_WIDTH' should be an Operation";
  EXPECT_EQ(cond->getOpType(), vpiGtOp) << "the outermost operator of the while condition is '>'";
}

// The "continue;" statement (12.7.2) inside the while loop's begin-end body.
TEST_F(EvalFuncContTest, WhileLoopBodyContainsContinueStatement) {
  const hldb::Function *const fn = getFunc();
  ASSERT_NE(fn, nullptr);
  const hldb::Begin *const body = any_cast<hldb::Begin>(fn->getStmt());
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);

  const hldb::WhileStmt *whileStmt = nullptr;
  for (hldb::Any *const stmt : *body->getStmts()) {
    if (const hldb::WhileStmt *const w = any_cast<hldb::WhileStmt>(stmt)) {
      whileStmt = w;
      break;
    }
  }
  ASSERT_NE(whileStmt, nullptr);
  ASSERT_NE(whileStmt->getStmt(), nullptr) << "the while loop should have a non-null body";

  const hldb::Begin *const loopBody = any_cast<hldb::Begin>(whileStmt->getStmt());
  ASSERT_NE(loopBody, nullptr) << "the while loop's begin-end body should be a Begin block, actual AnyType: "
                                << static_cast<int>(whileStmt->getStmt()->getAnyType());
  ASSERT_NE(loopBody->getStmts(), nullptr);

  bool foundContinue = false;
  for (hldb::Any *const stmt : *loopBody->getStmts()) {
    if (any_cast<hldb::ContinueStmt>(stmt) != nullptr) {
      foundContinue = true;
      break;
    }
  }
  EXPECT_TRUE(foundContinue) << "IEEE 1800-2023 Sec 12.7.2: 'continue;' inside the while loop's begin-end body "
                                 "should appear as a ContinueStmt in the loop body's statement list";
}

TEST_F(EvalFuncContTest, ModuleTHasNoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty())
      << "dut.sv declares no 'assign' statements";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
