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

// IEEE 1800-2023 Sec 25.3: interface items include procedural blocks, like module items
// ("initial q = 1'b0;" and "always @(posedge clk) q <= d;" inside proc_if).
// Source: tests/Google/chapter-25/ch25_suite2/25.3--if_process_items.sv

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
#include <hldb/initial.h>
#include <hldb/interface.h>
#include <hldb/logic_typespec.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class IfProcessItemsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3--if_process_items.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getInterface() {
    return hldb::findByDefName<hldb::Interface>("proc_if", m_design->getAllInterfaces());
  }

  static const hldb::Variable *getVariable(std::string_view name) {
    const hldb::Interface *const iface = getInterface();
    if (iface == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>(name, iface->getVariables());
  }

  static const hldb::Initial *getInitial() {
    const hldb::Interface *const iface = getInterface();
    if (iface == nullptr || iface->getProcesses() == nullptr) return nullptr;
    for (const hldb::Process *const p : *iface->getProcesses()) {
      if (const hldb::Initial *const i = any_cast<hldb::Initial>(p)) return i;
    }
    return nullptr;
  }

  static const hldb::Always *getAlways() {
    const hldb::Interface *const iface = getInterface();
    if (iface == nullptr || iface->getProcesses() == nullptr) return nullptr;
    for (const hldb::Process *const p : *iface->getProcesses()) {
      if (const hldb::Always *const a = any_cast<hldb::Always>(p)) return a;
    }
    return nullptr;
  }
};

TEST_F(IfProcessItemsTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfProcessItemsTest, QAndDAreLogicVariables) {
  // Lines 24-25: "logic q;" and "logic d;".
  for (const std::string_view name : {std::string_view("q"), std::string_view("d")}) {
    const hldb::Variable *const v = getVariable(name);
    ASSERT_NE(v, nullptr) << name;
    ASSERT_NE(v->getTypespec(), nullptr) << name;
    EXPECT_NE(v->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr) << name;
  }
}

TEST_F(IfProcessItemsTest, InterfaceHasTwoProcesses) {
  // Lines 26-27: one initial and one always.
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getProcesses(), nullptr);
  EXPECT_EQ(iface->getProcesses()->size(), 2u);
  EXPECT_NE(getInitial(), nullptr);
  EXPECT_NE(getAlways(), nullptr);
}

TEST_F(IfProcessItemsTest, InitialBlockingAssignsZeroToQ) {
  // Line 26: "initial q = 1'b0;".
  const hldb::Initial *const init = getInitial();
  ASSERT_NE(init, nullptr);
  const hldb::Assignment *const as = init->getStmt<hldb::Assignment>();
  ASSERT_NE(as, nullptr);
  EXPECT_TRUE(as->getBlocking());
  ASSERT_NE(as->getLhs<hldb::RefObj>(), nullptr);
  EXPECT_EQ(as->getLhs<hldb::RefObj>()->getActual(), getVariable("q"));
  const hldb::Constant *const rhs = as->getRhs<hldb::Constant>();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getDecompile(), "1'b0");
  EXPECT_EQ(rhs->getConstType(), vpiBinaryConst);
}

TEST_F(IfProcessItemsTest, AlwaysIsPlainAlways) {
  // Line 27: "always", not always_comb / always_ff / always_latch.
  const hldb::Always *const al = getAlways();
  ASSERT_NE(al, nullptr);
  EXPECT_EQ(al->getAlwaysType(), vpiAlways);
}

TEST_F(IfProcessItemsTest, AlwaysEventIsPosedgeOfPortClk) {
  // Line 27: "@(posedge clk)" where clk is the interface port (line 23).
  const hldb::Always *const al = getAlways();
  ASSERT_NE(al, nullptr);
  const hldb::EventControl *const ec = al->getStmt<hldb::EventControl>();
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

TEST_F(IfProcessItemsTest, AlwaysBodyIsNonblockingQFromD) {
  // Line 27: "q <= d;".
  const hldb::Always *const al = getAlways();
  ASSERT_NE(al, nullptr);
  const hldb::EventControl *const ec = al->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  const hldb::Assignment *const as = ec->getStmt<hldb::Assignment>();
  ASSERT_NE(as, nullptr);
  EXPECT_FALSE(as->getBlocking()) << "'<=' is a nonblocking assignment";
  ASSERT_NE(as->getLhs<hldb::RefObj>(), nullptr);
  ASSERT_NE(as->getRhs<hldb::RefObj>(), nullptr);
  EXPECT_EQ(as->getLhs<hldb::RefObj>()->getActual(), getVariable("q"));
  EXPECT_EQ(as->getRhs<hldb::RefObj>()->getActual(), getVariable("d"));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
