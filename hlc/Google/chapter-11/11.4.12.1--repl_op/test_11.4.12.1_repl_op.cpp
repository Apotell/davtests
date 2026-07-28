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

// Tests for 11.4.12.1--repl_op.sv (tags: 11.4.12.1)
//   bit [15:0] a;
//   bit [1:0] b = 2'b10;
//   initial begin
//     a = {8{b}};
//   end
//
// IEEE 1800-2017 11.4.12.1 defines the replication operator
// "{count{expr}}" as distinct from plain concatenation "{expr, expr}",
// even though a replication's single repeated operand still needs to be
// wrapped in its own concatenation per the grammar (a
// Multiple_concatenation always contains exactly one Concatenation). The
// corner this file exercises is that shape: the compiler must produce an
// outer Operation (vpiMultiConcatOp) whose first operand is the repeat
// count (a plain Constant, not folded away) and whose second operand is
// an *inner* Operation (vpiConcatOp) wrapping the single replicated
// value -- not a flattened "8 copies of b" list, and not the count
// silently dropped.
//
// Checked:
//   - module getTypespecs() has exactly 2 entries: BitTypespec [15:0]
//     (for "a"), BitTypespec [1:0] (for "b")
//   - net "b" has a declaration-time getValue<Constant>() of "2'b10"
//     (constType binary(3), value "10"), whose own typespec resolves to
//     LogicTypespec (matching the analogous finding in
//     11.4.12--concat_op.sv: a binary literal's typespec is
//     LogicTypespec even when it initializes a "bit" variable)
//   - the initial block is a Begin with exactly 1 statement: a blocking
//     Assignment, lhs RefObj "a", rhs an Operation (vpiMultiConcatOp,
//     2 operands):
//       operand 0: Constant "8" (the replication count, preserved as a
//       real operand, not folded/discarded)
//       operand 1: Operation (vpiConcatOp, 1 operand: RefObj "b") -- the
//       replicated value, itself wrapped in its own concatenation node
//   - design-level typespecs (3): ModuleTypespec, IntTypespec (signed),
//     LogicTypespec
//   - compiler emits zero errors
//
// Not checked:
//   - this file has no $display assertion at all (unlike its "-sim"
//     sibling, 11.4.12.1--repl_op-sim.sv), so there is no runtime
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
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ReplOpTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.4.12.1--repl_op.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()); }
};

// --- module-level typespecs / nets -----------------------------------------

TEST_F(ReplOpTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ReplOpTest, ModuleHasTwoDistinctBitTypespecs) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  ASSERT_EQ(top->getTypespecs()->size(), 2u);
}

TEST_F(ReplOpTest, NetAHasNoDeclarationTimeInitializer) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  // "a" is declared bare ("bit [15:0] a;"), with no decl-assignment --
  // its entire value must come from the "a = {8{b}};" assignment below.
  EXPECT_EQ(a->getValue<hldb::Constant>(), nullptr) << "'a' is declared without an initializer";
}

TEST_F(ReplOpTest, NetBHasBinaryInitializer) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  EXPECT_NE(b->getTypespec<hldb::RefTypespec>()->getActual<hldb::BitTypespec>(), nullptr);
  const hldb::Constant *const init = b->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), 3 /* vpiBinaryConst */);
  EXPECT_EQ(init->getDecompile(), "2'b10");
  EXPECT_EQ(init->getValue(), "10");
  EXPECT_NE(init->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>(), nullptr);
}

// --- the point of the file: replication count + inner concatenation ------

TEST_F(ReplOpTest, InitialBlockHasOneStatement) {
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

TEST_F(ReplOpTest, AssignmentRhsIsMultiConcatOfEightCopiesOfB) {
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

  const hldb::Operation *const multiConcat = assign->getRhs<hldb::Operation>();
  ASSERT_NE(multiConcat, nullptr);
  EXPECT_EQ(multiConcat->getOpType(), vpiMultiConcatOp);
  ASSERT_NE(multiConcat->getOperands(), nullptr);
  ASSERT_EQ(multiConcat->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::Constant>(multiConcat->getOperands()->at(0))->getDecompile(), "8")
      << "replication count must be preserved as a real operand";

  const hldb::Operation *const innerConcat = any_cast<hldb::Operation>(multiConcat->getOperands()->at(1));
  ASSERT_NE(innerConcat, nullptr) << "the replicated value must be wrapped in its own "
                                      "concatenation Operation";
  EXPECT_EQ(innerConcat->getOpType(), vpiConcatOp);
  ASSERT_NE(innerConcat->getOperands(), nullptr);
  ASSERT_EQ(innerConcat->getOperands()->size(), 1u);
  EXPECT_EQ(any_cast<hldb::RefObj>(innerConcat->getOperands()->at(0))->getName(), "b");
}

// --- design-level typespecs / compiler diagnostics -------------------------

TEST_F(ReplOpTest, DesignHasThreeTypespecs) {
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

TEST_F(ReplOpTest, CompilerReportsZeroErrors) {
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
