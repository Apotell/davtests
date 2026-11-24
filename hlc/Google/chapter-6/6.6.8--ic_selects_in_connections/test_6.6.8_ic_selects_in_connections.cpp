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

// Tests for tests/Google/chapter-6/6.6.8--ic_selects_in_connections.sv
// Expected results follow IEEE 1800-2023 (6.6.8 generic interconnect, 6.7.1, 23.3.3.7.1,
// 37.17, 37.24); they are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - w, hi, lo, m are vpiInterconnect nets of top
//   - CHK1: u1.p = w[0], u1b.p = w[1] (bit selects with constant index)
//   - CHK2: u2.p = w[3:2] (part select, range 3:2)
//   - CHK3: u3.p = w[4 +: 4] (indexed part select, vpiPosIndexed, base 4, width 4)
//   - CHK4: u4.p = {hi, lo} (concatenation of interconnect nets only -- a legal net_lvalue)
//   - CHK5: u5 in the nested generate loops is connected through an element select of m
//   - Resolution: all interconnects merge with 'inout wire p' -> vpiWire
//   - No errors: the whole file is legal
// Checked only in an elaborated design (m_design->getElaborated()):
//   - ResolvedNetTypeIsWire: vpiResolvedNetType is the simulated net's type (23.3.3.7.1, 37.17 detail 12)
// Not checked:
//   - Which element m[i][j] each loop iteration receives: the definition model holds the loop once
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: ic_selects_in_connections
//  19  :description: selects and concatenations of interconnect nets used as port connections
//  20  :tags: 6.6.8
//  21  */
//  22
//  23  // Checks in this file:
//  24  //   CHK1: bit select of an interconnect vector as a port connection
//  25  //   CHK2: part select of an interconnect vector as a port connection
//  26  //   CHK3: indexed part select w[+:] of an interconnect vector
//  27  //   CHK4: concatenation of interconnect nets only as a port connection
//  28  //   CHK5: element of a 2-D unpacked interconnect array as a port connection
//  29
//  30  module leaf1(inout wire p);
//  31  endmodule
//  32
//  33  module leaf2(inout wire [1:0] p);
//  34  endmodule
//  35
//  36  module leaf4(inout wire [3:0] p);
//  37  endmodule
//  38
//  39  module top;
//  40    interconnect [7:0] w;
//  41    interconnect [1:0] hi, lo;
//  42    interconnect m [0:1][0:2];
//  43    // CHK1
//  44    leaf1 u1(w[0]);
//  45    leaf1 u1b(w[1]);
//  46    // CHK2
//  47    leaf2 u2(w[3:2]);
//  48    // CHK3
//  49    leaf4 u3(w[4 +: 4]);
//  50    // CHK4
//  51    leaf4 u4({hi, lo});
//  52    // CHK5
//  53    for (genvar i = 0; i < 2; i++) begin : gi
//  54      for (genvar j = 0; j < 3; j++) begin : gj
//  55        leaf1 u5(m[i][j]);
//  56      end
//  57    end
//  58  endmodule

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/expr.h>
#include <hldb/gen_block.h>
#include <hldb/gen_for.h>
#include <hldb/gen_if.h>
#include <hldb/gen_if_else.h>
#include <hldb/gen_region.h>
#include <hldb/indexed_part_select.h>
#include <hldb/indexed_range.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/named_argument.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/part_select.h>
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

class IcSelectsInConnectionsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.6.8--ic_selects_in_connections.hlc"}); }
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

static std::string_view decompile(const hldb::Any *e) {
  const hldb::Constant *const c = any_cast<hldb::Constant>(e);
  return (c == nullptr) ? std::string_view() : c->getDecompile();
}

TEST_F(IcSelectsInConnectionsTest, NetsAreInterconnect) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module top not found";
  for (const char *const name : {"w", "hi", "lo", "m"}) {
    const hldb::Net *const net = getNet(top, name);
    ASSERT_NE(net, nullptr) << name;
    EXPECT_EQ(net->getNetType(), vpiInterconnect) << name;
  }
}

// CHK1: leaf1 u1(w[0]); leaf1 u1b(w[1]);
TEST_F(IcSelectsInConnectionsTest, BitSelects) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const std::pair<const char *, const char *> conns[] = {{"u1", "0"}, {"u1b", "1"}};
  for (const std::pair<const char *, const char *> &conn : conns) {
    const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(getHighConn(getInst(top, conn.first), "p"));
    ASSERT_NE(sel, nullptr) << conn.first << ".p should be a bit select of w";
    EXPECT_EQ(decompile(sel->getIndex()), std::string_view(conn.second));
    EXPECT_EQ(getRootActual(sel), getNet(top, "w"));
  }
}

// CHK2: leaf2 u2(w[3:2]);
TEST_F(IcSelectsInConnectionsTest, PartSelect) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::PartSelect *const ps = any_cast<hldb::PartSelect>(getHighConn(getInst(top, "u2"), "p"));
  ASSERT_NE(ps, nullptr) << "u2.p should be the part select w[3:2]";
  ASSERT_NE(ps->getRange(), nullptr);
  EXPECT_EQ(decompile(ps->getRange()->getLeftExpr()), std::string_view("3"));
  EXPECT_EQ(decompile(ps->getRange()->getRightExpr()), std::string_view("2"));
  EXPECT_EQ(getRootActual(ps), getNet(top, "w"));
}

// CHK3: leaf4 u3(w[4 +: 4]);
TEST_F(IcSelectsInConnectionsTest, IndexedPartSelect) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::IndexedPartSelect *const ips = any_cast<hldb::IndexedPartSelect>(getHighConn(getInst(top, "u3"), "p"));
  ASSERT_NE(ips, nullptr) << "u3.p should be the indexed part select w[4 +: 4]";
  ASSERT_NE(ips->getRange(), nullptr);
  EXPECT_EQ(ips->getRange()->getIndexedPartSelectType(), vpiPosIndexed);
  EXPECT_EQ(decompile(ips->getRange()->getBaseExpr()), std::string_view("4"));
  EXPECT_EQ(decompile(ips->getRange()->getWidthExpr()), std::string_view("4"));
  EXPECT_EQ(getRootActual(ips), getNet(top, "w"));
}

// CHK4: leaf4 u4({hi, lo}); -- every operand is an interconnect, so this is a legal net_lvalue.
TEST_F(IcSelectsInConnectionsTest, ConcatenationOfInterconnects) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Operation *const cat = any_cast<hldb::Operation>(getHighConn(getInst(top, "u4"), "p"));
  ASSERT_NE(cat, nullptr) << "u4.p should be a concatenation";
  EXPECT_EQ(cat->getOpType(), vpiConcatOp);
  ASSERT_NE(cat->getOperands(), nullptr);
  ASSERT_EQ(cat->getOperands()->size(), 2u);
  EXPECT_EQ(getRootActual(cat->getOperands()->at(0)), getNet(top, "hi"));
  EXPECT_EQ(getRootActual(cat->getOperands()->at(1)), getNet(top, "lo"));
}

// CHK5: leaf1 u5(m[i][j]); inside two nested generate loops.
TEST_F(IcSelectsInConnectionsTest, TwoDimensionalElements) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  std::vector<const hldb::RefInstance *> insts;
  std::vector<const hldb::Net *> unused;
  collectFromGenerate(top, "u5", "", &insts, &unused);
  ASSERT_FALSE(insts.empty()) << "u5 not found inside the generate loops";
  for (const hldb::RefInstance *const inst : insts) {
    const hldb::Any *const hc = getHighConn(inst, "p");
    ASSERT_NE(hc, nullptr) << "u5.p unconnected";
    EXPECT_NE(any_cast<hldb::Select>(hc), nullptr) << "u5.p is an element select";
    EXPECT_EQ(getRootActual(hc), getNet(top, "m"));
  }
}

// Every net is connected to 'inout wire p' of a leaf.
// 23.3.3.7.1: an interconnect merged with a typed net takes that net's type; 37.17 detail 12 exposes
// the result as vpiResolvedNetType. The resolved type exists only after elaboration (simulated net,
// 23.3.3.7), so it is checked only in an elaborated design.
TEST_F(IcSelectsInConnectionsTest, ResolvedNetTypeIsWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  for (const char *const name : {"w", "hi", "lo", "m"}) {
    const hldb::Net *const net = getNet(top, name);
    ASSERT_NE(net, nullptr) << name;
    if (m_design->getElaborated()) {
      EXPECT_EQ(net->getResolvedNetType(), vpiWire) << name;
    }
  }
}

// The file is legal SystemVerilog, so compilation must not report any error.
TEST_F(IcSelectsInConnectionsTest, CompilesWithoutErrors) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
