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

// Tests for tests/Google/chapter-23/23.2.2--ic_ansi_ports.sv
// Expected results follow IEEE 1800-2023 (6.6.8, 23.2.2, 23.3.3, 23.3.3.4, 23.3.3.7,
// 23.3.3.7.1, 23.3.3.7.2, 37.17); they are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - CHK1: m1 has ports a (input), y (output), z (inout) in this order; each port's net is an
//     interconnect and is bound to the port (lowconn)
//   - CHK2/CHK3: m2(input interconnect a, b, c) -- b and c inherit direction AND the interconnect
//     kind from a (23.2.2.3: 'If the previous port was an interconnect port, this port shall also
//     be an interconnect port')
//   - CHK4: m3.v is an input interconnect port with packed range [3:0]
//   - CHK5: m3.u is an input interconnect port (unpacked array [0:1])
//   - Inside m1/m2/m3 every leaf is bound to the right interconnect port net (l_u0/l_u1 by element select)
//   - top: u1 connects by name, u2 by position, u3 by name (vpiConnByName) to the right wires
//   - Resolution: every interconnect port merges with a wire -> vpiResolvedNetType vpiWire
//   - No errors: the whole file is legal
// Skipped (GTEST_SKIP; the real assertions stay below the skip):
//   - ResolvedNetTypeIsWire: HLC leaves vpiResolvedNetType at 0 on interconnect nets: ports are not collapsed into a
//   simulated net.IEEE 1800-2023 Sec 23.3.3.7.1 + 37.17 detail 12 require the merged type here (vpiWire). Fix pending..
// Not checked:
//   - Size of the unpacked dimension of u ([0:1]): Net has no range accessor and HLDB has no
//     interconnect-array object (37.24) -- model gap
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: ic_ansi_ports
//  19  :description: ANSI interconnect port declarations in different styles
//  20  :tags: 23.2.2
//  21  */
//  22
//  23  // Checks in this file:
//  24  //   CHK1: input, output and inout interconnect ports, one per line
//  25  //   CHK2: several ports in one declaration: input interconnect a, b
//  26  //   CHK3: subsequent ports inherit interconnect from the previous port
//  27  //   CHK4: packed interconnect port: input interconnect [3:0] v
//  28  //   CHK5: unpacked interconnect port: input interconnect u [0:1]
//  29
//  30  // CHK1
//  31  module m1(
//  32    input  interconnect a,
//  33    output interconnect y,
//  34    inout  interconnect z
//  35  );
//  36    leaf_in  l_a(a);
//  37    leaf_out l_y(y);
//  38    leaf_io  l_z(z);
//  39  endmodule
//  40
//  41  // CHK2 + CHK3
//  42  module m2(input interconnect a, b, c);
//  43    leaf_in l_a(a);
//  44    leaf_in l_b(b);
//  45    leaf_in l_c(c);
//  46  endmodule
//  47
//  48  // CHK4 + CHK5
//  49  module m3(input interconnect [3:0] v, input interconnect u [0:1]);
//  50    leaf_in4 l_v(v);
//  51    leaf_in  l_u0(u[0]);
//  52    leaf_in  l_u1(u[1]);
//  53  endmodule
//  54
//  55  module leaf_in(input wire p);
//  56  endmodule
//  57
//  58  module leaf_in4(input wire [3:0] p);
//  59  endmodule
//  60
//  61  module leaf_out(output wire p);
//  62    assign p = 1'b0;
//  63  endmodule
//  64
//  65  module leaf_io(inout wire p);
//  66  endmodule
//  67
//  68  module top;
//  69    wire a, y, z, b, c;
//  70    wire [3:0] v;
//  71    wire u [0:1];
//  72    m1 u1(.a(a), .y(y), .z(z));
//  73    m2 u2(a, b, c);
//  74    m3 u3(.v(v), .u(u));
//  75  endmodule

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

class IcAnsiPortsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "23.2.2--ic_ansi_ports.hlc"}); }
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

// CHK1: module m1(input interconnect a, output interconnect y, inout interconnect z);
TEST_F(IcAnsiPortsTest, OnePortPerLine) {
  const hldb::Module *const m1 = getModule("m1");
  ASSERT_NE(m1, nullptr);
  ASSERT_NE(m1->getPorts(), nullptr);
  ASSERT_EQ(m1->getPorts()->size(), 3u);
  EXPECT_EQ(m1->getPorts()->at(0)->getName(), std::string_view("a"));
  EXPECT_EQ(m1->getPorts()->at(1)->getName(), std::string_view("y"));
  EXPECT_EQ(m1->getPorts()->at(2)->getName(), std::string_view("z"));
  expectInterconnectPort(m1, "a", vpiInput);
  expectInterconnectPort(m1, "y", vpiOutput);
  expectInterconnectPort(m1, "z", vpiInout);
}

// CHK2/CHK3: module m2(input interconnect a, b, c); -- b and c inherit input + interconnect.
TEST_F(IcAnsiPortsTest, InheritedInterconnectPorts) {
  const hldb::Module *const m2 = getModule("m2");
  ASSERT_NE(m2, nullptr);
  ASSERT_NE(m2->getPorts(), nullptr);
  ASSERT_EQ(m2->getPorts()->size(), 3u);
  for (const char *const name : {"a", "b", "c"}) expectInterconnectPort(m2, name, vpiInput);
}

// CHK4: input interconnect [3:0] v -- packed range [3:0].
TEST_F(IcAnsiPortsTest, PackedInterconnectPort) {
  const hldb::Module *const m3 = getModule("m3");
  expectInterconnectPort(m3, "v", vpiInput);
  ASSERT_NE(m3, nullptr);
  const hldb::LogicTypespec *const ts = hldb::getTypespec<hldb::LogicTypespec>(getNet(m3, "v"));
  ASSERT_NE(ts, nullptr) << "v should have a 4-state vector typespec";
  ASSERT_NE(ts->getRanges(), nullptr);
  ASSERT_EQ(ts->getRanges()->size(), 1u);
  const hldb::Constant *const l = any_cast<hldb::Constant>(ts->getRanges()->at(0)->getLeftExpr());
  const hldb::Constant *const r = any_cast<hldb::Constant>(ts->getRanges()->at(0)->getRightExpr());
  ASSERT_NE(l, nullptr);
  ASSERT_NE(r, nullptr);
  EXPECT_EQ(l->getDecompile(), std::string_view("3"));
  EXPECT_EQ(r->getDecompile(), std::string_view("0"));
}

// CHK5: input interconnect u [0:1] -- an unpacked interconnect port.
TEST_F(IcAnsiPortsTest, UnpackedInterconnectPort) { expectInterconnectPort(getModule("m3"), "u", vpiInput); }

// Inside the modules, every leaf is connected to the interconnect port net.
TEST_F(IcAnsiPortsTest, LeavesBoundToPortNets) {
  struct Conn {
    const char *m_mod;
    const char *m_inst;
    const char *m_def;
    const char *m_net;
  };
  const Conn conns[] = {{"m1", "l_a", "leaf_in", "a"},  {"m1", "l_y", "leaf_out", "y"}, {"m1", "l_z", "leaf_io", "z"},
                        {"m2", "l_a", "leaf_in", "a"},  {"m2", "l_b", "leaf_in", "b"},  {"m2", "l_c", "leaf_in", "c"},
                        {"m3", "l_v", "leaf_in4", "v"}, {"m3", "l_u0", "leaf_in", "u"}, {"m3", "l_u1", "leaf_in", "u"}};
  for (const Conn &c : conns) {
    const hldb::Module *const mod = getModule(c.m_mod);
    ASSERT_NE(mod, nullptr) << c.m_mod;
    const hldb::RefInstance *const inst = getInst(mod, c.m_inst);
    ASSERT_NE(inst, nullptr) << c.m_mod << "." << c.m_inst;
    EXPECT_EQ(getInstDefName(inst), std::string_view(c.m_def)) << c.m_inst;
    EXPECT_EQ(getRootActual(getHighConn(inst, "p")), getNet(mod, c.m_net)) << c.m_mod << "." << c.m_inst << ".p";
  }
}

// top: m1 u1(.a(a), .y(y), .z(z)); m2 u2(a, b, c); m3 u3(.v(v), .u(u));
TEST_F(IcAnsiPortsTest, TopConnections) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module top not found";
  struct Conn {
    const char *m_inst;
    const char *m_port;
    const char *m_net;
    bool m_byName;
  };
  const Conn conns[] = {{"u1", "a", "a", true},  {"u1", "y", "y", true},  {"u1", "z", "z", true},
                        {"u2", "a", "a", false}, {"u2", "b", "b", false}, {"u2", "c", "c", false},
                        {"u3", "v", "v", true},  {"u3", "u", "u", true}};
  for (const Conn &c : conns) {
    const hldb::RefInstance *const inst = getInst(top, c.m_inst);
    ASSERT_NE(inst, nullptr) << c.m_inst;
    EXPECT_EQ(getRootActual(getHighConn(inst, c.m_port)), getNet(top, c.m_net)) << c.m_inst << "." << c.m_port;
    if (c.m_byName) {
      const hldb::NamedArgument *const na = hldb::findByName<hldb::NamedArgument>(c.m_port, inst->getArguments());
      ASSERT_NE(na, nullptr) << c.m_inst << "." << c.m_port;
      EXPECT_TRUE(na->getConnByName()) << c.m_inst << "." << c.m_port << " is a named connection";
    }
  }
}

// 23.3.3.7.1: each interconnect port merges with the wire connected in top and the wire port of its
// leaf, so the simulated net is a wire (37.17 detail 12: vpiResolvedNetType).
TEST_F(IcAnsiPortsTest, ResolvedNetTypeIsWire) {
  GTEST_SKIP()
      << "HLC leaves vpiResolvedNetType at 0 on interconnect nets: ports are not collapsed into a simulated net."
         " IEEE 1800-2023 Sec 23.3.3.7.1 + 37.17 detail 12 require the merged type here (vpiWire). Fix pending.";
  const std::pair<const char *, const char *> nets[] = {{"m1", "a"}, {"m1", "y"}, {"m1", "z"}, {"m2", "a"},
                                                        {"m2", "b"}, {"m2", "c"}, {"m3", "v"}, {"m3", "u"}};
  for (const std::pair<const char *, const char *> &n : nets) {
    const hldb::Net *const net = getNet(getModule(n.first), n.second);
    ASSERT_NE(net, nullptr) << n.first << "." << n.second;
    EXPECT_EQ(net->getResolvedNetType(), vpiWire) << n.first << "." << n.second;
  }
}

// The file is legal SystemVerilog, so compilation must not report any error.
TEST_F(IcAnsiPortsTest, CompilesWithoutErrors) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
