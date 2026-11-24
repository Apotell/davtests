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

// Tests for tests/Google/chapter-23/23.2.2--ic_non_ansi_ports.sv
// Expected results follow IEEE 1800-2023 (6.6.8, 23.2.2, 23.3.3, 23.3.3.4, 23.3.3.7,
// 23.3.3.7.1, 23.3.3.7.2, 37.17); they are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - m(a, b, c, d, e) has the five ports in header order
//   - CHK1: a is input; CHK2: b is output with packed range [3:0]; CHK3: c is inout;
//     CHK4: d and e are input -- each port's net is an interconnect (23.2.2.1 port declarations)
//   - CHK4: each port net is declared in the body (lines 32-38); d and e share line 38
//   - Inside m: l_a, l_b, l_c, l_d, l_e are bound to the matching interconnect port nets
//   - CHK5: top connects u_pos by position and u_named by name in a different order; both bind
//     every port to the wire of the same name
//   - Resolution: every interconnect port merges with wires -> vpiWire
//   - No errors: the whole file is legal
// Checked only in an elaborated design (m_design->getElaborated()):
//   - ResolvedNetTypeIsWire: vpiResolvedNetType is the simulated net's type (23.3.3.7.1, 37.17 detail 12)
// Not checked:
//   - Nothing intentionally left out
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: ic_non_ansi_ports
//  19  :description: non-ANSI interconnect port declarations
//  20  :tags: 23.2.2
//  21  */
//  22
//  23  // Checks in this file:
//  24  //   CHK1: input interconnect a; declared in the module body
//  25  //   CHK2: output interconnect [3:0] b; packed non-ANSI port
//  26  //   CHK3: inout interconnect c; non-ANSI port
//  27  //   CHK4: two ports declared together: input interconnect d, e;
//  28  //   CHK5: non-ANSI interconnect ports connected by position and by name
//  29
//  30  module m(a, b, c, d, e);
//  31    // CHK1
//  32    input interconnect a;
//  33    // CHK2
//  34    output interconnect [3:0] b;
//  35    // CHK3
//  36    inout interconnect c;
//  37    // CHK4
//  38    input interconnect d, e;
//  39
//  40    leaf1 l_a(a);
//  41    leaf4 l_b(b);
//  42    leaf1 l_c(c);
//  43    leaf1 l_d(d);
//  44    leaf1 l_e(e);
//  45  endmodule
//  46
//  47  module leaf1(inout wire p);
//  48  endmodule
//  49
//  50  module leaf4(inout wire [3:0] p);
//  51  endmodule
//  52
//  53  module top;
//  54    wire a, c, d, e;
//  55    wire [3:0] b;
//  56    // CHK5
//  57    m u_pos(a, b, c, d, e);
//  58    m u_named(.e(e), .d(d), .c(c), .b(b), .a(a));
//  59  endmodule

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/expr.h>
#include <hldb/gen_block.h>
#include <hldb/gen_for.h>
#include <hldb/gen_if.h>
#include <hldb/gen_if_else.h>
#include <hldb/gen_region.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/named_argument.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/select.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

#include <string_view>
#include <vector>

namespace hlc {

class IcNonAnsiPortsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "23.2.2--ic_non_ansi_ports.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // Definition of module 'top'.
  static const hldb::Module *getTop() { return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules()); }

  // Net 'name' declared directly in 'scope' (module, interface, begin or generate block).
  static const hldb::Net *getNet(const hldb::Any *scope, std::string_view name) {
    return hldb::findByName<hldb::Net>(name, hldb::getNets(scope));
  }

  // Instance 'name' instantiated directly in the module/interface 'scope'.
  static const hldb::RefInstance *getInst(const hldb::Any *scope, std::string_view name) {
    if (const hldb::Module *const m = any_cast<hldb::Module>(scope)) {
      return hldb::findByName<hldb::RefInstance>(name, m->getRefInstances());
    }
    if (const hldb::Interface *const i = any_cast<hldb::Interface>(scope)) {
      return hldb::findByName<hldb::RefInstance>(name, i->getRefInstances());
    }
    return nullptr;
  }

  // The module or interface typespec an instance refers to.
  static const hldb::Typespec *getInstTypespec(const hldb::RefInstance *inst) {
    return ((inst == nullptr) || (inst->getTypespec() == nullptr)) ? nullptr : inst->getTypespec()->getActual();
  }

  // Name of the module/interface definition an instance refers to.
  static std::string_view getInstDefName(const hldb::RefInstance *inst) {
    return hldb::getDefName(getInstTypespec(inst));
  }

  // The module or interface definition an instance refers to.
  static const hldb::Any *getInstDefinition(const hldb::RefInstance *inst) {
    const hldb::Typespec *const ts = getInstTypespec(inst);
    if (const hldb::ModuleTypespec *const mts = any_cast<hldb::ModuleTypespec>(ts)) {
      if (mts->getModule() != nullptr) return mts->getModule();
    }
    if (const hldb::InterfaceTypespec *const its = any_cast<hldb::InterfaceTypespec>(ts)) {
      if (its->getInterface() != nullptr) return its->getInterface();
    }
    const std::string_view def = getInstDefName(inst);
    if (const hldb::Module *const m = hldb::findByDefName<hldb::Module>(def, m_design->getAllModules())) return m;
    return hldb::findByDefName<hldb::Interface>(def, m_design->getAllInterfaces());
  }

  // The expression the parent connects to port 'port' of 'inst' (vpiHighConn): a named, .name or .*
  // connection is matched by port name; a positional one by the port's position in the definition.
  static const hldb::Any *getHighConn(const hldb::RefInstance *inst, std::string_view port) {
    if ((inst == nullptr) || (inst->getArguments() == nullptr)) return nullptr;
    if (const hldb::NamedArgument *const na = hldb::findByName<hldb::NamedArgument>(port, inst->getArguments())) {
      return na->getHighConn();
    }
    const hldb::PortCollection *const ports = hldb::getPorts(getInstDefinition(inst));
    if (ports == nullptr) return nullptr;
    for (size_t i = 0; i < ports->size(); ++i) {
      if (ports->at(i)->getName() == port) {
        return (i < inst->getArguments()->size()) ? inst->getArguments()->at(i)->getHighConn() : nullptr;
      }
    }
    return nullptr;
  }

  // Strips every select (bit, part, indexed part) and returns what the innermost reference is bound to.
  static const hldb::Any *getRootActual(const hldb::Any *expr) {
    while (expr != nullptr) {
      if (const hldb::Select *const sel = any_cast<hldb::Select>(expr)) {
        expr = sel->getPrefix();
        continue;
      }
      if (const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr)) return ref->getActual();
      return expr;
    }
    return nullptr;
  }

  // Walks generate statements/blocks and collects every instance named 'inst' and every net named 'net'
  // found inside them.
  static void walkGenerate(const hldb::Any *item, std::string_view inst, std::string_view net,
                           std::vector<const hldb::RefInstance *> *insts, std::vector<const hldb::Net *> *nets) {
    if (item == nullptr) return;
    if (const hldb::RefInstance *const ri = any_cast<hldb::RefInstance>(item)) {
      if (ri->getName() == inst) insts->emplace_back(ri);
      return;
    }
    if (const hldb::Net *const n = hldb::findByName<hldb::Net>(net, hldb::getNets(item))) nets->emplace_back(n);
    const hldb::AnyCollection *stmts = nullptr;
    if (const hldb::GenFor *const g = any_cast<hldb::GenFor>(item)) {
      walkGenerate(g->getStmt(), inst, net, insts, nets);
    } else if (const hldb::GenIf *const g = any_cast<hldb::GenIf>(item)) {
      walkGenerate(g->getStmt(), inst, net, insts, nets);
    } else if (const hldb::GenIfElse *const g = any_cast<hldb::GenIfElse>(item)) {
      walkGenerate(g->getStmt(), inst, net, insts, nets);
      walkGenerate(g->getElseStmt(), inst, net, insts, nets);
    } else if (const hldb::GenRegion *const g = any_cast<hldb::GenRegion>(item)) {
      stmts = g->getStmts();
    } else if (const hldb::GenBlock *const g = any_cast<hldb::GenBlock>(item)) {
      stmts = g->getStmts();
    } else if (const hldb::Begin *const g = any_cast<hldb::Begin>(item)) {
      stmts = g->getStmts();
    }
    if (stmts != nullptr) {
      for (const hldb::Any *const s : *stmts) walkGenerate(s, inst, net, insts, nets);
    }
  }

  // All instances named 'inst' and nets named 'net' inside the generate constructs of module 'mod'.
  static void collectFromGenerate(const hldb::Module *mod, std::string_view inst, std::string_view net,
                                  std::vector<const hldb::RefInstance *> *insts, std::vector<const hldb::Net *> *nets) {
    if ((mod == nullptr) || (mod->getGenStmts() == nullptr)) return;
    for (const hldb::Any *const g : *mod->getGenStmts()) walkGenerate(g, inst, net, insts, nets);
  }

  // Port 'name' of 'mod' has direction 'dir' and its internal net (lowconn) is an interconnect.
  static void expectInterconnectPort(const hldb::Module *mod, std::string_view name, int32_t dir) {
    ASSERT_NE(mod, nullptr);
    const hldb::Port *const port = hldb::findByName<hldb::Port>(name, mod->getPorts());
    ASSERT_NE(port, nullptr) << "port " << name << " not found in " << mod->getDefName();
    EXPECT_EQ(port->getDirection(), dir) << name;
    const hldb::Net *const net = getNet(mod, name);
    ASSERT_NE(net, nullptr) << "no net for port " << name;
    EXPECT_EQ(net->getNetType(), vpiInterconnect) << name << " is an interconnect port";
    EXPECT_EQ(getRootActual(port->getLowConn()), net) << name << ": the port's internal connection is its net";
  }

  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }
};

TEST_F(IcNonAnsiPortsTest, PortsInHeaderOrder) {
  const hldb::Module *const m = getModule("m");
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getPorts(), nullptr);
  ASSERT_EQ(m->getPorts()->size(), 5u);
  const char *const order[] = {"a", "b", "c", "d", "e"};
  for (size_t i = 0; i < 5; ++i) EXPECT_EQ(m->getPorts()->at(i)->getName(), std::string_view(order[i])) << i;
}

// CHK1-CHK4: input interconnect a; output interconnect [3:0] b; inout interconnect c; input interconnect d, e;
TEST_F(IcNonAnsiPortsTest, DirectionsAndInterconnectNets) {
  const hldb::Module *const m = getModule("m");
  expectInterconnectPort(m, "a", vpiInput);
  expectInterconnectPort(m, "b", vpiOutput);
  expectInterconnectPort(m, "c", vpiInout);
  expectInterconnectPort(m, "d", vpiInput);
  expectInterconnectPort(m, "e", vpiInput);
}

// CHK2: output interconnect [3:0] b; -- packed range [3:0].
TEST_F(IcNonAnsiPortsTest, PackedOutputPort) {
  const hldb::Module *const m = getModule("m");
  ASSERT_NE(m, nullptr);
  const hldb::LogicTypespec *const ts = hldb::getTypespec<hldb::LogicTypespec>(getNet(m, "b"));
  ASSERT_NE(ts, nullptr) << "b should have a 4-state vector typespec";
  ASSERT_NE(ts->getRanges(), nullptr);
  ASSERT_EQ(ts->getRanges()->size(), 1u);
  const hldb::Constant *const l = any_cast<hldb::Constant>(ts->getRanges()->at(0)->getLeftExpr());
  const hldb::Constant *const r = any_cast<hldb::Constant>(ts->getRanges()->at(0)->getRightExpr());
  ASSERT_NE(l, nullptr);
  ASSERT_NE(r, nullptr);
  EXPECT_EQ(l->getDecompile(), std::string_view("3"));
  EXPECT_EQ(r->getDecompile(), std::string_view("0"));
}

// CHK4: each port is declared in the body; d and e come from the same declaration on line 38.
TEST_F(IcNonAnsiPortsTest, BodyDeclarationLines) {
  const hldb::Module *const m = getModule("m");
  ASSERT_NE(m, nullptr);
  const std::pair<const char *, uint32_t> lines[] = {{"a", 32u}, {"b", 34u}, {"c", 36u}, {"d", 38u}, {"e", 38u}};
  for (const std::pair<const char *, uint32_t> &l : lines) {
    const hldb::Net *const net = getNet(m, l.first);
    ASSERT_NE(net, nullptr) << l.first;
    EXPECT_EQ(net->getStartLine(), l.second) << l.first << " is declared on line " << l.second;
  }
}

TEST_F(IcNonAnsiPortsTest, LeavesBoundToPortNets) {
  const hldb::Module *const m = getModule("m");
  ASSERT_NE(m, nullptr);
  const std::pair<const char *, const char *> conns[] = {
      {"l_a", "a"}, {"l_b", "b"}, {"l_c", "c"}, {"l_d", "d"}, {"l_e", "e"}};
  for (const std::pair<const char *, const char *> &c : conns) {
    EXPECT_EQ(getRootActual(getHighConn(getInst(m, c.first), "p")), getNet(m, c.second)) << c.first;
  }
}

// CHK5: m u_pos(a, b, c, d, e); m u_named(.e(e), .d(d), .c(c), .b(b), .a(a));
TEST_F(IcNonAnsiPortsTest, PositionalAndNamedConnections) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module top not found";
  for (const char *const name : {"u_pos", "u_named"}) {
    const hldb::RefInstance *const inst = getInst(top, name);
    ASSERT_NE(inst, nullptr) << name;
    EXPECT_EQ(getInstDefName(inst), std::string_view("m")) << name;
    for (const char *const port : {"a", "b", "c", "d", "e"}) {
      EXPECT_EQ(getRootActual(getHighConn(inst, port)), getNet(top, port)) << name << "." << port;
    }
  }
  const hldb::RefInstance *const named = getInst(top, "u_named");
  ASSERT_NE(named, nullptr);
  const hldb::NamedArgument *const na = hldb::findByName<hldb::NamedArgument>("a", named->getArguments());
  ASSERT_NE(na, nullptr);
  EXPECT_TRUE(na->getConnByName());
}

// The resolved type exists only after elaboration (simulated net, 23.3.3.7), so it is checked only
// in an elaborated design.
TEST_F(IcNonAnsiPortsTest, ResolvedNetTypeIsWire) {
  const hldb::Module *const m = getModule("m");
  ASSERT_NE(m, nullptr);
  for (const char *const name : {"a", "b", "c", "d", "e"}) {
    const hldb::Net *const net = getNet(m, name);
    ASSERT_NE(net, nullptr) << name;
    if (m_design->getElaborated()) {
      EXPECT_EQ(net->getResolvedNetType(), vpiWire) << name << " merges with wires in top and in its leaf";
    }
  }
}

// The file is legal SystemVerilog, so compilation must not report any error.
TEST_F(IcNonAnsiPortsTest, CompilesWithoutErrors) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
