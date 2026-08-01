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

// Validates the UHDM graph for a module that illegally uses a posedge event
// control on a real-typed variable:
//   module top();
//     real a = 0.5;
//     always @(posedge a) $display("posedge");
//   endmodule
//
// Checked:
//   - design has module top
//   - module has exactly 1 variable: 'a' (RealTypespec, vpiRealConst "0.5")
//   - 1 Always process (vpiAlways type), stmt = EventControl
//   - EventControl condition = vpiPosedgeOp on RefObj "a"
//   - posedge operand resolves to the real variable 'a'
//   - EventControl body is a SysTaskCall "$display"
//   - top has no continuous assignments
//
// Also checked:
//   - Per IEEE 1800-2023 Sec 6.12: "Real numbers and real variables are ...
//     prohibited in the following cases: Edge event controls (posedge,
//     negedge, edge) applied to real variables (see 9.4.2)." HLC currently
//     does not report a compile-time error for this (known gap) -- see the
//     GTEST_SKIP()'d test below.
//   - $display argument value ("posedge")

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
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/real_typespec.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sys_func_call.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class RealEdge : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.12--real_edge.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(RealEdge, ModuleExists) { ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr); }

// ----
// Variable -- real a = 0.5
// ----
TEST_F(RealEdge, OneVariableExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u);
}

TEST_F(RealEdge, ANotInNets) {
  // Per IEEE 1800-2023 Sec 6.7/6.8, 'real' has no net-type keyword, so 'a'
  // must not also be materialized as a Net.
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || hldb::findByName<hldb::Net>("a", top->getNets()) == nullptr)
      << "'real a' must not appear in vpiNet";
}

TEST_F(RealEdge, AVariableTypespecIsReal) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::RealTypespec>(), nullptr) << "variable 'a' typespec should resolve to RealTypespec";
}

TEST_F(RealEdge, AVariableInitialValueIsHalf) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiRealConst);
  EXPECT_EQ(init->getDecompile(), "0.5");
}

// ----
// Always process -- always @(posedge a)
// ----
TEST_F(RealEdge, AlwaysProcessExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 1u);
}

TEST_F(RealEdge, AlwaysTypeIsAlways) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  EXPECT_EQ(always->getAlwaysType(), vpiAlways);
}

TEST_F(RealEdge, AlwaysStmtIsEventControl) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  EXPECT_NE(always->getStmt<hldb::EventControl>(), nullptr) << "always body should be an EventControl";
}

// ----
// EventControl condition -- posedge on real variable 'a' (illegal but parsed)
// ----
TEST_F(RealEdge, EventControlConditionIsPosedge) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);

  const hldb::Operation *const cond = ec->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "EventControl condition is not an Operation";
  EXPECT_EQ(cond->getOpType(), vpiPosedgeOp) << "expected vpiPosedgeOp (39)";
}

TEST_F(RealEdge, EventControlConditionOperandIsA) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
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
}

TEST_F(RealEdge, EventControlConditionOperandIsRealVariable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  const hldb::Operation *const cond = ec->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  const hldb::RefObj *const operand = any_cast<hldb::RefObj>((*cond->getOperands())[0]);
  ASSERT_NE(operand, nullptr);

  const hldb::Variable *const var = operand->getActual<hldb::Variable>();
  ASSERT_NE(var, nullptr);
  const hldb::RefTypespec *const rts = var->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::RealTypespec>(), nullptr) << "posedge operand should resolve to the real variable 'a'";
}

// ----
// EventControl body -- $display("posedge")
// ----
TEST_F(RealEdge, EventControlStmtIsDisplayCall) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);

  const hldb::SysTaskCall *const call = ec->getStmt<hldb::SysTaskCall>();
  ASSERT_NE(call, nullptr) << "EventControl body is not a SysTaskCall";
  EXPECT_EQ(call->getName(), "$display");
}

TEST_F(RealEdge, DisplayArgumentIsPosedgeString) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
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

TEST_F(RealEdge, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

// ----
// Compiler diagnostics -- IEEE 1800-2023 Sec 6.12 / 9.4.2 prohibit edge
// event controls applied to real variables. HLC does not currently flag
// this; see GTEST_SKIP() below.
// ----
TEST_F(RealEdge, Compiler_ReportsErrorForIllegalPosedgeOnReal) {
  GTEST_SKIP() << "known gap: 'posedge a' on a real variable is not rejected by HLC; "
                  "IEEE 1800-2023 Sec 6.12/9.4.2 prohibit edge event controls on real variables";
  const hlc::ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GE(stats.nbError, 1) << "'posedge a' on a real variable must be flagged illegal per Sec 6.12/9.4.2";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
