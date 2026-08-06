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

// Tests for 11.10--string_bit_array-sim.sv (tags: 11.10)
//   module top();
//     bit [8*14:1] a;
//     initial begin
//       a = "Test";
//       $display(":assert: ('Test' == '%0s')", a);
//     end
//   endmodule
//
// This is the "sim" counterpart of 11.10--string_bit_array.sv: the exact
// same "bit [8*14:1] a; a = \"Test\";", plus a $display that spells out
// the expected observable outcome ('Test' == '%0s' formatted with a).
//
// What to check and why (IEEE 1800-2023 Sec 11.10 "String literal
// expressions", p.304 -- see 11.10--string_bit_array.sv's header for the
// full clause text and the chapter-5 precedent this reuses):
//   Same Constant shape as the non-sim file (vpiStringConst, value
//   "Test", size 32, StringTypespec), plus this file's own corner: the
//   $display call's format-string argument and its "a" argument, in that
//   order, confirming the assertion targets the same variable that was
//   just assigned.
//
// What is checked:
//   - module top has zero nets and exactly 1 Variable "a" (bare "bit",
//     no net-type keyword, no port list -- IEEE 1800-2023 Sec 6.7/6.8),
//     with no declaration-time initializer and a BitTypespec whose range
//     is the unfolded "8*14 : 1" expression (same shape as the non-sim
//     sibling)
//   - the initial block is a Begin with exactly 2 statements:
//       [0] blocking Assignment: lhs RefObj "a", rhs Constant
//           (vpiStringConst, value "Test", size 32, StringTypespec)
//       [1] SysTaskCall "$display" with 2 arguments: Constant string
//           ":assert: ('Test' == '%0s')" and RefObj "a"
//   - compiler emits zero errors
//
// What is NOT checked and why:
//   - the exact design-level typespec count is inferred (ModuleTypespec +
//     shared IntTypespec for the "8"/"14" range literals + shared
//     StringTypespec for "Test"/the format string = 3), following this
//     codebase's consistently-observed typespec-sharing convention, not
//     independently re-run for this new file -- build and run to confirm.
//   - whether "a" actually reads back as "Test" when the $display
//     executes. Variable::getValue<T>() only exposes a declaration-time
//     initializer (none here); there is no field anywhere that captures
//     a post-assignment runtime value. Genuine simulation-only gap (see
//     the GTEST_SKIP() canary below), not a shortcut -- the literal
//     itself is already fully checked above as the Assignment's rhs.

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
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/string_typespec.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class StringBitArraySimTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.10--string_bit_array-sim.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
  static const hldb::Begin *getInitialBody() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    return (init == nullptr) ? nullptr : init->getStmt<hldb::Begin>();
  }
};

// --- module / variable ------------------------------------------------------

TEST_F(StringBitArraySimTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(StringBitArraySimTest, ModuleHasNoNetsAndOneVariableA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "'bit' is not a net-type keyword (IEEE 1800-2023 Sec 6.7)";
  ASSERT_NE(top->getVariables(), nullptr);
  ASSERT_EQ(top->getVariables()->size(), 1u);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr) << "Variable 'a' not found";
  EXPECT_EQ(a->getValue(), nullptr) << "'a' is declared bare; its value comes only from the "
                                        "procedural assignment below";
}

TEST_F(StringBitArraySimTest, VariableAHasBitTypespecWithUnfoldedRangeEightTimesFourteenToOne) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::BitTypespec *const bt = a->getTypespec<hldb::RefTypespec>()->getActual<hldb::BitTypespec>();
  ASSERT_NE(bt, nullptr) << "'bit [8*14:1] a' must produce a BitTypespec";
  ASSERT_NE(bt->getRanges(), nullptr);
  ASSERT_EQ(bt->getRanges()->size(), 1u);
  const hldb::Range *const range = bt->getRanges()->at(0);
  ASSERT_NE(range, nullptr);
  const hldb::Operation *const left = range->getLeftExpr<hldb::Operation>();
  ASSERT_NE(left, nullptr) << "'8*14' is not folded to a literal -- range left must be an Operation";
  EXPECT_EQ(left->getOpType(), vpiMultOp);
  const hldb::Constant *const right = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "1");
}

// --- initial block: a = "Test"; $display(...) -------------------------------

TEST_F(StringBitArraySimTest, InitialBlockHasTwoStatements) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  EXPECT_EQ(blk->getStmts()->size(), 2u);
}

TEST_F(StringBitArraySimTest, FirstStatementAssignsTestStringLiteralToA) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  const hldb::Assignment *const assign = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(assign, nullptr);
  EXPECT_TRUE(assign->getBlocking());
  EXPECT_EQ(assign->getLhs<hldb::RefObj>()->getName(), "a");
  const hldb::Constant *const lit = assign->getRhs<hldb::Constant>();
  ASSERT_NE(lit, nullptr) << "'\"Test\"' should be a Constant";
  EXPECT_EQ(lit->getConstType(), vpiStringConst);
  EXPECT_EQ(lit->getValue(), "Test");
  EXPECT_EQ(lit->getSize(), 32) << "\"Test\" = 4 chars x 8 bits = 32 bits";
  ASSERT_NE(lit->getTypespec(), nullptr);
  EXPECT_NE(lit->getTypespec()->getActual<hldb::StringTypespec>(), nullptr);
}

TEST_F(StringBitArraySimTest, SecondStatementDisplaysAssertionAboutA) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(1));
  ASSERT_NE(disp, nullptr);
  EXPECT_EQ(disp->getName(), "$display");
  ASSERT_NE(disp->getArguments(), nullptr);
  ASSERT_EQ(disp->getArguments()->size(), 2u);
  EXPECT_EQ(any_cast<hldb::Constant>(disp->getArguments()->at(0))->getValue(), ":assert: ('Test' == '%0s')");
  const hldb::RefObj *const arg = any_cast<hldb::RefObj>(disp->getArguments()->at(1));
  ASSERT_NE(arg, nullptr);
  EXPECT_EQ(arg->getName(), "a");
  EXPECT_NE(arg->getActual<hldb::Variable>(), nullptr);
}

// --- compiler diagnostics ---------------------------------------------------

TEST_F(StringBitArraySimTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

// --- known gap: post-execution value of 'a' requires simulation ------------

TEST_F(StringBitArraySimTest, AEndsUpEqualToTest) {
  GTEST_SKIP() << "HLC is a static compiler/elaborator: Variable::getValue<T>() only ever exposes "
                  "a declaration-time initializer (none here); there is no field anywhere that "
                  "captures a post-assignment runtime value. The literal itself is fully checked "
                  "above; only observing it AFTER execution is the genuine simulation-only gap.";
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const finalValue = a->getValue<hldb::Constant>();
  ASSERT_NE(finalValue, nullptr) << "no field captures a's post-assignment runtime value";
  EXPECT_EQ(finalValue->getValue(), "Test");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
