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

// Tests for tests/GenIfElse/dut.sv, module 'top':
//
//   module top;
//      if (1) begin
//       assign a = b;
//      end
//      else begin
//       assign c = d;
//       assign e = f;
//      end
//   endmodule
//
// -- rules under test ---------------------------------------------------
//
// IEEE 1800-2023 Sec 27.3: "if_generate_construct ::= if ( constant_
// expression ) generate_block_or_null [ else generate_block_or_null ]" may
// appear directly as a module item with no surrounding "generate"/
// "endgenerate" region, so top->getGenStmts() must yield the GenIfElse
// directly (no GenRegion wrapper) -- unlike GenIf/GenIf/test_GenIf.cpp's
// construct, which sits inside a generate-for loop.
//
// The condition '1' is a bare constant_expression (Sec 27.5 requires it be
// evaluable at elaboration time; a plain literal trivially is), so
// getCondition() must be a Constant decompiling to "1" -- not wrapped in
// any Operation.
//
// Both branches use explicit (unnamed) begin/end blocks (Sec 27.3's
// generate_block "[ generate_block_identifier : ] begin ... end"), so
// getStmt()/getElseStmt() must each be a Begin, holding one ContAssign
// ('a = b') in the then-branch and two ContAssigns ('c = d', 'e = f') in
// the else-branch -- contrast with GenIf/GenIf/test_GenIf.cpp, whose
// single-statement branches have no begin/end and so attach their
// ContAssign directly.
//
// GenIfElse.hlc uses "-d db -d ast" (no "-d inst"), so no elaboration/
// unrolling is checked here.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/cont_assign.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_if_else.h>
#include <hldb/module.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

namespace hlc {

class GenIfElseTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "GenIfElse.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  // top->getGenStmts() -> GenIfElse directly (no surrounding GenRegion).
  static const hldb::GenIfElse *getIfElse() {
    const hldb::Module *const m = getModule("top");
    if (m == nullptr || m->getGenStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *m->getGenStmts()) {
      const hldb::GenIfElse *const ifElse = any_cast<hldb::GenIfElse>(stmt);
      if (ifElse != nullptr) return ifElse;
    }
    return nullptr;
  }

  static const hldb::ContAssign *findContAssign(const hldb::Begin *blk, std::string_view lhsName) {
    if (blk == nullptr || blk->getStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *blk->getStmts()) {
      const hldb::ContAssign *const ca = any_cast<hldb::ContAssign>(stmt);
      if (ca != nullptr && ca->getLhs() != nullptr && ca->getLhs()->getName() == lhsName) return ca;
    }
    return nullptr;
  }
};

TEST_F(GenIfElseTest, ModuleTopExists) { ASSERT_NE(getModule("top"), nullptr) << "module 'top' not found"; }

TEST_F(GenIfElseTest, GenIfElseIsDirectModuleItemWithNoSurroundingGenRegion) {
  const hldb::Module *const m = getModule("top");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getGenStmts(), nullptr) << "'if (1) begin ... end else begin ... end' not found";
  ASSERT_EQ(m->getGenStmts()->size(), 1u) << "exactly one generate item at module scope";
  const hldb::GenIfElse *const ifElse = any_cast<hldb::GenIfElse>(m->getGenStmts()->at(0));
  ASSERT_NE(ifElse, nullptr) << "Sec 27.3: an un-wrapped if_generate_construct must appear directly as a "
                                 "GenIfElse, not behind a GenRegion";
}

// 'if (1)' -- bare constant_expression condition, not wrapped in an
// Operation.
TEST_F(GenIfElseTest, ConditionIsBareConstantOne) {
  const hldb::GenIfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  const hldb::Constant *const cond = ifElse->getCondition<hldb::Constant>();
  ASSERT_NE(ifElse->getCondition(), nullptr);
  ASSERT_NE(cond, nullptr) << "'if (1)': bare literal condition must be a Constant";
  EXPECT_EQ(cond->getDecompile(), "1");
}

// then-branch: 'begin assign a = b; end' (unnamed, one statement).
TEST_F(GenIfElseTest, ThenBranchIsUnnamedBeginWithAssignAFromB) {
  const hldb::GenIfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  const hldb::Begin *const thenBlk = ifElse->getStmt<hldb::Begin>();
  ASSERT_NE(ifElse->getStmt(), nullptr);
  ASSERT_NE(thenBlk, nullptr) << "then-branch 'begin assign a = b; end' must be a Begin";
  EXPECT_TRUE(thenBlk->getName().empty()) << "then-branch 'begin ... end' has no label";
  ASSERT_NE(thenBlk->getStmts(), nullptr);
  ASSERT_EQ(thenBlk->getStmts()->size(), 1u);
  const hldb::ContAssign *const ab = findContAssign(thenBlk, "a");
  ASSERT_NE(ab, nullptr) << "'assign a = b;' not found in then-branch";
  const hldb::RefObj *const rhs = ab->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), std::string_view{"b"});
}

// else-branch: 'begin assign c = d; assign e = f; end' (unnamed, two
// statements).
TEST_F(GenIfElseTest, ElseBranchIsUnnamedBeginWithTwoAssigns) {
  const hldb::GenIfElse *const ifElse = getIfElse();
  ASSERT_NE(ifElse, nullptr);
  const hldb::Begin *const elseBlk = ifElse->getElseStmt<hldb::Begin>();
  ASSERT_NE(ifElse->getElseStmt(), nullptr);
  ASSERT_NE(elseBlk, nullptr) << "else-branch 'begin assign c = d; assign e = f; end' must be a Begin";
  EXPECT_TRUE(elseBlk->getName().empty()) << "else-branch 'begin ... end' has no label";
  ASSERT_NE(elseBlk->getStmts(), nullptr);
  ASSERT_EQ(elseBlk->getStmts()->size(), 2u);

  const hldb::ContAssign *const cd = findContAssign(elseBlk, "c");
  ASSERT_NE(cd, nullptr) << "'assign c = d;' not found in else-branch";
  const hldb::RefObj *const cdRhs = cd->getRhs<hldb::RefObj>();
  ASSERT_NE(cdRhs, nullptr);
  EXPECT_EQ(cdRhs->getName(), std::string_view{"d"});

  const hldb::ContAssign *const ef = findContAssign(elseBlk, "e");
  ASSERT_NE(ef, nullptr) << "'assign e = f;' not found in else-branch";
  const hldb::RefObj *const efRhs = ef->getRhs<hldb::RefObj>();
  ASSERT_NE(efRhs, nullptr);
  EXPECT_EQ(efRhs->getName(), std::string_view{"f"});
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
