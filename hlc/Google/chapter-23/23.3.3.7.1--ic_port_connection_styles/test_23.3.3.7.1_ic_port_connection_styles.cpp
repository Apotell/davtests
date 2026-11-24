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

// Tests for tests/Google/chapter-23/23.3.3.7.1--ic_port_connection_styles.sv
// Expected results follow IEEE 1800-2023 (6.6.8, 23.2.2, 23.3.3, 23.3.3.4, 23.3.3.7,
// 23.3.3.7.1, 23.3.3.7.2, 37.17); they are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - n1, n2, p, n5 in top are vpiInterconnect nets; mid.p is an inout interconnect port
//   - CHK1: u1(n1) positional -> leaf.p bound to n1 (not a named connection)
//   - CHK2: u2(.p(n2)) -> bound to n2, vpiConnByName
//   - CHK3: u3(.p) implicit named connection -> bound to top's p (23.3.2.3)
//   - CHK4: u4(.*) wildcard connection -> mid.p bound to top's p (23.3.2.4)
//   - CHK5: u5(n5) -> mid.p bound to n5; inside mid, u_leaf(.p(p)) binds leaf.p to mid's port net
//   - Resolution: every interconnect reaches leaf's 'inout wire p' -> vpiWire
//   - No errors: the whole file is legal
// Checked only in an elaborated design (m_design->getElaborated()):
//   - ResolvedNetTypeIsWire: vpiResolvedNetType is the simulated net's type (23.3.3.7.1, 37.17 detail 12)
// Not checked:
//   - Nothing intentionally left out
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: ic_port_connection_styles
//  19  :description: interconnect nets connected to ports with all connection styles and through hierarchy
//  20  :tags: 23.3.3.7.1
//  21  */
//  22
//  23  // Checks in this file:
//  24  //   CHK1: interconnect net connected by position
//  25  //   CHK2: interconnect net connected by name
//  26  //   CHK3: interconnect net connected with .name
//  27  //   CHK4: interconnect net connected with .*
//  28  //   CHK5: interconnect net -> interconnect port -> wire port (two levels)
//  29
//  30  module leaf(inout wire p);
//  31  endmodule
//  32
//  33  module mid(inout interconnect p);
//  34    leaf u_leaf(.p(p));
//  35  endmodule
//  36
//  37  module top;
//  38    interconnect n1, n2;
//  39    interconnect p, n5;
//  40    // CHK1
//  41    leaf u1(n1);
//  42    // CHK2
//  43    leaf u2(.p(n2));
//  44    // CHK3
//  45    leaf u3(.p);
//  46    // CHK4
//  47    mid u4(.*);
//  48    // CHK5
//  49    mid u5(n5);
//  50  endmodule

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

class IcPortConnectionStylesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "23.3.3.7.1--ic_port_connection_styles.hlc"}); }
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

TEST_F(IcPortConnectionStylesTest, Declarations) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module top not found";
  for (const char *const name : {"n1", "n2", "p", "n5"}) {
    const hldb::Net *const net = getNet(top, name);
    ASSERT_NE(net, nullptr) << name;
    EXPECT_EQ(net->getNetType(), vpiInterconnect) << name;
  }
  expectInterconnectPort(getModule("mid"), "p", vpiInout);
}

// CHK1-CHK5: each connection style binds the port to the right interconnect.
TEST_F(IcPortConnectionStylesTest, ConnectionStyles) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const std::pair<const char *, const char *> conns[] = {
      {"u1", "n1"}, {"u2", "n2"}, {"u3", "p"}, {"u4", "p"}, {"u5", "n5"}};
  for (const std::pair<const char *, const char *> &c : conns) {
    const hldb::RefInstance *const inst = getInst(top, c.first);
    ASSERT_NE(inst, nullptr) << c.first;
    const hldb::Any *const hc = getHighConn(inst, "p");
    ASSERT_NE(hc, nullptr) << c.first << ".p is unconnected";
    EXPECT_EQ(getRootActual(hc), getNet(top, c.second)) << c.first << ".p should bind to " << c.second;
  }
}

// CHK2/CHK3: .p(n2) and .p are named connections; CHK1 is positional.
TEST_F(IcPortConnectionStylesTest, ConnectionByName) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  for (const char *const name : {"u2", "u3"}) {
    const hldb::RefInstance *const inst = getInst(top, name);
    ASSERT_NE(inst, nullptr) << name;
    const hldb::NamedArgument *const na = hldb::findByName<hldb::NamedArgument>("p", inst->getArguments());
    ASSERT_NE(na, nullptr) << name;
    EXPECT_TRUE(na->getConnByName()) << name;
  }
  const hldb::RefInstance *const u1 = getInst(top, "u1");
  ASSERT_NE(u1, nullptr);
  ASSERT_NE(u1->getArguments(), nullptr);
  ASSERT_EQ(u1->getArguments()->size(), 1u);
  EXPECT_FALSE(u1->getArguments()->at(0)->getConnByName()) << "u1(n1) is positional";
}

// CHK5: inside mid, leaf u_leaf(.p(p)) binds to mid's own interconnect port net.
TEST_F(IcPortConnectionStylesTest, MidForwardsItsPort) {
  const hldb::Module *const mid = getModule("mid");
  ASSERT_NE(mid, nullptr);
  const hldb::RefInstance *const leaf = getInst(mid, "u_leaf");
  ASSERT_NE(leaf, nullptr);
  EXPECT_EQ(getInstDefName(leaf), std::string_view("leaf"));
  EXPECT_EQ(getRootActual(getHighConn(leaf, "p")), getNet(mid, "p"));
}

// The resolved type exists only after elaboration (simulated net, 23.3.3.7), so it is checked only
// in an elaborated design.
TEST_F(IcPortConnectionStylesTest, ResolvedNetTypeIsWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  for (const char *const name : {"n1", "n2", "p", "n5"}) {
    const hldb::Net *const net = getNet(top, name);
    ASSERT_NE(net, nullptr) << name;
    if (m_design->getElaborated()) {
      EXPECT_EQ(net->getResolvedNetType(), vpiWire) << name << " reaches leaf's 'inout wire p'";
    }
  }
  const hldb::Net *const midP = getNet(getModule("mid"), "p");
  ASSERT_NE(midP, nullptr);
  if (m_design->getElaborated()) {
    EXPECT_EQ(midP->getResolvedNetType(), vpiWire) << "mid.p merges with leaf's wire";
  }
}

// The file is legal SystemVerilog, so compilation must not report any error.
TEST_F(IcPortConnectionStylesTest, CompilesWithoutErrors) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
