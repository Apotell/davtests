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
//   - HLC doesn't flag the illegal posedge on real
//   - $display argument value ("posedge")

#include <hlc/Common/Session.h>
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
#include <hldb/vpi_user.h>

namespace hlc {

class RealEdge : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.12--real_edge.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(RealEdge, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Net — real a = 0.5
// ---------------------------------------------------------------------------
TEST_F(RealEdge, OneNetExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(RealEdge, ANetTypespecIsReal) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const hldb::RefTypespec *const rts = a->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::RealTypespec>(), nullptr) << "net 'a' typespec should resolve to RealTypespec";
}

TEST_F(RealEdge, ANetInitialValueIsHalf) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", top->getNets());
  ASSERT_NE(a, nullptr);
  const hldb::Constant *const init = a->getValue<hldb::Constant>();
  ASSERT_NE(init, nullptr);
  EXPECT_EQ(init->getConstType(), vpiRealConst);
  EXPECT_EQ(init->getDecompile(), "0.5");
}

// ---------------------------------------------------------------------------
// Always process — always @(posedge a)
// ---------------------------------------------------------------------------
TEST_F(RealEdge, AlwaysProcessExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  EXPECT_EQ(top->getProcesses()->size(), 1u);
}

TEST_F(RealEdge, AlwaysTypeIsAlways) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  EXPECT_EQ(always->getAlwaysType(), vpiAlways);
}

TEST_F(RealEdge, AlwaysStmtIsEventControl) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  EXPECT_NE(always->getStmt<hldb::EventControl>(), nullptr) << "always body should be an EventControl";
}

// ---------------------------------------------------------------------------
// EventControl condition — posedge on real net 'a' (illegal but parsed)
// ---------------------------------------------------------------------------
TEST_F(RealEdge, EventControlConditionIsPosedge) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
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
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
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

TEST_F(RealEdge, EventControlConditionOperandIsRealNet) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  const hldb::Operation *const cond = ec->getCondition<hldb::Operation>();
  ASSERT_NE(cond, nullptr);
  const hldb::RefObj *const operand = any_cast<hldb::RefObj>((*cond->getOperands())[0]);
  ASSERT_NE(operand, nullptr);

  const hldb::Net *const net = operand->getActual<hldb::Net>();
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rts = net->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::RealTypespec>(), nullptr) << "posedge operand should resolve to the real net 'a'";
}

// ---------------------------------------------------------------------------
// EventControl body — $display("posedge")
// ---------------------------------------------------------------------------
TEST_F(RealEdge, EventControlStmtIsDisplayCall) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Always *const always = dynamic_cast<const hldb::Always *>(top->getProcesses()->at(0));
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);

  const hldb::SysFuncCall *const call = ec->getStmt<hldb::SysFuncCall>();
  ASSERT_NE(call, nullptr) << "EventControl body is not a SysFuncCall";
  EXPECT_EQ(call->getName(), "$display");
}

TEST_F(RealEdge, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
