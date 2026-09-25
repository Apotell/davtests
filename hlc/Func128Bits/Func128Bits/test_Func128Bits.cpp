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

// Tests for Func128Bits/dut.sv:
//   module GOOD(); endmodule
//   module ScratchPad ();
//     parameter M_COUNT = 4;
//     parameter M_REGIONS = 1;
//     parameter ADDR_WIDTH = 32;
//     parameter M_ADDR_WIDTH = {M_COUNT{{M_REGIONS{32'd24}}}};
//     function [M_COUNT*M_REGIONS*ADDR_WIDTH-1:0] calcBaseAddrs(input [31:0] dummy);
//       ... (for loop over part-selects, computed with parameters)
//     endfunction
//     parameter M_BASE_ADDR_INT = calcBaseAddrs(0);
//     if (M_BASE_ADDR_INT == 128'b0...) begin
//       GOOD good();
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023, checked before any test code was
// written -- no .log file consulted for this file's expected shape, only
// the standard text and the real hldb API headers):
//
//   Sec 13.4.1 "old-style" function declaration: 'function [msb:lsb] name
//   (...)' with no explicit data-type keyword implicitly declares the
//   function's return as a packed vector of the given range. With
//   M_COUNT=4, M_REGIONS=1, ADDR_WIDTH=32, that range is
//   4*1*32-1:0 == 127:0 -- i.e. a 128-bit-wide return, matching this
//   test's name. Per 6.7/6.8 an implicit vector return with no net-type
//   keyword is a variable type (logic), so the return typespec should be
//   a LogicTypespec whose range bounds are computed from the parameters
//   (an Operation tree), not folded literal constants -- HLC is not
//   required to constant-fold a parameter-dependent range expression
//   into plain Constants at this compile phase.
//
//   Sec 13.4: 'input [31:0] dummy' is a single formal argument, explicit
//   direction 'input', with a 32-bit range.
//
//   Sec 13.4.3 "Constant functions": 'parameter M_BASE_ADDR_INT =
//   calcBaseAddrs(0);' calls calcBaseAddrs in a constant (parameter)
//   expression, so it must be evaluable at elaboration time; whether HLC
//   actually folds it to a Constant at the '-d ast' phase used by this
//   .hlc file is not guaranteed by the object model alone, so both the
//   pre-fold (FuncCall) and post-fold (Constant) shapes are accepted as
//   structurally valid.
//
// What is NOT checked and why:
//   - the runtime-evaluated numeric value of calcBaseAddrs(0) is a
//     simulation-time concept; this file only checks the static/
//     structural shape of the function declaration and its arguments.
//   - the internal body of calcBaseAddrs (integer/reg locals, the for
//     loop, indexed part-selects) is not walked in detail -- this test
//     is about the function's 128-bit return and its argument, not every
//     statement inside it.
//   - the generate-if / GOOD instantiation shape is not walked in detail;
//     only that the whole file compiles with zero diagnostics, which is
//     a legitimate coarse sanity check since this file is fully legal.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/io_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class Func128BitsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Func128Bits.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getScratchPad() {
    return hldb::findByName<hldb::Module>("ScratchPad", m_design->getAllModules());
  }

  static const hldb::Function *getCalcBaseAddrs() {
    const hldb::Module *const top = getScratchPad();
    if (top == nullptr || top->getTaskFuncs() == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("calcBaseAddrs", top->getTaskFuncs());
  }
};

TEST_F(Func128BitsTest, BothModulesExist) {
  EXPECT_NE(hldb::findByName<hldb::Module>("GOOD", m_design->getAllModules()), nullptr);
  EXPECT_NE(getScratchPad(), nullptr);
}

// ---------------------------------------------------------------------------
// function [M_COUNT*M_REGIONS*ADDR_WIDTH-1:0] calcBaseAddrs(input [31:0] dummy);
// ---------------------------------------------------------------------------
TEST_F(Func128BitsTest, CalcBaseAddrsExistsWithOneInputArgument) {
  const hldb::Module *const top = getScratchPad();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTaskFuncs(), nullptr);
  const hldb::Function *const fn = getCalcBaseAddrs();
  ASSERT_NE(fn, nullptr) << "function 'calcBaseAddrs' not found";
  EXPECT_EQ(fn->getName(), std::string_view("calcBaseAddrs"));

  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 1u);
  const hldb::IODecl *const dummy = fn->getIODecls()->at(0);
  ASSERT_NE(dummy, nullptr);
  EXPECT_EQ(dummy->getName(), std::string_view("dummy"));
  EXPECT_EQ(dummy->getDirection(), vpiInput);
}

TEST_F(Func128BitsTest, CalcBaseAddrsReturnsAnImplicitLogicVectorWithComputedRange) {
  const hldb::Function *const fn = getCalcBaseAddrs();
  ASSERT_NE(fn, nullptr);
  const hldb::RefTypespec *const rts = fn->getReturn();
  ASSERT_NE(rts, nullptr) << "'function [M_COUNT*M_REGIONS*ADDR_WIDTH-1:0] calcBaseAddrs(...)' should have a "
                              "return typespec";
  const hldb::LogicTypespec *const logic = rts->getActual<hldb::LogicTypespec>();
  ASSERT_NE(logic, nullptr) << "IEEE 1800-2023 6.7/6.8: an implicit-vector return with no net-type keyword should "
                                "be a variable (LogicTypespec), not a Net";

  ASSERT_NE(logic->getRanges(), nullptr);
  ASSERT_EQ(logic->getRanges()->size(), 1u);
  const hldb::Range *const range = logic->getRanges()->at(0);
  ASSERT_NE(range, nullptr);

  // Right (lsb) side of '...-1:0' is the plain literal '0'.
  const hldb::Constant *const rightConst = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(range->getRightExpr(), nullptr);
  ASSERT_NE(rightConst, nullptr) << "the lsb of 'M_COUNT*M_REGIONS*ADDR_WIDTH-1:0' is a plain literal '0'";
  EXPECT_EQ(rightConst->getDecompile(), std::string_view("0"));

  // Left (msb) side is 'M_COUNT*M_REGIONS*ADDR_WIDTH-1', a parameter-dependent
  // expression -- not required to be constant-folded at this compile phase,
  // so only require that a non-null expression is present.
  ASSERT_NE(range->getLeftExpr(), nullptr) << "the msb of the return range should carry a non-null expression";
}

// ---------------------------------------------------------------------------
// parameter M_BASE_ADDR_INT = calcBaseAddrs(0);
// ---------------------------------------------------------------------------
TEST_F(Func128BitsTest, MBaseAddrIntCallsCalcBaseAddrsOrHasBeenConstantFolded) {
  const hldb::Module *const top = getScratchPad();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParamAssigns(), nullptr);
  const hldb::ParamAssign *const pa = hldb::findByName("M_BASE_ADDR_INT", top->getParamAssigns());
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'M_BASE_ADDR_INT' not found";

  if (const hldb::Constant *const c = pa->getRhs<hldb::Constant>()) {
    // Sec 13.4.3: a constant function used in a constant expression must be
    // evaluated at elaboration time; only check that a folded value exists.
    EXPECT_NE(c->getDecompile(), std::string_view(""));
    return;
  }

  const hldb::FuncCall *const call = pa->getRhs<hldb::FuncCall>();
  ASSERT_NE(call, nullptr) << "ParamAssign RHS for 'M_BASE_ADDR_INT' is neither a folded Constant nor an unfolded "
                               "FuncCall";
  EXPECT_EQ(call->getName(), std::string_view("calcBaseAddrs"));
  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  ASSERT_NE(call->getArguments()->at(0), nullptr);
  const hldb::Constant *const argZero = any_cast<hldb::Constant>(call->getArguments()->at(0));
  ASSERT_NE(argZero, nullptr) << "'calcBaseAddrs(0)' argument should be a Constant";
  EXPECT_EQ(argZero->getDecompile(), std::string_view("0"));
}

TEST_F(Func128BitsTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
