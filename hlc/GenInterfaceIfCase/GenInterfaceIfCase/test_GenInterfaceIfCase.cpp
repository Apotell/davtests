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

// Tests for tests/GenInterfaceIfCase/dut.sv:
//
//   interface gen_if_case #(parameter int MODE = 1, parameter int N = 2) (input logic clk);
//     logic [7:0] data;
//     logic       valid;
//     logic       ready;
//
//     generate
//       if (MODE == 1)
//         assign ready = valid;
//       else
//         assign ready = 1'b0;
//
//       if (MODE == 2) g_m2: begin
//         logic x;
//         assign x = valid;
//       end
//
//       if (N == 1)
//         assign data = 8'h01;
//       else if (N == 2)
//         assign data = 8'h02;
//       else
//         assign data = 8'h00;
//
//       case (N)
//         1: assign valid = 1'b0;
//         2: g_two: begin
//           logic [1:0] sel;
//           assign sel = 2'b01;
//         end
//         default: assign valid = 1'b1;
//       endcase
//     endgenerate
//   endinterface
//
// The if and case constructs here are parsed through the interface-specific
// generate rules (generate_interface_conditional_statement,
// generate_interface_case_statement, genvar_interface_case_item,
// generate_interface_block).
//
// What to check and why (IEEE 1800-2023):
//   - Sec 27.3: "generate ... endgenerate" is a generate region; it is a
//     GenRegion holding the four constructs, in source order, as vpiStmt.
//   - Sec 27.5: "if ( constant_expression ) generate_block [ else
//     generate_block ]" -- with an else branch the construct is a GenIfElse,
//     without one a GenIf. A case generate is a GenCase whose items are
//     CaseItems (vpiExpr = the case item expressions, none for default).
//   - Sec 27.5: a generate block may be a single item without begin-end, but
//     it is still a generate block, so each such branch/case item is an
//     unnamed GenRegion holding that item. A labeled "name: begin ... end"
//     block is a GenRegion carrying the name, and the declarations inside it
//     belong to that block, not to the interface.
//   - Sec 37.85, detail 2: "For an unnamed generate, an implicit scope shall
//     be created. Its vpiImplicitDecl property shall return TRUE." -- the
//     implicit blocks report getImplicitDecl() true; the written begin...end
//     blocks report false.
//   - Sec 27.5: "If a generate block in a conditional generate construct
//     consists of only one item that is itself a conditional generate
//     construct and if that item is not surrounded by begin-end keywords,
//     then this generate block is not treated as a separate scope" -- the
//     "else if" is directly nested: the else branch is the inner GenIfElse
//     itself, with no GenRegion between them.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/case_item.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/gen_case.h>
#include <hldb/gen_if.h>
#include <hldb/gen_if_else.h>
#include <hldb/gen_region.h>
#include <hldb/interface.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class GenInterfaceIfCaseTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "GenInterfaceIfCase.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Looked up by definition name: the parameterized interface's own name carries its parameter list.
  static const hldb::Interface *getInterface() {
    if (m_design->getAllInterfaces() == nullptr) return nullptr;
    for (const hldb::Interface *const iface : *m_design->getAllInterfaces()) {
      if (iface->getDefName() == "gen_if_case") return iface;
    }
    return nullptr;
  }

  // interface -> getGenStmts() -> the "generate ... endgenerate" GenRegion
  static const hldb::GenRegion *getRegion() {
    const hldb::Interface *const iface = getInterface();
    if ((iface == nullptr) || (iface->getGenStmts() == nullptr) || (iface->getGenStmts()->size() != 1u)) {
      return nullptr;
    }
    return any_cast<hldb::GenRegion>(iface->getGenStmts()->at(0));
  }

  static const hldb::Any *getRegionStmt(uint32_t index) {
    const hldb::GenRegion *const region = getRegion();
    if ((region == nullptr) || (region->getStmts() == nullptr) || (region->getStmts()->size() <= index)) {
      return nullptr;
    }
    return region->getStmts()->at(index);
  }

  // An unnamed, implicit generate block holding exactly one ContAssign whose lhs is `lhsName`.
  static void expectImplicitBlockWithAssign(const hldb::Any *stmt, std::string_view lhsName) {
    const hldb::GenRegion *const block = any_cast<hldb::GenRegion>(stmt);
    ASSERT_NE(stmt, nullptr);
    ASSERT_NE(block, nullptr) << "a branch without begin-end is still a generate block (Sec 27.5)";
    EXPECT_TRUE(block->getName().empty());
    EXPECT_TRUE(block->getImplicitDecl()) << "an implicit generate scope has vpiImplicitDecl TRUE (Sec 37.85)";
    ASSERT_NE(block->getStmts(), nullptr);
    ASSERT_EQ(block->getStmts()->size(), 1u);
    const hldb::ContAssign *const assign = any_cast<hldb::ContAssign>(block->getStmts()->at(0));
    ASSERT_NE(assign, nullptr);
    const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getName(), lhsName);
  }
};

TEST_F(GenInterfaceIfCaseTest, InterfaceExists) { ASSERT_NE(getInterface(), nullptr); }

TEST_F(GenInterfaceIfCaseTest, GenerateRegionHoldsFourConstructsInOrder) {
  const hldb::GenRegion *const region = getRegion();
  ASSERT_NE(region, nullptr) << "'generate ... endgenerate' should be the interface's only GenRegion";
  ASSERT_NE(region->getStmts(), nullptr);
  ASSERT_EQ(region->getStmts()->size(), 4u);
  EXPECT_NE(any_cast<hldb::GenIfElse>(region->getStmts()->at(0)), nullptr);
  EXPECT_NE(any_cast<hldb::GenIf>(region->getStmts()->at(1)), nullptr);
  EXPECT_NE(any_cast<hldb::GenIfElse>(region->getStmts()->at(2)), nullptr);
  EXPECT_NE(any_cast<hldb::GenCase>(region->getStmts()->at(3)), nullptr);
}

// if (MODE == 1) assign ready = valid; else assign ready = 1'b0;
TEST_F(GenInterfaceIfCaseTest, IfElseBranchesAreImplicitBlocks) {
  const hldb::GenIfElse *const ifElse = any_cast<hldb::GenIfElse>(getRegionStmt(0));
  ASSERT_NE(ifElse, nullptr);

  const hldb::Operation *const cond = ifElse->getCondition<hldb::Operation>();
  ASSERT_NE(ifElse->getCondition(), nullptr);
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiEqOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "MODE");

  expectImplicitBlockWithAssign(ifElse->getStmt(), "ready");
  expectImplicitBlockWithAssign(ifElse->getElseStmt(), "ready");

  const hldb::GenRegion *const thenBlock = ifElse->getStmt<hldb::GenRegion>();
  ASSERT_NE(thenBlock, nullptr);
  const hldb::ContAssign *const assign = any_cast<hldb::ContAssign>(thenBlock->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const rhs = assign->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "valid");
}

// if (MODE == 2) g_m2: begin logic x; assign x = valid; end
TEST_F(GenInterfaceIfCaseTest, IfWithLabeledBlockOwnsItsDeclaration) {
  const hldb::GenIf *const genIf = any_cast<hldb::GenIf>(getRegionStmt(1));
  ASSERT_NE(genIf, nullptr) << "no else branch, so a GenIf";
  ASSERT_NE(genIf->getCondition(), nullptr);

  const hldb::GenRegion *const block = genIf->getStmt<hldb::GenRegion>();
  ASSERT_NE(genIf->getStmt(), nullptr);
  ASSERT_NE(block, nullptr) << "'g_m2: begin ... end' should be a GenRegion";
  EXPECT_EQ(block->getName(), "g_m2");
  EXPECT_FALSE(block->getImplicitDecl()) << "a written begin...end block is not implicit";

  ASSERT_NE(block->getVariables(), nullptr);
  ASSERT_EQ(block->getVariables()->size(), 1u);
  const hldb::Variable *const x = block->getVariables()->at(0);
  ASSERT_NE(x, nullptr);
  EXPECT_EQ(x->getName(), "x");

  ASSERT_NE(block->getStmts(), nullptr);
  ASSERT_EQ(block->getStmts()->size(), 1u);
  const hldb::ContAssign *const assign = any_cast<hldb::ContAssign>(block->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "x");
}

// if (N == 1) ... else if (N == 2) ... else ...
TEST_F(GenInterfaceIfCaseTest, ElseIfIsDirectlyNested) {
  const hldb::GenIfElse *const outer = any_cast<hldb::GenIfElse>(getRegionStmt(2));
  ASSERT_NE(outer, nullptr);
  expectImplicitBlockWithAssign(outer->getStmt(), "data");

  const hldb::GenIfElse *const inner = outer->getElseStmt<hldb::GenIfElse>();
  ASSERT_NE(outer->getElseStmt(), nullptr);
  ASSERT_NE(inner, nullptr) << "a lone 'if' in the else branch is directly nested: no GenRegion in between (Sec 27.5)";
  ASSERT_NE(inner->getCondition(), nullptr);
  expectImplicitBlockWithAssign(inner->getStmt(), "data");
  expectImplicitBlockWithAssign(inner->getElseStmt(), "data");
}

// case (N) 1: ...; 2: g_two: begin ... end default: ...; endcase
TEST_F(GenInterfaceIfCaseTest, CaseGenerateItems) {
  const hldb::GenCase *const genCase = any_cast<hldb::GenCase>(getRegionStmt(3));
  ASSERT_NE(genCase, nullptr);

  const hldb::RefObj *const cond = genCase->getCondition<hldb::RefObj>();
  ASSERT_NE(genCase->getCondition(), nullptr);
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getName(), "N");

  ASSERT_NE(genCase->getCaseItems(), nullptr);
  ASSERT_EQ(genCase->getCaseItems()->size(), 3u);

  const hldb::CaseItem *const item1 = genCase->getCaseItems()->at(0);
  ASSERT_NE(item1, nullptr);
  ASSERT_NE(item1->getExprs(), nullptr);
  ASSERT_EQ(item1->getExprs()->size(), 1u);
  const hldb::Constant *const expr1 = any_cast<hldb::Constant>(item1->getExprs()->at(0));
  ASSERT_NE(expr1, nullptr);
  EXPECT_EQ(expr1->getDecompile(), "1");
  expectImplicitBlockWithAssign(item1->getStmt(), "valid");

  const hldb::CaseItem *const item2 = genCase->getCaseItems()->at(1);
  ASSERT_NE(item2, nullptr);
  ASSERT_NE(item2->getExprs(), nullptr);
  ASSERT_EQ(item2->getExprs()->size(), 1u);
  const hldb::GenRegion *const gTwo = item2->getStmt<hldb::GenRegion>();
  ASSERT_NE(item2->getStmt(), nullptr);
  ASSERT_NE(gTwo, nullptr) << "'g_two: begin ... end' should be a GenRegion";
  EXPECT_EQ(gTwo->getName(), "g_two");
  EXPECT_FALSE(gTwo->getImplicitDecl()) << "a written begin...end block is not implicit";
  ASSERT_NE(gTwo->getVariables(), nullptr);
  ASSERT_EQ(gTwo->getVariables()->size(), 1u);
  EXPECT_EQ(gTwo->getVariables()->at(0)->getName(), "sel");
  ASSERT_NE(gTwo->getStmts(), nullptr);
  ASSERT_EQ(gTwo->getStmts()->size(), 1u);
  EXPECT_NE(any_cast<hldb::ContAssign>(gTwo->getStmts()->at(0)), nullptr);

  const hldb::CaseItem *const itemDefault = genCase->getCaseItems()->at(2);
  ASSERT_NE(itemDefault, nullptr);
  EXPECT_TRUE((itemDefault->getExprs() == nullptr) || itemDefault->getExprs()->empty())
      << "the default item has no case item expressions";
  expectImplicitBlockWithAssign(itemDefault->getStmt(), "valid");
}

TEST_F(GenInterfaceIfCaseTest, BlockDeclarationsAreNotInterfaceDeclarations) {
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getVariables(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("data", iface->getVariables()), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("x", iface->getVariables()), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("sel", iface->getVariables()), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
