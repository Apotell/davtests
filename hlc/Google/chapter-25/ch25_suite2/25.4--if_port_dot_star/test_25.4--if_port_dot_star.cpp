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

// IEEE 1800-2023 Sec 25.4 with Sec 23.3.2.4: ".*" at an interface instance connects each port to
// the identically named item of the instantiating scope -- clk to top's clk, rst to top's rst.
// Source: tests/Google/chapter-25/ch25_suite2/25.4--if_port_dot_star.sv

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

#include <map>
#include <string_view>

namespace hlc {

class IfPortDotStarTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.4--if_port_dot_star.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::Interface *getInterface() {
    return hldb::findByDefName<hldb::Interface>("clk_if", m_design->getAllInterfaces());
  }

  // The connections ".*" made at u_if, keyed by the port name.
  static std::map<std::string_view, const hldb::NamedArgument *> getWildcardConnections() {
    std::map<std::string_view, const hldb::NamedArgument *> result;
    const hldb::Module *const top = getTop();
    if (top == nullptr) return result;
    const hldb::RefInstance *const inst = hldb::findByName<hldb::RefInstance>("u_if", top->getRefInstances());
    if (inst == nullptr || inst->getArguments() == nullptr) return result;
    for (const hldb::NamedArgument *const arg : *inst->getArguments()) {
      if (!arg->getWildcardConn()) continue;
      const hldb::RefObj *const low = arg->getLowConn<hldb::RefObj>();
      if (low != nullptr) result.emplace(low->getName(), arg);
    }
    return result;
  }

  static void expectConnection(std::string_view name) {
    const std::map<std::string_view, const hldb::NamedArgument *> conns = getWildcardConnections();
    ASSERT_NE(conns.find(name), conns.end()) << "no .* connection for port " << name;
    const hldb::NamedArgument *const arg = conns.at(name);
    EXPECT_TRUE(arg->getConnByName()) << name;
    EXPECT_EQ(arg->getLowConn<hldb::RefObj>()->getActual(),
              hldb::findByName<hldb::Port>(name, getInterface()->getPorts()))
        << name;
    const hldb::RefObj *const high = arg->getHighConn<hldb::RefObj>();
    ASSERT_NE(high, nullptr) << name;
    EXPECT_EQ(high->getActual(), hldb::findByName<hldb::Variable>(name, getTop()->getVariables())) << name;
  }
};

TEST_F(IfPortDotStarTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfPortDotStarTest, InterfaceHasInputPortsClkAndRst) {
  // Line 23: "input logic clk, input logic rst".
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getPorts(), nullptr);
  ASSERT_EQ(iface->getPorts()->size(), 2u);
  for (const std::string_view name : {std::string_view("clk"), std::string_view("rst")}) {
    const hldb::Port *const p = hldb::findByName<hldb::Port>(name, iface->getPorts());
    ASSERT_NE(p, nullptr) << name;
    EXPECT_EQ(p->getDirection(), vpiInput) << name;
  }
}

TEST_F(IfPortDotStarTest, TopHasVariablesClkAndRst) {
  // Lines 27-28: "logic clk;" and "logic rst;".
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("clk", top->getVariables()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("rst", top->getVariables()), nullptr);
}

TEST_F(IfPortDotStarTest, WildcardMakesTwoConnections) {
  // Line 29: ".*" -- one connection for each of the two ports.
  EXPECT_EQ(getWildcardConnections().size(), 2u);
}

TEST_F(IfPortDotStarTest, ClkConnectsToTopClk) {
  // Line 29: port clk <- top's clk.
  expectConnection("clk");
}

TEST_F(IfPortDotStarTest, RstConnectsToTopRst) {
  // Line 29: port rst <- top's rst.
  expectConnection("rst");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
