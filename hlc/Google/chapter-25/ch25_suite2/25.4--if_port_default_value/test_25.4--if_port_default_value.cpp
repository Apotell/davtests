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

// IEEE 1800-2023 Sec 25.4 with Sec 23.2.2.4: an input port may have a default value; when the
// instance leaves that port out, the default is used and no connection is required.
// "en_if u_if(.clk(clk));" omits en, which defaults to 1'b1.
// Source: tests/Google/chapter-25/ch25_suite2/25.4--if_port_default_value.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/module.h>
#include <hldb/named_argument.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class IfPortDefaultValueTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.4--if_port_default_value.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getInterface() {
    return hldb::findByDefName<hldb::Interface>("en_if", m_design->getAllInterfaces());
  }

  static const hldb::RefInstance *getInstance() {
    const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>("u_if", top->getRefInstances());
  }
};

TEST_F(IfPortDefaultValueTest, CompilerReportsZeroErrors) {
  // Sec 23.2.2.4: omitting a port that has a default is legal.
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfPortDefaultValueTest, InterfaceHasInputPortsClkAndEn) {
  // Line 23: "input logic clk, input logic en = 1'b1".
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getPorts(), nullptr);
  ASSERT_EQ(iface->getPorts()->size(), 2u);
  for (const std::string_view name : {std::string_view("clk"), std::string_view("en")}) {
    const hldb::Port *const p = hldb::findByName<hldb::Port>(name, iface->getPorts());
    ASSERT_NE(p, nullptr) << name;
    EXPECT_EQ(p->getDirection(), vpiInput) << name;
  }
}

TEST_F(IfPortDefaultValueTest, EnHasDefaultValueOne) {
  // Line 23: "en = 1'b1" -- the default value of port en.
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  const hldb::Net *const en = hldb::findByName<hldb::Net>("en", iface->getNets());
  ASSERT_NE(en, nullptr);
  const hldb::Constant *const def = en->getValue<hldb::Constant>();
  ASSERT_NE(def, nullptr) << "port en has no default value";
  EXPECT_EQ(def->getDecompile(), "1'b1");
  EXPECT_EQ(def->getConstType(), vpiBinaryConst);
}

TEST_F(IfPortDefaultValueTest, ClkHasNoDefaultValue) {
  // Line 23: "input logic clk" has no "= ..." part.
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  const hldb::Net *const clk = hldb::findByName<hldb::Net>("clk", iface->getNets());
  ASSERT_NE(clk, nullptr);
  EXPECT_EQ(clk->getValue(), nullptr);
}

TEST_F(IfPortDefaultValueTest, NoMissingConnectionReportedForEn) {
  // Line 28: en is omitted but has a default (line 23).
  EXPECT_EQ(findEvent(EventType::CompMissingArgument, "en"), nullptr);
}

TEST_F(IfPortDefaultValueTest, InstanceConnectsOnlyClk) {
  // Line 28: "(.clk(clk))" -- one connection, to port clk, from top's clk.
  const hldb::RefInstance *const inst = getInstance();
  ASSERT_NE(inst, nullptr);
  ASSERT_NE(inst->getArguments(), nullptr);
  ASSERT_EQ(inst->getArguments()->size(), 1u);
  const hldb::NamedArgument *const arg = inst->getArguments()->at(0);
  const hldb::RefObj *const low = arg->getLowConn<hldb::RefObj>();
  ASSERT_NE(low, nullptr);
  EXPECT_EQ(low->getName(), "clk");
  const hldb::RefObj *const high = arg->getHighConn<hldb::RefObj>();
  ASSERT_NE(high, nullptr);
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  EXPECT_EQ(high->getActual(), hldb::findByName<hldb::Variable>("clk", top->getVariables()));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
