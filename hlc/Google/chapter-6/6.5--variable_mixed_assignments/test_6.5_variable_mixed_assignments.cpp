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

// Tests for 6.5--variable_mixed_assignments.sv (tags: 6.5)
//   :should_fail_because: mixing procedural and continuous assignments is illegal
//   module top();
//     wire clk = 0;
//     int v;
//     assign v = 12;
//     always @(posedge clk) v <= ~v;
//   endmodule
//
// What to check and why (IEEE 1800-2023 10.3.2, p.248-249, checked before
// any test code was written):
//   "Variables can only be driven by one continuous assignment or by one
//   primitive output or module output. It shall be an error for a
//   variable driven by a continuous assignment or output to have an
//   initializer in the declaration OR ANY PROCEDURAL ASSIGNMENT."
//   Here "v" (declared "int", a variable-type keyword -- IEEE 1800-2023
//   6.7's net_type list never includes it) is driven by BOTH
//   "assign v = 12;" (continuous) AND "v <= ~v;" inside the always block
//   (procedural) -- exactly the combination the spec says "shall be an
//   error." This matches the file's own :should_fail_because: tag
//   precisely.
//
//   A prior version of this test (a) used hldb::Net/getNets() for "v",
//   and (b) had a test named Compiler_NoErrorsReported asserting
//   stats.nbError == 0, documented as "HLC does not reject mixing...".
//   Both of those encoded suspected/confirmed bugs as if they were
//   correct, passing behavior. This version instead: targets
//   hldb::Variable for "v" (real bug if it still resolves to Net), and
//   asserts errors ARE reported (real bug, currently failing, since HLC
//   evidently does not catch this) -- matching the discipline of
//   "generate the test case as it should be, not as it happens to pass."
//
// What is checked:
//   - module top has exactly 1 Net, "clk" (wire is a real net-type
//     keyword, IEEE 1800-2023 6.7 -- this classification is correct and
//     unaffected by the bug being documented here), initial value "0"
//   - module top has exactly 1 Variable, "v" (int -- must NOT resolve to
//     a Net); "v" has no declaration-time initializer
//   - exactly 1 ContAssign: LHS RefObj "v" resolves via
//     getActual<hldb::Variable>() (not Net) to that Variable; RHS
//     Constant "12"
//   - exactly 1 Always process: EventControl(posedge clk) whose operand
//     RefObj "clk" resolves to the clk Net; body is a non-blocking
//     Assignment (v <= ~v) whose LHS RefObj "v" resolves via
//     getActual<hldb::Variable>() to the same Variable, RHS is an
//     Operation (vpiBitNegOp) over RefObj "v"
//   - THE POINT OF THIS FILE: the compiler should report at least one
//     error for the illegal continuous+procedural mix on "v", per IEEE
//     1800-2023 10.3.2 quoted above -- this is a real, non-skipped,
//     currently-failing assertion, not a passing "no errors" check
//
// What is NOT checked and why:
//   - none: every corner above is fully structural and checkable without
//     simulation.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assignment.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class VariableMixedAssignmentsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.5--variable_mixed_assignments.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

// --- existence: module, clk as a real Net, v as a Variable -----------------

TEST_F(VariableMixedAssignmentsTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(VariableMixedAssignmentsTest, ClkIsANetWireInitializedToZero) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_EQ(top->getNets()->size(), 1u) << "only 'clk' should be a Net; 'v' should be a Variable";
  const hldb::Net *const clk = hldb::findByName<hldb::Net>("clk", top->getNets());
  ASSERT_NE(clk, nullptr);
  EXPECT_EQ(clk->getNetType(), vpiWire) << "'wire' is a real IEEE 1800-2023 6.7 net-type keyword";
  ASSERT_NE(clk->getValue<hldb::Constant>(), nullptr);
  EXPECT_EQ(clk->getValue<hldb::Constant>()->getDecompile(), "0");
}

TEST_F(VariableMixedAssignmentsTest, VIsAVariableNotANet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr)
      << "'int v' should be a Variable; if this is null, hldb likely misclassified it as a Net";
  ASSERT_EQ(top->getVariables()->size(), 1u);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("v", top->getVariables());
  ASSERT_NE(v, nullptr);
  EXPECT_NE(v->getTypespec<hldb::RefTypespec>()->getActual<hldb::IntTypespec>(), nullptr);
  EXPECT_EQ(v->getValue<hldb::Constant>(), nullptr) << "'int v;' has no inline initializer of its own";
}

// --- shape: continuous assignment to v ------------------------------------

TEST_F(VariableMixedAssignmentsTest, ContAssignLhsResolvesToVariableVNotNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
  const hldb::ContAssign *const ca = top->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "v");
  EXPECT_EQ(lhs->getActual<hldb::Net>(), nullptr) << "'v' must not resolve to a Net";
  EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr) << "'v' should resolve to the Variable";
}

TEST_F(VariableMixedAssignmentsTest, ContAssignRhsIsConstantTwelve) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Constant *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "12");
}

// --- shape: procedural assignment to the SAME v inside always -------------

TEST_F(VariableMixedAssignmentsTest, AlwaysBlockIsPosedgeClkThenNonBlockingAssignToV) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Always *const always = any_cast<hldb::Always>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  EXPECT_EQ(always->getAlwaysType(), vpiAlways);

  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  const hldb::Operation *const cond = ec->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  EXPECT_EQ(cond->getOpType(), vpiPosedgeOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 1u);
  const hldb::RefObj *const clkRef = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  ASSERT_NE(clkRef, nullptr);
  EXPECT_EQ(clkRef->getName(), "clk");
  EXPECT_NE(clkRef->getActual<hldb::Net>(), nullptr) << "'clk' operand should resolve to the real clk Net";

  const hldb::Assignment *const assign = ec->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr);
  EXPECT_FALSE(assign->getBlocking()) << "'v <= ~v' is non-blocking";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "v");
  EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr)
      << "the procedural assignment's LHS 'v' should resolve to the SAME Variable object the "
         "continuous assignment above targets";

  const hldb::Operation *const neg = assign->getRhs<hldb::Operation>();
  ASSERT_NE(neg, nullptr);
  EXPECT_EQ(neg->getOpType(), vpiBitNegOp);
  ASSERT_NE(neg->getOperands(), nullptr);
  ASSERT_EQ(neg->getOperands()->size(), 1u);
  EXPECT_EQ(any_cast<hldb::RefObj>(neg->getOperands()->at(0))->getName(), "v");
}

// --- the actual point of the file: this mix should be a compiler error ----

TEST_F(VariableMixedAssignmentsTest, CompilerShouldRejectMixedContinuousAndProceduralAssignmentButDoesNot) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GT(stats.nbFatal + stats.nbSyntax + stats.nbError, 0)
      << "IEEE 1800-2023 10.3.2: 'it shall be an error for a variable driven by a continuous "
         "assignment ... to have ... any procedural assignment' -- 'v' has both here, matching "
         "this file's own :should_fail_because: tag -- HLC currently accepts it with zero "
         "diagnostics";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
