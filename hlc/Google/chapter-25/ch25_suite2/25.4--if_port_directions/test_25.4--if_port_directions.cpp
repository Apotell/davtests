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

// IEEE 1800-2023 Sec 25.4 with Sec 23.2.2.3:
// "interface dir_if(input logic a, output logic b, inout wire c);"
//   - each port keeps its direction;
//   - "input logic a" has a data type but no net type or var, so it is a net (default wire);
//   - "output logic b" with a data type is a variable;
//   - "inout wire c" is a wire net.
// Source: tests/Google/chapter-25/ch25_suite2/25.4--if_port_directions.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/logic_typespec.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class IfPortDirectionsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.4--if_port_directions.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getInterface() {
    return hldb::findByDefName<hldb::Interface>("dir_if", m_design->getAllInterfaces());
  }

  static const hldb::Port *getPort(std::string_view name) {
    const hldb::Interface *const iface = getInterface();
    if (iface == nullptr) return nullptr;
    return hldb::findByName<hldb::Port>(name, iface->getPorts());
  }
};

TEST_F(IfPortDirectionsTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfPortDirectionsTest, InterfaceHasThreePorts) {
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getPorts(), nullptr);
  EXPECT_EQ(iface->getPorts()->size(), 3u);
}

TEST_F(IfPortDirectionsTest, PortDirections) {
  const hldb::Port *const a = getPort("a");
  const hldb::Port *const b = getPort("b");
  const hldb::Port *const c = getPort("c");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(a->getDirection(), vpiInput);
  EXPECT_EQ(b->getDirection(), vpiOutput);
  EXPECT_EQ(c->getDirection(), vpiInout);
}

TEST_F(IfPortDirectionsTest, InputLogicIsWireNet) {
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", iface->getNets());
  ASSERT_NE(a, nullptr) << "Sec 23.2.2.3: an input port with only a data type is a net";
  EXPECT_EQ(a->getNetType(), vpiWire);
  EXPECT_EQ(hldb::findByName<hldb::Variable>("a", iface->getVariables()), nullptr);
}

TEST_F(IfPortDirectionsTest, OutputLogicIsVariable) {
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("b", iface->getVariables()), nullptr)
      << "Sec 23.2.2.3: an output port with a data type is a variable";
  EXPECT_EQ(hldb::findByName<hldb::Net>("b", iface->getNets()), nullptr);
}

TEST_F(IfPortDirectionsTest, InoutWireIsWireNet) {
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  const hldb::Net *const c = hldb::findByName<hldb::Net>("c", iface->getNets());
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getNetType(), vpiWire);
}

TEST_F(IfPortDirectionsTest, PortsKeepHeaderOrder) {
  // Line 23: a, b, c in that order.
  const hldb::Port *const a = getPort("a");
  const hldb::Port *const b = getPort("b");
  const hldb::Port *const c = getPort("c");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  ASSERT_NE(c, nullptr);
  EXPECT_LT(a->getPortIndex(), b->getPortIndex());
  EXPECT_LT(b->getPortIndex(), c->getPortIndex());
}

TEST_F(IfPortDirectionsTest, AllDataTypesAreLogic) {
  // Line 23: a and b are declared logic; c ("inout wire c") has no data type, so it is logic
  // (Sec 6.7.1).
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  const hldb::Net *const a = hldb::findByName<hldb::Net>("a", iface->getNets());
  const hldb::Variable *const b = hldb::findByName<hldb::Variable>("b", iface->getVariables());
  const hldb::Net *const c = hldb::findByName<hldb::Net>("c", iface->getNets());
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(a->getTypespec(), nullptr);
  ASSERT_NE(b->getTypespec(), nullptr);
  ASSERT_NE(c->getTypespec(), nullptr);
  EXPECT_NE(a->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr);
  EXPECT_NE(b->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr);
  EXPECT_NE(c->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(IfPortDirectionsTest, PortLowConnsBindDeclarations) {
  // Line 23: inside the interface each port connects to its own net or variable.
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  const hldb::Port *const a = getPort("a");
  const hldb::Port *const b = getPort("b");
  const hldb::Port *const c = getPort("c");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  ASSERT_NE(c, nullptr);
  ASSERT_NE(a->getLowConn<hldb::RefObj>(), nullptr);
  ASSERT_NE(b->getLowConn<hldb::RefObj>(), nullptr);
  ASSERT_NE(c->getLowConn<hldb::RefObj>(), nullptr);
  EXPECT_EQ(a->getLowConn<hldb::RefObj>()->getActual(), hldb::findByName<hldb::Net>("a", iface->getNets()));
  EXPECT_EQ(b->getLowConn<hldb::RefObj>()->getActual(), hldb::findByName<hldb::Variable>("b", iface->getVariables()));
  EXPECT_EQ(c->getLowConn<hldb::RefObj>()->getActual(), hldb::findByName<hldb::Net>("c", iface->getNets()));
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
