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

// Tests for 11.3.6--assign_in_expr-sim.sv (tags: 11.3.6)
//   module top();
//     int a;
//     int b;
//     int c;
//
//     initial begin
//       a = (b = (c = 5));
//       $display(":assert: (5 == %d)", a);
//       $display(":assert: (5 == %d)", b);
//       $display(":assert: (5 == %d)", c);
//     end
//   endmodule
//
// This is the "sim" counterpart of 11.3.6--assign_in_expr.sv: the exact
// same 3-deep parenthesized assignment chain "a = (b = (c = 5))", but now
// followed by three $display assertions that spell out what every clean
// simulator should observe -- a, b, and c should all read back as 5,
// since the chain terminates in the literal 5 and blocking assignments
// take effect immediately within the same initial block. The corner
// unique to this file (versus its non-sim sibling) is that the source
// itself states the expected numeric outcome three times over, once per
// variable, rather than only asserting the AST shape of the assignment.
//
// Checked:
//   - module top has exactly 3 variables, "a", "b", "c", all int
//     (RefTypespec -> IntTypespec). Per IEEE 1800-2023 Sec 6.7/6.8: "int"
//     has no net-type keyword and there is no port list, so all three are
//     Variables, not Nets; module has no nets (getNets() is null).
//   - the initial block is a Begin with exactly 4 statements:
//       [0] the same 3-deep chain as the non-sim file: Assignment(lhs a)
//           -> rhs Assignment(lhs b) -> rhs Assignment(lhs c) -> rhs
//           Constant "5" -- confirming the chain bottoms out at the
//           literal that every subsequent $display expects to see
//       [1..3] three SysTaskCall "$display", each with 2 arguments: a
//           Constant string ":assert: (5 == %d)" and a RefObj naming
//           "a", "b", "c" respectively, in that order -- confirming each
//           assertion targets the variable the spec says should equal 5
//           and that all three variables in the chain get their own
//           independent assertion (not, say, only checking "a" and
//           assuming b/c follow)
//   - design-level typespecs (3): ModuleTypespec, IntTypespec (signed),
//     StringTypespec
//   - compiler emits zero errors, confirming this legal parenthesized
//     chain plus its assertions parse cleanly per IEEE 11.3.6
//
// Not checked (GTEST_SKIP, with a real reason):
//   - Whether a, b, and c actually equal 5 after the initial block runs.
//     HLC is a compiler/elaborator, not a simulator -- Net has no
//     "current value after execution" field (only getValue<T>() for a
//     *declaration-time* initializer, which none of a/b/c have here).
//     Confirming the three $display calls print exactly what the source
//     says they should print is a genuine execution trace, which this
//     environment cannot produce. If simulation/co-sim support is ever
//     added, replace this with a real check that all three assertions
//     print true.

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
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_task_call.h>
#include <hldb/variable.h>

namespace hlc {

class AssignInExprSimTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "11.3.6--assign_in_expr-sim.hlc"}); }
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

// --- module / nets ----

TEST_F(AssignInExprSimTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(AssignInExprSimTest, ModuleHasThreeIntVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  ASSERT_EQ(top->getVariables()->size(), 3u);
  const char *const names[3] = {"a", "b", "c"};
  for (uint32_t i = 0; i < 3u; ++i) {
    const hldb::Variable *const var = hldb::findByName<hldb::Variable>(names[i], top->getVariables());
    ASSERT_NE(var, nullptr) << "variable " << names[i];
    EXPECT_NE(var->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
  }
}

TEST_F(AssignInExprSimTest, ModuleHasNoNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->getNets(), nullptr);
}

// --- the 3-deep chain, terminating in the literal every assert expects ----

TEST_F(AssignInExprSimTest, InitialBlockHasFourStatements) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  ASSERT_NE(blk->getStmts(), nullptr);
  ASSERT_EQ(blk->getStmts()->size(), 4u);
}

TEST_F(AssignInExprSimTest, AssignmentChainBottomsOutAtConstantFive) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);

  const hldb::Assignment *const toA = any_cast<hldb::Assignment>(blk->getStmts()->at(0));
  ASSERT_NE(toA, nullptr);
  EXPECT_EQ(toA->getLhs<hldb::RefObj>()->getName(), "a");
  const hldb::Assignment *const toB = toA->getRhs<hldb::Assignment>();
  ASSERT_NE(toB, nullptr);
  EXPECT_EQ(toB->getLhs<hldb::RefObj>()->getName(), "b");
  const hldb::Assignment *const toC = toB->getRhs<hldb::Assignment>();
  ASSERT_NE(toC, nullptr);
  EXPECT_EQ(toC->getLhs<hldb::RefObj>()->getName(), "c");
  const hldb::Constant *const five = toC->getRhs<hldb::Constant>();
  ASSERT_NE(five, nullptr);
  EXPECT_EQ(five->getDecompile(), "5") << "every $display below expects this literal";
}

TEST_F(AssignInExprSimTest, EachVariableGetsItsOwnFiveEqualsAssertion) {
  const hldb::Begin *const blk = getInitialBody();
  ASSERT_NE(blk, nullptr);
  const char *const expectedNames[3] = {"a", "b", "c"};
  for (uint32_t i = 0; i < 3u; ++i) {
    const hldb::SysTaskCall *const disp = any_cast<hldb::SysTaskCall>(blk->getStmts()->at(1u + i));
    ASSERT_NE(disp, nullptr) << "statement index " << (1u + i);
    EXPECT_EQ(disp->getName(), "$display");
    ASSERT_NE(disp->getArguments(), nullptr);
    ASSERT_EQ(disp->getArguments()->size(), 2u);
    EXPECT_EQ(any_cast<hldb::Constant>(disp->getArguments()->at(0))->getValue(), ":assert: (5 == %d)");
    EXPECT_EQ(any_cast<hldb::RefObj>(disp->getArguments()->at(1))->getName(), expectedNames[i]);
  }
}

// --- design-level typespecs / compiler diagnostics ----

TEST_F(AssignInExprSimTest, DesignHasThreeTypespecs) {
  ASSERT_NE(m_design->getTypespecs(), nullptr);
  EXPECT_EQ(m_design->getTypespecs()->size(), 3u);
}

TEST_F(AssignInExprSimTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

// --- the actual point of the file: runtime values of a, b, c ----

TEST_F(AssignInExprSimTest, AllThreeVariablesEndUpEqualToFive) {
  GTEST_SKIP() << "The source asserts a == 5, b == 5, and c == 5 after 'a = (b = (c = 5))' "
                  "runs. HLC is a static compiler/elaborator: Variable exposes getValue<T>() "
                  "only for a declaration-time initializer (none of a/b/c have one here), not a "
                  "post-execution value -- there is no field anywhere in the object model that "
                  "records what a blocking assignment actually produced at runtime. Confirming "
                  "the three ':assert: (5 == %d)' $display calls print true is a genuine "
                  "simulation-only gap. If simulation/co-sim support is ever added, replace "
                  "this with a real check of the printed output.";
  // If the GTEST_SKIP() above is ever removed, this must still compile and
  // exercise a real, currently-failing check -- not silently pass.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const char *const names[3] = {"a", "b", "c"};
  for (uint32_t i = 0; i < 3u; ++i) {
    const hldb::Variable *const var = hldb::findByName<hldb::Variable>(names[i], top->getVariables());
    ASSERT_NE(var, nullptr) << "variable " << names[i];
    // Variable::getValue<T>() only ever exposes a declaration-time
    // initializer; none of a/b/c has one (all are assigned inside the
    // initial block), so this is null today -- there is no field anywhere
    // that captures what the assignment chain actually produced at
    // runtime.
    const hldb::Constant *const finalValue = var->getValue<hldb::Constant>();
    ASSERT_NE(finalValue, nullptr) << names[i] << "'s post-assignment runtime value is not captured anywhere";
    EXPECT_EQ(finalValue->getDecompile(), "5");
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
