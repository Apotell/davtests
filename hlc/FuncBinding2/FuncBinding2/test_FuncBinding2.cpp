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

// Tests for FuncBinding2.hlc (tests/FuncBinding2/dut.sv):
//
//   module vend(clock, newspaper);
//     input clock;
//     output newspaper;
//     wire newspaper;
//     parameter s0 = 2'b00;
//
//     function [2:0] fsm;
//       input [1:0] fsm_PRES_STATE;
//       reg fsm_newspaper;
//       begin
//         case (fsm_PRES_STATE)
//           s0: begin
//             if (fsm_coin == 2'b10) begin
//               fsm_newspaper = 1'b0;
//             end
//           end
//         endcase
//         fsm = fsm_newspaper;
//       end
//     endfunction
//
//     always @(posedge clock)
//     begin
//       assign newspaper = fsm(PRES_STATE);
//     end
//   endmodule
//
// What distinguishes this from FuncBinding.hlc: 'fsm' itself is a perfectly
// ordinary, cleanly-bindable function (declared and called in the same
// module, old-style Verilog-2001 'input'-only ports, assignment-to-
// function-name return per Sec 13.4.1). The corner case under test is that
// TWO identifiers used inside/around the call are never declared anywhere
// in 'vend': the actual argument 'PRES_STATE' at the call site 'fsm(PRES_STATE)',
// and 'fsm_coin' referenced inside the function body's 'if' condition.
//
// IEEE 1800-2023 Sec 6.10 "Implicit declarations" only infers an implicit
// net for a scalar (1-bit) identifier that is the target of a continuous
// assignment or is connected to an unconnected port -- it does NOT cover a
// general-expression *read* reference such as an actual argument to a
// function call or an operand of '=='. Sec 6.3/23.9: any other undeclared
// identifier reference is simply unresolvable. So both 'PRES_STATE' and
// 'fsm_coin' should fail to bind (COMP_FAILED_TO_BIND), while 'fsm' itself,
// its own IODecl 'fsm_PRES_STATE', its local 'fsm_newspaper', and the
// module-level 'newspaper'/'clock'/'s0' should all bind cleanly.
//
// No .log file was consulted; accessor names were confirmed against the
// real hldb headers under
// E:\Davenche\hlc\hlc_03\out\install\x64-Debug\include\hldb. Per the test
// guide, error presence/absence is checked via Test::findError() rather
// than counting nbError/nbWarning.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assign_stmt.h>
#include <hldb/begin.h>
#include <hldb/case_item.h>
#include <hldb/case_stmt.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/io_decl.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FuncBinding2Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FuncBinding2.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("vend", m_design->getAllModules()); }

  static const hldb::Function *getFsmFunction(const hldb::Module *m) {
    return (m == nullptr) ? nullptr : hldb::findByName<hldb::Function>("fsm", m->getTaskFuncs());
  }
};

TEST_F(FuncBinding2Test, ModuleExists) { ASSERT_NE(getTop(), nullptr); }

TEST_F(FuncBinding2Test, NewspaperIsAWireAndClockIsAnInputPort) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const newspaper = hldb::findByName<hldb::Net>("newspaper", top->getNets());
  ASSERT_NE(newspaper, nullptr) << "'wire newspaper;' not found";
  EXPECT_EQ(newspaper->getNetType(), vpiWire);

  ASSERT_NE(top->getIODecls(), nullptr);
  const hldb::IODecl *clock = nullptr;
  for (const hldb::IODecl *const io : *top->getIODecls()) {
    if (io->getName() == "clock") clock = io;
  }
  ASSERT_NE(clock, nullptr) << "'input clock;' not found";
  EXPECT_EQ(clock->getDirection(), vpiInput);
}

TEST_F(FuncBinding2Test, S0ParamIsBoundConstant2b00) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParameters(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Parameter>("s0", top->getParameters()), nullptr)
      << "'parameter s0 = 2'b00;' not found";
}

// 'function [2:0] fsm; input [1:0] fsm_PRES_STATE; ...' -- old-style
// Verilog-2001 function header; the formal is implicitly 'input' (Sec 13.4).
TEST_F(FuncBinding2Test, FsmFunctionDeclaredWithOneInputIODecl) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Function *const fn = getFsmFunction(top);
  ASSERT_NE(fn, nullptr) << "'fsm' function not found";

  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 1u);
  const hldb::IODecl *const io = fn->getIODecls()->at(0);
  ASSERT_NE(io, nullptr);
  EXPECT_EQ(io->getName(), std::string_view("fsm_PRES_STATE"));
  EXPECT_EQ(io->getDirection(), vpiInput);

  // 'reg fsm_newspaper;' -- a local variable of the function's own scope.
  ASSERT_NE(fn->getVariables(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("fsm_newspaper", fn->getVariables()), nullptr)
      << "'reg fsm_newspaper;' not found among the function's local variables";
}

// 'if (fsm_coin == 2'b10) ...' inside the function body -- 'fsm_coin' is
// never declared anywhere in 'vend' (Sec 6.10 does not extend implicit-net
// inference to a general expression read), so it must fail to bind.
TEST_F(FuncBinding2Test, UndeclaredFsmCoinFailsToBind) {
  ASSERT_NE(getTop(), nullptr);
  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, std::string_view("fsm_coin")), nullptr)
      << "'fsm_coin' is never declared in 'vend' and must produce COMP_FAILED_TO_BIND";
}

// 'assign newspaper = fsm(PRES_STATE);' -- the actual argument 'PRES_STATE'
// is never declared anywhere in 'vend' either; it must fail to bind, even
// though the call itself resolves fine to the 'fsm' function.
TEST_F(FuncBinding2Test, CallSiteBindsToFsmButUndeclaredPresStateArgumentFailsToBind) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Function *const fn = getFsmFunction(top);
  ASSERT_NE(fn, nullptr);

  EXPECT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, std::string_view("PRES_STATE")), nullptr)
      << "'PRES_STATE' is never declared in 'vend' and must produce COMP_FAILED_TO_BIND";

  ASSERT_NE(top->getProcesses(), nullptr);
  const hldb::Always *always = nullptr;
  for (const hldb::Process *const p : *top->getProcesses()) {
    if (const hldb::Always *const a = any_cast<hldb::Always>(p)) always = a;
  }
  ASSERT_NE(always, nullptr) << "'always @(posedge clock) ...' not found";
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  const hldb::Begin *const blk = ec->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 1u);
  const hldb::AssignStmt *const asg = any_cast<hldb::AssignStmt>(blk->getStmts()->at(0));
  ASSERT_NE(asg, nullptr) << "'assign newspaper = fsm(PRES_STATE);' should be a procedural AssignStmt";

  const hldb::FuncCall *const call = asg->getRhs<hldb::FuncCall>();
  ASSERT_NE(call, nullptr) << "RHS should still be a FuncCall to 'fsm', despite its argument failing to bind";
  EXPECT_EQ(call->getName(), std::string_view("fsm"));
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), fn)
      << "'fsm(...)' itself must still bind to the 'fsm' declaration; only its argument is unresolved";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
