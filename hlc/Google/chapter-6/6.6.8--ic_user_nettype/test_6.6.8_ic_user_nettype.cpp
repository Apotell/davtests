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

// Tests for tests/Google/chapter-6/6.6.8--ic_user_nettype.sv
// Expected results follow IEEE 1800-2023 (6.6.8 generic interconnect, 6.7.1, 23.3.3.7.1,
// 37.17, 37.24); they are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - NetsPkg declares realNet and sumNet as nettypes; sumNet's resolution function is rsum,
//     realNet has none (6.6.7)
//   - n1..n4, n5r, n5w in top are vpiInterconnect nets
//   - CHK1-5: each instance in top is of the right module and is connected to the right interconnect
//   - CHK4: pass and generic_pass have an inout port p whose net is an interconnect (interconnect
//     port), and pass forwards p to rrcv u_r
//   - CHK5: n5w merges with wdrv's 'output wire o' -> vpiResolvedNetType == vpiWire
//   - No errors: the whole file is legal
// Checked only in an elaborated design (m_design->getElaborated()):
//   - ResolvedNetTypeIsWire: vpiResolvedNetType is the simulated net's type (23.3.3.7.1, 37.17 detail 12)
// Skipped (GTEST_SKIP; the real assertions stay below the skip):
//   - CompilesWithoutErrors: HLC reports DL2510 for an imported nettype used as a port type
// Not checked:
//   - Resolved type of n1..n5r (a user nettype): there is no vpiNettypeNet constant in the
//     HLDB headers (37.17 detail 11), so the expected value cannot be spelled -- model gap
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: ic_user_nettype
//  19  :description: interconnect nets resolving to user-defined nettypes
//  20  :tags: 6.6.8
//  21  */
//  22
//  23  // Checks in this file:
//  24  //   CHK1: interconnect connected to a nettype real port
//  25  //   CHK2: interconnect connected to a nettype with a resolution function
//  26  //   CHK3: interconnect driven by a realNet output and read by a realNet input
//  27  //   CHK4: interconnect port forwarding a nettype net to a child
//  28  //   CHK5: same interconnect-ported module reused with wire and with nettype
//  29
//  30  package NetsPkg;
//  31    nettype real realNet;
//  32    function automatic real rsum(input real drivers[]);
//  33      rsum = 0.0;
//  34      foreach (drivers[i]) rsum += drivers[i];
//  35    endfunction
//  36    nettype real sumNet with rsum;
//  37  endpackage
//  38
//  39  import NetsPkg::*;
//  40
//  41  module rdrv(output realNet o);
//  42    assign o = 1.5;
//  43  endmodule
//  44
//  45  module rrcv(input realNet i);
//  46  endmodule
//  47
//  48  module sdrv(output sumNet o);
//  49    assign o = 2.0;
//  50  endmodule
//  51
//  52  module wdrv(output wire o);
//  53    assign o = 1'b1;
//  54  endmodule
//  55
//  56  module pass(inout interconnect p);
//  57    rrcv u_r(p);
//  58  endmodule
//  59
//  60  module generic_pass(inout interconnect p);
//  61  endmodule
//  62
//  63  module top;
//  64    // CHK1
//  65    interconnect n1;
//  66    rrcv u1(n1);
//  67    // CHK2
//  68    interconnect n2;
//  69    sdrv u2a(n2);
//  70    sdrv u2b(n2);
//  71    // CHK3
//  72    interconnect n3;
//  73    rdrv u3a(n3);
//  74    rrcv u3b(n3);
//  75    // CHK4
//  76    interconnect n4;
//  77    rdrv u4a(n4);
//  78    pass u4b(n4);
//  79    // CHK5
//  80    interconnect n5r, n5w;
//  81    rdrv u5a(n5r);
//  82    generic_pass u5b(n5r);
//  83    wdrv u5c(n5w);
//  84    generic_pass u5d(n5w);
//  85  endmodule

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
#include <hldb/package.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/select.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/typedef.h>
#include <hldb/vpi_user.h>

#include <string_view>
#include <vector>

namespace hlc {

class IcUserNettypeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.6.8--ic_user_nettype.hlc"}); }
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

  // The expression the parent connects to port 'port' of 'inst' (vpiHighConn). All connections in
  // these sources are positional, so the argument position is the port's position in the definition.
  static const hldb::Any *getHighConn(const hldb::RefInstance *inst, std::string_view port) {
    if ((inst == nullptr) || (inst->getArguments() == nullptr)) return nullptr;
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
};

TEST_F(IcUserNettypeTest, NettypeDeclarations) {
  const hldb::Package *const pkg = hldb::findByDefName<hldb::Package>("NetsPkg", m_design->getAllPackages());
  ASSERT_NE(pkg, nullptr) << "package NetsPkg not found";
  const hldb::Typedef *const real = hldb::findByName<hldb::Typedef>("realNet", pkg->getTypedefs());
  const hldb::Typedef *const sum = hldb::findByName<hldb::Typedef>("sumNet", pkg->getTypedefs());
  ASSERT_NE(real, nullptr);
  ASSERT_NE(sum, nullptr);
  EXPECT_TRUE(real->getIsNettype());
  EXPECT_TRUE(sum->getIsNettype());
  EXPECT_EQ(real->getResolutionFunc(), nullptr) << "nettype real realNet has no 'with' clause";
  ASSERT_NE(sum->getResolutionFunc(), nullptr) << "nettype real sumNet with rsum";
  EXPECT_EQ(sum->getResolutionFunc()->getName(), std::string_view("rsum"));
}

TEST_F(IcUserNettypeTest, TopNetsAreInterconnect) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module top not found";
  for (const char *const name : {"n1", "n2", "n3", "n4", "n5r", "n5w"}) {
    const hldb::Net *const net = getNet(top, name);
    ASSERT_NE(net, nullptr) << name;
    EXPECT_EQ(net->getNetType(), vpiInterconnect) << name;
  }
}

TEST_F(IcUserNettypeTest, PortConnections) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  struct Conn {
    const char *m_inst;
    const char *m_def;
    const char *m_port;
    const char *m_net;
  };
  const Conn conns[] = {
      {"u1", "rrcv", "i", "n1"},   {"u2a", "sdrv", "o", "n2"},         {"u2b", "sdrv", "o", "n2"},
      {"u3a", "rdrv", "o", "n3"},  {"u3b", "rrcv", "i", "n3"},         {"u4a", "rdrv", "o", "n4"},
      {"u4b", "pass", "p", "n4"},  {"u5a", "rdrv", "o", "n5r"},        {"u5b", "generic_pass", "p", "n5r"},
      {"u5c", "wdrv", "o", "n5w"}, {"u5d", "generic_pass", "p", "n5w"}};
  for (const Conn &c : conns) {
    const hldb::RefInstance *const inst = getInst(top, c.m_inst);
    ASSERT_NE(inst, nullptr) << c.m_inst;
    EXPECT_EQ(getInstDefName(inst), std::string_view(c.m_def)) << c.m_inst;
    EXPECT_EQ(getRootActual(getHighConn(inst, c.m_port)), getNet(top, c.m_net)) << c.m_inst << "." << c.m_port;
  }
}

// CHK4/CHK5: module pass(inout interconnect p); module generic_pass(inout interconnect p);
TEST_F(IcUserNettypeTest, InterconnectPorts) {
  for (const char *const name : {"pass", "generic_pass"}) {
    const hldb::Module *const mod = hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
    ASSERT_NE(mod, nullptr) << name;
    const hldb::Port *const p = hldb::findByName<hldb::Port>("p", mod->getPorts());
    ASSERT_NE(p, nullptr) << name;
    EXPECT_EQ(p->getDirection(), vpiInout) << name;
    const hldb::Net *const net = getNet(mod, "p");
    ASSERT_NE(net, nullptr) << name << " has a net for its interconnect port";
    EXPECT_EQ(net->getNetType(), vpiInterconnect) << name;
  }
  // pass forwards its port to rrcv u_r.
  const hldb::Module *const pass = hldb::findByDefName<hldb::Module>("pass", m_design->getAllModules());
  ASSERT_NE(pass, nullptr);
  const hldb::RefInstance *const ur = getInst(pass, "u_r");
  ASSERT_NE(ur, nullptr);
  EXPECT_EQ(getInstDefName(ur), std::string_view("rrcv"));
  EXPECT_EQ(getRootActual(getHighConn(ur, "i")), getNet(pass, "p"));
}

// CHK5: n5w is shared with wdrv's 'output wire o', so the merged net is a wire.
// 23.3.3.7.1: an interconnect merged with a typed net takes that net's type; 37.17 detail 12 exposes
// the result as vpiResolvedNetType.
// The resolved type exists only after elaboration (simulated net, 23.3.3.7), so it is checked only
// in an elaborated design.
TEST_F(IcUserNettypeTest, ResolvedNetTypeIsWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  for (const char *const name : {"n5w"}) {
    const hldb::Net *const net = getNet(top, name);
    ASSERT_NE(net, nullptr) << name;
    if (m_design->getElaborated()) {
      EXPECT_EQ(net->getResolvedNetType(), vpiWire) << name;
    }
  }
}

// The file is legal SystemVerilog, so compilation must not report any error.
TEST_F(IcUserNettypeTest, CompilesWithoutErrors) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
