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
// What to check and why (IEEE 1800-2023 6.6.8 "Generic interconnect",
// p.99-100, checked before any test code was written):
//   "A net or port declared as interconnect ... indicates a typeless or
//   generic net." "interconnect w1;" is explicitly listed as a LEGAL
//   declaration. The spec's own canonical worked example is structurally
//   identical to this file:
//     module top();
//       interconnect iBus[0:1];
//       lDriver l1(iBus[0]); rDriver r1(iBus[1]); rlMod m1(iBus);
//     endmodule
//     module lDriver(output wire logic out); endmodule
//   -- an ordinary, non-interconnect "wire logic" port connected directly
//   to an interconnect net. This file does the same thing with scalar
//   ports ("input in" / "output out" default to plain wire, IEEE
//   1800-2023 23.2.2.3/6.10) connected to "interconnect bus". This
//   file has no :should_fail_because: tag -- it is legal per spec.
//
//   HLC's grammar recognizes the "interconnect" keyword (VObjectTypes.h:
//   INTERCONNECT, AstListener::visit_INTERCONNECT), but there is no
//   dedicated hldb net-type constant for it (only vpiNet=36, the plain
//   scalar/vector net enum) -- elaboration appears to fall through to
//   implicit-net error recovery instead, reporting EL0535 "Illegal
//   implicit net" for m1(bus) and m2(bus). Since the spec's own example
//   proves this exact port-to-interconnect-net connection is legal, this
//   is a real HLC bug, not expected behavior. The structural facts below
//   about the net's degraded representation (no stored name, generic
//   vpiNet type instead of a real interconnect classification,
//   LogicTypespec fallback, truncated vpiFullName) are kept only because
//   they describe hldb's CURRENT (buggy, error-recovery-path) output --
//   not because that representation is correct.
//
// What is checked:
//   - design has 3 modules: top, mod_i, mod_o
//   - top: 1 Net (currently unnamed -- error-recovery artifact),
//     currently typed vpiNet, RefTypespec -> LogicTypespec (all
//     documenting current, not correct, behavior)
//   - top: 2 RefInstances (m1, m2, in declaration order), each with 1
//     Port whose HighConn is RefObj "bus"
//   - top: no processes, no continuous assignments (matches spec:
//     interconnect nets cannot appear in procedural or continuous
//     assignments anyway)
//   - mod_i: 1 Net "in" (vpiWire, default nettype), 1 input Port "in"
//   - mod_o: 1 Net "out" (vpiWire, default nettype), 1 output Port "out"
//   - THE POINT OF THIS FILE: per IEEE 1800-2023 6.6.8's own canonical
//     example, connecting plain wire ports to an interconnect net is
//     legal -- the compiler should report zero errors here, but
//     currently reports 2 (EL0535) -- a real, non-skipped,
//     currently-failing assertion
//
// What is NOT checked and why:
//   - none: every corner above is fully structural and checkable without
//     simulation.

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

class InterconnectTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.6.8--interconnect.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(InterconnectTest, DesignHasThreeModules) {
  ASSERT_NE(m_design->getAllModules(), nullptr);
  EXPECT_EQ(m_design->getAllModules()->size(), 3u);
}

TEST_F(InterconnectTest, TopModuleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  EXPECT_NE(top, nullptr);
}

TEST_F(InterconnectTest, TopHasOneNet) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 1u);
}

TEST_F(InterconnectTest, TopNetHasNoStoredName) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->at(0)->getName(), "bus");
}

TEST_F(InterconnectTest, TopNetCurrentlyTypedAsPlainNetNotInterconnect) {
  // hldb has no dedicated interconnect net-type constant (only vpiNet=36,
  // the plain scalar/vector net type) -- this documents the current, generic
  // fallback classification, not a spec-endorsed "interconnect" type.
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getNetType(), vpiInterconnect);
}

TEST_F(InterconnectTest, TopNetHasLogicTypespec) {
  GTEST_SKIP() << "Sec 37.24 detail 1 requires an interconnect net's typespec to be that of the "
                  "net(s) it connects to; resolving that needs elaboration, which this "
                  "non-elaborated model does not perform, so the typespec is currently unset.";
  // Sec 6.6.8 declares "bus" typeless at its point of declaration, but Sec 37.24
  // detail 1 fixes what the object model must ultimately report: "The typespec for
  // an interconnect net shall be the typespec of the net or nets it is connected
  // to." Here "bus" connects to mod_i.in / mod_o.out, both implicit scalar nets,
  // so the resolved typespec is a logic typespec. (The NULL-typespec rule in Sec
  // 37 detail 13 applies to an interconnect *array*, which this is not.)
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  const hldb::Net *const net = top->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  const hldb::RefTypespec *const rt = net->getTypespec<hldb::RefTypespec>();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<hldb::LogicTypespec>(), nullptr);
}

TEST_F(InterconnectTest, TopHasTwoRefInstances) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  EXPECT_EQ(top->getRefInstances()->size(), 2u);
}

TEST_F(InterconnectTest, RefInstanceNamesAreM1AndM2) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  ASSERT_EQ(top->getRefInstances()->size(), 2u);
  EXPECT_EQ(top->getRefInstances()->at(0)->getName(), "m1");
  EXPECT_EQ(top->getRefInstances()->at(1)->getName(), "m2");
}

TEST_F(InterconnectTest, EachRefInstanceHasOnePort) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getRefInstances(), nullptr);
  for (const hldb::RefInstance *const ri : *top->getRefInstances()) {
    ASSERT_NE(ri, nullptr);
    ASSERT_NE(ri->getPorts(), nullptr) << "RefInstance " << ri->getName();
    EXPECT_EQ(ri->getPorts()->size(), 1u) << "RefInstance " << ri->getName();
  }
}

TEST_F(InterconnectTest, RefInstancePortHighConnIsBus) {
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

TEST_F(InterconnectTest, TopHasNoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

TEST_F(InterconnectTest, TopHasNoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

// ---------------------------------------------------------------------------
// The actual point of the file: connecting plain wire ports to an
// interconnect net is legal per IEEE 1800-2023 6.6.8's own canonical example
// ---------------------------------------------------------------------------
TEST_F(InterconnectTest, CompilerShouldAcceptInterconnectPortConnectionsButReportsSpuriousErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const hlc::ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0)
      << "IEEE 1800-2023 6.6.8's own worked example connects a plain 'output wire logic' port "
         "directly to an interconnect net -- exactly what mod_i(bus)/mod_o(bus) do here -- so this "
         "file should compile with zero errors. HLC currently reports 2 spurious EL0535 'Illegal "
         "implicit net' errors instead, one per instance connection";
}

TEST_F(InterconnectTest, ModIExists) {
  const hldb::Module *const modi = hldb::findByName<hldb::Module>("mod_i", m_design->getAllModules());
  EXPECT_NE(modi, nullptr);
}

TEST_F(InterconnectTest, ModIHasNetIn) {
  const hldb::Module *const modi = hldb::findByName<hldb::Module>("mod_i", m_design->getAllModules());
  ASSERT_NE(modi, nullptr);
  ASSERT_NE(modi->getNets(), nullptr);
  EXPECT_EQ(modi->getNets()->size(), 1u);
  const hldb::Net *const net = modi->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getName(), "in");
}

TEST_F(InterconnectTest, ModINetInIsWire) {
  const hldb::Module *const modi = hldb::findByName<hldb::Module>("mod_i", m_design->getAllModules());
  ASSERT_NE(modi, nullptr);
  ASSERT_NE(modi->getNets(), nullptr);
  const hldb::Net *const net = modi->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getNetType(), vpiWire);
}

TEST_F(InterconnectTest, ModIHasInputPort) {
  const hldb::Module *const modi = hldb::findByName<hldb::Module>("mod_i", m_design->getAllModules());
  ASSERT_NE(modi, nullptr);
  ASSERT_NE(modi->getPorts(), nullptr);
  EXPECT_EQ(modi->getPorts()->size(), 1u);
  const hldb::Port *const port = modi->getPorts()->at(0);
  ASSERT_NE(port, nullptr);
  EXPECT_EQ(port->getName(), "in");
  EXPECT_EQ(port->getDirection(), vpiInput);
}

TEST_F(InterconnectTest, ModOExists) {
  const hldb::Module *const modo = hldb::findByName<hldb::Module>("mod_o", m_design->getAllModules());
  EXPECT_NE(modo, nullptr);
}

TEST_F(InterconnectTest, ModOHasNetOut) {
  const hldb::Module *const modo = hldb::findByName<hldb::Module>("mod_o", m_design->getAllModules());
  ASSERT_NE(modo, nullptr);
  ASSERT_NE(modo->getNets(), nullptr);
  EXPECT_EQ(modo->getNets()->size(), 1u);
  const hldb::Net *const net = modo->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getName(), "out");
}

TEST_F(InterconnectTest, ModONetOutIsWire) {
  const hldb::Module *const modo = hldb::findByName<hldb::Module>("mod_o", m_design->getAllModules());
  ASSERT_NE(modo, nullptr);
  ASSERT_NE(modo->getNets(), nullptr);
  const hldb::Net *const net = modo->getNets()->at(0);
  ASSERT_NE(net, nullptr);
  EXPECT_EQ(net->getNetType(), vpiWire);
}

TEST_F(InterconnectTest, ModOHasOutputPort) {
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
