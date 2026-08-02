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

// Tests for 11.4.12--concat_op.sv (tags: 11.4.12)
//   bit [15:0] a;
//   bit [7:0] b = 8'b10101100;
//   bit [7:0] c = 8'b01010011;
//   initial begin
//     a = {b, c};
//   end
//
// IEEE 1800-2017 11.4.12 defines "{expr, expr, ...}" concatenation. This
// file's corner is twofold: (1) that two 8-bit operands concatenate into
// a single Operation (vpiConcatOp) rather than being pre-combined or
// treated as separate assignments, and (2) a subtlety in how the
// compiler represents the *declaration-time literal* used to initialize
// b/c: even though b and c are themselves typed "bit" (BitTypespec), the
// binary literal "8'b10101100" that initializes them decompiles through
// a LogicTypespec-typed Constant, not a BitTypespec-typed one -- worth
// pinning down explicitly since it is not the same typespec kind as the
// variable it initializes.
//
// Checked:
//   - module getTypespecs() has exactly 3 entries: BitTypespec [15:0]
//     (for "a"), BitTypespec [7:0] (for "b"), BitTypespec [7:0] (for
//     "c") -- three distinct entries since each variable was declared on
//     its own line, even though b and c share the same range
//   - module top has exactly 3 variables (bare "bit" has no net-type
//     keyword, so these are hldb::Variable, not hldb::Net -- IEEE 1800-2023
//     Sec 6.7/6.8), "a", "b", "c", each resolving to its own BitTypespec
//     above
//   - variable "b"'s and variable "c"'s declaration-time getValue<Constant>()
//     are present, with the exact binary decompile text ("8'b10101100",
//     "8'b01010011") and constType binary(3), and -- the corner noted
//     above -- each Constant's own typespec resolves to LogicTypespec,
//     not BitTypespec
//   - the initial block is a Begin with exactly 1 statement: a blocking
//     Assignment, lhs RefObj "a", rhs an Operation (vpiConcatOp,
//     2 operands: RefObj "b", RefObj "c")
//   - design-level typespecs (3): ModuleTypespec, IntTypespec (signed),
//     LogicTypespec
//   - compiler emits zero errors
//
// Not checked:
//   - this file has no $display assertion at all (unlike its "-sim"
//     sibling, 11.4.12--concat_op-sim.sv), so there is no runtime
//     numeric outcome authored into the source to check even in
//     principle.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ConcatOpTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.4.12--concat_op.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module-level typespecs / nets -----------------------------------------

TEST_F(ConcatOpTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ConcatOpTest, ModuleHasThreeDistinctBitTypespecs) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  ASSERT_EQ(top->getTypespecs()->size(), 3u);
  uint32_t bitTypespecCount = 0;
  for (const hldb::Typespec *const ts : *top->getTypespecs()) {
    if (any_cast<hldb::BitTypespec>(ts) != nullptr) ++bitTypespecCount;
  }
  EXPECT_EQ(bitTypespecCount, 3u);
}

TEST_F(ConcatOpTest, VariableAIsSixteenBitBitTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::BitTypespec *const bt = a->getTypespec<hldb::RefTypespec>()->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr);
  ASSERT_NE(bt->getRanges(), nullptr);
  ASSERT_EQ(bt->getRanges()->size(), 1u);
  EXPECT_EQ(bt->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "15");
  EXPECT_EQ(bt->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
  // "a" is declared bare ("bit [15:0] a;"), with no decl-assignment --
  // unlike b/c below, its entire value must come from the "a = {b, c};"
  // assignment in the initial block.
  EXPECT_EQ(a->getValue<hldb::Constant>(), nullptr) << "'a' is declared without an initializer";
}

TEST_F(ConcatOpTest, VariablesBAndCAreEightBitWithBinaryInitializers) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const char *const names[2] = {"b", "c"};
  const char *const decompiles[2] = {"8'b10101100", "8'b01010011"};
  const char *const values[2] = {"10101100", "01010011"};
  for (uint32_t i = 0; i < 2u; ++i) {
    const hldb::Variable *const variable = hldb::findByName<hldb::Variable>(names[i], top->getVariables());
    ASSERT_NE(variable, nullptr) << "variable " << names[i];
    EXPECT_NE(variable->getTypespec<hldb::RefTypespec>()->getActual<hldb::BitTypespec>(), nullptr);
    const hldb::Constant *const init = variable->getValue<hldb::Constant>();
    ASSERT_NE(init, nullptr);
    EXPECT_EQ(init->getConstType(), vpiBinaryConst);
    EXPECT_EQ(init->getDecompile(), decompiles[i]);
    EXPECT_EQ(init->getValue(), values[i]);
    // Corner: the initializer Constant's own typespec is LogicTypespec,
    // not BitTypespec, even though the variable it initializes is "bit".
    EXPECT_NE(init->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>(), nullptr)
        << names[i] << "'s initializer Constant should resolve to LogicTypespec";
  }
}

// --- the point of the file: two operands concatenate into one Operation --

TEST_F(ConcatOpTest, InitialBlockHasOneStatement) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 1u);
}

TEST_F(ConcatOpTest, AssignmentRhsIsConcatenationOfBAndC) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);

  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "a");

  const hldb::Operation *const concat = assign->getRhs<hldb::Operation>();
  ASSERT_NE(concat, nullptr);
  EXPECT_EQ(concat->getOpType(), vpiConcatOp);
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::RefObj>(concat->getOperands()->at(0))->getName(), "b");
  EXPECT_EQ(any_cast<hldb::RefObj>(concat->getOperands()->at(1))->getName(), "c");
}

// --- design-level typespecs / compiler diagnostics -------------------------

TEST_F(ConcatOpTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  ASSERT_EQ(m_design->getTypespecs()->size(), 3u);
  uint32_t moduleTypespecCount = 0;
  uint32_t intTypespecCount = 0;
  uint32_t logicTypespecCount = 0;
  for (const hldb::Typespec *const ts : *m_design->getTypespecs()) {
    if (any_cast<hldb::ModuleTypespec>(ts) != nullptr) ++moduleTypespecCount;
    if (any_cast<hldb::IntTypespec>(ts) != nullptr) ++intTypespecCount;
    if (any_cast<hldb::LogicTypespec>(ts) != nullptr) ++logicTypespecCount;
  }
  EXPECT_EQ(moduleTypespecCount, 1u);
  EXPECT_EQ(intTypespecCount, 1u);
  EXPECT_EQ(logicTypespecCount, 1u);
}

TEST_F(ConcatOpTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
