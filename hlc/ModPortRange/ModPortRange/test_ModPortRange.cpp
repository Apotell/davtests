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

// Tests for tests/ModPortRange/dut.sv (tags: ModPortRange)
//   interface MyInterface;
//      logic my_logic;
//      modport MyModPort ( output my_logic );
//   endinterface
//
//   module range_itf_port (
//       MyInterface.MyModPort my_port1[1:0],
//       input logic my_port2[1:0]
//   );
//   endmodule
//
//   interface mem_if (input wire clk);
//     modport  system (input clk);
//     modport  memory (output clk);
//   endinterface
//
//   module memory_ctrl1 (mem_if sif1, mem_if.system sif2);
//   endmodule
//
//   interface ConnectTB  (input wire [1:0] con_i) ;
//   endinterface
//
//   module middle (ConnectTB conn1 [1:0]);
//   endmodule
//
//   module range_itf_port2 (
//       MyInterface.MyModPort my_port1,
//       MyInterface.MyModPort my_port2[1:0],
//       MyInterface  my_port3,
//       MyInterface  my_port4[1:0]
//   );
//   endmodule
//
// The file exercises interface ports and modport ports, with and without an
// unpacked dimension ("range") on the port identifier.
//
// What is checked (IEEE 1800-2023):
//   - 6.8: "logic my_logic;" inside MyInterface (no net type keyword) is a
//     variable.
//   - 25.5: MyInterface has modport MyModPort with "output my_logic"; mem_if
//     has modports "system" (input clk) and "memory" (output clk). The names
//     used are declared by the same interface (my_logic as a variable, clk
//     as an interface port), so no COMP_MODPORT_UNDEFINED_PORT is raised.
//   - 23.2.2 (ansi_port_declaration ::= [ interface_port_header ]
//     port_identifier { unpacked_dimension }) and 37.14 detail 1:
//       * "Intf.modport name" ports have vpiPortType vpiModportPort;
//         "Intf name" ports have vpiPortType vpiInterfacePort.
//       * the modport identifier ("MyModPort", "system") resolves -- no
//         COMP_FAILED_TO_BIND for it.
//       * interface_port_header has no port_direction, so no
//         COMP_PORT_MISSING_DIRECTION warning applies to these ports.
//       * an interface port with an unpacked dimension [1:0] is an array of
//         interface ports: its typespec is an unpacked ArrayTypespec over the
//         interface typespec (with the named modport where given).
//   - 23.2.2.3: "input logic my_port2[1:0]" has an explicit direction and
//     data type but no port kind; for an input port the kind defaults to a
//     net of default net type -- so my_port2 is a wire net whose type is an
//     unpacked array (7.4.2) of logic, range [1:0]. Being an ordinary port,
//     its vpiPortType is vpiPort (37.14 detail 1).
//   - 25.4: interface ports "input wire clk" and "input wire [1:0] con_i" are
//     ports of the interface with direction input.
//
// What is NOT checked and why:
//   - whether "modport memory (output clk)" on an interface *input* port is
//     legal: 25.5 only says modport names shall be declared by the
//     interface; the standard has no explicit rule forbidding an output
//     modport view of an interface input port, so nothing is asserted.
//   - vpiDirection of interface/modport ports: not defined by the standard.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/modport.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class ModPortRangeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ModPortRange.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getIface(std::string_view name) {
    return hldb::findByName<hldb::Interface>(name, m_design->getAllInterfaces());
  }

  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByName<hldb::Module>(name, m_design->getAllModules());
  }

  static const hldb::Port *getPort(std::string_view module, std::string_view port) {
    const hldb::Module *const m = getModule(module);
    if (m == nullptr) return nullptr;
    return hldb::findByName<hldb::Port>(port, m->getPorts());
  }

  static const hldb::Modport *getModport(std::string_view iface, std::string_view name) {
    const hldb::Interface *const i = getIface(iface);
    if (i == nullptr) return nullptr;
    return hldb::findByName<hldb::Modport>(name, i->getModports());
  }

  // Checks a range is [left:right] with constant bounds.
  static void expectRange(const hldb::Range *range, std::string_view left, std::string_view right) {
    ASSERT_NE(range, nullptr);
    const hldb::Constant *const l = range->getLeftExpr<hldb::Constant>();
    const hldb::Constant *const r = range->getRightExpr<hldb::Constant>();
    ASSERT_NE(l, nullptr);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(l->getDecompile(), left);
    EXPECT_EQ(r->getDecompile(), right);
  }

  // Checks an interface port declared with unpacked dimension [1:0] is an
  // array of interface typespec (optionally restricted to modport).
  static void expectInterfacePortArray(const hldb::Port *port, const hldb::Interface *iface,
                                       const hldb::Modport *modport) {
    ASSERT_NE(port, nullptr);
    ASSERT_NE(port->getTypespec(), nullptr);
    const hldb::ArrayTypespec *const at = port->getTypespec()->getActual<hldb::ArrayTypespec>();
    ASSERT_NE(at, nullptr) << port->getName() << ": [1:0] makes this an array of interface ports";
    EXPECT_FALSE(at->getPacked()) << port->getName() << ": dimension after the identifier is unpacked";
    expectRange(at->getRange(), "1", "0");
    ASSERT_NE(at->getElemTypespec(), nullptr);
    const hldb::InterfaceTypespec *const its = at->getElemTypespec()->getActual<hldb::InterfaceTypespec>();
    ASSERT_NE(its, nullptr) << port->getName();
    EXPECT_EQ(its->getInterface(), iface) << port->getName();
    EXPECT_EQ(its->getModport(), modport) << port->getName();
  }

  // Checks a scalar interface port's typespec.
  static void expectInterfacePort(const hldb::Port *port, const hldb::Interface *iface, const hldb::Modport *modport) {
    ASSERT_NE(port, nullptr);
    ASSERT_NE(port->getTypespec(), nullptr);
    const hldb::InterfaceTypespec *const its = port->getTypespec()->getActual<hldb::InterfaceTypespec>();
    ASSERT_NE(its, nullptr) << port->getName();
    EXPECT_EQ(its->getInterface(), iface) << port->getName();
    EXPECT_EQ(its->getModport(), modport) << port->getName();
  }
};

// ===========================================================================
// interface MyInterface
// ===========================================================================

TEST_F(ModPortRangeTest, MyInterfaceDeclaresVariableMyLogic) {
  const hldb::Interface *const iface = getIface("MyInterface");
  ASSERT_NE(iface, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("my_logic", iface->getVariables()), nullptr)
      << "6.8: 'logic my_logic;' without a net type keyword is a variable";
  EXPECT_EQ(hldb::findByName<hldb::Net>("my_logic", iface->getNets()), nullptr);
}

TEST_F(ModPortRangeTest, MyModPortHasOutputMyLogic) {
  const hldb::Interface *const iface = getIface("MyInterface");
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getModports(), nullptr);
  ASSERT_EQ(iface->getModports()->size(), 1u);
  const hldb::Modport *const mp = getModport("MyInterface", "MyModPort");
  ASSERT_NE(mp, nullptr);
  ASSERT_NE(mp->getIODecls(), nullptr);
  ASSERT_EQ(mp->getIODecls()->size(), 1u);
  EXPECT_EQ(mp->getIODecls()->at(0)->getName(), "my_logic");
  EXPECT_EQ(mp->getIODecls()->at(0)->getDirection(), vpiOutput);
  EXPECT_EQ(findError(ErrorDefinition::COMP_MODPORT_UNDEFINED_PORT, "my_logic"), nullptr)
      << "25.5: my_logic is declared by MyInterface";
}

// ===========================================================================
// interface mem_if (input wire clk)
// ===========================================================================

TEST_F(ModPortRangeTest, MemIfHasInputWirePortClk) {
  const hldb::Interface *const iface = getIface("mem_if");
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getPorts(), nullptr);
  ASSERT_EQ(iface->getPorts()->size(), 1u);
  EXPECT_EQ(iface->getPorts()->at(0)->getName(), "clk");
  EXPECT_EQ(iface->getPorts()->at(0)->getDirection(), vpiInput);
  const hldb::Net *const clk = hldb::findByName<hldb::Net>("clk", iface->getNets());
  ASSERT_NE(clk, nullptr);
  EXPECT_EQ(clk->getNetType(), vpiWire);
}

TEST_F(ModPortRangeTest, MemIfModportsSystemAndMemory) {
  const hldb::Interface *const iface = getIface("mem_if");
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getModports(), nullptr);
  ASSERT_EQ(iface->getModports()->size(), 2u);

  const hldb::Modport *const sys = getModport("mem_if", "system");
  ASSERT_NE(sys, nullptr);
  ASSERT_NE(sys->getIODecls(), nullptr);
  ASSERT_EQ(sys->getIODecls()->size(), 1u);
  EXPECT_EQ(sys->getIODecls()->at(0)->getName(), "clk");
  EXPECT_EQ(sys->getIODecls()->at(0)->getDirection(), vpiInput);

  const hldb::Modport *const mem = getModport("mem_if", "memory");
  ASSERT_NE(mem, nullptr);
  ASSERT_NE(mem->getIODecls(), nullptr);
  ASSERT_EQ(mem->getIODecls()->size(), 1u);
  EXPECT_EQ(mem->getIODecls()->at(0)->getName(), "clk");
  EXPECT_EQ(mem->getIODecls()->at(0)->getDirection(), vpiOutput);

  EXPECT_EQ(findError(ErrorDefinition::COMP_MODPORT_UNDEFINED_PORT, "clk"), nullptr)
      << "25.5: clk is declared by mem_if (as an interface port)";
}

TEST_F(ModPortRangeTest, ConnectTBHasInputVectorPort) {
  const hldb::Interface *const iface = getIface("ConnectTB");
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getPorts(), nullptr);
  ASSERT_EQ(iface->getPorts()->size(), 1u);
  EXPECT_EQ(iface->getPorts()->at(0)->getName(), "con_i");
  EXPECT_EQ(iface->getPorts()->at(0)->getDirection(), vpiInput);
  const hldb::Net *const con = hldb::findByName<hldb::Net>("con_i", iface->getNets());
  ASSERT_NE(con, nullptr);
  EXPECT_EQ(con->getNetType(), vpiWire);
  EXPECT_TRUE(con->getVector());
}

// ===========================================================================
// module range_itf_port
// ===========================================================================

TEST_F(ModPortRangeTest, RangeItfPortMyPort1IsModportPortArray) {
  const hldb::Port *const p = getPort("range_itf_port", "my_port1");
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getPortType(), vpiModportPort);
  expectInterfacePortArray(p, getIface("MyInterface"), getModport("MyInterface", "MyModPort"));
}

TEST_F(ModPortRangeTest, RangeItfPortMyPort2IsInputWireUnpackedArray) {
  const hldb::Port *const p = getPort("range_itf_port", "my_port2");
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getDirection(), vpiInput);

  const hldb::Module *const m = getModule("range_itf_port");
  ASSERT_NE(m, nullptr);
  const hldb::Net *const net = hldb::findByName<hldb::Net>("my_port2", m->getNets());
  ASSERT_NE(net, nullptr) << "23.2.2.3: input port without kind defaults to a net";
  EXPECT_EQ(net->getNetType(), vpiWire);
  ASSERT_NE(net->getTypespec(), nullptr);
  const hldb::ArrayTypespec *const at = net->getTypespec()->getActual<hldb::ArrayTypespec>();
  ASSERT_NE(at, nullptr);
  EXPECT_FALSE(at->getPacked());
  expectRange(at->getRange(), "1", "0");
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(ModPortRangeTest, RangeItfPortMyPort2PortTypeIsPlainPort) {
  const hldb::Port *const p = getPort("range_itf_port", "my_port2");
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getPortType(), vpiPort) << "37.14 detail 1: a non-interface port has vpiPortType vpiPort";
}

// ===========================================================================
// module memory_ctrl1 (mem_if sif1, mem_if.system sif2)
// ===========================================================================

TEST_F(ModPortRangeTest, MemoryCtrl1InterfaceAndModportPorts) {
  const hldb::Port *const sif1 = getPort("memory_ctrl1", "sif1");
  ASSERT_NE(sif1, nullptr);
  EXPECT_EQ(sif1->getPortType(), vpiInterfacePort);
  expectInterfacePort(sif1, getIface("mem_if"), nullptr);

  const hldb::Port *const sif2 = getPort("memory_ctrl1", "sif2");
  ASSERT_NE(sif2, nullptr);
  EXPECT_EQ(sif2->getPortType(), vpiModportPort);
  expectInterfacePort(sif2, getIface("mem_if"), getModport("mem_if", "system"));
}

// ===========================================================================
// module middle (ConnectTB conn1 [1:0])
// ===========================================================================

TEST_F(ModPortRangeTest, MiddleConn1IsInterfacePortArray) {
  const hldb::Port *const p = getPort("middle", "conn1");
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getPortType(), vpiInterfacePort);
  expectInterfacePortArray(p, getIface("ConnectTB"), nullptr);
}

// ===========================================================================
// module range_itf_port2
// ===========================================================================

TEST_F(ModPortRangeTest, RangeItfPort2HasFourPortsInOrder) {
  const hldb::Module *const m = getModule("range_itf_port2");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getPorts(), nullptr);
  ASSERT_EQ(m->getPorts()->size(), 4u);
  EXPECT_EQ(m->getPorts()->at(0)->getName(), "my_port1");
  EXPECT_EQ(m->getPorts()->at(1)->getName(), "my_port2");
  EXPECT_EQ(m->getPorts()->at(2)->getName(), "my_port3");
  EXPECT_EQ(m->getPorts()->at(3)->getName(), "my_port4");
}

TEST_F(ModPortRangeTest, RangeItfPort2PortTypes) {
  const hldb::Interface *const iface = getIface("MyInterface");
  const hldb::Modport *const mp = getModport("MyInterface", "MyModPort");
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(mp, nullptr);

  const hldb::Port *const p1 = getPort("range_itf_port2", "my_port1");
  ASSERT_NE(p1, nullptr);
  EXPECT_EQ(p1->getPortType(), vpiModportPort);
  expectInterfacePort(p1, iface, mp);

  const hldb::Port *const p2 = getPort("range_itf_port2", "my_port2");
  ASSERT_NE(p2, nullptr);
  EXPECT_EQ(p2->getPortType(), vpiModportPort);
  expectInterfacePortArray(p2, iface, mp);

  const hldb::Port *const p3 = getPort("range_itf_port2", "my_port3");
  ASSERT_NE(p3, nullptr);
  EXPECT_EQ(p3->getPortType(), vpiInterfacePort);
  expectInterfacePort(p3, iface, nullptr);

  const hldb::Port *const p4 = getPort("range_itf_port2", "my_port4");
  ASSERT_NE(p4, nullptr);
  EXPECT_EQ(p4->getPortType(), vpiInterfacePort);
  expectInterfacePortArray(p4, iface, nullptr);
}

// ===========================================================================
// Diagnostics: modport names resolve; interface ports need no direction
// ===========================================================================

TEST_F(ModPortRangeTest, ModportIdentifiersInPortHeadersBind) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "MyModPort"), nullptr)
      << "25.5: MyModPort is a modport of MyInterface";
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "system"), nullptr)
      << "25.5: system is a modport of mem_if";
}

TEST_F(ModPortRangeTest, InterfacePortsNotWarnedMissingDirection) {
  for (std::string_view name : {"my_port1", "sif1", "sif2", "conn1", "my_port3", "my_port4"}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_PORT_MISSING_DIRECTION, name), nullptr)
        << name << ": 23.2.2 interface_port_header has no port_direction";
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
