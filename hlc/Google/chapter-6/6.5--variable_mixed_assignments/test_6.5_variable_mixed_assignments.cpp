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
//   - design has module work@top
//   - module has exactly 2 nets: 'clk' (vpiWire, init vpiUIntConst "0") and 'v' (int, no init)
//   - 1 ContAssign: LHS RefObj "v" resolves to net 'v', RHS Constant "12"
//   - 1 Always process (vpiAlways): EventControl(@(posedge clk)) → Assignment v <= ~v
//   - posedge operand RefObj "clk" resolves to the clk Net
//   - Assignment is non-blocking (v <= ~v)
//   - ~v operand is RefObj "v"
//
// Not checked:
//   - Surelog doesn't flag the mixed continuous + procedural assignment error

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/always.h>
#include <uhdm/assignment.h>
#include <uhdm/constant.h>
#include <uhdm/cont_assign.h>
#include <uhdm/design.h>
#include <uhdm/event_control.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/operation.h>
#include <uhdm/process_stmt.h>
#include <uhdm/ref_obj.h>
#include <uhdm/vpi_user.h>

namespace SURELOG {

class VariableMixedAssignments : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.5--variable_mixed_assignments.hlc"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

TEST_F(VariableMixedAssignments, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Net declarations — wire clk = 0 and int v
// ---------------------------------------------------------------------------
TEST_F(VariableMixedAssignments, TwoNetsExist) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr) << "module has no nets";
  EXPECT_EQ(top->getNets()->size(), 2u) << "expected nets 'clk' and 'v'";
}

TEST_F(VariableMixedAssignments, ClkNetExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_NE(uhdm::findByName<uhdm::Net>("clk", top->getNets()), nullptr)
      << "net 'clk' not found";
}

TEST_F(VariableMixedAssignments, VNetExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  ASSERT_NE(uhdm::findByName<uhdm::Net>("v", top->getNets()), nullptr)
      << "net 'v' not found";
}

TEST_F(VariableMixedAssignments, ClkNetIsWireType) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const clk = uhdm::findByName<uhdm::Net>("clk", top->getNets());
  ASSERT_NE(clk, nullptr);
  EXPECT_EQ(clk->getNetType(), vpiWire) << "expected clk to be a wire net";
}

TEST_F(VariableMixedAssignments, ClkNetInitialValueIsZero) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const clk = uhdm::findByName<uhdm::Net>("clk", top->getNets());
  ASSERT_NE(clk, nullptr);

  const uhdm::Constant *const init = clk->getValue<uhdm::Constant>();
  ASSERT_NE(init, nullptr) << "net 'clk' has no initial value Constant";
  EXPECT_EQ(init->getDecompile(), "0") << "expected clk initial value '0'";
}

// ---------------------------------------------------------------------------
// Continuous assignment — assign v = 12
// ---------------------------------------------------------------------------
TEST_F(VariableMixedAssignments, ContAssignExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr) << "module has no continuous assignments";
  EXPECT_EQ(top->getContAssigns()->size(), 1u);
}

TEST_F(VariableMixedAssignments, ContAssignLhsIsV) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const uhdm::ContAssign *const ca = top->getContAssigns()->at(0);
  ASSERT_NE(ca, nullptr);
  const uhdm::RefObj *const lhs = ca->getLhs<uhdm::RefObj>();
  ASSERT_NE(lhs, nullptr) << "ContAssign LHS is not a RefObj";
  EXPECT_EQ(lhs->getName(), "v");
}

TEST_F(VariableMixedAssignments, ContAssignRhsIsConstant12) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);

  const uhdm::Constant *const rhs =
      top->getContAssigns()->at(0)->getRhs<uhdm::Constant>();
  ASSERT_NE(rhs, nullptr) << "ContAssign RHS is not a Constant";
  EXPECT_EQ(rhs->getDecompile(), "12");
}

// ---------------------------------------------------------------------------
// Always block — always @(posedge clk) v <= ~v
// ---------------------------------------------------------------------------
TEST_F(VariableMixedAssignments, AlwaysProcessExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr) << "module has no processes";
  EXPECT_EQ(top->getProcesses()->size(), 1u);
}

TEST_F(VariableMixedAssignments, AlwaysTypeIsAlways) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);

  const uhdm::Always *const always =
      dynamic_cast<const uhdm::Always*>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr) << "process is not an Always";
  EXPECT_EQ(always->getAlwaysType(), vpiAlways);
}

TEST_F(VariableMixedAssignments, AlwaysStmtIsEventControl) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);

  const uhdm::Always *const always =
      dynamic_cast<const uhdm::Always*>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  EXPECT_NE(always->getStmt<uhdm::EventControl>(), nullptr)
      << "always body is not an EventControl";
}

TEST_F(VariableMixedAssignments, EventControlConditionIsPosedge) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Always *const always =
      dynamic_cast<const uhdm::Always*>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const uhdm::EventControl *const ec = always->getStmt<uhdm::EventControl>();
  ASSERT_NE(ec, nullptr);

  const uhdm::Operation *const cond = ec->getCondition<uhdm::Operation>();
  ASSERT_NE(cond, nullptr) << "EventControl condition is not an Operation";
  EXPECT_EQ(cond->getOpType(), vpiPosedgeOp)
      << "expected posedge operation (vpiPosedgeOp=39)";
}

TEST_F(VariableMixedAssignments, EventControlConditionOperandIsClk) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Always *const always =
      dynamic_cast<const uhdm::Always*>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const uhdm::EventControl *const ec = always->getStmt<uhdm::EventControl>();
  ASSERT_NE(ec, nullptr);
  const uhdm::Operation *const cond = ec->getCondition<uhdm::Operation>();
  ASSERT_NE(cond, nullptr);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 1u);

  const uhdm::RefObj *const operand =
      dynamic_cast<const uhdm::RefObj*>((*cond->getOperands())[0]);
  ASSERT_NE(operand, nullptr) << "posedge operand is not a RefObj";
  EXPECT_EQ(operand->getName(), "clk");
}

TEST_F(VariableMixedAssignments, EventControlStmtIsAssignment) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Always *const always =
      dynamic_cast<const uhdm::Always*>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const uhdm::EventControl *const ec = always->getStmt<uhdm::EventControl>();
  ASSERT_NE(ec, nullptr);
  EXPECT_NE(ec->getStmt<uhdm::Assignment>(), nullptr)
      << "EventControl body is not an Assignment";
}

TEST_F(VariableMixedAssignments, ProceduralAssignmentLhsIsV) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Always *const always =
      dynamic_cast<const uhdm::Always*>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const uhdm::EventControl *const ec = always->getStmt<uhdm::EventControl>();
  ASSERT_NE(ec, nullptr);
  const uhdm::Assignment *const assign = ec->getStmt<uhdm::Assignment>();
  ASSERT_NE(assign, nullptr);

  const uhdm::RefObj *const lhs = assign->getLhs<uhdm::RefObj>();
  ASSERT_NE(lhs, nullptr) << "procedural assignment LHS is not a RefObj";
  EXPECT_EQ(lhs->getName(), "v");
}

TEST_F(VariableMixedAssignments, ProceduralAssignmentRhsIsBitwiseNeg) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Always *const always =
      dynamic_cast<const uhdm::Always*>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const uhdm::EventControl *const ec = always->getStmt<uhdm::EventControl>();
  ASSERT_NE(ec, nullptr);
  const uhdm::Assignment *const assign = ec->getStmt<uhdm::Assignment>();
  ASSERT_NE(assign, nullptr);

  const uhdm::Operation *const rhs = assign->getRhs<uhdm::Operation>();
  ASSERT_NE(rhs, nullptr) << "procedural assignment RHS is not an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiBitNegOp)
      << "expected bitwise negation (vpiBitNegOp=4)";
}

TEST_F(VariableMixedAssignments, BitwiseNegOperandIsV) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Always *const always =
      dynamic_cast<const uhdm::Always*>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const uhdm::EventControl *const ec = always->getStmt<uhdm::EventControl>();
  ASSERT_NE(ec, nullptr);
  const uhdm::Assignment *const assign = ec->getStmt<uhdm::Assignment>();
  ASSERT_NE(assign, nullptr);
  const uhdm::Operation *const neg = assign->getRhs<uhdm::Operation>();
  ASSERT_NE(neg, nullptr);
  ASSERT_NE(neg->getOperands(), nullptr);
  ASSERT_EQ(neg->getOperands()->size(), 1u);

  const uhdm::RefObj *const operand =
      dynamic_cast<const uhdm::RefObj*>((*neg->getOperands())[0]);
  ASSERT_NE(operand, nullptr) << "~v operand is not a RefObj";
  EXPECT_EQ(operand->getName(), "v");
}

// ---------------------------------------------------------------------------
// Additional structural checks
// ---------------------------------------------------------------------------
TEST_F(VariableMixedAssignments, ProceduralAssignmentIsNonBlocking) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Always *const always =
      dynamic_cast<const uhdm::Always*>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const uhdm::EventControl *const ec = always->getStmt<uhdm::EventControl>();
  ASSERT_NE(ec, nullptr);
  const uhdm::Assignment *const assign = ec->getStmt<uhdm::Assignment>();
  ASSERT_NE(assign, nullptr);
  EXPECT_FALSE(assign->getBlocking())
      << "v <= ~v is a non-blocking assignment (<=), getBlocking() must be false";
}

TEST_F(VariableMixedAssignments, PosedgeClkResolvesToNetClk) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Always *const always =
      dynamic_cast<const uhdm::Always*>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const uhdm::EventControl *const ec = always->getStmt<uhdm::EventControl>();
  ASSERT_NE(ec, nullptr);
  const uhdm::Operation *const cond = ec->getCondition<uhdm::Operation>();
  ASSERT_NE(cond, nullptr);
  ASSERT_NE(cond->getOperands(), nullptr);
  const uhdm::RefObj *const clkRef =
      dynamic_cast<const uhdm::RefObj*>((*cond->getOperands())[0]);
  ASSERT_NE(clkRef, nullptr);
  EXPECT_NE(clkRef->getActual<uhdm::Net>(), nullptr)
      << "posedge operand RefObj 'clk' should resolve to the clk Net";
}

TEST_F(VariableMixedAssignments, VNetHasNoInitialValue) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const v = uhdm::findByName<uhdm::Net>("v", top->getNets());
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getValue<uhdm::Any>(), nullptr)
      << "int v has no inline initializer";
}

TEST_F(VariableMixedAssignments, ContAssignLhsResolvesToNetV) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  const uhdm::RefObj *const lhs =
      top->getContAssigns()->at(0)->getLhs<uhdm::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_NE(lhs->getActual<uhdm::Net>(), nullptr)
      << "ContAssign LHS RefObj 'v' should resolve to the net 'v'";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
