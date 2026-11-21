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

// IEEE 1800-2023 Sec 25.4: an interface port is visible inside the interface like a local signal.
// "always_ff @(posedge clk) count <= count + 4'd1;" clocks on the port clk.
// Source: tests/Google/chapter-25/ch25_suite2/25.4--if_port_used_inside.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/assignment.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/interface.h>
#include <hldb/logic_typespec.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class IfPortUsedInsideTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.4--if_port_used_inside.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getInterface() {
    return hldb::findByDefName<hldb::Interface>("cnt_if", m_design->getAllInterfaces());
  }

  static const hldb::Variable *getCount() {
    const hldb::Interface *const iface = getInterface();
    if (iface == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("count", iface->getVariables());
  }

  static const hldb::Always *getAlways() {
    const hldb::Interface *const iface = getInterface();
    if (iface == nullptr || iface->getProcesses() == nullptr || iface->getProcesses()->size() != 1) return nullptr;
    return any_cast<hldb::Always>(iface->getProcesses()->at(0));
  }

  static const hldb::EventControl *getEventControl() {
    const hldb::Always *const al = getAlways();
    if (al == nullptr) return nullptr;
    return al->getStmt<hldb::EventControl>();
  }
};

TEST_F(IfPortUsedInsideTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfPortUsedInsideTest, CountIsLogic3To0) {
  // Line 24: "logic [3:0] count;".
  const hldb::Variable *const count = getCount();
  ASSERT_NE(count, nullptr);
  ASSERT_NE(count->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lts = count->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lts, nullptr);
  ASSERT_NE(lts->getRanges(), nullptr);
  ASSERT_EQ(lts->getRanges()->size(), 1u);
  ASSERT_NE(lts->getRanges()->at(0)->getLeftExpr<hldb::Constant>(), nullptr);
  ASSERT_NE(lts->getRanges()->at(0)->getRightExpr<hldb::Constant>(), nullptr);
  EXPECT_EQ(lts->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "3");
  EXPECT_EQ(lts->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

TEST_F(IfPortUsedInsideTest, OneAlwaysFfProcess) {
  // Line 25: the single procedure is an always_ff.
  const hldb::Always *const al = getAlways();
  ASSERT_NE(al, nullptr) << "cnt_if has exactly one always procedure";
  EXPECT_EQ(al->getAlwaysType(), vpiAlwaysFF);
}

TEST_F(IfPortUsedInsideTest, EventIsPosedgeOfPortClk) {
  // Line 25: "@(posedge clk)" -- clk resolves to the port net of line 23.
  const hldb::EventControl *const ec = getEventControl();
  ASSERT_NE(ec, nullptr);
  const hldb::Operation *const op = ec->getCondition<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiPosedgeOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 1u);
  const hldb::RefObj *const clk = any_cast<hldb::RefObj>(op->getOperands()->at(0));
  ASSERT_NE(clk, nullptr);
  const hldb::Any *const expected = hldb::findByName<hldb::Net>("clk", getInterface()->getNets());
  ASSERT_NE(expected, nullptr);
  EXPECT_EQ(clk->getActual(), expected);
}

TEST_F(IfPortUsedInsideTest, BodyIsNonblockingToCount) {
  // Line 25: "count <= ...".
  const hldb::EventControl *const ec = getEventControl();
  ASSERT_NE(ec, nullptr);
  const hldb::Assignment *const as = ec->getStmt<hldb::Assignment>();
  ASSERT_NE(as, nullptr);
  EXPECT_FALSE(as->getBlocking());
  ASSERT_NE(as->getLhs<hldb::RefObj>(), nullptr);
  EXPECT_EQ(as->getLhs<hldb::RefObj>()->getActual(), getCount());
}

TEST_F(IfPortUsedInsideTest, RhsIsCountPlusOne) {
  // Line 25: "count + 4'd1".
  const hldb::EventControl *const ec = getEventControl();
  ASSERT_NE(ec, nullptr);
  const hldb::Assignment *const as = ec->getStmt<hldb::Assignment>();
  ASSERT_NE(as, nullptr);
  const hldb::Operation *const add = as->getRhs<hldb::Operation>();
  ASSERT_NE(add, nullptr);
  EXPECT_EQ(add->getOpType(), vpiAddOp);
  ASSERT_NE(add->getOperands(), nullptr);
  ASSERT_EQ(add->getOperands()->size(), 2u);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(add->getOperands()->at(0));
  const hldb::Constant *const rhs = any_cast<hldb::Constant>(add->getOperands()->at(1));
  ASSERT_NE(lhs, nullptr);
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(lhs->getActual(), getCount());
  EXPECT_EQ(rhs->getDecompile(), "4'd1");
  EXPECT_EQ(rhs->getConstType(), vpiDecConst);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
