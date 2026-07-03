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
//   - design has module work@top
//   - module has exactly 1 net: 'a' (RealTypespec, vpiRealConst "0.5")
//   - 1 Always process (vpiAlways type), stmt = EventControl
//   - EventControl condition = vpiPosedgeOp on RefObj "a"
//   - posedge operand resolves to the real Net 'a'
//   - EventControl body is a SysFuncCall "$display"
//   - work@top has no continuous assignments
//
// Not checked:
//   - Surelog doesn't flag the illegal posedge on real
//   - $display argument value ("posedge")

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/always.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/event_control.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/operation.h>
#include <uhdm/real_typespec.h>
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/sys_func_call.h>
#include <uhdm/vpi_user.h>

namespace SURELOG {

class RealEdge : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.12--real_edge.hlc"});

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

TEST_F(RealEdge, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Net — real a = 0.5
// ---------------------------------------------------------------------------
TEST_F(RealEdge, OneNetExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(RealEdge, ANetTypespecIsReal) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const uhdm::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<uhdm::RealTypespec>(), nullptr)
      << "net 'a' typespec should resolve to RealTypespec";
}

TEST_F(RealEdge, ANetInitialValueIsHalf) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const a = uhdm::findByName<uhdm::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const uhdm::Constant *const init = a->getValue<uhdm::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiRealConst);
  EXPECT_EQ(init->getDecompile(), "0.5");
}

// ---------------------------------------------------------------------------
// Always process — always @(posedge a)
// ---------------------------------------------------------------------------
TEST_F(RealEdge, AlwaysProcessExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 1u);
}

TEST_F(RealEdge, AlwaysTypeIsAlways) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  const uhdm::Always *const always =
      dynamic_cast<const uhdm::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  EXPECT_EQ(always->getAlwaysType(), vpiAlways);
}

TEST_F(RealEdge, AlwaysStmtIsEventControl) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Always *const always =
      dynamic_cast<const uhdm::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  EXPECT_NE(always->getStmt<uhdm::EventControl>(), nullptr)
      << "always body should be an EventControl";
}

// ---------------------------------------------------------------------------
// EventControl condition — posedge on real net 'a' (illegal but parsed)
// ---------------------------------------------------------------------------
TEST_F(RealEdge, EventControlConditionIsPosedge) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Always *const always =
      dynamic_cast<const uhdm::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const uhdm::EventControl *const ec = always->getStmt<uhdm::EventControl>();
  ASSERT_NE(ec, nullptr);

  const uhdm::Operation *const cond = ec->getCondition<uhdm::Operation>();
  ASSERT_NE(cond, nullptr) << "EventControl condition is not an Operation";
  EXPECT_EQ(cond->getOpType(), vpiPosedgeOp)
      << "expected vpiPosedgeOp (39)";
}

TEST_F(RealEdge, EventControlConditionOperandIsA) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Always *const always =
      dynamic_cast<const uhdm::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const uhdm::EventControl *const ec = always->getStmt<uhdm::EventControl>();
  ASSERT_NE(ec, nullptr);
  const uhdm::Operation *const cond = ec->getCondition<uhdm::Operation>();
  ASSERT_NE(cond, nullptr);
  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 1u);

  const uhdm::RefObj *const operand =
      any_cast<uhdm::RefObj>((*cond->getOperands())[0]);
  ASSERT_NE(operand, nullptr) << "posedge operand is not a RefObj";
  EXPECT_EQ(operand->getName(), "a");
}

TEST_F(RealEdge, EventControlConditionOperandIsRealNet) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Always *const always =
      dynamic_cast<const uhdm::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const uhdm::EventControl *const ec = always->getStmt<uhdm::EventControl>();
  ASSERT_NE(ec, nullptr);
  const uhdm::Operation *const cond = ec->getCondition<uhdm::Operation>();
  ASSERT_NE(cond, nullptr);
  const uhdm::RefObj *const operand =
      any_cast<uhdm::RefObj>((*cond->getOperands())[0]);
  ASSERT_NE(operand, nullptr);

  const uhdm::Net *const net = operand->getActual<uhdm::Net>();
  ASSERT_NE(net, nullptr);
  const uhdm::RefTypespec *const rts = net->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<uhdm::RealTypespec>(), nullptr)
      << "posedge operand should resolve to the real net 'a'";
}

// ---------------------------------------------------------------------------
// EventControl body — $display("posedge")
// ---------------------------------------------------------------------------
TEST_F(RealEdge, EventControlStmtIsDisplayCall) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Always *const always =
      dynamic_cast<const uhdm::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const uhdm::EventControl *const ec = always->getStmt<uhdm::EventControl>();
  ASSERT_NE(ec, nullptr);

  const uhdm::SysFuncCall *const call = ec->getStmt<uhdm::SysFuncCall>();
  ASSERT_NE(call, nullptr) << "EventControl body is not a SysFuncCall";
  EXPECT_EQ(call->getName(), "$display");
}

TEST_F(RealEdge, NoContAssigns) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
