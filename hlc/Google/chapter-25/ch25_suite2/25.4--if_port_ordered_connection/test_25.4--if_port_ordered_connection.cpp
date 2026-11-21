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

// IEEE 1800-2023 Sec 25.4 with Sec 23.3.2.1: interface ports can be connected by position.
// "clk_if u_if(c, r);" -- c goes to the first port (clk), r to the second (rst).
// Source: tests/Google/chapter-25/ch25_suite2/25.4--if_port_ordered_connection.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/module.h>
#include <hldb/named_argument.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class IfPortOrderedConnectionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.4--if_port_ordered_connection.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Interface *getInterface() {
    return hldb::findByDefName<hldb::Interface>("clk_if", m_design->getAllInterfaces());
  }

  static const hldb::RefInstance *getInstance() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>("u_if", top->getRefInstances());
  }

  // Checks connection number index: positional, port side is portName, actual is top's varName.
  static void expectConnection(size_t index, std::string_view portName, std::string_view varName) {
    const hldb::RefInstance *const inst = getInstance();
    ASSERT_NE(inst, nullptr);
    ASSERT_NE(inst->getArguments(), nullptr);
    ASSERT_GT(inst->getArguments()->size(), index);
    const hldb::NamedArgument *const arg = inst->getArguments()->at(index);
    EXPECT_FALSE(arg->getConnByName()) << portName;
    const hldb::RefObj *const low = arg->getLowConn<hldb::RefObj>();
    ASSERT_NE(low, nullptr) << portName;
    const hldb::Any *const expected = hldb::findByName<hldb::Port>(portName, getInterface()->getPorts());
    ASSERT_NE(expected, nullptr) << portName;
    EXPECT_EQ(low->getActual(), expected) << portName;
    const hldb::RefObj *const high = arg->getHighConn<hldb::RefObj>();
    ASSERT_NE(high, nullptr) << portName;
    const hldb::Any *const expected2 = hldb::findByName<hldb::Variable>(varName, getTop()->getVariables());
    ASSERT_NE(expected2, nullptr) << portName;
    EXPECT_EQ(high->getActual(), expected2) << portName;
  }
};

TEST_F(IfPortOrderedConnectionTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfPortOrderedConnectionTest, InterfaceHasPortsClkThenRst) {
  // Line 23: "input logic clk, input logic rst" in that order.
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  const hldb::Port *const clk = hldb::findByName<hldb::Port>("clk", iface->getPorts());
  const hldb::Port *const rst = hldb::findByName<hldb::Port>("rst", iface->getPorts());
  ASSERT_NE(clk, nullptr);
  ASSERT_NE(rst, nullptr);
  EXPECT_EQ(clk->getDirection(), vpiInput);
  EXPECT_EQ(rst->getDirection(), vpiInput);
  EXPECT_LT(clk->getPortIndex(), rst->getPortIndex());
}

TEST_F(IfPortOrderedConnectionTest, TopHasVariablesCAndR) {
  // Lines 27-28: "logic c;" and "logic r;".
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("c", top->getVariables()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("r", top->getVariables()), nullptr);
}

TEST_F(IfPortOrderedConnectionTest, InstanceHasTwoConnections) {
  // Line 29: "(c, r)".
  const hldb::RefInstance *const inst = getInstance();
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getArguments(), nullptr);
  EXPECT_EQ(inst->getArguments()->size(), 2u);
}

TEST_F(IfPortOrderedConnectionTest, FirstPositionConnectsClkToC) {
  // Line 29: position 0 -> port clk, actual c.
  expectConnection(0, "clk", "c");
}

TEST_F(IfPortOrderedConnectionTest, SecondPositionConnectsRstToR) {
  // Line 29: position 1 -> port rst, actual r.
  expectConnection(1, "rst", "r");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
