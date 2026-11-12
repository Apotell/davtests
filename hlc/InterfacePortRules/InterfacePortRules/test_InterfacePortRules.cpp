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

// Diagnostics for interface, modport, port and interconnect rules (IEEE 1800-2023 Ch. 23, 25,
// 6.6.8, 13.5, 28.3.6). Each test names the construct of tests/InterfacePortRules/dut.sv it
// checks and the clause that makes it illegal; every event is checked at the offending
// construct by name, line and column.

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

namespace hlc {

class InterfacePortRules : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "InterfacePortRules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ----
// Interconnect (Sec 23.2.2.1, 23.3.3.2, 28.3.6)
// ----

TEST_F(InterfacePortRules, SignedInterconnectPort) {
  // "input interconnect signed [3:0] sp" -- Sec 23.2.2.1; reported at the port.
  EXPECT_NE(findEvent(EventType::CompSignedInterconnectPort, "sp", 5, 51), nullptr);
}

TEST_F(InterfacePortRules, VariableConnectedToInterconnectPort) {
  // "ic_leaf u_var (lv);" -- Sec 23.3.3.2.
  EXPECT_NE(findEvent(EventType::CompIllegalVariableOnInterconnect, "lv", 17, 18), nullptr);
}

TEST_F(InterfacePortRules, InterconnectNetConnectedToVariablePort) {
  // "ic_var_leaf u_icn (icn);" with "input var logic vp" -- Sec 23.3.3.2.
  EXPECT_NE(findEvent(EventType::CompIllegalVariableOnInterconnect, "vp", 18, 22), nullptr);
}

TEST_F(InterfacePortRules, InterconnectWidthDiffersFromInstanceArrayLength) {
  // "ic_bit arr[3:0] (bus3);" with "interconnect [2:0] bus3" -- Sec 28.3.6.
  EXPECT_NE(findEvent(EventType::CompInterconnectInstanceArrayWidth, "bus3", 26, 20), nullptr);
}

// ----
// Port connections (Sec 23.3.2.2, 23.3.3.4, 25.5)
// ----

TEST_F(InterfacePortRules, InterfacePortLeftUnconnected) {
  // "needs_bus u_open ();" -- Sec 23.3.3.4.
  EXPECT_NE(findEvent(EventType::CompUnconnectedInterfacePort, "b", 46, 13), nullptr);
}

TEST_F(InterfacePortRules, InterfacePortConnectedToOtherInterface) {
  // "needs_bus u_wrong (.b(oi));" -- Sec 23.3.3.4: "an interface instance of the identical type".
  EXPECT_NE(findEvent(EventType::CompIllegalInterfacePortConnection, "oi", 47, 25), nullptr);
}

TEST_F(InterfacePortRules, ModportNamesDifferBetweenInstanceAndHeader) {
  // "needs_mp u_mp (.b(bi.sp));" with header "bus_if.mp b" -- Sec 25.5.
  EXPECT_NE(findEvent(EventType::CompIllegalInterfacePortConnection, "bi.sp", 48, 21), nullptr);
}

TEST_F(InterfacePortRules, NamedConnectionToUnknownPort) {
  // ".zz(bi)" -- Sec 23.3.2.2: "The port_name shall be the name specified in the module
  // declaration."
  EXPECT_NE(findEvent(EventType::CompUnknownPort, "zz", 49, 33), nullptr);
}

// ----
// Port declarations (Sec 23.2.2.1, 23.2.2.4, 23.3.2)
// ----

TEST_F(InterfacePortRules, PortOfTheListWithoutDeclaration) {
  // "module undeclared (a, b); input a;" -- Sec 23.2.2.1.
  EXPECT_NE(findEvent(EventType::CompUndeclaredNonAnsiPort, "b", 53, 23), nullptr);
}

TEST_F(InterfacePortRules, InterfaceInstanceWithoutParentheses) {
  // "bus_if bn;" -- Sec 23.3.2: hierarchical_instance ::= name_of_instance ( ... ).
  EXPECT_NE(findEvent(EventType::CompInterfaceInstanceWithoutParentheses, "bn", 59, 10), nullptr);
}

TEST_F(InterfacePortRules, DefaultValueOnInoutPort) {
  // "inout wire io = 1'b0" -- Sec 23.2.2.4: defaults only for input ports.
  EXPECT_NE(findEvent(EventType::CompIllegalDefaultPortValue, "io", 121, 43), nullptr);
}

// ----
// Modports (Sec 25.5, 25.5.4, 25.7, 25.7.4)
// ----

TEST_F(InterfacePortRules, ConstantExpressionForOutputModportPort) {
  // "modport bad (output .Q(2));" -- Sec 25.5.4.
  EXPECT_NE(findEvent(EventType::CompIllegalModportExpression, "Q", 65, 26), nullptr);
}

TEST_F(InterfacePortRules, ImportPrototypeDoesNotMatch) {
  // "import function int f(int y)" for "function int f(int x)" -- Sec 25.7: "The formal argument
  // names in the prototype shall be the same".
  EXPECT_NE(findEvent(EventType::CompModportPrototypeMismatch, "f", 72, 28), nullptr);
}

TEST_F(InterfacePortRules, ImportByNameOfExternSubroutine) {
  // "import t_ext" for "extern forkjoin task t_ext" -- Sec 25.7 / 25.7.4 example: "import requires
  // the full task prototype".
  EXPECT_NE(findEvent(EventType::CompModportImportNeedsPrototype, "t_ext", 73, 31), nullptr);
}

TEST_F(InterfacePortRules, ImportOfUndeclaredSubroutine) {
  // "import nosuch" -- Sec 25.5: names used in a modport are declared by its interface.
  EXPECT_NE(findEvent(EventType::CompModportUndefinedSubroutine, "nosuch", 74, 36), nullptr);
}

TEST_F(InterfacePortRules, CallNeedingDefaultThroughImportByName) {
  // "p.g()" through "import g", g having a default argument -- Sec 25.7.
  EXPECT_NE(findEvent(EventType::CompModportImportNeedsPrototype, "g", 81, 15), nullptr);
}

TEST_F(InterfacePortRules, ExportedSubroutineNotDefined) {
  // "no_export u_none (.p(t));" -- Sec 25.7: the module does not define the exported task.
  EXPECT_NE(findEvent(EventType::CompModportExportUndefined, "w", 90, 24), nullptr);
}

TEST_F(InterfacePortRules, SubroutineExportedTwice) {
  // "has_export u_two (.p(t));" after u_one -- Sec 25.7.4. u_none defines no w, so u_one is the
  // first exporter and only u_two is reported.
  EXPECT_NE(findEvent(EventType::CompMultipleExport, "w", 92, 24), nullptr);
  EXPECT_EQ(findEvents(EventType::CompMultipleExport, 91).size(), 0U);
}

TEST_F(InterfacePortRules, ModportOfHeaderDoesNotExist) {
  // "bus_if.nosuch b" -- Sec 25.5.
  EXPECT_NE(findEvent(EventType::CompUndefinedModport, "nosuch", 97, 28), nullptr);
}

// ----
// Members and calls through an interface (Sec 23.8, 13.5)
// ----

TEST_F(InterfacePortRules, MemberTheInterfaceDoesNotDeclare) {
  // "b.nomember" -- Sec 23.8.
  EXPECT_NE(findEvent(EventType::CompUnknownInterfaceMember, "nomember", 101, 16), nullptr);
}

TEST_F(InterfacePortRules, CallWithMoreArgumentsThanFormals) {
  // "p.one(1, 2)" for "function int one(int a)" -- Sec 13.5.
  EXPECT_NE(findEvent(EventType::CompTooManyArguments, "one", 110, 24), nullptr);
}

// ----
// Specify blocks (Sec 25.6)
// ----

TEST_F(InterfacePortRules, RefPortAsModulePathDestination) {
  // "(i => r) = 1;" with "ref logic r" -- Sec 25.6.
  EXPECT_NE(findEvent(EventType::CompIllegalSpecifyTerminal, "r", 116, 11), nullptr);
}

// ----
// Name spaces (Sec 3.13)
// ----

TEST_F(InterfacePortRules, DuplicateInstanceName) {
  // The second "bus_if same ();".
  EXPECT_NE(findEvent(EventType::CompDuplicateDeclaration, "same", 127, 10), nullptr);
}

TEST_F(InterfacePortRules, DuplicateModportName) {
  // The second "modport m".
  EXPECT_NE(findEvent(EventType::CompDuplicateDeclaration, "m", 132, 11), nullptr);
}

// ----
// Port redeclaration, output initializers, shared signing (Sec 23.2.2.1, 23.2.2.3)
// ----

TEST_F(InterfacePortRules, PortRedeclaredAsInterfacePort) {
  // "output s; bus_if s;" -- Sec 23.2.2.1: no second port declaration of s.
  EXPECT_NE(findEvent(EventType::CompDuplicateDeclaration, "s", 139, 10), nullptr);
}

TEST_F(InterfacePortRules, InitializedNetOutputPort) {
  // "output o = 1'b0" -- an output with no data type is a net (Sec 23.2.2.3).
  EXPECT_NE(findEvent(EventType::CompIllegalDefaultPortValue, "o", 144, 39), nullptr);
}

TEST_F(InterfacePortRules, InitializedOutputInheritingNetKind) {
  // "output wire a, b = 1'b0" -- b inherits a's net kind (Sec 23.2.2.3).
  EXPECT_NE(findEvent(EventType::CompIllegalDefaultPortValue, "b", 146, 50), nullptr);
}

TEST_F(InterfacePortRules, OneSignedForTwoInterconnectPorts) {
  // "input interconnect signed [3:0] p1, p2" -- one "signed" written, reported at the first port.
  EXPECT_NE(findEvent(EventType::CompSignedInterconnectPort, "p1", 150, 56), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
