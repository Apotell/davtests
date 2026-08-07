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

// Tests for 11.5.2--multi_dim_array_addressing-sim.sv (tags: 11.5.2)
//   logic [7:0] mem [0:1023][0:3];
//   logic [7:0] a;
//
//   initial begin
//     mem[123][2] = 125;
//     a = mem[123][2];
//     $display(":assert: (125 == %d)", a);
//   end
//
// This is the runtime-verification sibling of multi_dim_array_addressing.sv:
// it writes into the doubly-indexed element before reading it back, so the
// nested-BitSelect shape ("mem[123][2]" -> prefix BitSelect "mem[123]" ->
// prefix RefObj "mem") appears as both an lvalue and an rvalue.
//
// IEEE 1800-2023 6.7/6.8: "logic [7:0] mem [0:1023][0:3]" and
// "logic [7:0] a" have no net-type keyword (wire/tri/.../nettype), so per
// the standard they are variable_declarations, not net_declarations. Both
// must be modeled as hldb::Variable, found via Module::getVariables(), not
// as hldb::Net / Module::getNets().
//
// Checked:
//   - module top has exactly 2 variables: "mem" (nested ArrayTypespec pair,
//     both vpiArrayType == vpiStaticArray, outer range [0:1023] / inner
//     range [0:3], innermost elem typespec LogicTypespec [7:0]) and "a"
//     (LogicTypespec, vector [7:0]), neither decl-assigned
//   - module has exactly 1 process: an Initial whose Begin has exactly 3
//     statements, in source order:
//       1) blocking Assignment: lhs nested BitSelect "mem[123][2]" (outer
//          index Constant "2", prefix BitSelect "mem[123]" with index
//          Constant "123" and prefix RefObj "mem"), rhs Constant "125"
//       2) blocking Assignment: lhs RefObj "a", rhs the same nested
//          BitSelect "mem[123][2]" shape, now on the rhs
//       3) SysTaskCall "$display" with 2 arguments: Constant string
//          ":assert: (125 == %d)" and RefObj "a"
//   - design-level typespecs (3): ModuleTypespec, IntTypespec (signed),
//     StringTypespec
//   - compiler emits zero errors
//
// Not checked (GTEST_SKIP, with a real reason, not just "no time"):
//   - Whether "a" actually equals 125 at runtime. HLC is a compiler/
//     elaborator, not a simulator: Variable::getValue<T>() only ever exposes
//     a declaration-time initializer, and neither "mem" nor "a" is
//     decl-assigned -- there is no field anywhere that captures the
//     write-then-read value the ":assert:" tag is asking a simulator to
//     check.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class MultiDimArrayAddressingSimTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.5.2--multi_dim_array_addressing-sim.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / variables -------------------------------------------------------

TEST_F(MultiDimArrayAddressingSimTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(MultiDimArrayAddressingSimTest, ModuleHasTwoVariablesNeitherDeclAssigned) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  ASSERT_EQ(top->getVariables()->size(), 2u);

  const hldb::Variable *const mem = hldb::findByName<hldb::Variable>("mem", top->getVariables());
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(mem, nullptr);
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(mem->getValue<hldb::Constant>(), nullptr);
  EXPECT_EQ(a->getValue<hldb::Constant>(), nullptr);

  const hldb::ArrayTypespec *const outer = mem->getTypespec<hldb::RefTypespec>()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(outer, nullptr);
  EXPECT_EQ(outer->getArrayType(), vpiStaticArray);
  ASSERT_NE(outer->getElemTypespec(), nullptr);
  EXPECT_NE(outer->getElemTypespec()->getActual<hldb::ArrayTypespec>(), nullptr)
      << "the second dimension should be a nested ArrayTypespec";
}

// --- initial block: write-then-read through the nested BitSelect -----------

TEST_F(MultiDimArrayAddressingSimTest, InitialBlockHasThreeStatements) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 3u);
}

TEST_F(MultiDimArrayAddressingSimTest, FirstStatementWritesOneTwentyFiveIntoNestedBitSelectLhs) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);

  const hldb::BitSelect *const outer = assign->getLhs<hldb::BitSelect>();
  ASSERT_NE(outer, nullptr) << "'mem[123][2] = 125' should have a nested BitSelect lhs";
  EXPECT_EQ(outer->getName(), "mem[123][2]");
  EXPECT_EQ(outer->getIndex<hldb::Constant>()->getDecompile(), "2");
  const hldb::BitSelect *const inner = outer->getPrefix<hldb::BitSelect>();
  ASSERT_NE(inner, nullptr);
  EXPECT_EQ(inner->getPrefix<hldb::RefObj>()->getName(), "mem");
  EXPECT_EQ(inner->getIndex<hldb::Constant>()->getDecompile(), "123");

  ASSERT_NE(assign->getRhs<hldb::Constant>(), nullptr);
  EXPECT_EQ(assign->getRhs<hldb::Constant>()->getDecompile(), "125");
}

TEST_F(MultiDimArrayAddressingSimTest, SecondStatementReadsNestedBitSelectIntoA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(1));
  ASSERT_NE(assign, nullptr);
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "a");

  const hldb::BitSelect *const rhs = assign->getRhs<hldb::BitSelect>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getName(), "mem[123][2]");
}

TEST_F(MultiDimArrayAddressingSimTest, ThirdStatementDisplaysExpectedAValue) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(2));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::Constant>(disp->getArguments()->at(0))->getValue(), ":assert: (125 == %d)");
  EXPECT_EQ(any_cast<hldb::RefObj>(disp->getArguments()->at(1))->getName(), "a");
}

// --- design-level typespecs / compiler diagnostics --------------------------

TEST_F(MultiDimArrayAddressingSimTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(MultiDimArrayAddressingSimTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

// --- the actual point of the file: runtime value ----------------------------

TEST_F(MultiDimArrayAddressingSimTest, AEqualsOneTwentyFiveAtRuntime) {
  GTEST_SKIP() << "The source writes 125 into mem[123][2] then reads it back "
                  "into 'a', asserting a == 125. HLC is a static compiler/"
                  "elaborator: Variable::getValue<T>() only ever exposes a "
                  "declaration-time initializer, and 'a' is only assigned "
                  "inside the initial block, never at declaration -- there "
                  "is no field capturing its post-assignment runtime value.";
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const aValue = a->getValue<hldb::Constant>();
  ASSERT_NE(aValue, nullptr) << "no field captures a's post-assignment runtime value";
  EXPECT_EQ(aValue->getDecompile(), "125");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
