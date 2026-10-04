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

// Tests for tests/GenForDec/dut.sv -- four generate-for loops exercising
// every genvar_iteration form IEEE 1800-2023 Sec 27.4 allows besides plain
// "i = i + 1" (already covered by DoubleLoop/DoubleLoop/test_DoubleLoop.cpp):
// post-decrement ("i--"), post-increment ("i++"), and the two compound-
// assignment forms ("i += 1", "i -= 1"):
//
//   module top();
//      for (i = 3; i > 0 ; i--)   begin assign tmp[i] = 1'b1; end
//      for (i = 0; i < 2 ; i++)   begin assign tmp[i] = 1'b1; end
//      for (i = 0; i < 2 ; i+=1)  begin assign tmp[i] = 1'b1; end
//      for (i = 3; i > 0 ; i-=1)  begin assign tmp[i] = 1'b1; end
//   endmodule
//
// Note: despite this test's name, the actual source declares no "genvar"
// anywhere (neither inline in a loop header nor as a standalone
// declaration) -- it is not an inline-genvar-declaration variant. As
// directed, this file is written from what tests/GenForDec/dut.sv actually
// contains, not from the name; like GenFor/GenFor/test_GenFor.cpp, it
// leaves the (missing) genvar-declaration legality question to a dedicated
// error-catalog test and focuses purely on parse-time loop_generate_
// construct shape.
//
// -- rules under test ---------------------------------------------------
//
// Sec 27.4 "genvar_iteration ::= genvar_identifier assignment_operator
// genvar_expression | inc_or_dec_operator genvar_identifier |
// genvar_identifier inc_or_dec_operator":
//   - "i--" / "i++" (inc_or_dec_operator forms) are standalone unary
//     Operations (vpiPostDecOp / vpiPostIncOp) with one operand, RefObj
//     "i" -- the same shape already established across this suite for a
//     plain post-increment/-decrement step (see e.g.
//     Google/chapter-12/12.7.1--for/test_12.7.1_for.cpp,
//     Google/chapter-11/11.4.2--unary_op_dec/test_11.4.2_unary_op_dec.cpp).
//   - "i += 1" / "i -= 1" (assignment_operator forms): per Sec 11.4.13 "A
//     compound assignment operator shall be equivalent to the expanded
//     assignment using the corresponding binary operator", so "i += 1" is
//     defined identically to "i = i + 1" and "i -= 1" identically to
//     "i = i - 1" -- the same Assignment(RefObj, Operation(vpiAddOp/
//     vpiSubOp, RefObj, Constant)) shape DoubleLoop already established
//     for the expanded form.
//
// GenForDec.hlc uses "-d db -d ast" (no "-d inst"), so no elaboration/
// unrolling is checked here.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/cont_assign.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_for.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

#include <vector>

namespace hlc {

class GenForDecTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "GenForDec.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  // All four loops are direct GenFor items at module scope (no surrounding
  // "generate"/"endgenerate"), in source order.
  static std::vector<const hldb::GenFor *> getLoops() {
    std::vector<const hldb::GenFor *> result;
    const hldb::Module *const m = getModule("top");
    if (m == nullptr || m->getGenStmts() == nullptr) return result;
    for (const hldb::Any *const stmt : *m->getGenStmts()) {
      const hldb::GenFor *const loop = any_cast<hldb::GenFor>(stmt);
      if (loop != nullptr) result.emplace_back(loop);
    }
    return result;
  }

  // Checks 'genvar_initialization ; genvar_expression': "i = start; i <op> bound".
  static void CheckHeader(const hldb::GenFor *loop, int32_t start, std::string_view relOp, int32_t bound) {
    ASSERT_NE(loop, nullptr);
    ASSERT_NE(loop->getForInitStmts(), nullptr);
    ASSERT_EQ(loop->getForInitStmts()->size(), 1u);
    const hldb::Assignment *const init = any_cast<hldb::Assignment>(loop->getForInitStmts()->at(0));
    ASSERT_NE(init, nullptr);
    const hldb::RefObj *const initLhs = init->getLhs<hldb::RefObj>();
    ASSERT_NE(initLhs, nullptr);
    EXPECT_EQ(initLhs->getName(), std::string_view{"i"});
    const hldb::Constant *const initRhs = init->getRhs<hldb::Constant>();
    ASSERT_NE(initRhs, nullptr);
    EXPECT_EQ(initRhs->getDecompile(), std::to_string(start));

    const hldb::Operation *const cond = loop->getCondition<hldb::Operation>();
    ASSERT_NE(loop->getCondition(), nullptr);
    ASSERT_NE(cond, nullptr) << relOp << ": condition must be an Operation";
    ASSERT_NE(cond->getOperands(), nullptr);
    ASSERT_EQ(cond->getOperands()->size(), 2u);
    const hldb::RefObj *const condLhs = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
    ASSERT_NE(condLhs, nullptr);
    EXPECT_EQ(condLhs->getName(), std::string_view{"i"});
    const hldb::Constant *const condRhs = any_cast<hldb::Constant>(cond->getOperands()->at(1));
    ASSERT_NE(condRhs, nullptr);
    EXPECT_EQ(condRhs->getDecompile(), std::to_string(bound));
  }

  // Checks the loop body: 'begin assign tmp[i] = 1'b1; end'.
  static void CheckBody(const hldb::GenFor *loop) {
    ASSERT_NE(loop, nullptr);
    const hldb::Begin *const body = loop->getStmt<hldb::Begin>();
    ASSERT_NE(loop->getStmt(), nullptr);
    ASSERT_NE(body, nullptr);
    ASSERT_NE(body->getStmts(), nullptr);
    ASSERT_EQ(body->getStmts()->size(), 1u);
    const hldb::ContAssign *const assign = any_cast<hldb::ContAssign>(body->getStmts()->at(0));
    ASSERT_NE(assign, nullptr);
    ASSERT_NE(assign->getLhs(), nullptr);
    const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
    ASSERT_NE(rhs, nullptr);
    EXPECT_EQ(rhs->getDecompile(), "1");
  }
};

TEST_F(GenForDecTest, ModuleTopExists) { ASSERT_NE(getModule("top"), nullptr) << "module 'top' not found"; }

TEST_F(GenForDecTest, ExactlyFourLoops) { EXPECT_EQ(getLoops().size(), 4u); }

// for (i = 3; i > 0 ; i--) begin ... end
TEST_F(GenForDecTest, Loop1_HeaderIsIFrom3DownToNonPositive) {
  const std::vector<const hldb::GenFor *> loops = getLoops();
  ASSERT_EQ(loops.size(), 4u);
  CheckHeader(loops[0], 3, "i > 0", 0);
  EXPECT_EQ(loops[0]->getCondition<hldb::Operation>()->getOpType(), vpiGtOp);
}

TEST_F(GenForDecTest, Loop1_IterationIsPostDecrement) {
  const std::vector<const hldb::GenFor *> loops = getLoops();
  ASSERT_EQ(loops.size(), 4u);
  ASSERT_NE(loops[0]->getForIncStmts(), nullptr);
  ASSERT_EQ(loops[0]->getForIncStmts()->size(), 1u);
  const hldb::Operation *const dec = any_cast<hldb::Operation>(loops[0]->getForIncStmts()->at(0));
  ASSERT_NE(dec, nullptr) << "'i--': genvar_iteration must be a standalone Operation";
  EXPECT_EQ(dec->getOpType(), vpiPostDecOp);
  ASSERT_NE(dec->getOperands(), nullptr);
  ASSERT_EQ(dec->getOperands()->size(), 1u);
  const hldb::RefObj *const operand = any_cast<hldb::RefObj>(dec->getOperands()->at(0));
  ASSERT_NE(operand, nullptr);
  EXPECT_EQ(operand->getName(), std::string_view{"i"});
}

TEST_F(GenForDecTest, Loop1_Body) {
  const std::vector<const hldb::GenFor *> loops = getLoops();
  ASSERT_EQ(loops.size(), 4u);
  CheckBody(loops[0]);
}

// for (i = 0; i < 2 ; i++) begin ... end
TEST_F(GenForDecTest, Loop2_HeaderIsIFrom0Below2) {
  const std::vector<const hldb::GenFor *> loops = getLoops();
  ASSERT_EQ(loops.size(), 4u);
  CheckHeader(loops[1], 0, "i < 2", 2);
  EXPECT_EQ(loops[1]->getCondition<hldb::Operation>()->getOpType(), vpiLtOp);
}

TEST_F(GenForDecTest, Loop2_IterationIsPostIncrement) {
  const std::vector<const hldb::GenFor *> loops = getLoops();
  ASSERT_EQ(loops.size(), 4u);
  ASSERT_NE(loops[1]->getForIncStmts(), nullptr);
  ASSERT_EQ(loops[1]->getForIncStmts()->size(), 1u);
  const hldb::Operation *const inc = any_cast<hldb::Operation>(loops[1]->getForIncStmts()->at(0));
  ASSERT_NE(inc, nullptr) << "'i++': genvar_iteration must be a standalone Operation";
  EXPECT_EQ(inc->getOpType(), vpiPostIncOp);
  ASSERT_NE(inc->getOperands(), nullptr);
  ASSERT_EQ(inc->getOperands()->size(), 1u);
  const hldb::RefObj *const operand = any_cast<hldb::RefObj>(inc->getOperands()->at(0));
  ASSERT_NE(operand, nullptr);
  EXPECT_EQ(operand->getName(), std::string_view{"i"});
}

TEST_F(GenForDecTest, Loop2_Body) {
  const std::vector<const hldb::GenFor *> loops = getLoops();
  ASSERT_EQ(loops.size(), 4u);
  CheckBody(loops[1]);
}

// for (i = 0; i < 2 ; i+=1) begin ... end -- Sec 11.4.13: "i += 1" is
// equivalent to "i = i + 1".
TEST_F(GenForDecTest, Loop3_HeaderIsIFrom0Below2) {
  const std::vector<const hldb::GenFor *> loops = getLoops();
  ASSERT_EQ(loops.size(), 4u);
  CheckHeader(loops[2], 0, "i < 2", 2);
  EXPECT_EQ(loops[2]->getCondition<hldb::Operation>()->getOpType(), vpiLtOp);
}

TEST_F(GenForDecTest, Loop3_CompoundPlusEqualsIsEquivalentToIPlusOne) {
  const std::vector<const hldb::GenFor *> loops = getLoops();
  ASSERT_EQ(loops.size(), 4u);
  ASSERT_NE(loops[2]->getForIncStmts(), nullptr);
  ASSERT_EQ(loops[2]->getForIncStmts()->size(), 1u);
  const hldb::Assignment *const inc = any_cast<hldb::Assignment>(loops[2]->getForIncStmts()->at(0));
  ASSERT_NE(inc, nullptr) << "Sec 11.4.13: 'i += 1' must be equivalent to the Assignment 'i = i + 1'";
  const hldb::RefObj *const lhs = inc->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view{"i"});
  const hldb::Operation *const rhs = inc->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getOpType(), vpiAddOp);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);
  const hldb::RefObj *const rhsVar = any_cast<hldb::RefObj>(rhs->getOperands()->at(0));
  ASSERT_NE(rhsVar, nullptr);
  EXPECT_EQ(rhsVar->getName(), std::string_view{"i"});
  const hldb::Constant *const rhsOne = any_cast<hldb::Constant>(rhs->getOperands()->at(1));
  ASSERT_NE(rhsOne, nullptr);
  EXPECT_EQ(rhsOne->getDecompile(), "1");
}

TEST_F(GenForDecTest, Loop3_Body) {
  const std::vector<const hldb::GenFor *> loops = getLoops();
  ASSERT_EQ(loops.size(), 4u);
  CheckBody(loops[2]);
}

// for (i = 3; i > 0 ; i-=1) begin ... end -- Sec 11.4.13: "i -= 1" is
// equivalent to "i = i - 1".
TEST_F(GenForDecTest, Loop4_HeaderIsIFrom3DownToNonPositive) {
  const std::vector<const hldb::GenFor *> loops = getLoops();
  ASSERT_EQ(loops.size(), 4u);
  CheckHeader(loops[3], 3, "i > 0", 0);
  EXPECT_EQ(loops[3]->getCondition<hldb::Operation>()->getOpType(), vpiGtOp);
}

TEST_F(GenForDecTest, Loop4_CompoundMinusEqualsIsEquivalentToIMinusOne) {
  const std::vector<const hldb::GenFor *> loops = getLoops();
  ASSERT_EQ(loops.size(), 4u);
  ASSERT_NE(loops[3]->getForIncStmts(), nullptr);
  ASSERT_EQ(loops[3]->getForIncStmts()->size(), 1u);
  const hldb::Assignment *const inc = any_cast<hldb::Assignment>(loops[3]->getForIncStmts()->at(0));
  ASSERT_NE(inc, nullptr) << "Sec 11.4.13: 'i -= 1' must be equivalent to the Assignment 'i = i - 1'";
  const hldb::RefObj *const lhs = inc->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view{"i"});
  const hldb::Operation *const rhs = inc->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getOpType(), vpiSubOp);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);
  const hldb::RefObj *const rhsVar = any_cast<hldb::RefObj>(rhs->getOperands()->at(0));
  ASSERT_NE(rhsVar, nullptr);
  EXPECT_EQ(rhsVar->getName(), std::string_view{"i"});
  const hldb::Constant *const rhsOne = any_cast<hldb::Constant>(rhs->getOperands()->at(1));
  ASSERT_NE(rhsOne, nullptr);
  EXPECT_EQ(rhsOne->getDecompile(), "1");
}

TEST_F(GenForDecTest, Loop4_Body) {
  const std::vector<const hldb::GenFor *> loops = getLoops();
  ASSERT_EQ(loops.size(), 4u);
  CheckBody(loops[3]);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
