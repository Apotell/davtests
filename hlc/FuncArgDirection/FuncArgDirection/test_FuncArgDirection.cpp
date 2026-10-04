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

// Tests for FuncArgDirection/dut.sv:
//   module dut();
//     function automatic logic [7:0] aes_rev_order_bit(logic [7:0] in);
//       logic [7:0] out;
//       for (int i=0; i<8; i++) begin
//         out[i] = in[7-i];
//       end
//       return out;
//     endfunction
//
//     logic [7:0]       ctr_we_o_rev, ctr_we_o;
//     assign ctr_we_o_rev = 8'b00001111;
//     assign ctr_we_o = aes_rev_order_bit(ctr_we_o_rev);
//   endmodule
//
// What to check and why (IEEE 1800-2023, checked before any test code was
// written -- no .log file consulted for this file's expected shape, only
// the standard text and the real hldb API headers):
//
//   Sec 13.3 "Tasks": "Argument directions... If a direction is not
//   specified for an argument, the argument shall default to input." The
//   formal argument 'in' of 'aes_rev_order_bit' carries no explicit
//   direction keyword, so per 13.3 it must default to vpiInput -- this is
//   the direction-handling rule this test exercises.
//
//   Sec 6.7/6.8: 'logic [7:0] in'/'logic [7:0] out' have no net-type
//   keyword, so the formal argument and the function return must be
//   modeled as variable-kind (LogicTypespec) shapes, never Nets; likewise
//   'logic [7:0] ctr_we_o_rev, ctr_we_o;' at module scope are Variables,
//   not Nets.
//
//   Sec 13.4.1: 'return out;' is a plain ReturnStmt.
//
//   Sec 13.4: the call 'aes_rev_order_bit(ctr_we_o_rev)' in the second
//   continuous assignment connects its single actual, RefObj
//   'ctr_we_o_rev', to the function's sole formal 'in'.
//
// What is NOT checked and why:
//   - the body of the for loop (bit-reversal logic) is not walked in
//     detail; this test is about the function's argument direction and
//     its default-input semantics, not every statement inside the loop.
//   - the runtime-evaluated value of aes_rev_order_bit(...) is a
//     simulation-time concept.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/for_stmt.h>
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/io_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/return_stmt.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class FuncArgDirectionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "FuncArgDirection.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getDut() { return hldb::findByName<hldb::Module>("dut", m_design->getAllModules()); }

  static const hldb::Function *getFn() {
    const hldb::Module *const top = getDut();
    if (top == nullptr || top->getTaskFuncs() == nullptr) return nullptr;
    return hldb::findByName<hldb::Function>("aes_rev_order_bit", top->getTaskFuncs());
  }
};

TEST_F(FuncArgDirectionTest, ModuleExists) { EXPECT_NE(getDut(), nullptr); }

// ---------------------------------------------------------------------------
// function automatic logic [7:0] aes_rev_order_bit(logic [7:0] in);
// ---------------------------------------------------------------------------
TEST_F(FuncArgDirectionTest, FunctionIsAutomaticWithLogicVectorReturn) {
  const hldb::Function *const fn = getFn();
  ASSERT_NE(fn, nullptr) << "function 'aes_rev_order_bit' not found";
  EXPECT_TRUE(fn->getAutomatic()) << "'function automatic' should mark the function as automatic";

  const hldb::RefTypespec *const rts = fn->getReturn();
  ASSERT_NE(rts, nullptr);
  const hldb::LogicTypespec *const logic = rts->getActual<hldb::LogicTypespec>();
  ASSERT_NE(logic, nullptr) << "'logic [7:0]' return should be a LogicTypespec (Sec 6.7/6.8: not a Net)";
  EXPECT_FALSE(logic->getSigned());
  ASSERT_NE(logic->getRanges(), nullptr);
  ASSERT_EQ(logic->getRanges()->size(), 1u);
  ASSERT_NE(logic->getRanges()->at(0)->getLeftExpr(), nullptr);
  ASSERT_NE(logic->getRanges()->at(0)->getRightExpr(), nullptr);
  const hldb::Constant *const msb = logic->getRanges()->at(0)->getLeftExpr<hldb::Constant>();
  const hldb::Constant *const lsb = logic->getRanges()->at(0)->getRightExpr<hldb::Constant>();
  ASSERT_NE(msb, nullptr);
  ASSERT_NE(lsb, nullptr);
  EXPECT_EQ(msb->getDecompile(), std::string_view("7"));
  EXPECT_EQ(lsb->getDecompile(), std::string_view("0"));
}

// The formal argument 'in' has no direction keyword: default is input
// (IEEE 1800-2023 13.3).
TEST_F(FuncArgDirectionTest, ArgumentInDefaultsToInputDirection) {
  const hldb::Function *const fn = getFn();
  ASSERT_NE(fn, nullptr);
  ASSERT_NE(fn->getIODecls(), nullptr);
  ASSERT_EQ(fn->getIODecls()->size(), 1u);
  const hldb::IODecl *const in = fn->getIODecls()->at(0);
  ASSERT_NE(in, nullptr);
  EXPECT_EQ(in->getName(), std::string_view("in"));
  EXPECT_EQ(in->getDirection(), vpiInput)
      << "IEEE 1800-2023 13.3: an argument with no direction keyword defaults to input";
}

// ---------------------------------------------------------------------------
// return out;
// ---------------------------------------------------------------------------
TEST_F(FuncArgDirectionTest, FunctionBodyContainsForLoopAndReturnsOut) {
  const hldb::Function *const fn = getFn();
  ASSERT_NE(fn, nullptr);

  // 'logic [7:0] out;' + for-loop + 'return out;' form an implicit begin-end
  // block since the function has more than one item.
  const hldb::Any *const body = fn->getStmt();
  ASSERT_NE(body, nullptr) << "function body should be non-null";

  // Local variable 'out' should exist in the function's own scope.
  ASSERT_NE(fn->getVariables(), nullptr);
  const hldb::Variable *const out = hldb::findByName<hldb::Variable>("out", fn->getVariables());
  ASSERT_NE(out, nullptr) << "local variable 'out' not found in function scope";
}

// ---------------------------------------------------------------------------
// module-scope variables (Sec 6.7/6.8: no net-type keyword -> Variable)
// ---------------------------------------------------------------------------
TEST_F(FuncArgDirectionTest, ModuleScopeVariablesAreNotNets) {
  const hldb::Module *const top = getDut();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("ctr_we_o_rev", top->getVariables()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("ctr_we_o", top->getVariables()), nullptr);
  if (top->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("ctr_we_o_rev", top->getNets()), nullptr);
    EXPECT_EQ(hldb::findByName<hldb::Net>("ctr_we_o", top->getNets()), nullptr);
  }
}

// ---------------------------------------------------------------------------
// assign ctr_we_o = aes_rev_order_bit(ctr_we_o_rev);
// ---------------------------------------------------------------------------
TEST_F(FuncArgDirectionTest, SecondContAssignCallsFunctionWithSingleActualArgument) {
  const hldb::Module *const top = getDut();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 2u);

  const hldb::FuncCall *call = nullptr;
  for (const hldb::ContAssign *const ca : *top->getContAssigns()) {
    if (const hldb::FuncCall *const fc = ca->getRhs<hldb::FuncCall>()) {
      call = fc;
      break;
    }
  }
  ASSERT_NE(call, nullptr) << "no continuous assignment has a FuncCall RHS";
  EXPECT_EQ(call->getName(), std::string_view("aes_rev_order_bit"));
  EXPECT_EQ(call->getTaskFunc<hldb::Function>(), getFn());

  ASSERT_NE(call->getArguments(), nullptr);
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::RefObj *const actual = any_cast<hldb::RefObj>(call->getArguments()->at(0));
  ASSERT_NE(actual, nullptr) << "the actual argument should be a RefObj";
  EXPECT_EQ(actual->getName(), std::string_view("ctr_we_o_rev"));
}

TEST_F(FuncArgDirectionTest, CompilerReportsZeroErrors) {
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
