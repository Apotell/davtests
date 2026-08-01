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

// Tests for 6.5--variable_mixed_assignments.sv (tags: 6.5,
// :should_fail_because: a variable driven by both a continuous assignment
// and a procedural assignment is illegal)
//   module top();
//     wire clk = 0;  int v;
//     assign v = 12;
//     always @(posedge clk) v <= ~v;
//   endmodule
//
// What to check and why (IEEE 1800-2023 10.3.2 "Continuous assignment",
// p.248-249, checked before any test code was written):
//   "Variables can only be driven by one continuous assignment or by one
//   or more procedural drivers; they cannot be driven by both a
//   continuous assignment and a procedural driver at the same time."
//   This file gives "v" BOTH a continuous assignment ("assign v = 12;")
//   AND a procedural assignment ("v <= ~v;" inside an always block) --
//   exactly the construct this sentence forbids. "clk" is declared
//   "wire", which IS a net-type keyword (IEEE 1800-2023 6.7), so "clk"
//   correctly stays a Net -- "clk" itself and its use as an unrelated
//   posedge sensitivity trigger are not part of the bug. "int v" is not
//   a net-type keyword, so "v" must be a Variable per 6.8, and it's
//   specifically that double-driven Variable that is illegal.
//
//   A prior version of this test used hldb::Net/getNets() for BOTH "clk"
//   and "v" (asserting "2 nets"), and ended with a test asserting
//   nbError == 0 as "Compiler_NoErrorsReported" -- treating this spec
//   violation as correct, passing behavior. This version fixes "v" to
//   Variable (real bug if it's still a Net) and replaces the old
//   "no errors" test with one that documents the actual bug: the
//   compiler currently reports NO error for a construct the spec says
//   "cannot" happen.
//
// What is checked:
//   - module top exists, has exactly 1 Net "clk" (vpiWire, initial value
//     Constant "0") and exactly 1 Variable "v" (no declaration-time
//     initializer)
//   - exactly 1 ContAssign: LHS RefObj "v" resolves via
//     getActual<hldb::Variable>() (NOT Net) to that same Variable, RHS is
//     Constant "12"
//   - exactly 1 Always process (vpiAlways): EventControl(@(posedge clk))
//     whose condition is a posedge Operation over RefObj "clk" (resolving
//     to the clk Net) -> Assignment "v <= ~v" (non-blocking; LHS RefObj
//     "v" resolves via getActual<hldb::Variable>(), NOT Net; RHS is a
//     bitwise-negation Operation whose single operand is RefObj "v")
//   - THE POINT OF THIS FILE: the compiler currently reports zero errors
//     for double-driving "v" via both a continuous and a procedural
//     assignment, which IEEE 1800-2023 10.3.2 says "cannot" be done --
//     a real, non-skipped, currently-failing assertion documenting this
//     bug (not yet personally verified by re-running with the check
//     removed, so no GTEST_SKIP() is added per the established gating
//     rule)
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
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_obj.h>
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

TEST_F(VariableMixedAssignmentsTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

// ---------------------------------------------------------------------------
// Declarations -- wire clk = 0 (Net) and int v (Variable, NOT Net)
// ---------------------------------------------------------------------------
TEST_F(VariableMixedAssignmentsTest, ModuleHasOneNetClk) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr) << "'wire clk' is a net-type keyword (IEEE 1800-2023 6.7)";
  ASSERT_EQ(top->getNets()->size(), 1u) << "expected exactly 1 net: 'clk'";
}

TEST_F(VariableMixedAssignmentsTest, ClkNetExists) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(hldb::findByName<hldb::Net>("clk", top->getNets()), nullptr) << "net 'clk' not found";
}

TEST_F(VariableMixedAssignmentsTest, ClkNetIsWireType) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const clk = hldb::findByName<hldb::Net>("clk", top->getNets());
  ASSERT_NE(clk, nullptr);
  EXPECT_EQ(clk->getNetType(), vpiWire);
}

TEST_F(VariableMixedAssignmentsTest, ClkNetInitialValueIsZero) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const clk = hldb::findByName<hldb::Net>("clk", top->getNets());
  ASSERT_NE(clk, nullptr);
  const hldb::Constant *const init = clk->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "net 'clk' has no initial value Constant";
  EXPECT_EQ(init->getDecompile(), "0");
}

TEST_F(VariableMixedAssignmentsTest, ModuleHasOneVariableV) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr)
      << "'int v' should be represented as a Variable (IEEE 1800-2023 6.8); if this is null, "
         "hldb likely misclassified 'v' as a Net instead";
  ASSERT_EQ(top->getVariables()->size(), 1u);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("v", top->getVariables());
  ASSERT_NE(v, nullptr) << "Variable 'v' not found";
}

TEST_F(VariableMixedAssignmentsTest, VariableHasNoInitialValue) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("v", top->getVariables());
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getValue<hldb::Any>(), nullptr) << "'int v' has no inline initializer";
}

// ---------------------------------------------------------------------------
// Continuous assignment -- assign v = 12
// ---------------------------------------------------------------------------
TEST_F(VariableMixedAssignmentsTest, ContAssignExists) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr) << "module has no continuous assignments";
  EXPECT_EQ(top->getContAssigns()->size(), 1u);
}

TEST_F(VariableMixedAssignmentsTest, ContAssignLhsResolvesToVariableVNotNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  const hldb::RefObj *const lhs = top->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "ContAssign LHS is not a RefObj";
  EXPECT_EQ(lhs->getName(), "v");
  EXPECT_EQ(lhs->getActual<hldb::Net>(), nullptr)
      << "'v' must NOT resolve to a Net -- 'int' is a variable-type keyword (IEEE 1800-2023 6.7 "
         "does not list it)";
  EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr)
      << "ContAssign LHS RefObj 'v' should resolve to the Variable 'v'";
}

TEST_F(VariableMixedAssignmentsTest, ContAssignRhsIsConstant12) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  const hldb::Constant *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "ContAssign RHS is not a Constant";
  EXPECT_EQ(rhs->getDecompile(), "12");
}

// ---------------------------------------------------------------------------
// Always block -- always @(posedge clk) v <= ~v
// ---------------------------------------------------------------------------
TEST_F(VariableMixedAssignmentsTest, AlwaysProcessExists) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr) << "module has no processes";
  EXPECT_EQ(top->getProcesses()->size(), 1u);
}

TEST_F(VariableMixedAssignmentsTest, AlwaysTypeIsAlways) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr) << "process is not an Always";
  EXPECT_EQ(always->getAlwaysType(), vpiAlways);
}

TEST_F(VariableMixedAssignmentsTest, AlwaysStmtIsEventControl) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  EXPECT_NE(always->getStmt<hldb::EventControl>(), nullptr) << "always body is not an EventControl";
}

TEST_F(VariableMixedAssignmentsTest, EventControlConditionIsPosedge) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  const hldb::Operation *const cond = ec->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "EventControl condition is not an Operation";
  EXPECT_EQ(cond->getOpType(), vpiPosedgeOp) << "expected posedge operation (vpiPosedgeOp=39)";
}

TEST_F(VariableMixedAssignmentsTest, EventControlConditionOperandIsClk) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  const hldb::Operation *const cond = ec->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 1u);
  const hldb::RefObj *const operand = dynamic_cast<const hldb::RefObj *>((*cond->getOperands())[0]);
  ASSERT_NE(operand, nullptr) << "posedge operand is not a RefObj";
  EXPECT_EQ(operand->getName(), "clk");
}

TEST_F(VariableMixedAssignmentsTest, PosedgeClkResolvesToNetClk) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  const hldb::Operation *const cond = ec->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  ASSERT_NE(cond->getOperands(), nullptr);
  const hldb::RefObj *const clkRef = dynamic_cast<const hldb::RefObj *>((*cond->getOperands())[0]);
  ASSERT_NE(clkRef, nullptr);
  EXPECT_NE(clkRef->getActual<hldb::Net>(), nullptr) << "posedge operand RefObj 'clk' should resolve to the clk Net";
}

TEST_F(VariableMixedAssignmentsTest, EventControlStmtIsAssignment) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  EXPECT_NE(ec->getStmt<hldb::Assignment>(), nullptr) << "EventControl body is not an Assignment";
}

TEST_F(VariableMixedAssignmentsTest, ProceduralAssignmentIsNonBlocking) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  const hldb::Assignment *const assign = ec->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr);
  EXPECT_FALSE(assign->getBlocking()) << "v <= ~v is a non-blocking assignment (<=), getBlocking() must be false";
}

TEST_F(VariableMixedAssignmentsTest, ProceduralAssignmentLhsResolvesToVariableVNotNet) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  const hldb::Assignment *const assign = ec->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr);
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "procedural assignment LHS is not a RefObj";
  EXPECT_EQ(lhs->getName(), "v");
  EXPECT_EQ(lhs->getActual<hldb::Net>(), nullptr) << "'v' must NOT resolve to a Net";
  EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr) << "'v' should resolve to the Variable 'v'";
}

TEST_F(VariableMixedAssignmentsTest, ProceduralAssignmentRhsIsBitwiseNeg) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  const hldb::Assignment *const assign = ec->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr);
  const hldb::Operation *const rhs = assign->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "procedural assignment RHS is not an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiBitNegOp) << "expected bitwise negation (vpiBitNegOp=4)";
}

TEST_F(VariableMixedAssignmentsTest, BitwiseNegOperandIsV) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  const hldb::Assignment *const assign = ec->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr);
  const hldb::Operation *const neg = assign->getRhs<hldb::Operation>();
  ASSERT_NE(neg, nullptr);
  ASSERT_NE(neg->getOperands(), nullptr);
  ASSERT_EQ(neg->getOperands()->size(), 1u);
  const hldb::RefObj *const operand = dynamic_cast<const hldb::RefObj *>((*neg->getOperands())[0]);
  ASSERT_NE(operand, nullptr) << "~v operand is not a RefObj";
  EXPECT_EQ(operand->getName(), "v");
}

// ---------------------------------------------------------------------------
// The actual point of this file: v is driven by BOTH assign and always
// ---------------------------------------------------------------------------
TEST_F(VariableMixedAssignmentsTest, CompilerShouldRejectMixedContinuousAndProceduralAssignmentButDoesNot) {
  const hlc::ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GT(stats.nbFatal + stats.nbSyntax + stats.nbError, 0)
      << "IEEE 1800-2023 10.3.2: 'Variables can only be driven by one continuous assignment or "
         "by one or more procedural drivers; they cannot be driven by both a continuous "
         "assignment and a procedural driver at the same time.' 'v' is driven by 'assign v = 12;' "
         "AND 'v <= ~v;' inside 'always @(posedge clk)' -- HLC currently reports zero errors for "
         "this, which is a compiler bug";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
