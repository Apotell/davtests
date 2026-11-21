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

// IEEE 1800-2023 Sec 25.4 with Sec 23.3.2.2: an interface port is connected at the interface
// instance like a module port ("clk_if u_if(.clk(clk));"); the connection names port clk and
// binds the top-level variable clk.
// Source: tests/Google/chapter-25/ch25_suite2/25.4--if_port_connection.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/named_argument.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>

namespace hlc {

class IfPortConnectionTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.4--if_port_connection.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }
};

TEST_F(IfPortConnectionTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfPortConnectionTest, InstanceHasOneNamedConnectionClk) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::RefInstance *const inst = hldb::findByName<hldb::RefInstance>("u_if", top->getRefInstances());
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getArguments(), nullptr);
  ASSERT_EQ(inst->getArguments()->size(), 1u);
  const hldb::NamedArgument *const arg = inst->getArguments()->at(0);
  EXPECT_TRUE(arg->getConnByName()) << ".clk(clk) is a named connection";
  const hldb::RefObj *const low = arg->getLowConn<hldb::RefObj>();
  ASSERT_NE(low, nullptr) << "the port side of .clk(...) is a reference";
  EXPECT_EQ(low->getName(), "clk");
  const hldb::Interface *const iface = hldb::findByDefName<hldb::Interface>("clk_if", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  const hldb::Any *const expected = hldb::findByName<hldb::Port>("clk", iface->getPorts());
  ASSERT_NE(expected, nullptr) << ".clk names port clk of clk_if";
  EXPECT_EQ(low->getActual(), expected) << ".clk names port clk of clk_if";
}

TEST_F(IfPortConnectionTest, ConnectionBindsTopVariableClk) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::RefInstance *const inst = hldb::findByName<hldb::RefInstance>("u_if", top->getRefInstances());
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getArguments(), nullptr);
  ASSERT_EQ(inst->getArguments()->size(), 1u);
  const hldb::RefObj *const high = inst->getArguments()->at(0)->getHighConn<hldb::RefObj>();
  ASSERT_NE(high, nullptr) << "the actual of .clk(...) is a reference";
  const hldb::Any *const expected = hldb::findByName<hldb::Variable>("clk", top->getVariables());
  ASSERT_NE(expected, nullptr);
  EXPECT_EQ(high->getActual(), expected);
}

TEST_F(IfPortConnectionTest, TopClkIsLogicVariable) {
  // Line 27: "logic clk;" in top is a variable of type logic.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const clk = hldb::findByName<hldb::Variable>("clk", top->getVariables());
  ASSERT_NE(clk, nullptr);
  ASSERT_NE(clk->getTypespec(), nullptr);
  EXPECT_NE(clk->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(IfPortConnectionTest, UIfIsOnlyInstanceTypedByClkIf) {
  // Line 28: "clk_if u_if(...)" is the single instance in top.
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  ASSERT_EQ(top->getRefInstances()->size(), 1u);
  const hldb::RefInstance *const inst = top->getRefInstances()->at(0);
  EXPECT_EQ(inst->getName(), "u_if");
  ASSERT_NE(inst->getTypespec(), nullptr);
  const hldb::InterfaceTypespec *const its = inst->getTypespec()->getActual<hldb::InterfaceTypespec>();
  ASSERT_NE(its, nullptr);
  const hldb::Any *const expected = hldb::findByDefName<hldb::Interface>("clk_if", m_design->getAllInterfaces());
  ASSERT_NE(expected, nullptr);
  EXPECT_EQ(its->getInterface(), expected);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
