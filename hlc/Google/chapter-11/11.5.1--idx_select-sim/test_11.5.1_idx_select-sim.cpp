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

// Tests for 11.5.1--idx_select-sim.sv (tags: 11.5.1)
//   logic [15:0] a = 16'h1000;
//   logic b;
//   logic c;
//
//   initial begin
//     b = a[12];
//     c = a[5];
//     $display(":assert: (1 == %d)", b);
//     $display(":assert: (0 == %d)", c);
//   end
//
// Per IEEE 1800-2017 11.5.1: a[12] must select bit 12 of a = 16'h1000
// (0001_0000_0000_0000), which is 1; a[5] must select bit 5, which is 0.
// This file is the runtime-verification sibling of idx_select.sv: it adds
// a decl-assignment on "a" and two independent bit-selects into two
// separate destination nets, each checked by its own $display.
//
// Checked:
//   - module top has exactly 3 variables: "a" (LogicTypespec, vector
//     [15:0], decl-assigned Constant "16'h1000"), "b" and "c" (both
//     LogicTypespec, scalar -- no packed dimension -- neither
//     decl-assigned). Per IEEE 1800-2023 Sec 6.7/6.8: "logic" with no
//     net-type keyword is a variable, not a net.
//   - module has exactly 1 process: an Initial whose Begin has exactly 4
//     statements, in source order:
//       1) blocking Assignment: lhs RefObj "b", rhs BitSelect "a[12]"
//          (prefix RefObj "a", index Constant "12")
//       2) blocking Assignment: lhs RefObj "c", rhs BitSelect "a[5]"
//          (prefix RefObj "a", index Constant "5")
//       3) SysTaskCall "$display" with 2 arguments: Constant string
//          ":assert: (1 == %d)" and RefObj "b"
//       4) SysTaskCall "$display" with 2 arguments: Constant string
//          ":assert: (0 == %d)" and RefObj "c"
//   - design-level typespecs (4): ModuleTypespec, IntTypespec (signed, for
//     the module's own bit-range), IntTypespec (plain, for the bit-select
//     index constants), StringTypespec
//   - compiler emits zero errors
//
// Not checked (GTEST_SKIP, with a real reason, not just "no time"):
//   - Whether b actually equals 1 and c actually equals 0 at runtime. HLC
//     is a compiler/elaborator, not a simulator: Variable::getValue<T>()
//     only ever exposes a declaration-time initializer, and "b"/"c" are
//     only ever assigned inside the initial block, never at declaration -- so
//     there is no field anywhere that captures the post-assignment value
//     the two ":assert:" tags in the source are asking a simulator to
//     check.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class IdxSelectSimTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.5.1--idx_select-sim.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / nets ----------------------------------------------------------

TEST_F(IdxSelectSimTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(IdxSelectSimTest, ModuleHasThreeVariablesOnlyADeclAssigned) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  ASSERT_EQ(top->getVariables()->size(), 3u);

  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  const hldb::Variable *const c = hldb::findByName<hldb::Variable>("c", top->getVariables());
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  ASSERT_NE(c, nullptr);

  ASSERT_NE(a->getValue<hldb::Constant>(), nullptr);
  EXPECT_EQ(a->getValue<hldb::Constant>()->getDecompile(), "16'h1000");
  EXPECT_EQ(b->getValue<hldb::Constant>(), nullptr) << "'b' has no decl-assignment";
  EXPECT_EQ(c->getValue<hldb::Constant>(), nullptr) << "'c' has no decl-assignment";

  const hldb::LogicTypespec *const bType = b->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
  const hldb::LogicTypespec *const cType = c->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(bType, nullptr);
  ASSERT_NE(cType, nullptr);
  EXPECT_EQ(bType->getRanges(), nullptr) << "'b' is a bare scalar 'logic b;'";
  EXPECT_EQ(cType->getRanges(), nullptr) << "'c' is a bare scalar 'logic c;'";
}

// --- initial block: two independent bit-selects -----------------------------

TEST_F(IdxSelectSimTest, InitialBlockHasFourStatements) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 4u);
}

TEST_F(IdxSelectSimTest, FirstTwoStatementsAreBitSelectsOfBitsTwelveAndFive) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);

  const hldb::Assignment *const bAssign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(bAssign, nullptr);
  EXPECT_EQ(bAssign->getLhs<hldb::RefObj>()->getName(), "b");
  const hldb::BitSelect *const bSel = bAssign->getRhs<hldb::BitSelect>();
  ASSERT_NE(bSel, nullptr);
  EXPECT_EQ(bSel->getPrefix<hldb::RefObj>()->getName(), "a");
  EXPECT_EQ(bSel->getIndex<hldb::Constant>()->getDecompile(), "12");

  const hldb::Assignment *const cAssign = any_cast<hldb::Assignment>(blk->getStmts()->at(1));
  ASSERT_NE(cAssign, nullptr);
  EXPECT_EQ(cAssign->getLhs<hldb::RefObj>()->getName(), "c");
  const hldb::BitSelect *const cSel = cAssign->getRhs<hldb::BitSelect>();
  ASSERT_NE(cSel, nullptr);
  EXPECT_EQ(cSel->getPrefix<hldb::RefObj>()->getName(), "a");
  EXPECT_EQ(cSel->getIndex<hldb::Constant>()->getDecompile(), "5");
}

TEST_F(IdxSelectSimTest, LastTwoStatementsDisplayExpectedBAndCValues) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);

  const hldb::SysTaskCall *const dispB = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(2));
  ASSERT_NE(dispB, nullptr);
  EXPECT_EQ(dispB->getName(), "$display");
  ASSERT_NE(dispB->getArguments(), nullptr);
  ASSERT_EQ(dispB->getArguments()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::Constant>(dispB->getArguments()->at(0))->getValue(), ":assert: (1 == %d)");
  EXPECT_EQ(any_cast<hldb::RefObj>(dispB->getArguments()->at(1))->getName(), "b");

  const hldb::SysTaskCall *const dispC = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(3));
  ASSERT_NE(dispC, nullptr);
  EXPECT_EQ(dispC->getName(), "$display");
  ASSERT_NE(dispC->getArguments(), nullptr);
  ASSERT_EQ(dispC->getArguments()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::Constant>(dispC->getArguments()->at(0))->getValue(), ":assert: (0 == %d)");
  EXPECT_EQ(any_cast<hldb::RefObj>(dispC->getArguments()->at(1))->getName(), "c");
}

// --- design-level typespecs / compiler diagnostics --------------------------

TEST_F(IdxSelectSimTest, DesignHasFourTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 4u);
}

TEST_F(IdxSelectSimTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

// --- the actual point of the file: runtime bit values -----------------------

TEST_F(IdxSelectSimTest, BEqualsOneAndCEqualsZeroAtRuntime) {
  GTEST_SKIP() << "IEEE 1800-2017 11.5.1: a = 16'h1000 = 0001_0000_0000_0000b, "
                  "so a[12] == 1 and a[5] == 0, per the two ':assert:' tags "
                  "authored into the source. HLC is a static compiler/"
                  "elaborator: Variable::getValue<T>() only ever exposes a "
                  "declaration-time initializer, and 'b'/'c' are only "
                  "assigned inside the initial block, never at declaration "
                  "-- there is no field capturing their post-assignment "
                  "runtime value.";
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", top->getVariables());
  const hldb::Variable *const c = hldb::findByName<hldb::Variable>("c", top->getVariables());
  ASSERT_NE(b, nullptr);
  ASSERT_NE(c, nullptr);
  const hldb::Constant *const bValue = b->getValue<hldb::Constant>();
  const hldb::Constant *const cValue = c->getValue<hldb::Constant>();
  ASSERT_NE(bValue, nullptr) << "no field captures b's post-assignment runtime value";
  ASSERT_NE(cValue, nullptr) << "no field captures c's post-assignment runtime value";
  EXPECT_EQ(bValue->getDecompile(), "1");
  EXPECT_EQ(cValue->getDecompile(), "0");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
