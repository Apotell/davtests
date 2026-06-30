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

// Tests for 6.6.8--interconnect.sv (tags: 6.6.8)
//   module top();
//     interconnect bus;
//     mod_i m1(bus); mod_o m2(bus);
//   endmodule
//   module mod_i(input in); endmodule
//   module mod_o(output out); endmodule
//
// Checked:
//   - design has 3 modules: work@top, work@mod_i, work@mod_o
//   - work@top: 1 Net (no stored name — EL0535 error recovery), vpiNet=36, RefTypespec→LogicTypespec
//   - work@top: 2 RefInstances (m1, m2), each with 1 Port whose HighConn is RefObj "bus"
//   - work@top: no processes, no continuous assignments
//   - work@mod_i: 1 Net "in" (vpiWire), 1 Port "in" (input)
//   - work@mod_o: 1 Net "out" (vpiWire), 1 Port "out" (output)
//
// Not checked:
//   - Surelog EL0535 error count (2 errors emitted, for m1(bus) and m2(bus))
//   - net vpiFullName is "work@top" (parent scope, no own name segment)
//   - RefInstance names m1/m2 — only port HighConn verified

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/design.h>
#include <uhdm/logic_typespec.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/port.h>
#include <uhdm/ref_instance.h>
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/vpi_user.h>

namespace SURELOG {

class Interconnect : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.6.8--interconnect.hlc"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

TEST_F(Interconnect, DesignHasThreeModules) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  EXPECT_EQ(m_design->getAllModules()->size(), 3u);
}

TEST_F(Interconnect, TopModuleExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(Interconnect, TopHasOneNet) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(Interconnect, TopNetHasNoStoredName) {
  // Surelog emits EL0535 for `interconnect bus` — the net is created via
  // error-recovery but vpiName is never set, so getName() returns empty.
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_TRUE(top->getNets()->at(0)->getName().empty());
}

TEST_F(Interconnect, TopNetIsInterconnectType) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  // vpiNet = 36 is the UHDM representation for the interconnect net type
  EXPECT_EQ(net->getNetType(), vpiNet);
}

TEST_F(Interconnect, TopNetHasLogicTypespec) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const uhdm::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const uhdm::RefTypespec *const rt = net->getTypespec<uhdm::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<uhdm::LogicTypespec>(), nullptr);
}

TEST_F(Interconnect, TopHasTwoRefInstances) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  EXPECT_EQ(top->getRefInstances()->size(), 2u);
}

TEST_F(Interconnect, EachRefInstanceHasOnePort) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  for (const uhdm::RefInstance *const ri : *top->getRefInstances()) {
    ASSERT_NE(ri, nullptr);
    ASSERT_NE(ri->getPorts(), nullptr) << "RefInstance " << ri->getName();
    EXPECT_EQ(ri->getPorts()->size(), 1u) << "RefInstance " << ri->getName();
  }
}

TEST_F(Interconnect, RefInstancePortHighConnIsBus) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  for (const uhdm::RefInstance *const ri : *top->getRefInstances()) {
    ASSERT_NE(ri, nullptr);
    ASSERT_NE(ri->getPorts(), nullptr);
    const uhdm::Port *const port = static_cast<const uhdm::Port *>(ri->getPorts()->at(0));
    ASSERT_NE(port, nullptr) << "RefInstance " << ri->getName();
    const uhdm::RefObj *const hc = port->getHighConn<uhdm::RefObj>();
    ASSERT_NE(hc, nullptr) << "RefInstance " << ri->getName();
    EXPECT_EQ(hc->getName(), "bus") << "RefInstance " << ri->getName();
  }
}

TEST_F(Interconnect, TopHasNoProcesses) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

TEST_F(Interconnect, TopHasNoContAssigns) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

TEST_F(Interconnect, ModIExists) {
  const uhdm::Module *const modi =
      uhdm::findByName<uhdm::Module>("work@mod_i", m_design->getAllModules());
  EXPECT_NE(modi, nullptr);
}

TEST_F(Interconnect, ModIHasNetIn) {
  const uhdm::Module *const modi =
      uhdm::findByName<uhdm::Module>("work@mod_i", m_design->getAllModules());
  ASSERT_NE(modi, nullptr);
  ASSERT_NE(modi->getNets(), nullptr);
  EXPECT_EQ(modi->getNets()->size(), 1u);
  const uhdm::Net *const net = modi->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getName(), "in");
}

TEST_F(Interconnect, ModINetInIsWire) {
  const uhdm::Module *const modi =
      uhdm::findByName<uhdm::Module>("work@mod_i", m_design->getAllModules());
  ASSERT_NE(modi, nullptr);
  ASSERT_NE(modi->getNets(), nullptr);
  const uhdm::Net *const net = modi->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getNetType(), vpiWire);
}

TEST_F(Interconnect, ModIHasInputPort) {
  const uhdm::Module *const modi =
      uhdm::findByName<uhdm::Module>("work@mod_i", m_design->getAllModules());
  ASSERT_NE(modi, nullptr);
  ASSERT_NE(modi->getPorts(), nullptr);
  EXPECT_EQ(modi->getPorts()->size(), 1u);
  const uhdm::Port *const port = modi->getPorts()->at(0);
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), "in");
  EXPECT_EQ(port->getDirection(), vpiInput);
}

TEST_F(Interconnect, ModOExists) {
  const uhdm::Module *const modo =
      uhdm::findByName<uhdm::Module>("work@mod_o", m_design->getAllModules());
  EXPECT_NE(modo, nullptr);
}

TEST_F(Interconnect, ModOHasNetOut) {
  const uhdm::Module *const modo =
      uhdm::findByName<uhdm::Module>("work@mod_o", m_design->getAllModules());
  ASSERT_NE(modo, nullptr);
  ASSERT_NE(modo->getNets(), nullptr);
  EXPECT_EQ(modo->getNets()->size(), 1u);
  const uhdm::Net *const net = modo->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getName(), "out");
}

TEST_F(Interconnect, ModONetOutIsWire) {
  const uhdm::Module *const modo =
      uhdm::findByName<uhdm::Module>("work@mod_o", m_design->getAllModules());
  ASSERT_NE(modo, nullptr);
  ASSERT_NE(modo->getNets(), nullptr);
  const uhdm::Net *const net = modo->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getNetType(), vpiWire);
}

TEST_F(Interconnect, ModOHasOutputPort) {
  const uhdm::Module *const modo =
      uhdm::findByName<uhdm::Module>("work@mod_o", m_design->getAllModules());
  ASSERT_NE(modo, nullptr);
  ASSERT_NE(modo->getPorts(), nullptr);
  EXPECT_EQ(modo->getPorts()->size(), 1u);
  const uhdm::Port *const port = modo->getPorts()->at(0);
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), "out");
  EXPECT_EQ(port->getDirection(), vpiOutput);
}

}  // namespace SURELOG
