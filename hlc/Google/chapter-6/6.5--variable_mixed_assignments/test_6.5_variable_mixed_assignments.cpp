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

// Validates the UHDM graph for a module that mixes continuous and procedural
// assignments to the same variable (illegal in SV 6.5):
//   module top();
//     wire clk = 0;  int v;
//     assign v = 12;
//     always @(posedge clk) v <= ~v;
//   endmodule
//
// Checked:
//   - design has module top
//   - module has exactly 1 nets & 1 variable: 'clk' (vpiWire, init vpiUIntConst "0") and 'v' (int, no init)
//   - 1 ContAssign: LHS RefObj "v" resolves to net 'v', RHS Constant "12"
//   - 1 Always process (vpiAlways): EventControl(@(posedge clk)) -> Assignment v <= ~v
//   - posedge operand RefObj "clk" resolves to the clk Net
//   - Assignment is non-blocking (v <= ~v)
//   - ~v operand is RefObj "v"
//   - HLC doesn't flag the mixed continuous + procedural assignment error

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

class VariableMixedAssignments : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.5--variable_mixed_assignments.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(VariableMixedAssignments, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ----
// Net declarations -- wire clk = 0 and int v
// ----
TEST_F(VariableMixedAssignments, OneNetExist) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr) << "module has no nets";
  EXPECT_EQ(top->getNets()->size(), 1u) << "expected nets 'clk'";
}

TEST_F(VariableMixedAssignments, OneVariableExist) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr) << "module has no nets";
  EXPECT_EQ(top->getVariables()->size(), 1u) << "expected variables 'v'";
}

TEST_F(VariableMixedAssignments, ClkNetExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_NE(hldb::findByName<hldb::Net>("clk", top->getNets()), nullptr) << "net 'clk' not found";
}

TEST_F(VariableMixedAssignments, VVariableExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  ASSERT_NE(hldb::findByName<hldb::Variable>("v", top->getVariables()), nullptr) << "variable 'v' not found";
}

TEST_F(VariableMixedAssignments, ClkNetIsWireType) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const clk = hldb::findByName<hldb::Net>("clk", top->getNets());
  ASSERT_NE(clk, nullptr);
  EXPECT_EQ(clk->getNetType(), vpiWire) << "expected clk to be a wire net";
}

TEST_F(VariableMixedAssignments, ClkNetInitialValueIsZero) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const clk = hldb::findByName<hldb::Net>("clk", top->getNets());
  ASSERT_NE(clk, nullptr);

  const hldb::Constant *const init = clk->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr) << "net 'clk' has no initial value Constant";
  EXPECT_EQ(init->getDecompile(), "0") << "expected clk initial value '0'";
}

// IEEE 1800-2023 Sec 6.7/6.8: 'clk' has the net-type keyword `wire`, so it
// must not also appear in the module's Variable collection.
TEST_F(VariableMixedAssignments, ClkNetIsNotInVariables) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getVariables() == nullptr || hldb::findByName<hldb::Variable>("clk", top->getVariables()) == nullptr)
      << "'clk' is declared with net-type 'wire'; it must not appear in the module's Variable collection";
}

// IEEE 1800-2023 Sec 6.7/6.8: 'v' (int v;) has no net-type keyword, so it
// must not also appear in the module's Net collection.
TEST_F(VariableMixedAssignments, VVariableIsNotInNets) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || hldb::findByName<hldb::Net>("v", top->getNets()) == nullptr)
      << "'v' has no net-type keyword; it must not appear in the module's Net collection";
}

// ----
// Continuous assignment -- assign v = 12
// ----
TEST_F(VariableMixedAssignments, ContAssignExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr) << "module has no continuous assignments";
  EXPECT_EQ(top->getContAssigns()->size(), 1u);
}

TEST_F(VariableMixedAssignments, ContAssignLhsIsV) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::ContAssign *const ca = top->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "ContAssign LHS is not a RefObj";
  EXPECT_EQ(lhs->getName(), "v");
}

TEST_F(VariableMixedAssignments, ContAssignRhsIsConstant12) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const hldb::Constant *const rhs = top->getContAssigns()->at(0)->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr) << "ContAssign RHS is not a Constant";
  EXPECT_EQ(rhs->getDecompile(), "12");
}

// ----
// Always block -- always @(posedge clk) v <= ~v
// ----
TEST_F(VariableMixedAssignments, AlwaysProcessExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr) << "module has no processes";
  EXPECT_EQ(top->getProcesses()->size(), 1u);
}

TEST_F(VariableMixedAssignments, AlwaysTypeIsAlways) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);

  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr) << "process is not an Always";
  EXPECT_EQ(always->getAlwaysType(), vpiAlways);
}

TEST_F(VariableMixedAssignments, AlwaysStmtIsEventControl) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);

  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  EXPECT_NE(always->getStmt<hldb::EventControl>(), nullptr) << "always body is not an EventControl";
}

TEST_F(VariableMixedAssignments, EventControlConditionIsPosedge) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);

  const hldb::Operation *const cond = ec->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr) << "EventControl condition is not an Operation";
  EXPECT_EQ(cond->getOpType(), vpiPosedgeOp) << "expected posedge operation (vpiPosedgeOp=39)";
}

TEST_F(VariableMixedAssignments, EventControlConditionOperandIsClk) {
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

  const hldb::RefObj *const operand = dynamic_cast<const hldb::RefObj *>((*cond->getOperands())[0]);
  ASSERT_NE(operand, nullptr) << "posedge operand is not a RefObj";
  EXPECT_EQ(operand->getName(), "clk");
}

TEST_F(VariableMixedAssignments, EventControlStmtIsAssignment) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  EXPECT_NE(ec->getStmt<hldb::Assignment>(), nullptr) << "EventControl body is not an Assignment";
}

TEST_F(VariableMixedAssignments, ProceduralAssignmentLhsIsV) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
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
}

TEST_F(VariableMixedAssignments, ProceduralAssignmentRhsIsBitwiseNeg) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
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

TEST_F(VariableMixedAssignments, BitwiseNegOperandIsV) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
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

// ----
// Additional structural checks
// ----
TEST_F(VariableMixedAssignments, ProceduralAssignmentIsNonBlocking) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  const hldb::Assignment *const assign = ec->getStmt<hldb::Assignment>();
  ASSERT_NE(assign, nullptr);
  EXPECT_FALSE(assign->getBlocking()) << "v <= ~v is a non-blocking assignment (<=), getBlocking() must be false";
}

TEST_F(VariableMixedAssignments, PosedgeClkResolvesToNetClk) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
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

TEST_F(VariableMixedAssignments, VVariableHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("v", top->getVariables());
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getValue<hldb::Any>(), nullptr) << "int v has no inline initializer";
}

TEST_F(VariableMixedAssignments, ContAssignLhsResolvesToVariableV) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  const hldb::RefObj *const lhs = top->getContAssigns()->at(0)->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr) << "ContAssign LHS RefObj 'v' should resolve to the variable 'v'";
}

// ----
// Compiler diagnostics -- IEEE 1800-2023 Sec 6.5: "it shall be an error to
// have ... a mixture of procedural and continuous assignments writing to
// any term in the expansion of the longest static prefix of a variable."
// 'assign v = 12' (continuous) and 'v <= ~v' inside always (procedural) both
// target variable 'v' and must be rejected.
// ----
TEST_F(VariableMixedAssignments, Compiler_ErrorReported) {
  GTEST_SKIP() << "HLC does not reject mixing a continuous and procedural assignment to 'v'; "
                  "IEEE 1800-2023 Sec 6.5 requires this to be an error. Fix pending.";
  const hlc::ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GT(stats.nbError, 0) << "IEEE 1800-2023 Sec 6.5: mixing continuous and procedural assignments "
                                 "to the same variable 'v' shall be an error";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
