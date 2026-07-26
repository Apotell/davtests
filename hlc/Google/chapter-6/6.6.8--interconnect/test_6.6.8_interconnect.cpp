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
//   - design has 3 modules: top, mod_i, mod_o
//   - top: 1 Net (no stored name — EL0535 error recovery), vpiNet=36, RefTypespec→LogicTypespec
//   - top: 2 RefInstances (m1, m2), each with 1 Port whose HighConn is RefObj "bus"
//   - top: no processes, no continuous assignments
//   - mod_i: 1 Net "in" (vpiWire), 1 Port "in" (input)
//   - mod_o: 1 Net "out" (vpiWire), 1 Port "out" (output)
//   - HLC emits exactly 2 EL0535 errors (for m1(bus) and m2(bus))
//   - net vpiFullName is "top" (parent scope, no own name segment)
//   - RefInstance names are "m1" and "m2" (in declaration order)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class Interconnect : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.6.8--interconnect.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(Interconnect, DesignHasThreeModules) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  EXPECT_EQ(m_design->getAllModules()->size(), 3u);
}

TEST_F(Interconnect, TopModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(Interconnect, TopHasOneNet) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(Interconnect, TopNetHasNoStoredName) {
  // HLC emits EL0535 for `interconnect bus` — the net is created via
  // error-recovery but vpiName is never set, so getName() returns empty.
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_TRUE(top->getNets()->at(0)->getName().empty());
}

TEST_F(Interconnect, TopNetIsInterconnectType) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  // vpiNet = 36 is the UHDM representation for the interconnect net type
  EXPECT_EQ(net->getNetType(), vpiNet);
}

TEST_F(Interconnect, TopNetHasLogicTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rt = net->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(Interconnect, TopHasTwoRefInstances) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  EXPECT_EQ(top->getRefInstances()->size(), 2u);
}

TEST_F(Interconnect, RefInstanceNamesAreM1AndM2) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  ASSERT_EQ(top->getRefInstances()->size(), 2u);
  EXPECT_EQ(top->getRefInstances()->at(0)->getName(), "m1");
  EXPECT_EQ(top->getRefInstances()->at(1)->getName(), "m2");
}

TEST_F(Interconnect, TopNetFullNameIsParentScope) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getFullName(), "top")
      << "the unnamed error-recovery net has no own name segment; vpiFullName is just the parent scope";
}

TEST_F(Interconnect, EachRefInstanceHasOnePort) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  for (const hldb::RefInstance *const ri : *top->getRefInstances()) {
    ASSERT_NE(ri, nullptr);
    ASSERT_NE(ri->getPorts(), nullptr) << "RefInstance " << ri->getName();
    EXPECT_EQ(ri->getPorts()->size(), 1u) << "RefInstance " << ri->getName();
  }
}

TEST_F(Interconnect, RefInstancePortHighConnIsBus) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  for (const hldb::RefInstance *const ri : *top->getRefInstances()) {
    ASSERT_NE(ri, nullptr);
    ASSERT_NE(ri->getPorts(), nullptr);
    const hldb::Port *const port = static_cast<const hldb::Port *>(ri->getPorts()->at(0));
    ASSERT_NE(port, nullptr) << "RefInstance " << ri->getName();
    const hldb::RefObj *const hc = port->getHighConn<hldb::RefObj>();
    ASSERT_NE(hc, nullptr) << "RefInstance " << ri->getName();
    EXPECT_EQ(hc->getName(), "bus") << "RefInstance " << ri->getName();
  }
}

TEST_F(Interconnect, TopHasNoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

TEST_F(Interconnect, TopHasNoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

// ---------------------------------------------------------------------------
// Compiler diagnostics -- HLC emits EL0535 for each implicit-net instantiation
// ---------------------------------------------------------------------------
TEST_F(Interconnect, Compiler_ReportsTwoErrors) {
  const hlc::ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 2) << "expected 2 EL0535 'Illegal implicit net' errors, for m1(bus) and m2(bus)";
}

TEST_F(Interconnect, ModIExists) {
  const hldb::Module *const modi = hldb::findByName<hldb::Module>("mod_i", m_design->getAllModules());
  EXPECT_NE(modi, nullptr);
}

TEST_F(Interconnect, ModIHasNetIn) {
  const hldb::Module *const modi = hldb::findByName<hldb::Module>("mod_i", m_design->getAllModules());
  ASSERT_NE(modi, nullptr);
  ASSERT_NE(modi->getNets(), nullptr);
  EXPECT_EQ(modi->getNets()->size(), 1u);
  const hldb::Net *const net = modi->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getName(), "in");
}

TEST_F(Interconnect, ModINetInIsWire) {
  const hldb::Module *const modi = hldb::findByName<hldb::Module>("mod_i", m_design->getAllModules());
  ASSERT_NE(modi, nullptr);
  ASSERT_NE(modi->getNets(), nullptr);
  const hldb::Net *const net = modi->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getNetType(), vpiWire);
}

TEST_F(Interconnect, ModIHasInputPort) {
  const hldb::Module *const modi = hldb::findByName<hldb::Module>("mod_i", m_design->getAllModules());
  ASSERT_NE(modi, nullptr);
  ASSERT_NE(modi->getPorts(), nullptr);
  EXPECT_EQ(modi->getPorts()->size(), 1u);
  const hldb::Port *const port = modi->getPorts()->at(0);
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), "in");
  EXPECT_EQ(port->getDirection(), vpiInput);
}

TEST_F(Interconnect, ModOExists) {
  const hldb::Module *const modo = hldb::findByName<hldb::Module>("mod_o", m_design->getAllModules());
  EXPECT_NE(modo, nullptr);
}

TEST_F(Interconnect, ModOHasNetOut) {
  const hldb::Module *const modo = hldb::findByName<hldb::Module>("mod_o", m_design->getAllModules());
  ASSERT_NE(modo, nullptr);
  ASSERT_NE(modo->getNets(), nullptr);
  EXPECT_EQ(modo->getNets()->size(), 1u);
  const hldb::Net *const net = modo->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getName(), "out");
}

TEST_F(Interconnect, ModONetOutIsWire) {
  const hldb::Module *const modo = hldb::findByName<hldb::Module>("mod_o", m_design->getAllModules());
  ASSERT_NE(modo, nullptr);
  ASSERT_NE(modo->getNets(), nullptr);
  const hldb::Net *const net = modo->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getNetType(), vpiWire);
}

TEST_F(Interconnect, ModOHasOutputPort) {
  const hldb::Module *const modo = hldb::findByName<hldb::Module>("mod_o", m_design->getAllModules());
  ASSERT_NE(modo, nullptr);
  ASSERT_NE(modo->getPorts(), nullptr);
  EXPECT_EQ(modo->getPorts()->size(), 1u);
  const hldb::Port *const port = modo->getPorts()->at(0);
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), "out");
  EXPECT_EQ(port->getDirection(), vpiOutput);
}
}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
