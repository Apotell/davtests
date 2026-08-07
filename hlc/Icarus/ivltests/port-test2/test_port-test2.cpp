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

// Tests for port-test2.v (tags: Icarus, ivltests, non-ANSI ports)
//
//   module port_3 (dummy_1, /*empty*/, in[7:0], dummy_2, out[7:0], /*empty*/);
//     input [7:0] in; output [7:0] out; output dummy_1; output dummy_2;
//     assign out = in;
//   endmodule
//
// Exercises IEEE 1800-2023 Annex A.1.3 list_of_ports grammar:
//   port ::= port_expression? | . port_identifier ( port_expression? )
//   port_expression ::= port_reference | { port_reference {, port_reference} }
//   port_reference ::= port_identifier constant_select
//
// Two constructs under test:
//  1. Sec 23.2.2.2 "empty port": a bare comma with nothing between reserves a
//     position in the port list that connects to nothing internally. Two of
//     port_3's six ports are empty this way.
//  2. A port_reference naming a bit-select of the internal signal it refers
//     to ("in[7:0]", "out[7:0]") -- here the select happens to restate the
//     full declared width of the underlying 8-bit signal, but it is still,
//     per the grammar, a port_identifier plus a constant_select, not a bare
//     identifier. Phase2ModelBuilder::leavePA_Port/leavePA_Port_reference
//     (src/DesignCompile/Phase2ModelBuilder.cpp) now models this: the
//     port's low-conn is a PartSelect(prefix=RefObj(actual=the Net),
//     range=[7:0]), resolved once the companion net_declaration runs.
//
// Still a known gap (not exercised by this file, grammar/SV3_1aParser.g4:283
// "OPEN_CURLY port_reference (COMMA port_reference)* CLOSE_CURLY"):
// a port_expression made of *multiple* concatenated port_references (e.g.
// ".p({a[3:0], b[7:4]})") is not modeled -- leavePA_Port only attaches a
// single port_reference's low-conn per port, per the TODO in leavePA_Port
// citing this exact file as the single-reference case that *is* handled.
//
//   module port_test;
//     ...
//     port_3 dut_3 (, , data[7:0], , out_3[7:0], );
//     ...
//   endmodule
//
// port_test instantiates port_3 as dut_3 with an all-positional connection
// list that mirrors the same empty-position construct on the instantiation
// side (IEEE 1800-2023 Sec 23.3.2: an empty ordered port connection means no
// connection is made for that port position).
//
// Checked:
//   - port_3 has exactly 6 formal ports, in declared order, with the two
//     "empty" positions unnamed and the four named ones getting direction
//     from their companion declarations elsewhere in the body (bare
//     "output dummy_1;"/"output dummy_2;" -> Sec 23.2.2.3: with neither an
//     explicit net-type keyword nor an explicit data type, an output port
//     still defaults to a net; "input [7:0] in;"/"output [7:0] out;" -> an
//     explicit 8-bit vector net)
//   - port_test has zero of its own ports, and instantiates port_3 (dut_3)
//     with 6 positional connections mirroring the same pattern: empty, empty,
//     "data[7:0]", empty, "out_3[7:0]", empty
//   - "in"/"out"'s port_reference constant_select becomes their low-conn, as
//     a PartSelect resolving back to the companion-declared Net (see
//     Port3InAndOutPortReferenceSelectBecomesLowConn)
//   - compiler emits zero errors
//
// Not checked (known gap -- this file has no "{ref1, ref2}" concatenated
// port_expression to exercise it; see grammar/SV3_1aParser.g4:283 and the
// TODO in Phase2ModelBuilder::leavePA_Port):
//   - a port_expression made of multiple concatenated port_references.
// Not checked (out of scope -- this file's own header comment says it is a
// "compile time test for various port declaration syntax options", not a
// simulation):
//   - the initial-block while-loop / $display / $finish behavior in
//     port_test.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/part_select.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class PortTest2Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "port-test2.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getPort3() {
    return hldb::findByName<hldb::Module>("port_3", m_design->getAllModules());
  }
  static const hldb::Module *getPortTest() {
    return hldb::findByName<hldb::Module>("port_test", m_design->getAllModules());
  }
};

// --- port_3: non-ANSI port list shape ---------------------------------------

TEST_F(PortTest2Test, Port3ModuleExists) { EXPECT_NE(getPort3(), nullptr); }

TEST_F(PortTest2Test, Port3HasSixPortsInDeclaredOrder) {
  const hldb::Module *const port3 = getPort3();
  ASSERT_NE(port3, nullptr);
  ASSERT_NE(port3->getPorts(), nullptr);
  ASSERT_EQ(port3->getPorts()->size(), 6u);

  const char *const kExpectedNames[6] = {"dummy_1", "", "in", "dummy_2", "out", ""};
  for (size_t i = 0; i < 6; ++i) {
    const hldb::Port *const port = port3->getPorts()->at(i);
    ASSERT_NE(port, nullptr) << "port index " << i;
    EXPECT_EQ(port->getName(), kExpectedNames[i]) << "port index " << i;
  }
}

TEST_F(PortTest2Test, Port3EmptyPortsHaveNoConnection) {
  // IEEE 1800-2023 Sec 23.2.2.2: an empty port entry ("port ::= port_expression?"
  // with nothing) reserves a position in the list of ports that is not
  // connected to any internal circuitry within the module.
  const hldb::Module *const port3 = getPort3();
  ASSERT_NE(port3, nullptr);
  ASSERT_EQ(port3->getPorts()->size(), 6u);

  for (const size_t i : {size_t{1}, size_t{5}}) {
    const hldb::Port *const port = port3->getPorts()->at(i);
    ASSERT_NE(port, nullptr) << "port index " << i;
    EXPECT_TRUE(port->getName().empty()) << "port index " << i;
    EXPECT_EQ(port->getLowConn(), nullptr) << "port index " << i;
    EXPECT_EQ(port->getHighConn(), nullptr) << "port index " << i;
  }
}

TEST_F(PortTest2Test, Port3EmptyPortsHaveNoDirection) {
  // IEEE 1800-2023 Sec 23.2.2.2: an empty port position has no internal
  // connection, so it has no direction either -- vpiNoDirection.
  const hldb::Module *const port3 = getPort3();
  ASSERT_NE(port3, nullptr);
  for (const size_t i : {size_t{1}, size_t{5}}) {
    const hldb::Port *const port = port3->getPorts()->at(i);
    ASSERT_NE(port, nullptr) << "port index " << i;
    EXPECT_EQ(port->getDirection(), vpiNoDirection) << "port index " << i;
  }
}

TEST_F(PortTest2Test, Port3NamedPortsGetDirectionFromCompanionDeclarations) {
  const hldb::Module *const port3 = getPort3();
  ASSERT_NE(port3, nullptr);

  const hldb::Port *const dummy1 = hldb::findByName<hldb::Port>("dummy_1", port3->getPorts());
  ASSERT_NE(dummy1, nullptr);
  EXPECT_EQ(dummy1->getDirection(), vpiOutput);

  const hldb::Port *const dummy2 = hldb::findByName<hldb::Port>("dummy_2", port3->getPorts());
  ASSERT_NE(dummy2, nullptr);
  EXPECT_EQ(dummy2->getDirection(), vpiOutput);

  const hldb::Port *const in = hldb::findByName<hldb::Port>("in", port3->getPorts());
  ASSERT_NE(in, nullptr);
  EXPECT_EQ(in->getDirection(), vpiInput);

  const hldb::Port *const out = hldb::findByName<hldb::Port>("out", port3->getPorts());
  ASSERT_NE(out, nullptr);
  EXPECT_EQ(out->getDirection(), vpiOutput);
}

TEST_F(PortTest2Test, Port3InAndOutAreEightBitWireNets) {
  const hldb::Module *const port3 = getPort3();
  ASSERT_NE(port3, nullptr);

  for (const char *const name : {"in", "out"}) {
    const hldb::Port *const port = hldb::findByName<hldb::Port>(name, port3->getPorts());
    ASSERT_NE(port, nullptr) << name;

    // "in"/"out" are port_references with a constant_select ("in[7:0]",
    // "out[7:0]"), so low-conn is a PartSelect, not a bare RefObj wrapper --
    // see Port3InAndOutPortReferenceSelectBecomesLowConn below for that
    // shape. Go through the PartSelect's own prefix to reach the Net.
    const hldb::PartSelect *const lowConn = port->getLowConn<hldb::PartSelect>();
    ASSERT_NE(lowConn, nullptr) << name;
    const hldb::RefObj *const prefix = lowConn->getPrefix<hldb::RefObj>();
    ASSERT_NE(prefix, nullptr) << name;
    const hldb::Net *const net = prefix->getActual<hldb::Net>();
    ASSERT_NE(net, nullptr) << name;
    EXPECT_EQ(net->getNetType(), vpiWire) << name;
    EXPECT_FALSE(net->getScalar()) << name;
    EXPECT_TRUE(net->getVector()) << name;

    const hldb::LogicTypespec *const ts = net->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(ts, nullptr) << name;
    ASSERT_NE(ts->getRanges(), nullptr) << name;
    EXPECT_EQ(ts->getRanges()->at(0)->getLeftExpr<hldb::Constant>()->getDecompile(), "7") << name;
    EXPECT_EQ(ts->getRanges()->at(0)->getRightExpr<hldb::Constant>()->getDecompile(), "0") << name;
  }
}

TEST_F(PortTest2Test, Port3DummyPortsAreScalarWireNets) {
  // "output dummy_1;"/"output dummy_2;" give no net-type keyword and no
  // explicit data type at all -- IEEE 1800-2023 Sec 23.2.2.3: with neither
  // given, an output port's kind still defaults to a net (only an explicit
  // data type with no net keyword makes an output default to a variable).
  const hldb::Module *const port3 = getPort3();
  ASSERT_NE(port3, nullptr);

  for (const char *const name : {"dummy_1", "dummy_2"}) {
    const hldb::Port *const port = hldb::findByName<hldb::Port>(name, port3->getPorts());
    ASSERT_NE(port, nullptr) << name;

    const hldb::RefObj *const lowConn = port->getLowConn<hldb::RefObj>();
    ASSERT_NE(lowConn, nullptr) << name;
    const hldb::Net *const net = lowConn->getActual<hldb::Net>();
    ASSERT_NE(net, nullptr) << name;
    EXPECT_EQ(net->getNetType(), vpiWire) << name;
    EXPECT_TRUE(net->getScalar()) << name;
    EXPECT_FALSE(net->getVector()) << name;
  }
}

TEST_F(PortTest2Test, Port3InAndOutPortReferenceSelectBecomesLowConn) {
  // Annex A.1.3 grammar: port_reference ::= port_identifier constant_select --
  // "in[7:0]"/"out[7:0]" in port_3's own port list are each a port_identifier
  // plus a constant_select, not a bare identifier. Per Sec 23.2.2.2 the
  // select narrows which bits of the module-internal signal the port formal
  // refers to (here happening to restate the full declared width). The
  // port's low-conn must be a PartSelect(prefix=RefObj(actual=the Net),
  // range=[7:0]), not a bare select-less reference to the Net.
  const hldb::Module *const port3 = getPort3();
  ASSERT_NE(port3, nullptr);

  for (const char *const name : {"in", "out"}) {
    const hldb::Port *const port = hldb::findByName<hldb::Port>(name, port3->getPorts());
    ASSERT_NE(port, nullptr) << name;

    const hldb::PartSelect *const sel = port->getLowConn<hldb::PartSelect>();
    ASSERT_NE(sel, nullptr) << name;

    const hldb::RefObj *const prefix = sel->getPrefix<hldb::RefObj>();
    ASSERT_NE(prefix, nullptr) << name;
    EXPECT_EQ(prefix->getName(), name) << name;
    const hldb::Net *const net = prefix->getActual<hldb::Net>();
    ASSERT_NE(net, nullptr) << name;
    EXPECT_EQ(net->getName(), name) << name;

    ASSERT_NE(sel->getRange(), nullptr) << name;
    EXPECT_EQ(sel->getRange()->getLeftExpr<hldb::Constant>()->getDecompile(), "7") << name;
    EXPECT_EQ(sel->getRange()->getRightExpr<hldb::Constant>()->getDecompile(), "0") << name;
  }
}

// --- port_test: instantiation with positional (incl. empty) connections ----

TEST_F(PortTest2Test, PortTestModuleExistsWithNoOwnPorts) {
  const hldb::Module *const portTest = getPortTest();
  ASSERT_NE(portTest, nullptr);
  EXPECT_EQ(portTest->getPorts(), nullptr);
}

TEST_F(PortTest2Test, PortTestInstantiatesPort3AsDut3) {
  const hldb::Module *const portTest = getPortTest();
  ASSERT_NE(portTest, nullptr);
  ASSERT_NE(portTest->getRefInstances(), nullptr);

  const hldb::RefInstance *const dut3 = hldb::findByName<hldb::RefInstance>("dut_3", portTest->getRefInstances());
  ASSERT_NE(dut3, nullptr);
  ASSERT_NE(dut3->getTypespec(), nullptr);

  const hldb::ModuleTypespec *const mts = dut3->getTypespec()->getActual<hldb::ModuleTypespec>();
  ASSERT_NE(mts, nullptr);
  EXPECT_EQ(mts->getModule(), getPort3());
}

TEST_F(PortTest2Test, PortTestDut3HasSixPositionalConnectionsMatchingPort3) {
  // IEEE 1800-2023 Sec 23.3.2: an empty ordered port connection means no
  // connection is made for that port position, mirroring port_3's own two
  // empty formal-port positions.
  const hldb::Module *const portTest = getPortTest();
  ASSERT_NE(portTest, nullptr);
  const hldb::RefInstance *const dut3 = hldb::findByName<hldb::RefInstance>("dut_3", portTest->getRefInstances());
  ASSERT_NE(dut3, nullptr);
  ASSERT_NE(dut3->getPorts(), nullptr);
  ASSERT_EQ(dut3->getPorts()->size(), 6u);

  for (const size_t i : {size_t{0}, size_t{1}, size_t{3}, size_t{5}}) {
    const hldb::Port *const conn = any_cast<hldb::Port>(dut3->getPorts()->at(i));
    ASSERT_NE(conn, nullptr) << "connection index " << i;
    EXPECT_EQ(conn->getLowConn(), nullptr) << "connection index " << i;
    EXPECT_EQ(conn->getHighConn(), nullptr) << "connection index " << i;
  }
}

TEST_F(PortTest2Test, PortTestDut3ConnectsDataBitsToIn) {
  const hldb::Module *const portTest = getPortTest();
  ASSERT_NE(portTest, nullptr);
  const hldb::RefInstance *const dut3 = hldb::findByName<hldb::RefInstance>("dut_3", portTest->getRefInstances());
  ASSERT_NE(dut3, nullptr);
  ASSERT_NE(dut3->getPorts(), nullptr);
  ASSERT_EQ(dut3->getPorts()->size(), 6u);

  const hldb::Port *const conn = any_cast<hldb::Port>(dut3->getPorts()->at(2));
  ASSERT_NE(conn, nullptr);
  const hldb::PartSelect *const sel = conn->getHighConn<hldb::PartSelect>();
  ASSERT_NE(sel, nullptr);
  EXPECT_EQ(sel->getPrefix<hldb::RefObj>()->getName(), "data");
  ASSERT_NE(sel->getRange(), nullptr);
  EXPECT_EQ(sel->getRange()->getLeftExpr<hldb::Constant>()->getDecompile(), "7");
  EXPECT_EQ(sel->getRange()->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

TEST_F(PortTest2Test, PortTestDut3ConnectsOut3BitsToOut) {
  const hldb::Module *const portTest = getPortTest();
  ASSERT_NE(portTest, nullptr);
  const hldb::RefInstance *const dut3 = hldb::findByName<hldb::RefInstance>("dut_3", portTest->getRefInstances());
  ASSERT_NE(dut3, nullptr);
  ASSERT_NE(dut3->getPorts(), nullptr);
  ASSERT_EQ(dut3->getPorts()->size(), 6u);

  const hldb::Port *const conn = any_cast<hldb::Port>(dut3->getPorts()->at(4));
  ASSERT_NE(conn, nullptr);
  const hldb::PartSelect *const sel = conn->getHighConn<hldb::PartSelect>();
  ASSERT_NE(sel, nullptr);
  EXPECT_EQ(sel->getPrefix<hldb::RefObj>()->getName(), "out_3");
  ASSERT_NE(sel->getRange(), nullptr);
  EXPECT_EQ(sel->getRange()->getLeftExpr<hldb::Constant>()->getDecompile(), "7");
  EXPECT_EQ(sel->getRange()->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

// --- compiler diagnostics ----------------------------------------------------

TEST_F(PortTest2Test, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
  EXPECT_EQ(stats.nbWarning, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
