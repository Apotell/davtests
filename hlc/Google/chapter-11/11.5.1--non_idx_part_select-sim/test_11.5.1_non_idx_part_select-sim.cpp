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

// Tests for 11.5.1--non_idx_part_select-sim.sv (tags: 11.5.1)
//   logic [15:0] a = 16'h1234;
//   logic [3:0] b;
//
//   initial begin
//     b = a[11:8];
//     $display(":assert: (2 == %d)", b);
//   end
//
// Per IEEE 1800-2017 11.5.1: a[11:8] selects bits 11 down to 8 of
// a = 16'h1234 (0001_0010_0011_0100b), which is 4'b0010 = 2.
//
// Checked:
//   - module top has exactly 2 nets: "a" (LogicTypespec, vector [15:0],
//     decl-assigned Constant "16'h1234") and "b" (LogicTypespec, vector
//     [3:0], not decl-assigned)
//   - module has exactly 1 process: an Initial whose Begin has exactly 2
//     statements: a blocking Assignment followed by a SysTaskCall
//   - the Assignment: lhs RefObj "b"; rhs PartSelect "a[11:8]" whose
//     vpiPrefix resolves to Net "a" and whose vpiRange has vpiLeftRange
//     Constant "11", vpiRightRange Constant "8"
//   - the SysTaskCall "$display" has 2 arguments: Constant string
//     ":assert: (2 == %d)" and RefObj "b"
//   - design-level typespecs (4): ModuleTypespec, IntTypespec (signed, for
//     the module's own bit-ranges), IntTypespec (plain, for the
//     part-select range constants), StringTypespec
//   - compiler emits zero errors
//
// Not checked (GTEST_SKIP, with a real reason, not just "no time"):
//   - Whether b actually equals 2 at runtime. HLC is a compiler/
//     elaborator, not a simulator: Net::getValue<T>() only ever exposes a
//     declaration-time initializer, and "b" is only ever assigned inside
//     the initial block, never at declaration -- so there is no field
//     capturing the post-assignment value the ":assert:" tag is asking a
//     simulator to check.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/int_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/part_select.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_task_call.h>
#include <hldb/vpi_user.h>

namespace hlc {

class NonIdxPartSelectSimTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.5.1--non_idx_part_select-sim.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- module / nets ----------------------------------------------------------

TEST_F(NonIdxPartSelectSimTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(NonIdxPartSelectSimTest, ModuleHasTwoNetsOnlyADeclAssigned) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 2u);

  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  ASSERT_NE(a->getValue<hldb::Constant>(), nullptr);
  EXPECT_EQ(a->getValue<hldb::Constant>()->getDecompile(), "16'h1234");
  EXPECT_EQ(b->getValue<hldb::Constant>(), nullptr) << "'b' has no decl-assignment";
}

// --- initial block: procedural constant-range part-select -------------------

TEST_F(NonIdxPartSelectSimTest, InitialBlockHasTwoStatements) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 2u);
}

TEST_F(NonIdxPartSelectSimTest, AssignmentRhsIsElevenToEightPartSelect) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "b");

  const hldb::PartSelect *const sel = assign->getRhs<hldb::PartSelect>();
  ASSERT_NE(sel, nullptr);
  EXPECT_EQ(sel->getName(), "a[11:8]");
  EXPECT_EQ(sel->getPrefix<hldb::RefObj>()->getName(), "a");
  ASSERT_NE(sel->getRange(), nullptr);
  EXPECT_EQ(sel->getRange()->getLeftExpr<hldb::Constant>()->getDecompile(), "11");
  EXPECT_EQ(sel->getRange()->getRightExpr<hldb::Constant>()->getDecompile(), "8");
}

TEST_F(NonIdxPartSelectSimTest, SecondStatementDisplaysExpectedBValue) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const blk = init->getStmt<hldb::Begin>();
  ASSERT_NE(blk, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(1));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::Constant>(disp->getArguments()->at(0))->getValue(), ":assert: (2 == %d)");
  EXPECT_EQ(any_cast<hldb::RefObj>(disp->getArguments()->at(1))->getName(), "b");
}

// --- design-level typespecs / compiler diagnostics --------------------------

TEST_F(NonIdxPartSelectSimTest, DesignHasFourTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 4u);
}

TEST_F(NonIdxPartSelectSimTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

// --- the actual point of the file: runtime part-select value ---------------

TEST_F(NonIdxPartSelectSimTest, BEqualsTwoAtRuntime) {
  GTEST_SKIP() << "IEEE 1800-2017 11.5.1: a = 16'h1234, so a[11:8] == 4'b0010 "
                  "== 2, per the ':assert:' tag authored into the source. "
                  "HLC is a static compiler/elaborator: Net::getValue<T>() "
                  "only ever exposes a declaration-time initializer, and "
                  "'b' is only assigned inside the initial block, never at "
                  "declaration -- there is no field capturing its "
                  "post-assignment runtime value.";
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const b = hldb::findByName<hldb::Net>("b", top->getNets());
  ASSERT_NE(b, nullptr);
  const hldb::Constant *const bValue = b->getValue<hldb::Constant>();
  ASSERT_NE(bValue, nullptr) << "no field captures b's post-assignment runtime value";
  EXPECT_EQ(bValue->getDecompile(), "2");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
