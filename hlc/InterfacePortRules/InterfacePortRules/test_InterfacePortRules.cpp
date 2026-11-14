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

#include <hldb/Utils.h>
#include <hldb/hldb.h>
#include <hldb/vpi_user.h>

#include <map>
#include <string_view>

namespace hlc {

class InterfacePortRules : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "InterfacePortRules.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::Module *findModule(std::string_view name) const {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }
  const hldb::Program *findProgram(std::string_view name) const {
    return hldb::findByDefName<hldb::Program>(name, m_design->getAllPrograms());
  }
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

// ----
// Events first raised on this branch (Sec 10.11, 23.6, 25.3, 25.5.5, 23.3.3.2, 25.5, 11.2)
// ----

TEST_F(InterfacePortRules, AliasOfNetsOfDifferentTypes) {
  // "wire a; tri1 b; alias a = b;" -- Sec 10.11: aliased nets shall be of the same net type.
  EXPECT_NE(findEvent(EventType::CompIllegalAlias, "b", 154, 45), nullptr);
}

TEST_F(InterfacePortRules, InstanceArrayIndexOutOfRange) {
  // "ipr_ii arr[0:3](); ... arr[4].s" -- Sec 23.6: the index shall be a legal index value.
  EXPECT_NE(findEvent(EventType::CompIndexOutOfRange, "arr[4]", 156, 55), nullptr);
}

TEST_F(InterfacePortRules, ModuleInstantiatedInInterface) {
  // "interface ipr_hasmod; ipr_leaf u(); endinterface" -- Sec 25.3: "modules can neither be
  // declared nor instantiated in interfaces."
  EXPECT_NE(findEvent(EventType::CompIllegalInstantiation, "u", 158, 32), nullptr);
}

TEST_F(InterfacePortRules, ModportNamesUndeclaredClockingBlock) {
  // "modport m(clocking nocb);" -- Sec 25.5.5: the clocking block shall be declared by the
  // modport's interface.
  EXPECT_NE(findEvent(EventType::CompUndefinedClockingBlock, "nocb", 159, 38), nullptr);
}

TEST_F(InterfacePortRules, VariableConnectedToInoutPort) {
  // "logic v; ipr_io u(.p(v));" with "inout wire p" -- Sec 23.3.3.2: a variable shall not be
  // connected to either side of an inout port.
  EXPECT_NE(findEvent(EventType::CompIllegalVariableOnInoutPort, "p", 161, 38), nullptr);
}

TEST_F(InterfacePortRules, AssignmentThroughModportInput) {
  // "initial p.d = 1;" through "modport m(input d)" -- Sec 25.5 with Sec 23.3.3.2: assignments to
  // variables declared as input ports shall be illegal.
  EXPECT_NE(findEvent(EventType::CompAssignmentToInputPort, "p.d", 163, 36), nullptr);
}

TEST_F(InterfacePortRules, InterfaceInstanceAsContinuousAssignmentOperand) {
  // "assign w = x;" with x an interface instance -- an interface instance is not a value
  // (Sec 11.2; only a virtual interface holds one, Sec 25.9).
  EXPECT_NE(findEvent(EventType::CompIllegalExpressionContext, "x", 165, 48), nullptr);
}

// ----
// Non-ANSI port declarations (Sec 23.2.2.1)
// ----

TEST_F(InterfacePortRules, InterfacePortDeclaredTwice) {
  // "ipr_if p; ipr_if p;" -- reported at the second declaration.
  EXPECT_NE(findEvent(EventType::CompDuplicateDeclaration, "p", 169, 39), nullptr);
}

TEST_F(InterfacePortRules, InputPortDeclaredTwice) {
  // "input q; input q;" -- the standard's own example ("Error - multiple declaration").
  EXPECT_NE(findEvent(EventType::CompDuplicateDeclaration, "q", 170, 37), nullptr);
}

TEST_F(InterfacePortRules, PortExpressionIdentifiersNeedPortDeclarations) {
  // "module ipr_undecl(.p(a), b[1:0], {c, d}); input c; wire a; wire [3:0] b;" -- every
  // port_identifier of a port expression, explicit, selected or concatenated, shall be declared
  // as a port.
  EXPECT_NE(findEvent(EventType::CompUndeclaredNonAnsiPort, "a", 171, 22), nullptr);
  EXPECT_NE(findEvent(EventType::CompUndeclaredNonAnsiPort, "b", 171, 26), nullptr);
  EXPECT_NE(findEvent(EventType::CompUndeclaredNonAnsiPort, "d", 171, 38), nullptr);
}

TEST_F(InterfacePortRules, VariableConnectedToGenericInterfacePort) {
  // "ipr_gen u(.p(o));" with "interface p" -- Sec 23.3.3.4: an interface port is connected to an
  // interface instance.
  EXPECT_NE(findEvent(EventType::CompIllegalInterfacePortConnection, "o", 174, 42), nullptr);
}

TEST_F(InterfacePortRules, DefaultOnOutputWithImplicitDataType) {
  // "output var integer x, [3:0] y = 0" -- y's implicit data type makes it an output net
  // (Sec 23.2.2.3), which cannot be initialized.
  EXPECT_NE(findEvent(EventType::CompIllegalDefaultPortValue, "y", 176, 47), nullptr);
}

TEST_F(InterfacePortRules, DefaultOnPortAfterExplicitPort) {
  // "output var logic x, .b(s), c = 1" -- after an explicit port, c inherits only the direction;
  // its kind defaults to net (Sec 23.2.2.3).
  EXPECT_NE(findEvent(EventType::CompIllegalDefaultPortValue, "c", 177, 46), nullptr);
}

// ----
// Parameterized interface instances and virtual interfaces (Sec 25.9, 25.10)
// ----

TEST_F(InterfacePortRules, ParameterizedInterfaceInstanceHasItsOwnTypespec) {
  // "ipr_pif #(16) wide();" -- the instance's interface typespec carries the override, bound to W.
  const hldb::Module *const m = findModule("ipr_virt");
  ASSERT_NE(m, nullptr);
  const hldb::RefInstance *const wide = hldb::findByName<hldb::RefInstance>("wide", m->getRefInstances());
  ASSERT_NE(wide, nullptr);
  ASSERT_NE(wide->getTypespec(), nullptr);
  const hldb::InterfaceTypespec *const its = wide->getTypespec()->getActual<hldb::InterfaceTypespec>();
  ASSERT_NE(its, nullptr);
  EXPECT_EQ(its->getDefName(), std::string_view("ipr_pif"));
  ASSERT_NE(its->getInterface(), nullptr);
  EXPECT_EQ(its->getInterface()->getDefName(), std::string_view("ipr_pif"));
  ASSERT_NE(its->getParamAssigns(), nullptr);
  ASSERT_EQ(its->getParamAssigns()->size(), 1U);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(its->getParamAssigns()->front()->getLhs());
  ASSERT_NE(lhs, nullptr);
  ASSERT_NE(lhs->getActual(), nullptr);
  EXPECT_EQ(lhs->getActual()->getAnyType(), hldb::AnyType::Parameter);
  EXPECT_EQ(lhs->getActual()->getName(), std::string_view("W"));
}

TEST_F(InterfacePortRules, VirtualInterfaceAssignedInstanceWithOtherParameterValue) {
  // "virtual ipr_pif #(8) v8; ... v8 = wide;" with "ipr_pif #(16) wide()" -- Sec 25.9: "The actual
  // values and types of those parameters shall match".
  EXPECT_NE(findEvent(EventType::CompIllegalVirtualInterface, "wide", 191, 10), nullptr);
}

TEST_F(InterfacePortRules, VirtualInterfaceAssignedInstanceOfOtherInterface) {
  // "v8 = q;" with q an ipr_qif -- Sec 25.9: an interface instance "of the same type".
  EXPECT_NE(findEvent(EventType::CompIllegalVirtualInterface, "q", 192, 10), nullptr);
}

TEST_F(InterfacePortRules, VirtualInterfaceAssignedOtherModport) {
  // "vmp = voth;" (modports mp and other) -- Sec 25.9: the modport "is also part of its type".
  EXPECT_NE(findEvent(EventType::CompIllegalVirtualInterface, "voth", 193, 11), nullptr);
}

TEST_F(InterfacePortRules, VirtualInterfaceWithModportAssignedToOneWithout) {
  // "vplain = vmp;" -- only one "with no modport selected may be assigned to a virtual interface
  // with a modport selected" (Sec 25.9), not the reverse.
  EXPECT_NE(findEvent(EventType::CompIllegalVirtualInterface, "vmp", 194, 14), nullptr);
}

TEST_F(InterfacePortRules, MemberHiddenByVirtualInterfaceModport) {
  // "y = vmp.h;" with "modport mp(input s)" -- Sec 25.10: a modport restricts access "through a port
  // connection or virtual interface".
  EXPECT_NE(findEvent(EventType::CompModportMemberNotListed, "h", 196, 13), nullptr);
}

// ----
// Interface and program ports have io decls (Sec 37.13)
// ----

TEST_F(InterfacePortRules, ProgramModportPortHasIODecl) {
  // "program ipr_prog(ipr_pif.mp p);"
  const hldb::Program *const p = findProgram("ipr_prog");
  ASSERT_NE(p, nullptr);
  const hldb::IODecl *const io = hldb::findByName<hldb::IODecl>("p", p->getIODecls());
  ASSERT_NE(io, nullptr);
  EXPECT_EQ(io->getDirection(), vpiNoDirection);
  const hldb::RefObj *const expr = io->getExpr<hldb::RefObj>();
  ASSERT_NE(expr, nullptr);
  ASSERT_NE(expr->getActual(), nullptr);
  EXPECT_EQ(expr->getActual()->getAnyType(), hldb::AnyType::Modport);
  EXPECT_EQ(expr->getActual()->getName(), std::string_view("mp"));
}

// ----
// Implicit .name and .* connections (Sec 23.3.2.3, 23.3.2.4)
// ----

TEST_F(InterfacePortRules, ImplicitNamedConnectionDeclaredLater) {
  // "ipr_c u(.a, ...); logic a;" -- a shall be declared before the .a connection.
  EXPECT_NE(findEvent(EventType::CompImplicitPortConnectionUndeclared, "a", 203, 27), nullptr);
}

TEST_F(InterfacePortRules, WildcardConnectionWithoutSignal) {
  // "logic a; ipr_c u(.*);" -- port d has no default and no signal d exists (port b has a default).
  EXPECT_NE(findEvent(EventType::CompImplicitPortConnectionUndeclared, "d", 204, 35), nullptr);
}

namespace {
// The connections of the one instance of `module` that were made from its ".*": the port each connects
// (low conn) and the declaration it names (high conn), by port name.
std::map<std::string_view, const hldb::NamedArgument *> wildcardConnections(const hldb::Module *module) {
  std::map<std::string_view, const hldb::NamedArgument *> result;
  if ((module == nullptr) || (module->getRefInstances() == nullptr) || module->getRefInstances()->empty()) return result;
  const hldb::NamedArgumentCollection *const arguments = module->getRefInstances()->front()->getArguments();
  if (arguments == nullptr) return result;
  for (const hldb::NamedArgument *const argument : *arguments) {
    if (!argument->getWildcardConn()) continue;
    const hldb::RefObj *const low = argument->getLowConn<hldb::RefObj>();
    if (low != nullptr) result.emplace(low->getName(), argument);
  }
  return result;
}
}  // namespace

TEST_F(InterfacePortRules, WildcardConnectionMakesOneNameConnectionPerUnconnectedPort) {
  // "ipr_wc_def u(.*, .b(a));" with a and y declared, port c having a default and no c in scope:
  // ".*" is ".name" for a and y only -- b is connected explicitly, c uses its default (Sec 23.3.2.4).
  const hldb::Module *const m = findModule("ipr_wc");
  ASSERT_NE(m, nullptr);
  const std::map<std::string_view, const hldb::NamedArgument *> connections = wildcardConnections(m);
  ASSERT_EQ(connections.size(), 2U);
  for (const std::string_view name : {std::string_view("a"), std::string_view("y")}) {
    ASSERT_NE(connections.find(name), connections.end()) << name;
    const hldb::NamedArgument *const argument = connections.at(name);
    EXPECT_TRUE(argument->getConnByName());
    const hldb::RefObj *const low = argument->getLowConn<hldb::RefObj>();
    ASSERT_NE(low, nullptr);
    ASSERT_NE(low->getActual(), nullptr);
    EXPECT_EQ(low->getActual()->getAnyType(), hldb::AnyType::Port);
    EXPECT_EQ(low->getActual()->getName(), name);
    const hldb::RefObj *const high = argument->getHighConn<hldb::RefObj>();
    ASSERT_NE(high, nullptr);
    EXPECT_EQ(high->getName(), name);
    ASSERT_NE(high->getActual(), nullptr);
    EXPECT_EQ(high->getActual()->getAnyType(), hldb::AnyType::Variable);  // "logic a" is a variable (Sec 6.8).
    EXPECT_EQ(high->getActual()->getParent(), m);
  }
}

// ----
// Names through a generic interface port (Sec 25.3.3, 13.5.5)
// ----

TEST_F(InterfacePortRules, GenericPortSignalInEventControlIsAReference) {
  // "always @(a.start) a.run;" in "module ipr_generic(interface a)": a.start is a signal, not a call;
  // a.run, used as a statement, is a subroutine call (Sec 13.5.5) through the port.
  const hldb::Module *const m = findModule("ipr_generic");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getProcesses(), nullptr);
  ASSERT_EQ(m->getProcesses()->size(), 1U);
  const hldb::Always *const always = any_cast<hldb::Always>(m->getProcesses()->front());
  ASSERT_NE(always, nullptr);
  const hldb::EventControl *const ec = always->getStmt<hldb::EventControl>();
  ASSERT_NE(ec, nullptr);
  ASSERT_NE(ec->getCondition(), nullptr);
  EXPECT_EQ(ec->getCondition()->getAnyType(), hldb::AnyType::RefObj);
  ASSERT_NE(ec->getStmt(), nullptr);
  EXPECT_EQ(ec->getStmt()->getAnyType(), hldb::AnyType::SubroutineCall);
  EXPECT_EQ(ec->getStmt()->getName(), std::string_view("run"));
}

TEST_F(InterfacePortRules, InterconnectAliasedWithWire) {
  // "interconnect i; wire w; alias i = w;" -- Sec 10.11: "The nets connected with an alias statement
  // shall be type compatible, that is, they have to be of the same net type", a rule "stricter than
  // the rule applied to nets joining at ports" (where an interconnect takes the other net's type).
  EXPECT_NE(findEvent(EventType::CompIllegalAlias, "w", 209, 55), nullptr);
}

TEST_F(InterfacePortRules, InnerIndexOfInstanceArrayOutOfRange) {
  // "ipr_ba u[3][4](); u[1][9].x = 0;" -- Sec 23.6: each index "shall evaluate to one of the legal
  // index values for that array"; 9 is outside the second dimension [0:3].
  EXPECT_NE(findEvent(EventType::CompIndexOutOfRange, "u[1][9]", 219, 5), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
