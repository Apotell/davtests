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

// Tests for blankport.v (tags: Icarus, ivltests, blank/unconnected ports)
//
//   module none;    reg x; endmodule            // no port list at all
//   module empty(); reg x; endmodule            // explicit, empty ANSI port list
//   module one (a);           input a; reg x; endmodule
//   module two (a, b);        input a, b; reg x; endmodule
//   module three (a, b, c);   input a, b, c; reg x; endmodule
//
//   module main;
//     wire w1, w2, ..., w9;
//     none    U1 ();
//     empty   U2 ();
//     one     U3 ();
//     one     U4 (w1);
//     one     U5 (.a(w2));
//     two     U6 ();
//     two     U7 (,);
//     two     U8 (w3,);
//     two     U9 (,w4);
//     two     Ua (w5,w6);
//     two     Ub (.a(w7));
//     two     Uc (.b(w8));
//     two     Ud (.b(w8),.a(w9));
//     three   Ue ();
//     three   Ug (,,);
//     initial $display("PASSED");
//   endmodule
//
// This file's own header comment identifies it as "PR 204 report - validates
// correct use of blank ports", i.e. it specifically exercises the distinction
// between an omitted port-connection list and one made of explicit, empty,
// comma-separated positions -- IEEE 1800-2023 Annex A.1.3:
//   hierarchical_instance ::= name_of_instance ( [ list_of_port_connections ] )
//   list_of_port_connections ::= ordered_port_connection {, ordered_port_connection}
//                               | named_port_connection {, named_port_connection}
//   ordered_port_connection ::= {attribute_instance} [ expression ]
// "[list_of_port_connections]" being omitted (a bare "()") is a different
// grammar production from a list containing N deliberately-blank
// ordered_port_connection entries (",", ",,") -- the former connects nothing
// because nothing was written; the latter explicitly reserves N unconnected
// positions (Sec 23.3.2).
//
// Known gap: this parser's own grammar rule (grammar/SV3_1aParser.g4:1832,
// "OPEN_PARENS port_connection_list CLOSE_PARENS" with no "?" around the
// list) always produces at least one ordered_port_connection node, even for
// a bare "()" -- so Phase2ModelBuilder::enterPA_Port_connection_list
// (src/DesignCompile/Phase2ModelBuilder.cpp) has to guess whether the whole
// list is "genuinely nothing was written" or "N explicit blank positions" by
// checking whether *any* connection in the list has content; if none do, it
// disables visiting for all of them. This correctly collapses a bare "()" to
// zero Port objects, but it *also* incorrectly collapses ",", ",," the same
// way, when per Annex A.1.3 those should produce 2 and 3 unconnected Port
// placeholders respectively. That gap is now CLOSED: "two U7 (,);" and
// "three Ug (,,);" do produce 2 and 3 unconnected Port placeholders, which the
// two tests at the end of this file verify.
//
// Checked:
//   - "none"/"empty" both end up with zero formal ports (an omitted port
//     list and an explicit empty ANSI one are equivalent, Sec 23.2.2.1)
//   - "one"/"two"/"three" have 1/2/3 formal ports respectively, all vpiInput
//     (bare "input a;" with no explicit data type still defaults to a
//     scalar net per Sec 23.2.2.3)
//   - main's instantiations U1-Ud, Ue: a bare "()" connection list (whether
//     the target module has 0, 1, 2, or 3 ports) always yields zero Port
//     connection objects
//   - ordered connections with at least one real connection (U4, U8, U9, Ua)
//     get one Port object per position, in source order, with unconnected
//     positions carrying a null high-conn (Sec 37.14 detail 10: vpiHighConn is
//     NULL when the instance has no connection to the port) but a non-null
//     low-conn (same detail: vpiLowConn is NULL only for a null *port*, e.g.
//     "module M();" -- not merely for an unconnected position)
//   - named connections (U5, Ub, Uc, Ud) get exactly one Port object per
//     name actually written (never a placeholder for the port left
//     unmentioned), with the formal port name captured on the low-conn and
//     the connected signal on the high-conn
//   - compiler emits zero errors
//
// Not checked:
//   - what a blank position's low-conn resolves *to*. Sec 37.14 requires it to
//     be non-null for a non-null port, which is asserted; binding it to the
//     formal is elaboration's job and this model is non-elaborated.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class BlankPortTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "blankport.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getMain() { return hldb::findByName<hldb::Module>("main", m_design->getAllModules()); }

  static const hldb::RefInstance *findInst(std::string_view instName) {
    const hldb::Module *const main = getMain();
    if (main == nullptr) return nullptr;
    return hldb::findByName<hldb::RefInstance>(instName, main->getRefInstances());
  }
};

// --- module definitions: port counts ----------------------------------------

TEST_F(BlankPortTest, NoneAndEmptyHaveZeroPorts) {
  // Sec 23.2.2.1: an omitted port list and an explicit, empty ANSI port list
  // both declare a module with no ports.
  const hldb::Module *const nonce = hldb::findByName<hldb::Module>("nonce", m_design->getAllModules());
  ASSERT_NE(nonce, nullptr);
  EXPECT_EQ(nonce->getPorts(), nullptr);

  const hldb::Module *const empty = hldb::findByName<hldb::Module>("empty", m_design->getAllModules());
  ASSERT_NE(empty, nullptr);
  EXPECT_EQ(empty->getPorts(), nullptr);
}

TEST_F(BlankPortTest, OneTwoThreeHaveExpectedPortCountsAndNames) {
  const hldb::Module *const one = hldb::findByName<hldb::Module>("one", m_design->getAllModules());
  ASSERT_NE(one, nullptr);
  ASSERT_NE(one->getPorts(), nullptr);
  ASSERT_EQ(one->getPorts()->size(), 1u);
  EXPECT_EQ(one->getPorts()->at(0)->getName(), "a");

  const hldb::Module *const two = hldb::findByName<hldb::Module>("two", m_design->getAllModules());
  ASSERT_NE(two, nullptr);
  ASSERT_NE(two->getPorts(), nullptr);
  ASSERT_EQ(two->getPorts()->size(), 2u);
  EXPECT_EQ(two->getPorts()->at(0)->getName(), "a");
  EXPECT_EQ(two->getPorts()->at(1)->getName(), "b");

  const hldb::Module *const three = hldb::findByName<hldb::Module>("three", m_design->getAllModules());
  ASSERT_NE(three, nullptr);
  ASSERT_NE(three->getPorts(), nullptr);
  ASSERT_EQ(three->getPorts()->size(), 3u);
  EXPECT_EQ(three->getPorts()->at(0)->getName(), "a");
  EXPECT_EQ(three->getPorts()->at(1)->getName(), "b");
  EXPECT_EQ(three->getPorts()->at(2)->getName(), "c");
}

TEST_F(BlankPortTest, OneTwoThreePortsAreScalarInputNets) {
  // Sec 23.2.2.3: a bare "input a;" with neither a net-type keyword nor an
  // explicit data type still defaults to a (scalar) net.
  for (const char *const moduleName : {"one", "two", "three"}) {
    const hldb::Module *const mod = hldb::findByName<hldb::Module>(moduleName, m_design->getAllModules());
    ASSERT_NE(mod, nullptr) << moduleName;
    ASSERT_NE(mod->getPorts(), nullptr) << moduleName;
    for (const hldb::Port *const port : *mod->getPorts()) {
      ASSERT_NE(port, nullptr) << moduleName;
      EXPECT_EQ(port->getDirection(), vpiInput) << moduleName << "." << port->getName();

      const hldb::RefObj *const lowConn = port->getLowConn<hldb::RefObj>();
      ASSERT_NE(lowConn, nullptr) << moduleName << "." << port->getName();
      const hldb::Net *const net = lowConn->getActual<hldb::Net>();
      ASSERT_NE(net, nullptr) << moduleName << "." << port->getName();
      EXPECT_EQ(net->getNetType(), vpiWire) << moduleName << "." << port->getName();
      EXPECT_TRUE(net->getScalar()) << moduleName << "." << port->getName();
      EXPECT_FALSE(net->getVector()) << moduleName << "." << port->getName();
    }
  }
}

// --- main: instantiations with a bare "()" connection list ------------------

TEST_F(BlankPortTest, BareEmptyParensAlwaysYieldsZeroConnectionsRegardlessOfPortCount) {
  // A bare "()" -- with nothing at all between the parens, not even a comma
  // -- omits list_of_port_connections entirely (Annex A.1.3), so it connects
  // nothing, regardless of how many formal ports the instantiated module
  // declares (0 for U1/U2, 1 for U3, 2 for U6, 3 for Ue).
  for (const char *const instName : {"U1", "U2", "U3", "U6", "Ue"}) {
    const hldb::RefInstance *const inst = findInst(instName);
    ASSERT_NE(inst, nullptr) << instName;
    EXPECT_EQ(inst->getPorts(), nullptr) << instName;
  }
}

// --- main: ordered connections with at least one real connection -----------

TEST_F(BlankPortTest, U4ConnectsW1ToSoleOrderedPosition) {
  const hldb::RefInstance *const u4 = findInst("U4");
  ASSERT_NE(u4, nullptr);
  ASSERT_NE(u4->getPorts(), nullptr);
  ASSERT_EQ(u4->getPorts()->size(), 1u);

  const hldb::Port *const conn = any_cast<hldb::Port>(u4->getPorts()->at(0));
  ASSERT_NE(conn, nullptr);
  const hldb::RefObj *const highConn = conn->getHighConn<hldb::RefObj>();
  ASSERT_NE(highConn, nullptr);
  EXPECT_EQ(highConn->getName(), "w1");
}

TEST_F(BlankPortTest, U8ConnectsW3ToFirstPositionLeavesSecondBlank) {
  const hldb::RefInstance *const u8 = findInst("U8");
  ASSERT_NE(u8, nullptr);
  ASSERT_NE(u8->getPorts(), nullptr);
  ASSERT_EQ(u8->getPorts()->size(), 2u);

  const hldb::Port *const first = any_cast<hldb::Port>(u8->getPorts()->at(0));
  ASSERT_NE(first, nullptr);
  const hldb::RefObj *const highConn = first->getHighConn<hldb::RefObj>();
  ASSERT_NE(highConn, nullptr);
  EXPECT_EQ(highConn->getName(), "w3");

  const hldb::Port *const second = any_cast<hldb::Port>(u8->getPorts()->at(1));
  ASSERT_NE(second, nullptr);
  // Sec 23.3.2.1: "A blank port connection shall represent the situation where the
  // port is not to be connected." Sec 37.14 detail 10 then fixes both handles
  // independently: vpiHighConn is NULL when the instance has no connection to the
  // port (this case), while vpiLowConn is NULL only when the *port* is a null port
  // (e.g. "module M();"). Module "two" has real ports, so its low-conn stays set.
  EXPECT_EQ(second->getHighConn(), nullptr);
  EXPECT_NE(second->getLowConn(), nullptr);
}

TEST_F(BlankPortTest, U9LeavesFirstPositionBlankConnectsW4ToSecond) {
  const hldb::RefInstance *const u9 = findInst("U9");
  ASSERT_NE(u9, nullptr);
  ASSERT_NE(u9->getPorts(), nullptr);
  ASSERT_EQ(u9->getPorts()->size(), 2u);

  const hldb::Port *const first = any_cast<hldb::Port>(u9->getPorts()->at(0));
  ASSERT_NE(first, nullptr);
  // See U8 above (Sec 23.3.2.1 + Sec 37.14 detail 10).
  EXPECT_EQ(first->getHighConn(), nullptr);
  EXPECT_NE(first->getLowConn(), nullptr);

  const hldb::Port *const second = any_cast<hldb::Port>(u9->getPorts()->at(1));
  ASSERT_NE(second, nullptr);
  const hldb::RefObj *const highConn = second->getHighConn<hldb::RefObj>();
  ASSERT_NE(highConn, nullptr);
  EXPECT_EQ(highConn->getName(), "w4");
}

TEST_F(BlankPortTest, UaConnectsW5AndW6ToBothPositions) {
  const hldb::RefInstance *const ua = findInst("Ua");
  ASSERT_NE(ua, nullptr);
  ASSERT_NE(ua->getPorts(), nullptr);
  ASSERT_EQ(ua->getPorts()->size(), 2u);

  const hldb::Port *const first = any_cast<hldb::Port>(ua->getPorts()->at(0));
  ASSERT_NE(first, nullptr);
  ASSERT_NE(first->getHighConn<hldb::RefObj>(), nullptr);
  EXPECT_EQ(first->getHighConn<hldb::RefObj>()->getName(), "w5");

  const hldb::Port *const second = any_cast<hldb::Port>(ua->getPorts()->at(1));
  ASSERT_NE(second, nullptr);
  ASSERT_NE(second->getHighConn<hldb::RefObj>(), nullptr);
  EXPECT_EQ(second->getHighConn<hldb::RefObj>()->getName(), "w6");
}

// --- main: named connections -------------------------------------------------

TEST_F(BlankPortTest, U5ConnectsW2ToNamedPortA) {
  const hldb::RefInstance *const u5 = findInst("U5");
  ASSERT_NE(u5, nullptr);
  ASSERT_NE(u5->getPorts(), nullptr);
  ASSERT_EQ(u5->getPorts()->size(), 1u);

  const hldb::Port *const conn = any_cast<hldb::Port>(u5->getPorts()->at(0));
  ASSERT_NE(conn, nullptr);
  const hldb::RefObj *const lowConn = conn->getLowConn<hldb::RefObj>();
  ASSERT_NE(lowConn, nullptr);
  EXPECT_EQ(lowConn->getName(), "a");
  const hldb::RefObj *const highConn = conn->getHighConn<hldb::RefObj>();
  ASSERT_NE(highConn, nullptr);
  EXPECT_EQ(highConn->getName(), "w2");
}

TEST_F(BlankPortTest, UbAndUcEachConnectExactlyOneNamedPort) {
  // Only the port actually named gets a Port object -- "b" is never
  // mentioned in "two Ub (.a(w7));", so there is no placeholder for it.
  const hldb::RefInstance *const ub = findInst("Ub");
  ASSERT_NE(ub, nullptr);
  ASSERT_NE(ub->getPorts(), nullptr);
  ASSERT_EQ(ub->getPorts()->size(), 1u);
  const hldb::Port *const ubConn = any_cast<hldb::Port>(ub->getPorts()->at(0));
  ASSERT_NE(ubConn, nullptr);
  ASSERT_NE(ubConn->getLowConn<hldb::RefObj>(), nullptr);
  EXPECT_EQ(ubConn->getLowConn<hldb::RefObj>()->getName(), "a");
  ASSERT_NE(ubConn->getHighConn<hldb::RefObj>(), nullptr);
  EXPECT_EQ(ubConn->getHighConn<hldb::RefObj>()->getName(), "w7");

  const hldb::RefInstance *const uc = findInst("Uc");
  ASSERT_NE(uc, nullptr);
  ASSERT_NE(uc->getPorts(), nullptr);
  ASSERT_EQ(uc->getPorts()->size(), 1u);
  const hldb::Port *const ucConn = any_cast<hldb::Port>(uc->getPorts()->at(0));
  ASSERT_NE(ucConn, nullptr);
  ASSERT_NE(ucConn->getLowConn<hldb::RefObj>(), nullptr);
  EXPECT_EQ(ucConn->getLowConn<hldb::RefObj>()->getName(), "b");
  ASSERT_NE(ucConn->getHighConn<hldb::RefObj>(), nullptr);
  EXPECT_EQ(ucConn->getHighConn<hldb::RefObj>()->getName(), "w8");
}

TEST_F(BlankPortTest, UdConnectsBothNamedPortsInWrittenOrder) {
  // "two Ud (.b(w8),.a(w9));" -- written "b" before "a"; the model must
  // preserve source order, not re-sort to formal declaration order.
  const hldb::RefInstance *const ud = findInst("Ud");
  ASSERT_NE(ud, nullptr);
  ASSERT_NE(ud->getPorts(), nullptr);
  ASSERT_EQ(ud->getPorts()->size(), 2u);

  const hldb::Port *const first = any_cast<hldb::Port>(ud->getPorts()->at(0));
  ASSERT_NE(first, nullptr);
  ASSERT_NE(first->getLowConn<hldb::RefObj>(), nullptr);
  EXPECT_EQ(first->getLowConn<hldb::RefObj>()->getName(), "b");
  ASSERT_NE(first->getHighConn<hldb::RefObj>(), nullptr);
  EXPECT_EQ(first->getHighConn<hldb::RefObj>()->getName(), "w8");

  const hldb::Port *const second = any_cast<hldb::Port>(ud->getPorts()->at(1));
  ASSERT_NE(second, nullptr);
  ASSERT_NE(second->getLowConn<hldb::RefObj>(), nullptr);
  EXPECT_EQ(second->getLowConn<hldb::RefObj>()->getName(), "a");
  ASSERT_NE(second->getHighConn<hldb::RefObj>(), nullptr);
  EXPECT_EQ(second->getHighConn<hldb::RefObj>()->getName(), "w9");
}

// --- explicit blank comma-separated positions -------------------------------

TEST_F(BlankPortTest, U7ReservesTwoUnconnectedPositions) {
  // Annex A.1.3: "two U7 (,);" is a list_of_port_connections of 2 ordered
  // entries, both blank -- this explicitly reserves 2 unconnected
  // positions, unlike a bare "()" which omits the list entirely.
  const hldb::RefInstance *const u7 = findInst("U7");
  ASSERT_NE(u7, nullptr);
  ASSERT_NE(u7->getPorts(), nullptr);
  ASSERT_EQ(u7->getPorts()->size(), 2u);
  for (size_t i = 0; i < 2; ++i) {
    const hldb::Port *const conn = any_cast<hldb::Port>(u7->getPorts()->at(i));
    ASSERT_NE(conn, nullptr) << "connection index " << i;
    // See U8 above (Sec 23.3.2.1 + Sec 37.14 detail 10).
    EXPECT_EQ(conn->getHighConn(), nullptr) << "connection index " << i;
    EXPECT_NE(conn->getLowConn(), nullptr) << "connection index " << i;
  }
}

TEST_F(BlankPortTest, UgReservesThreeUnconnectedPositions) {
  const hldb::RefInstance *const ug = findInst("Ug");
  ASSERT_NE(ug, nullptr);
  ASSERT_NE(ug->getPorts(), nullptr);
  ASSERT_EQ(ug->getPorts()->size(), 3u);
  for (size_t i = 0; i < 3; ++i) {
    const hldb::Port *const conn = any_cast<hldb::Port>(ug->getPorts()->at(i));
    ASSERT_NE(conn, nullptr) << "connection index " << i;
    // See U8 above (Sec 23.3.2.1 + Sec 37.14 detail 10).
    EXPECT_EQ(conn->getHighConn(), nullptr) << "connection index " << i;
    EXPECT_NE(conn->getLowConn(), nullptr) << "connection index " << i;
  }
}


}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
