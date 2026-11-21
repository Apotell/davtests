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

// IEEE 1800-2023 Sec 25.4: an interface can have ports like a module
// ("interface clk_if(input logic clk);").
// Source: tests/Google/chapter-25/ch25_suite2/25.4--if_input_port.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/io_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class IfInputPortTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.4--if_input_port.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getInterface() {
    return hldb::findByDefName<hldb::Interface>("clk_if", m_design->getAllInterfaces());
  }
};

TEST_F(IfInputPortTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfInputPortTest, InterfaceHasOneInputPortClk) {
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getPorts(), nullptr);
  ASSERT_EQ(iface->getPorts()->size(), 1u);
  const hldb::Port *const p = iface->getPorts()->at(0);
  EXPECT_EQ(p->getName(), "clk");
  EXPECT_EQ(p->getDirection(), vpiInput);
}

TEST_F(IfInputPortTest, ClkHasInputIODecl) {
  GTEST_SKIP() << "HLC Gap. Interface ports get no vpiIODecl (Sec 37.6, 37.13); Interface::getIODecls() is null.";
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  const hldb::IODecl *const io = hldb::findByName<hldb::IODecl>("clk", iface->getIODecls());
  ASSERT_NE(io, nullptr);
  EXPECT_EQ(io->getDirection(), vpiInput);
}

TEST_F(IfInputPortTest, ClkIsWireNetOfLogic) {
  // Line 23: Sec 23.2.2.3 -- an input port with a data type but no net type or "var" is a net
  // (default wire) of that data type.
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  const hldb::Net *const clk = hldb::findByName<hldb::Net>("clk", iface->getNets());
  ASSERT_NE(clk, nullptr);
  EXPECT_EQ(clk->getNetType(), vpiWire);
  ASSERT_NE(clk->getTypespec(), nullptr);
  EXPECT_NE(clk->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("clk", iface->getVariables()), nullptr);
}

TEST_F(IfInputPortTest, PortLowConnIsNetClk) {
  // Line 23: inside the interface, port clk connects to the net clk.
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  const hldb::Port *const p = hldb::findByName<hldb::Port>("clk", iface->getPorts());
  ASSERT_NE(p, nullptr);
  const hldb::RefObj *const low = p->getLowConn<hldb::RefObj>();
  ASSERT_NE(low, nullptr);
  EXPECT_EQ(low->getActual(), hldb::findByName<hldb::Net>("clk", iface->getNets()));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
