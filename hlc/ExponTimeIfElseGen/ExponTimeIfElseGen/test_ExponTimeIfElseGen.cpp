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

// ============================================================================
// SystemVerilog source under test:
// tests/ExponTimeIfElseGen/dut.sv
// ----------------------------------------------------------------------------
// module Foo ( );
//    parameter ADDR_OFFSET_PART_1 = 1;
//
//     generate
//         for( genvar i = 0 ; i <= 17 ; i++ ) begin
//             if( i == ADDR_OFFSET_PART_0 ) begin
//                 always_ff @(posedge clk) begin
//                     if( shift_foo_bar ) begin
//                         foo_bar_4[i] <= foo_bar_4[i+1];
//                     end
//                 end
//             end
//             else if( i == ADDR_OFFSET_PART_1 ) begin ... end
//             else if( i == ADDR_OFFSET_PART_2 ) begin ... end
//             else if( i == ADDR_OFFSET_PART_3 ) begin ... end
//             else if( i == ADDR_OFFSET_0 ) begin ... end
//             else if( i == ADDR_OFFSET_1 ) begin ... end
//             else if( i == ADDR_OFFSET_2 ) begin ... end
//             else if( i == ADDR_OFFSET_3 ) begin ... end
//             else if( i == ADDR_OFFSET_PART_0_STRIDE ) begin ... end
//             else if( i == ADDR_OFFSET_PART_1_STRIDE ) begin ... end
//         end
//     endgenerate
// endmodule // Foo
// ============================================================================
//
// NOTE on the test name: despite the name "ExponTimeIfElseGen", dut.sv was
// read in full and contains NEITHER an exponent literal (IEEE 1800-2023
// 5.7.1, e.g. "1.0e2") NOR a time literal (5.8, e.g. "10ns") anywhere --
// this is simply what this auto-generated test slot happens to be named.
// The actual constructs present are a loop generate construct (27.4) whose
// body is a 10-way if-else-if chain of conditional generate constructs
// (27.3), so that is what this file exercises.
//
// IEEE 1800-2023 constructs under test:
//   - 27.4 "Loop generate constructs": "for ( genvar_initialization ;
//     genvar_expression ; genvar_iteration ) generate_block". Here the
//     genvar is declared INLINE ("genvar i = 0"), which -- unlike a
//     predeclared "genvar i;" at module scope (see test_DoubleLoop.cpp) --
//     introduces an implicit local scope for "i" owned by the GenFor
//     itself (mirroring 12.7.1--for.sv's ForStmt "int i = 0" inline
//     declaration; see test_12.7.1_for.cpp). The step "i++" is a
//     genvar_iteration using inc_or_dec_operator (A.4.2), producing a
//     standalone Operation(vpiPostIncOp), not an Assignment (same shape as
//     "i++" in test_12.7.4_while.cpp / test_12.7.1_for.cpp).
//   - 27.3 "Conditional generate constructs": "if_generate_construct ::=
//     if ( constant_expression ) generate_block [ else generate_block ]".
//     A chain of "if / else if / else if / ..." with no final bare "else"
//     desugars to nested if-else, i.e. GenIfElse whose getElseStmt() is
//     itself the next GenIfElse, ending with a GenIfElse whose
//     getElseStmt() is null (no trailing "else" in the source).
//   - "parameter ADDR_OFFSET_PART_1 = 1;" (6.20.2): a parameter with no
//     explicit data type defaults to a signed "integer" per 6.20.2's
//     rules for an untyped parameter assignment with an integer literal
//     default -- not itself the focus, so only existence/value is checked.
//
// CHECKED (this file):
//   - module "Foo" exists.
//   - parameter "ADDR_OFFSET_PART_1" exists with default value "1".
//   - the generate region wraps a GenFor whose own scope owns Variable
//     "i" (inline genvar declaration).
//   - GenFor header: init "i = 0", condition "i <= 17" (vpiLeOp), step
//     "i++" (vpiPostIncOp).
//   - GenFor's body is an (unnamed) Begin holding exactly the 10-way
//     GenIfElse chain, one link per "if"/"else if", each comparing "i"
//     (vpiEqOp) against the corresponding named condition operand, in
//     source order; the final link's getElseStmt() is null.
//   - for the first branch (i == ADDR_OFFSET_PART_0), the full nested
//     body shape: Begin -> Always (vpiAlwaysFF) -> IfStmt(shift_foo_bar)
//     -> Begin -> Assignment (non-blocking) of "foo_bar_4[i] <=
//     foo_bar_4[i+1]".
//   - for every other branch, a lighter check that getStmt() resolves to
//     a Begin containing exactly one Always with getAlwaysType() ==
//     vpiAlwaysFF (the 10 branches are textually identical).
//
// NOT CHECKED (out of scope):
//   - whether identifiers such as "ADDR_OFFSET_PART_0", "clk",
//     "shift_foo_bar", and "foo_bar_4" (all otherwise undeclared in this
//     single-module file) successfully BIND to a declaration -- RefObj
//     existence/name is a syntactic fact independent of binding success,
//     and per the test-writing guide, diagnostics/ObjectBinder behavior
//     is not this file's concern.
//   - the elaborated/unrolled 18 iterations of the generate-for (only the
//     single generate_block template is checked, per -d db -d ast).
// ============================================================================

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/gen_for.h>
#include <hldb/gen_if_else.h>
#include <hldb/gen_region.h>
#include <hldb/if_stmt.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <hlc/Tests/Test.h>

#include <array>

namespace hlc {

class ExponTimeIfElseGenTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ExponTimeIfElseGen.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("Foo", m_design->getAllModules()); }

  static const hldb::GenFor *getGenFor() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getGenStmts() == nullptr) return nullptr;
    for (const hldb::Any *const stmt : *top->getGenStmts()) {
      const hldb::GenRegion *const region = any_cast<hldb::GenRegion>(stmt);
      if (region == nullptr) continue;
      const hldb::GenFor *const loop = region->getStmt<hldb::GenFor>();
      if (loop != nullptr) return loop;
    }
    return nullptr;
  }

  static const hldb::GenIfElse *getFirstGenIfElse() {
    const hldb::GenFor *const loop = getGenFor();
    if (loop == nullptr) return nullptr;
    const hldb::Begin *const body = loop->getStmt<hldb::Begin>();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) return nullptr;
    return any_cast<hldb::GenIfElse>(body->getStmts()->at(0));
  }

  // Verifies "i == <name>" as the GenIfElse condition.
  static void CheckConditionIsIEqualsNamedConst(const hldb::GenIfElse *link, std::string_view name) {
    ASSERT_NE(link, nullptr);
    const hldb::Operation *const cond = link->getCondition<hldb::Operation>();
    ASSERT_NE(cond, nullptr) << "'i == " << name << "' should be an Operation";
    EXPECT_EQ(cond->getOpType(), vpiEqOp);
    ASSERT_NE(cond->getOperands(), nullptr);
    ASSERT_EQ(cond->getOperands()->size(), 2u);
    const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
    ASSERT_NE(lhs, nullptr);
    EXPECT_EQ(lhs->getName(), "i");
    const hldb::RefObj *const rhs = any_cast<hldb::RefObj>(cond->getOperands()->at(1));
    ASSERT_NE(rhs, nullptr) << "'" << name << "' should be a RefObj (named condition operand)";
    EXPECT_EQ(rhs->getName(), name);
  }

  // Verifies the branch body resolves to: Begin -> Always(vpiAlwaysFF).
  static const hldb::Always *CheckBranchBodyIsSingleAlwaysFF(const hldb::GenIfElse *link) {
    if (link == nullptr) return nullptr;
    const hldb::Begin *const branchBody = link->getStmt<hldb::Begin>();
    if (branchBody == nullptr || branchBody->getStmts() == nullptr || branchBody->getStmts()->size() != 1u) {
      return nullptr;
    }
    const hldb::Always *const always = any_cast<hldb::Always>(branchBody->getStmts()->at(0));
    return always;
  }
};

// ---------------------------------------------------------------------------
// Module and parameter existence
// ---------------------------------------------------------------------------
TEST_F(ExponTimeIfElseGenTest, ModuleFooExists) { EXPECT_NE(getTop(), nullptr) << "module 'Foo' not found"; }

TEST_F(ExponTimeIfElseGenTest, ParameterAddrOffsetPart1EqualsOne) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParameters(), nullptr);
  const hldb::Parameter *const param = hldb::findByName<hldb::Parameter>("ADDR_OFFSET_PART_1", top->getParameters());
  ASSERT_NE(param, nullptr) << "parameter 'ADDR_OFFSET_PART_1' not found";
  const hldb::Constant *const value = param->getExpr<hldb::Constant>();
  ASSERT_NE(value, nullptr) << "'parameter ADDR_OFFSET_PART_1 = 1' should have a Constant default";
  EXPECT_EQ(value->getDecompile(), "1");
}

// ---------------------------------------------------------------------------
// 27.4: "for( genvar i = 0 ; i <= 17 ; i++ )"
// ---------------------------------------------------------------------------
TEST_F(ExponTimeIfElseGenTest, GenForExists) { EXPECT_NE(getGenFor(), nullptr); }

TEST_F(ExponTimeIfElseGenTest, GenForOwnsInlineGenvarIAsVariable) {
  const hldb::GenFor *const loop = getGenFor();
  ASSERT_NE(loop, nullptr);
  ASSERT_NE(loop->getVariables(), nullptr)
      << "'genvar i' declared inline in the loop header should live in the GenFor's own local scope";
  ASSERT_EQ(loop->getVariables()->size(), 1u);
  EXPECT_NE(hldb::findByName<hldb::Variable>("i", loop->getVariables()), nullptr) << "Variable 'i' not found";
}

TEST_F(ExponTimeIfElseGenTest, GenForInitAssignsZeroToVariableI) {
  const hldb::GenFor *const loop = getGenFor();
  ASSERT_NE(loop, nullptr);
  ASSERT_NE(loop->getForInitStmts(), nullptr);
  ASSERT_EQ(loop->getForInitStmts()->size(), 1u);
  const hldb::Assignment *const init = any_cast<hldb::Assignment>(loop->getForInitStmts()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Variable *const lhs = init->getLhs<hldb::Variable>();
  ASSERT_NE(lhs, nullptr) << "genvar_initialization LHS should be the Variable 'i' directly (its own declaration)";
  EXPECT_EQ(lhs->getName(), "i");
  const hldb::Constant *const rhs = init->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "0");
}

TEST_F(ExponTimeIfElseGenTest, GenForConditionIsILessOrEqual17) {
  const hldb::GenFor *const loop = getGenFor();
  ASSERT_NE(loop, nullptr);
  const hldb::Operation *const cond = loop->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "genvar_expression 'i <= 17' should be an Operation";
  EXPECT_EQ(cond->getOpType(), vpiLeOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "i");
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(cond->getOperands()->at(1));
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "17");
}

TEST_F(ExponTimeIfElseGenTest, GenForStepIsPostIncrementOfI) {
  const hldb::GenFor *const loop = getGenFor();
  ASSERT_NE(loop, nullptr);
  ASSERT_NE(loop->getForIncStmts(), nullptr);
  ASSERT_EQ(loop->getForIncStmts()->size(), 1u);
  const hldb::Operation *const step = any_cast<hldb::Operation>(loop->getForIncStmts()->at(0));
  ASSERT_NE(step, nullptr) << "genvar_iteration 'i++' should be a post-increment Operation";
  EXPECT_EQ(step->getOpType(), vpiPostIncOp);
  ASSERT_NE(step->getOperands(), nullptr);
  ASSERT_EQ(step->getOperands()->size(), 1u);
  const hldb::RefObj *const operand = any_cast<hldb::RefObj>(step->getOperands()->at(0));
  ASSERT_NE(operand, nullptr);
  EXPECT_EQ(operand->getName(), "i");
}

// ---------------------------------------------------------------------------
// 27.3: the 10-way if-else-if chain, one GenIfElse per link, chained via
// getElseStmt(); no trailing bare "else" so the last link's getElseStmt()
// is null.
// ---------------------------------------------------------------------------
TEST_F(ExponTimeIfElseGenTest, GenForBodyIsUnnamedBeginWithSingleIfElseChain) {
  const hldb::GenFor *const loop = getGenFor();
  ASSERT_NE(loop, nullptr);
  const hldb::Begin *const body = loop->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr) << "the for-loop body 'begin ... end' (no label) should be an unnamed Begin";
  EXPECT_TRUE(body->getName().empty());
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 1u) << "the begin/end block contains exactly the if-else-if chain";
  EXPECT_NE(any_cast<hldb::GenIfElse>(body->getStmts()->at(0)), nullptr)
      << "'if (...) ... else if (...) ...' should resolve to a GenIfElse";
}

TEST_F(ExponTimeIfElseGenTest, IfElseChainHasTenLinksInSourceOrder) {
  static constexpr std::array<std::string_view, 10> kExpectedNames = {
      "ADDR_OFFSET_PART_0", "ADDR_OFFSET_PART_1", "ADDR_OFFSET_PART_2", "ADDR_OFFSET_PART_3", "ADDR_OFFSET_0",
      "ADDR_OFFSET_1",      "ADDR_OFFSET_2",      "ADDR_OFFSET_3",      "ADDR_OFFSET_PART_0_STRIDE",
      "ADDR_OFFSET_PART_1_STRIDE",
  };

  const hldb::GenIfElse *link = getFirstGenIfElse();
  ASSERT_NE(link, nullptr);
  for (std::size_t index = 0; index < kExpectedNames.size(); ++index) {
    SCOPED_TRACE(::testing::Message() << "chain link #" << index << " (" << kExpectedNames[index] << ")");
    ASSERT_NE(link, nullptr) << "expected " << kExpectedNames.size() << " chain links, chain ended early";
    CheckConditionIsIEqualsNamedConst(link, kExpectedNames[index]);

    if (index + 1 < kExpectedNames.size()) {
      const hldb::GenIfElse *const next = any_cast<hldb::GenIfElse>(link->getElseStmt());
      ASSERT_NE(next, nullptr) << "'else if (...)' should chain to the next GenIfElse via getElseStmt()";
      link = next;
    } else {
      EXPECT_EQ(link->getElseStmt(), nullptr)
          << "the final 'else if' has no trailing bare 'else', so getElseStmt() should be null";
    }
  }
}

// ---------------------------------------------------------------------------
// First branch (i == ADDR_OFFSET_PART_0): full nested body shape.
// ---------------------------------------------------------------------------
TEST_F(ExponTimeIfElseGenTest, FirstBranchBodyIsAlwaysFFWithGuardedNonBlockingAssign) {
  const hldb::GenIfElse *const first = getFirstGenIfElse();
  ASSERT_NE(first, nullptr);

  const hldb::Always *const always = CheckBranchBodyIsSingleAlwaysFF(first);
  ASSERT_NE(always, nullptr) << "branch body 'begin always_ff ... end' should be a Begin holding one Always";
  EXPECT_EQ(always->getAlwaysType(), vpiAlwaysFF);

  const hldb::Begin *const alwaysBody = any_cast<hldb::Begin>(always->getStmt());
  ASSERT_NE(alwaysBody, nullptr) << "'always_ff @(...) begin ... end' should be a Begin";
  ASSERT_NE(alwaysBody->getStmts(), nullptr);
  ASSERT_EQ(alwaysBody->getStmts()->size(), 1u);

  const hldb::IfStmt *const guard = any_cast<hldb::IfStmt>(alwaysBody->getStmts()->at(0));
  ASSERT_NE(guard, nullptr) << "'if( shift_foo_bar )' should be an IfStmt";
  const hldb::RefObj *const guardCond = any_cast<hldb::RefObj>(guard->getCondition());
  ASSERT_NE(guardCond, nullptr);
  EXPECT_EQ(guardCond->getName(), "shift_foo_bar");

  const hldb::Begin *const guardBody = any_cast<hldb::Begin>(guard->getStmt());
  ASSERT_NE(guardBody, nullptr) << "'if(...) begin ... end' should be a Begin";
  ASSERT_NE(guardBody->getStmts(), nullptr);
  ASSERT_EQ(guardBody->getStmts()->size(), 1u);

  const hldb::Assignment *const nba = any_cast<hldb::Assignment>(guardBody->getStmts()->at(0));
  ASSERT_NE(nba, nullptr) << "'foo_bar_4[i] <= foo_bar_4[i+1];' should be an Assignment";
  EXPECT_FALSE(nba->getBlocking()) << "'<=' inside a procedural block is a non-blocking assignment (10.4.2)";

  const hldb::BitSelect *const lhs = any_cast<hldb::BitSelect>(nba->getLhs());
  ASSERT_NE(lhs, nullptr) << "'foo_bar_4[i]' should be a BitSelect";
  EXPECT_EQ(lhs->getName(), "foo_bar_4");
  const hldb::RefObj *const lhsIndex = any_cast<hldb::RefObj>(lhs->getIndex());
  ASSERT_NE(lhsIndex, nullptr);
  EXPECT_EQ(lhsIndex->getName(), "i");

  const hldb::BitSelect *const rhs = any_cast<hldb::BitSelect>(nba->getRhs());
  ASSERT_NE(rhs, nullptr) << "'foo_bar_4[i+1]' should be a BitSelect";
  EXPECT_EQ(rhs->getName(), "foo_bar_4");
  const hldb::Operation *const rhsIndex = any_cast<hldb::Operation>(rhs->getIndex());
  ASSERT_NE(rhsIndex, nullptr) << "'i+1' should be an Operation";
  EXPECT_EQ(rhsIndex->getOpType(), vpiAddOp);
}

// ---------------------------------------------------------------------------
// Remaining 9 branches: lighter structural check (the branches are
// textually identical, so only the AlwaysFF shape is re-verified per link).
// ---------------------------------------------------------------------------
TEST_F(ExponTimeIfElseGenTest, RemainingNineBranchesAreEachSingleAlwaysFF) {
  const hldb::GenIfElse *link = getFirstGenIfElse();
  ASSERT_NE(link, nullptr);
  for (int index = 0; index < 10; ++index) {
    ASSERT_NE(link, nullptr) << "chain ended early at link #" << index;
    const hldb::Always *const always = CheckBranchBodyIsSingleAlwaysFF(link);
    EXPECT_NE(always, nullptr) << "link #" << index << ": branch body should be a single Always";
    if (always != nullptr) {
      EXPECT_EQ(always->getAlwaysType(), vpiAlwaysFF) << "link #" << index;
    }
    link = any_cast<hldb::GenIfElse>(link->getElseStmt());
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
