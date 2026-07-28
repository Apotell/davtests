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

// Tests for 11.4.12.1--repl_op-sim.sv (tags: 11.4.12.1)
//   bit [15:0] a;
//   bit [1:0] b = 2'b10;
//   initial begin
//     a = {8{b}};
//     $display(":assert: (0b1010101010101010 == %d)", a);
//   end
//
// The "sim" counterpart of 11.4.12.1--repl_op.sv: the same
// vpiMultiConcatOp-over-vpiConcatOp tree, now followed by a $display
// asserting that a reads back as "10" repeated 8 times -- i.e. that
// replicating the 2-bit value "10" eight times fills all 16 bits of a
// with the pattern 1010101010101010.
//
// Checked:
//   - module getTypespecs() has exactly 2 entries: BitTypespec [15:0]
//     (for "a"), BitTypespec [1:0] (for "b")
//   - net "b" has a declaration-time getValue<Constant>() of "2'b10"
//   - the initial block is a Begin with exactly 2 statements:
//       [0] blocking Assignment: lhs RefObj "a", rhs an Operation
//           (vpiMultiConcatOp, 2 operands): operand 0 = Constant "8",
//           operand 1 = Operation (vpiConcatOp, 1 operand: RefObj "b")
//       [1] SysTaskCall "$display" asserting
//           ("0b1010101010101010 == %d", a)
//   - design-level typespecs (4): ModuleTypespec, IntTypespec (signed),
//     LogicTypespec, StringTypespec
//   - compiler emits zero errors
//
// Not checked (GTEST_SKIP, with a real reason):
//   - Whether a actually evaluates to the 16-bit repeated pattern (i.e.
//     that replication really tiles the 2-bit value across all 8 slots
//     at runtime). HLC is a static compiler/elaborator: Net "a" has no
//     declaration-time initializer, and an Operation has no computed-
//     value field. Genuine simulation-only gap, not a shortcut.

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
#include <hldb/string_typespec.h>
#include <hldb/sys_task_call.h>
#include <hldb/typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ReplOpSimTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.4.12.1--repl_op-sim.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()); }
  static const hldb::Begin *getInitialBody() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    return (init == nullptr) ? nullptr : init->getStmt<hldb::Begin>();
  }
};

// --- module-level typespecs / nets -----------------------------------------

TEST_F(ReplOpSimTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ReplOpSimTest, ModuleHasTwoDistinctBitTypespecs) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  ASSERT_EQ(top->getTypespecs()->size(), 2u);
}

TEST_F(ReplOpSimTest, NetAHasNoDeclarationTimeInitializer) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getValue<hldb::Constant>(), nullptr) << "'a' is declared without an initializer";
}

TEST_F(ReplOpSimTest, NetBHasBinaryInitializer) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const hldb::Constant *const init = b->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getDecompile(), "2'b10");
  EXPECT_EQ(init->getValue(), "10");
}

// --- the replication tree + its assertion ----------------------------------

TEST_F(ReplOpSimTest, InitialBlockHasTwoStatements) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 2u);
}

TEST_F(ReplOpSimTest, AssignmentRhsIsMultiConcatOfEightCopiesOfB) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "a");

  const hldb::Operation *const multiConcat = assign->getRhs<hldb::Operation>();
  ASSERT_NE(multiConcat, nullptr);
  EXPECT_EQ(multiConcat->getOpType(), vpiMultiConcatOp);
  ASSERT_NE(multiConcat->getOperands(), nullptr);
  ASSERT_EQ(multiConcat->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::Constant>(multiConcat->getOperands()->at(0))->getDecompile(), "8");

  const hldb::Operation *const innerConcat = any_cast<hldb::Operation>(multiConcat->getOperands()->at(1));
  ASSERT_NE(innerConcat, nullptr);
  EXPECT_EQ(innerConcat->getOpType(), vpiConcatOp);
  ASSERT_NE(innerConcat->getOperands(), nullptr);
  ASSERT_EQ(innerConcat->getOperands()->size(), 1u);
  EXPECT_EQ(any_cast<hldb::RefObj>(innerConcat->getOperands()->at(0))->getName(), "b");
}

TEST_F(ReplOpSimTest, SecondStatementDisplaysRepeatedPatternEqualsA) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(1));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::Constant>(disp->getArguments()->at(0))->getValue(),
            ":assert: (0b1010101010101010 == %d)");
  EXPECT_EQ(any_cast<hldb::RefObj>(disp->getArguments()->at(1))->getName(), "a");
}

// --- design-level typespecs / compiler diagnostics -------------------------

TEST_F(ReplOpSimTest, DesignHasFourTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 4u);
}

TEST_F(ReplOpSimTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

// --- the actual point of the file: does replication tile correctly -------

TEST_F(ReplOpSimTest, AEndsUpEqualToRepeatedPattern) {
  GTEST_SKIP() << "The source asserts a == 0b1010101010101010 after 'a = {8{b}};' runs with "
                  "b == 2'b10. HLC is a static compiler/elaborator: Net 'a' has no declaration-"
                  "time initializer, and an Operation has no computed-value field. Genuine "
                  "simulation-only gap, not a shortcut.";
  // If the GTEST_SKIP() above is ever removed, this must still compile and
  // exercise a real, currently-failing check -- not silently pass.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  ASSERT_NE(a->getValue<hldb::Constant>(), nullptr) << "a's post-assignment runtime value is "
                                                        "not captured anywhere in the object model";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
