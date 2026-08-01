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

// Tests for 11.4.12.1--nested_repl_op.sv (tags: 11.4.12.1)
//   bit [15:0] a;
//   bit [1:0] b = 2'b10;
//   bit [1:0] c = 2'b01;
//   bit [3:0] d = 4'b1111;
//   initial begin
//     a = {{3{b, c}}, d};
//   end
//
// This file builds on 11.4.12.1--repl_op.sv's single-level replication by
// nesting a replication *inside* an outer concatenation, and replicating
// a multi-operand group ("b, c") rather than a single variable. The
// corner this file exercises is the full 4-level tree IEEE 1800-2017
// 11.4.12.1 implies for "{{3{b, c}}, d}": an outer vpiConcatOp of 2
// operands (the replicated group, and "d"), whose first operand is a
// vpiMultiConcatOp (count 3) wrapping an *inner* vpiConcatOp of 2
// operands (b and c) -- i.e. one more level of nesting than
// 11.4.12.1--repl_op.sv needed, since here the replicated unit is itself
// a concatenation rather than a bare variable.
//
// Checked:
//   - module getTypespecs() has exactly 4 entries: BitTypespec [15:0]
//     ("a"), [1:0] ("b"), [1:0] ("c"), [3:0] ("d") -- four distinct
//     entries, one per separately-declared variable
//   - "a", "b", "c", "d" are all declared with bare "bit" (no net-type
//     keyword) and there is no port list, so per IEEE 1800-2023 Sec
//     6.7/6.8 all four are Variables, not Nets; module has no nets
//     (getNets() is null)
//   - variables "b", "c", "d" each have a declaration-time
//     getValue<Constant>() matching their literal ("2'b10", "2'b01",
//     "4'b1111")
//   - the initial block is a Begin with exactly 1 statement: a blocking
//     Assignment, lhs RefObj "a", rhs an Operation (vpiConcatOp,
//     2 operands):
//       operand 0: Operation (vpiMultiConcatOp, 2 operands): operand 0 =
//         Constant "3" (repeat count), operand 1 = Operation
//         (vpiConcatOp, 2 operands: RefObj "b", RefObj "c") -- the
//         replicated group is itself a 2-operand concatenation, one
//         level deeper than a bare-variable replication
//       operand 1: RefObj "d"
//   - design-level typespecs (3): ModuleTypespec, IntTypespec (signed),
//     LogicTypespec
//   - compiler emits zero errors
//
// Not checked:
//   - this file has no $display assertion at all (unlike its "-sim"
//     sibling, 11.4.12.1--nested_repl_op-sim.sv), so there is no runtime
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

class NestedReplOpTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.4.12.1--nested_repl_op.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module-level typespecs / variables -------------------------------------

TEST_F(NestedReplOpTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(NestedReplOpTest, ModuleHasFourDistinctBitTypespecs) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  ASSERT_EQ(top->getTypespecs()->size(), 4u);
}

TEST_F(NestedReplOpTest, ModuleHasNoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getNets(), nullptr) << "'bit' carries no net-type keyword; per IEEE 1800-2023 "
                                         "Sec 6.7/6.8 all four declarations are Variables";
}

TEST_F(NestedReplOpTest, VariableAHasNoDeclarationTimeInitializer) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  // "a" is declared bare ("bit [15:0] a;"), with no decl-assignment --
  // its entire value must come from the "a = {{3{b, c}}, d};" assignment.
  EXPECT_EQ(a->getValue(), nullptr) << "'a' is declared without an initializer";
}

TEST_F(NestedReplOpTest, VariablesBCDHaveExpectedInitializers) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const char *const names[3] = {"b", "c", "d"};
  const char *const decompiles[3] = {"2'b10", "2'b01", "4'b1111"};
  const char *const values[3] = {"10", "01", "1111"};
  for (uint32_t i = 0; i < 3u; ++i) {
    const hldb::Variable *const var = hldb::findByName<hldb::Variable>(names[i], top->getVariables());
    ASSERT_NE(var, nullptr) << "variable " << names[i];
    EXPECT_NE(var->getTypespec<hldb::RefTypespec>()->getActual<hldb::BitTypespec>(), nullptr);
    const hldb::Constant *const init = var->getValue<hldb::Constant>();
    ASSERT_NE(init, nullptr);
    EXPECT_EQ(init->getDecompile(), decompiles[i]);
    EXPECT_EQ(init->getValue(), values[i]);
  }
}

// --- the point of the file: replication of a 2-operand group ------------

TEST_F(NestedReplOpTest, InitialBlockHasOneStatement) {
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

TEST_F(NestedReplOpTest, AssignmentRhsIsOuterConcatOfReplicatedGroupAndD) {
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

  // Outer: {{3{b,c}}, d} -- a 2-operand concatenation.
  const hldb::Operation *const outerConcat = assign->getRhs<hldb::Operation>();
  ASSERT_NE(outerConcat, nullptr);
  EXPECT_EQ(outerConcat->getOpType(), vpiConcatOp);
  ASSERT_NE(outerConcat->getOperands(), nullptr);
  ASSERT_EQ(outerConcat->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::RefObj>(outerConcat->getOperands()->at(1))->getName(), "d");

  // First operand: {3{b,c}} -- a multi-concat wrapping a 2-operand concat.
  const hldb::Operation *const multiConcat = any_cast<hldb::Operation>(outerConcat->getOperands()->at(0));
  ASSERT_NE(multiConcat, nullptr) << "'{3{b, c}}' should be its own multi-concatenation Operation";
  EXPECT_EQ(multiConcat->getOpType(), vpiMultiConcatOp);
  ASSERT_NE(multiConcat->getOperands(), nullptr);
  ASSERT_EQ(multiConcat->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::Constant>(multiConcat->getOperands()->at(0))->getDecompile(), "3");

  const hldb::Operation *const innerConcat = any_cast<hldb::Operation>(multiConcat->getOperands()->at(1));
  ASSERT_NE(innerConcat, nullptr) << "the replicated unit '{b, c}' should be its own "
                                      "concatenation Operation, distinct from the outer one";
  EXPECT_EQ(innerConcat->getOpType(), vpiConcatOp);
  ASSERT_NE(innerConcat->getOperands(), nullptr);
  ASSERT_EQ(innerConcat->getOperands()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::RefObj>(innerConcat->getOperands()->at(0))->getName(), "b");
  EXPECT_EQ(any_cast<hldb::RefObj>(innerConcat->getOperands()->at(1))->getName(), "c");
}

// --- design-level typespecs / compiler diagnostics -------------------------

TEST_F(NestedReplOpTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(NestedReplOpTest, CompilerReportsZeroErrors) {
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
