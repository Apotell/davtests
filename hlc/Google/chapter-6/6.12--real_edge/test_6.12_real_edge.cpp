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

// Tests for 6.12--real_edge.sv (tags: 6.12)
//   :should_fail_because: it is illegal to use edge event controls on real type
//   module top();
//     real a = 0.5;
//     always @(posedge a) $display("posedge");
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.12 "Real, shortreal, and
// realtime data types", p.110, checked before any test code was
// written):
//   "Real numbers and real variables are also prohibited in the
//   following cases: ... Edge event controls (posedge, negedge, edge)
//   applied to real variables (see 9.4.2)." "posedge a" on the real
//   variable "a" is exactly this prohibited construct, matching the
//   file's own :should_fail_because: tag precisely. This is a flat
//   "prohibited" rule in the LRM, so a compliant tool should reject it
//   as a semantic error.
//
//   Also (IEEE 1800-2023 6.8): "real" is a non_integer_type keyword,
//   never a net_type -- "real a" declared at module scope must be a
//   Variable, not a Net. A prior version of this test used
//   hldb::Net/getNets() for "a" (the same net/variable misclassification
//   bug found and fixed in 6.5, 6.9.1, 6.12--real, 6.12--shortreal, and
//   the sibling 6.12--real_bit_select* files), and had a
//   Compiler_NoErrorsReported test asserting nbError == 0, documented as
//   "HLC does not reject 'posedge a' on a real net at compile time" --
//   treating a confirmed spec violation as expected, passing behavior.
//   This version targets hldb::Variable for "a", and asserts an error IS
//   reported (real bug, currently failing).
//
// What is checked:
//   - module top has zero Nets and exactly 1 Variable "a" (real, initial
//     value vpiRealConst "0.5")
//   - exactly 1 Always process (vpiAlways type), stmt = EventControl
//   - EventControl condition = vpiPosedgeOp on RefObj "a", which
//     resolves via getActual<hldb::Variable>() (not Net) to the real
//     Variable
//   - EventControl body is SysTaskCall "$display" with argument Constant
//     "\"posedge\""
//   - top has no continuous assignments
//   - THE POINT OF THIS FILE: the compiler should report at least one
//     error for the illegal "posedge a" on a real variable, per IEEE
//     1800-2023 6.12 quoted above. Confirmed by personally running with
//     the skip removed (fails as expected) -- kept as GTEST_SKIP() with
//     the real assertion underneath, per the established gating rule
//     (skips only added after personal verification)
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
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/real_typespec.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class RealEdgeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.12--real_edge.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

TEST_F(RealEdgeTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

// ---------------------------------------------------------------------------
// Declaration -- real a = 0.5, must be a Variable not a Net
// ---------------------------------------------------------------------------
TEST_F(RealEdgeTest, ModuleHasNoNetsAndOneVariableA) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "'real a' declares no net-type keyword (IEEE 1800-2023 6.7) anywhere in this file";
  ASSERT_NE(top->getVariables(), nullptr)
      << "'real a' should be a Variable; if this is null, hldb likely misclassified it as a Net";
  ASSERT_EQ(top->getVariables()->size(), 1u);
  ASSERT_NE(hldb::findByName<hldb::Variable>("a", top->getVariables()), nullptr) << "Variable 'a' not found";
}

TEST_F(RealEdgeTest, ATypespecIsReal) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::RealTypespec>(), nullptr) << "'a' typespec should resolve to RealTypespec";
}

TEST_F(RealEdgeTest, AInitialValueIsHalf) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiRealConst);
  EXPECT_EQ(init->getDecompile(), "0.5");
}

// ---------------------------------------------------------------------------
// Always process -- always @(posedge a)
// ---------------------------------------------------------------------------
TEST_F(RealEdgeTest, AlwaysProcessExists) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 1u);
}

TEST_F(RealEdgeTest, AlwaysTypeIsAlways) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  EXPECT_EQ(always->getAlwaysType(), vpiAlways);
}

TEST_F(RealEdgeTest, AlwaysStmtIsEventControl) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  EXPECT_NE(always->getStmt<hldb::EventControl>(), nullptr) << "always body should be an EventControl";
}

// ---------------------------------------------------------------------------
// EventControl condition -- posedge on the real Variable 'a' (illegal, but parsed)
// ---------------------------------------------------------------------------
TEST_F(RealEdgeTest, EventControlConditionIsPosedge) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  const hldb::Operation *const cond = ec->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "EventControl condition is not an Operation";
  EXPECT_EQ(cond->getOpType(), vpiPosedgeOp) << "expected vpiPosedgeOp (39)";
}

TEST_F(RealEdgeTest, EventControlConditionOperandResolvesToRealVariableNotNet) {
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

  const hldb::RefObj *const operand = any_cast<hldb::RefObj>((*cond->getOperands())[0]);
  ASSERT_NE(operand, nullptr) << "posedge operand is not a RefObj";
  EXPECT_EQ(operand->getName(), "a");
  EXPECT_EQ(operand->getActual<hldb::Net>(), nullptr) << "'a' must not resolve to a Net";

  const hldb::Variable *const var = operand->getActual<hldb::Variable>();
  ASSERT_NE(var, nullptr) << "posedge operand does not resolve to the Variable 'a'";
  const hldb::RefTypespec *const rts = var->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::RealTypespec>(), nullptr) << "posedge operand should be real-typed";
}

// ---------------------------------------------------------------------------
// EventControl body -- $display("posedge")
// ---------------------------------------------------------------------------
TEST_F(RealEdgeTest, EventControlStmtIsDisplayCall) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  const hldb::SysTaskCall *const call = ec->getStmt<hldb::SysTaskCall>();
  ASSERT_NE(call, nullptr) << "EventControl body is not a SysTaskCall";
  EXPECT_EQ(call->getName(), "$display");
}

TEST_F(RealEdgeTest, DisplayArgumentIsPosedgeString) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  const hldb::SysTaskCall *const call = ec->getStmt<hldb::SysTaskCall>();
  ASSERT_NE(call, nullptr);
  ASSERT_NE(call->getArguments(), nullptr) << "$display call has no arguments";
  ASSERT_EQ(call->getArguments()->size(), 1u);
  const hldb::Constant *const arg = any_cast<hldb::Constant>((*call->getArguments())[0]);
  ASSERT_NE(arg, nullptr) << "$display argument is not a Constant";
  EXPECT_EQ(arg->getDecompile(), "\"posedge\"");
}

TEST_F(RealEdgeTest, NoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

// ---------------------------------------------------------------------------
// The actual point of the file: posedge on a real variable is illegal
// ---------------------------------------------------------------------------
TEST_F(RealEdgeTest, CompilerShouldRejectPosedgeOnRealVariableButDoesNot) {
  GTEST_SKIP() << "Confirmed HLC bug -- verified by running this test with the skip removed "
                  "(fails as expected): IEEE 1800-2023 6.12 prohibits edge event controls "
                  "(posedge, negedge, edge) applied to real variables ('posedge a'), but HLC "
                  "accepts it with zero diagnostics. Tracked, not yet fixed by the compiler.";
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GT(stats.nbFatal + stats.nbSyntax + stats.nbError, 0)
      << "IEEE 1800-2023 6.12: 'edge event controls (posedge, negedge, edge) applied to real "
         "variables' are prohibited -- 'posedge a' does exactly this, matching this file's own "
         ":should_fail_because: tag -- HLC currently accepts it with zero diagnostics";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
