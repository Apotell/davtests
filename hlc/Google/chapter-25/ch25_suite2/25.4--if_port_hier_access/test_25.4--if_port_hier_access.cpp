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

// IEEE 1800-2023 Sec 25.4 with Sec 23.6: a port of an interface is an object of the interface
// instance and can be named hierarchically from the instantiating scope ("u_if.clk").
// Source: tests/Google/chapter-25/ch25_suite2/25.4--if_port_hier_access.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/module.h>
#include <hldb/named_argument.h>
#include <hldb/net.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>

namespace hlc {

class IfPortHierAccessTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.4--if_port_hier_access.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  static const hldb::ContAssign *getAssign() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getContAssigns() == nullptr || top->getContAssigns()->size() != 1) return nullptr;
    return top->getContAssigns()->at(0);
  }
};

TEST_F(IfPortHierAccessTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfPortHierAccessTest, TopHasVariablesCAndSeen) {
  // Lines 27-28: "logic c;" and "logic seen;".
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("c", top->getVariables()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("seen", top->getVariables()), nullptr);
}

TEST_F(IfPortHierAccessTest, InstanceConnectsClkToC) {
  // Line 29: "clk_if u_if(.clk(c));".
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::RefInstance *const inst = hldb::findByName<hldb::RefInstance>("u_if", top->getRefInstances());
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getArguments(), nullptr);
  ASSERT_EQ(inst->getArguments()->size(), 1u);
  const hldb::RefObj *const high = inst->getArguments()->at(0)->getHighConn<hldb::RefObj>();
  ASSERT_NE(high, nullptr);
  EXPECT_EQ(high->getActual(), hldb::findByName<hldb::Variable>("c", top->getVariables()));
}

TEST_F(IfPortHierAccessTest, AssignTargetsSeen) {
  // Line 30: "assign seen = ...".
  const hldb::ContAssign *const ca = getAssign();
  ASSERT_NE(ca, nullptr) << "top has exactly one continuous assignment";
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getActual(), hldb::findByName<hldb::Variable>("seen", getTop()->getVariables()));
}

TEST_F(IfPortHierAccessTest, HierarchicalNameResolvesToPortNetClk) {
  // Line 30: "u_if.clk" names the clk object of clk_if (line 23), a wire net.
  const hldb::ContAssign *const ca = getAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const rhs = ca->getRhs<hldb::RefObj>();
  ASSERT_NE(rhs, nullptr) << "u_if.clk is a reference";
  const hldb::Interface *const iface = hldb::findByDefName<hldb::Interface>("clk_if", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  EXPECT_EQ(rhs->getActual(), hldb::findByName<hldb::Net>("clk", iface->getNets()));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
