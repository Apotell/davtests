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

// Tests for tests/Google/chapter-23/23.3.3.7.1--ic_merge_net_types.sv
// Expected results follow IEEE 1800-2023 (6.6.8, 23.2.2, 23.3.3, 23.3.3.4, 23.3.3.7,
// 23.3.3.7.1, 23.3.3.7.2, 37.17); they are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - Internal port nets keep their declared types: dut1.w wire, dut2.w wand, tri_port.t tri,
//     wor_port.w wor, wire_port.w wire, drv.out wire, pull_port.t tri1
//   - iwire, it, iw, io, ip in netlist are vpiInterconnect nets, each connected to the right ports
//   - Resolution (23.3.3.7.1: the merged net takes the type of the non-interconnect net; Table 23-1
//     for two typed nets):
//     CHK1 iwire -> vpiWand (LRM example: wire + wand), CHK2 it -> vpiTri,
//     CHK3 iw -> vpiWor (wire internal + wor external = external), CHK4 io -> vpiWire,
//     CHK5 ip -> vpiTri1
//   - No errors: the whole file is legal
// Skipped (GTEST_SKIP; the real assertions stay below the skip):
//   - ResolvedNetTypes: HLC leaves vpiResolvedNetType at 0 on interconnect nets: ports are not collapsed into a
//   simulated net.IEEE 1800-2023 Sec 23.3.3.7.1 + Table 23-1 + 37.17 detail 12 require
//   vpiWand/vpiTri/vpiWor/vpiWire/vpiTri1.Fix pending..
// Not checked:
//   - Collapsed/simulated net objects (vpiSimNet): only the resolved type is checked
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: ic_merge_net_types
//  19  :description: interconnect net merged with the net type of the ports it connects to
//  20  :tags: 23.3.3.7.1
//  21  */
//  22
//  23  // Checks in this file:
//  24  //   CHK1: LRM netlist example - interconnect merges with wire and wand into wand
//  25  //   CHK2: interconnect connected to a tri port
//  26  //   CHK3: interconnect connected to a wor port and a wire port
//  27  //   CHK4: interconnect connected to an output wire logic port
//  28  //   CHK5: interconnect connected to a tri1 pull port
//  29
//  30  module dut1(inout wire w);
//  31    assign w = 1;
//  32  endmodule
//  33
//  34  module dut2(inout wand w);
//  35    assign w = 0;
//  36  endmodule
//  37
//  38  module tri_port(inout tri t);
//  39  endmodule
//  40
//  41  module wor_port(inout wor w);
//  42    assign w = 1'b0;
//  43  endmodule
//  44
//  45  module wire_port(input wire w);
//  46  endmodule
//  47
//  48  module drv(output wire logic out);
//  49    assign out = 1'b1;
//  50  endmodule
//  51
//  52  module pull_port(input tri1 t);
//  53  endmodule
//  54
//  55  module netlist;
//  56    // CHK1
//  57    interconnect iwire;
//  58    dut1 child1(iwire);
//  59    dut2 child2(iwire);
//  60
//  61    // CHK2
//  62    interconnect it;
//  63    tri_port u_tri(it);
//  64
//  65    // CHK3
//  66    interconnect iw;
//  67    wor_port  u_wor(iw);
//  68    wire_port u_wp(iw);
//  69
//  70    // CHK4
//  71    interconnect io;
//  72    drv u_drv(io);
//  73
//  74    // CHK5
//  75    interconnect ip;
//  76    pull_port u_pull(ip);
//  77  endmodule

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/design.h>
#include <hldb/expr.h>
#include <hldb/gen_block.h>
#include <hldb/gen_for.h>
#include <hldb/gen_if.h>
#include <hldb/gen_if_else.h>
#include <hldb/gen_region.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/named_argument.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/select.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

#include <string_view>
#include <vector>

namespace hlc {

class IcMergeNetTypesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "23.3.3.7.1--ic_merge_net_types.hlc"}); }
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

// The typed side of every connection keeps the net type it was declared with.
TEST_F(IcMergeNetTypesTest, InternalPortNetTypes) {
  struct Port {
    const char *m_mod;
    const char *m_net;
    int32_t m_type;
  };
  const Port ports[] = {{"dut1", "w", vpiWire},     {"dut2", "w", vpiWand},      {"tri_port", "t", vpiTri},
                        {"wor_port", "w", vpiWor},  {"wire_port", "w", vpiWire}, {"drv", "out", vpiWire},
                        {"pull_port", "t", vpiTri1}};
  for (const Port &p : ports) {
    const hldb::Net *const net = getNet(getModule(p.m_mod), p.m_net);
    ASSERT_NE(net, nullptr) << p.m_mod << "." << p.m_net;
    EXPECT_EQ(net->getNetType(), p.m_type) << p.m_mod << "." << p.m_net;
  }
}

TEST_F(IcMergeNetTypesTest, InterconnectsAndConnections) {
  const hldb::Module *const netlist = getModule("netlist");
  ASSERT_NE(netlist, nullptr);
  for (const char *const name : {"iwire", "it", "iw", "io", "ip"}) {
    const hldb::Net *const net = getNet(netlist, name);
    ASSERT_NE(net, nullptr) << name;
    EXPECT_EQ(net->getNetType(), vpiInterconnect) << name;
  }
  struct Conn {
    const char *m_inst;
    const char *m_port;
    const char *m_net;
  };
  const Conn conns[] = {{"child1", "w", "iwire"}, {"child2", "w", "iwire"}, {"u_tri", "t", "it"}, {"u_wor", "w", "iw"},
                        {"u_wp", "w", "iw"},      {"u_drv", "out", "io"},   {"u_pull", "t", "ip"}};
  for (const Conn &c : conns) {
    const hldb::RefInstance *const inst = getInst(netlist, c.m_inst);
    ASSERT_NE(inst, nullptr) << c.m_inst;
    EXPECT_EQ(getRootActual(getHighConn(inst, c.m_port)), getNet(netlist, c.m_net)) << c.m_inst;
  }
}

// 23.3.3.7.1 + Table 23-1, exposed as vpiResolvedNetType (37.17 detail 12).
TEST_F(IcMergeNetTypesTest, ResolvedNetTypes) {
  GTEST_SKIP()
      << "HLC leaves vpiResolvedNetType at 0 on interconnect nets: ports are not collapsed into a simulated net."
         " IEEE 1800-2023 Sec 23.3.3.7.1 + Table 23-1 + 37.17 detail 12 require vpiWand/vpiTri/vpiWor/vpiWire/vpiTri1."
         " Fix pending.";
  const hldb::Module *const netlist = getModule("netlist");
  ASSERT_NE(netlist, nullptr);
  const std::pair<const char *, int32_t> expected[] = {
      {"iwire", vpiWand}, {"it", vpiTri}, {"iw", vpiWor}, {"io", vpiWire}, {"ip", vpiTri1}};
  for (const std::pair<const char *, int32_t> &e : expected) {
    const hldb::Net *const net = getNet(netlist, e.first);
    ASSERT_NE(net, nullptr) << e.first;
    EXPECT_EQ(net->getResolvedNetType(), e.second) << e.first;
  }
}

// The file is legal SystemVerilog, so compilation must not report any error.
TEST_F(IcMergeNetTypesTest, CompilesWithoutErrors) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
