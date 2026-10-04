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

// Tests for tests/GenFor/dut.sv -- a single generate-for loop, used
// directly as a module item with no surrounding "generate ... endgenerate"
// region:
//
//   module top();
//      for (i = 0; i < 3 ; i = i + 1) begin
//         assign tmp[i] = 1'b1;
//      end
//   endmodule
//
// -- rules under test ---------------------------------------------------
//
// IEEE 1800-2023 Sec 27.3: "generate_region" is only a syntactic wrapper;
// a loop_generate_construct may appear directly as a module item without
// "generate"/"endgenerate", so top->getGenStmts() must yield the GenFor
// directly (no GenRegion wrapper), matching the un-wrapped for-loop shape
// already established for GenFor in this suite's
// DoubleLoop/DoubleLoop/test_DoubleLoop.cpp (there, wrapped in
// "generate...endgenerate", producing a GenRegion->GenFor; here, the same
// GenFor shape without the GenRegion layer).
//
// Sec 27.4 "Loop generate constructs":
// "loop_generate_construct ::= for ( genvar_initialization ; genvar_
// expression ; genvar_iteration ) generate_block" with
// "genvar_initialization ::= [ genvar ] genvar_identifier = constant_
// expression". This file's header "for (i = 0; i < 3; i = i + 1)" omits
// the inline "genvar" keyword, and the source declares no "genvar i;"
// anywhere either -- so 'i' is never declared as a genvar at all. Per Sec
// 27.4 this is required ("It shall be an error if the genvar_identifier ...
// has not been previously declared"); this file intentionally does not
// assert on whether/how that is diagnosed (a distinct, error-catalog-only
// concern), and instead only checks the parse-time structural shape HLC
// builds for the loop_generate_construct itself -- init/condition/
// increment/body -- mirroring the header-shape checks already established
// for GenFor in DoubleLoop's CheckLoopHeader().
//
// GenFor.hlc uses "-d db -d ast" (no "-d inst"), so no elaboration/
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

namespace hlc {

class GenForTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "GenFor.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  // top->getGenStmts() -> GenFor directly (no GenRegion: no
  // "generate"/"endgenerate" wraps this loop in the source).
  static const hldb::GenFor *getLoop() {
    const hldb::Module *const m = getModule("top");
    if (m == nullptr || m->getGenStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *m->getGenStmts()) {
      const hldb::GenFor *const loop = any_cast<hldb::GenFor>(stmt);
      if (loop != nullptr) return loop;
    }
    return nullptr;
  }
};

TEST_F(GenForTest, ModuleTopExists) { ASSERT_NE(getModule("top"), nullptr) << "module 'top' not found"; }

TEST_F(GenForTest, LoopIsDirectGenForWithNoSurroundingGenRegion) {
  const hldb::Module *const m = getModule("top");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getGenStmts(), nullptr) << "'for (i = 0; i < 3; i = i+1) begin ... end' not found";
  ASSERT_EQ(m->getGenStmts()->size(), 1u) << "exactly one generate item at module scope";
  const hldb::GenFor *const loop = any_cast<hldb::GenFor>(m->getGenStmts()->at(0));
  ASSERT_NE(loop, nullptr) << "Sec 27.3: an un-wrapped loop_generate_construct must appear directly as a GenFor, "
                               "not behind a GenRegion";
}

// genvar_initialization: 'i = 0'
TEST_F(GenForTest, LoopHeader_Init) {
  const hldb::GenFor *const loop = getLoop();
  ASSERT_NE(loop, nullptr);
  ASSERT_NE(loop->getForInitStmts(), nullptr);
  ASSERT_EQ(loop->getForInitStmts()->size(), 1u);
  const hldb::Assignment *const init = any_cast<hldb::Assignment>(loop->getForInitStmts()->at(0));
  ASSERT_NE(init, nullptr) << "genvar_initialization 'i = 0' should be an Assignment";
  const hldb::RefObj *const lhs = init->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "'i = 0': init LHS must be a RefObj";
  EXPECT_EQ(lhs->getName(), std::string_view{"i"});
  const hldb::Constant *const rhs = init->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "0");
}

// genvar_expression: 'i < 3'
TEST_F(GenForTest, LoopHeader_Condition) {
  const hldb::GenFor *const loop = getLoop();
  ASSERT_NE(loop, nullptr);
  const hldb::Operation *const cond = loop->getCondition<hldb::Operation>();
  ASSERT_NE(loop->getCondition(), nullptr);
  ASSERT_NE(cond, nullptr) << "'i < 3' must be an Operation";
  EXPECT_EQ(cond->getOpType(), vpiLtOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const condLhs = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  ASSERT_NE(condLhs, nullptr);
  EXPECT_EQ(condLhs->getName(), std::string_view{"i"});
  const hldb::Constant *const condRhs = any_cast<hldb::Constant>(cond->getOperands()->at(1));
  ASSERT_NE(condRhs, nullptr);
  EXPECT_EQ(condRhs->getDecompile(), "3");
}

// genvar_iteration: 'i = i + 1'
TEST_F(GenForTest, LoopHeader_Increment) {
  const hldb::GenFor *const loop = getLoop();
  ASSERT_NE(loop, nullptr);
  ASSERT_NE(loop->getForIncStmts(), nullptr);
  ASSERT_EQ(loop->getForIncStmts()->size(), 1u);
  const hldb::Assignment *const inc = any_cast<hldb::Assignment>(loop->getForIncStmts()->at(0));
  ASSERT_NE(inc, nullptr) << "genvar_iteration 'i = i + 1' should be an Assignment";
  EXPECT_TRUE(inc->getBlocking());
  const hldb::RefObj *const lhs = inc->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view{"i"});
  const hldb::Operation *const rhs = inc->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getOpType(), vpiAddOp);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 2u);
  const hldb::RefObj *const incVar = any_cast<hldb::RefObj>(rhs->getOperands()->at(0));
  ASSERT_NE(incVar, nullptr);
  EXPECT_EQ(incVar->getName(), std::string_view{"i"});
  const hldb::Constant *const incOne = any_cast<hldb::Constant>(rhs->getOperands()->at(1));
  ASSERT_NE(incOne, nullptr);
  EXPECT_EQ(incOne->getDecompile(), "1");
}

// generate_block body: 'begin assign tmp[i] = 1'b1; end' (unnamed).
TEST_F(GenForTest, LoopBody_IsUnnamedBeginWithSingleContAssign) {
  const hldb::GenFor *const loop = getLoop();
  ASSERT_NE(loop, nullptr);
  const hldb::Begin *const body = loop->getStmt<hldb::Begin>();
  ASSERT_NE(loop->getStmt(), nullptr);
  ASSERT_NE(body, nullptr) << "loop body 'begin assign tmp[i] = 1'b1; end' must be a Begin";
  EXPECT_TRUE(body->getName().empty()) << "the loop body's 'begin ... end' has no label";
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 1u);
  const hldb::ContAssign *const assign = any_cast<hldb::ContAssign>(body->getStmts()->at(0));
  ASSERT_NE(assign, nullptr) << "'assign tmp[i] = 1'b1;' not found inside the loop body";
  ASSERT_NE(assign->getLhs(), nullptr) << "'tmp[i]' LHS must be present";
  const hldb::Constant *const rhs = assign->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "'assign tmp[i] = 1'b1;': RHS must be a Constant";
  EXPECT_EQ(rhs->getDecompile(), "1");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
