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

// Tests for tests/Google/chapter-6/6.6.8--ic_decl_forms.sv
// Expected results follow IEEE 1800-2023 (6.6.8 generic interconnect, 6.7.1, 23.3.3.7.1,
// 37.17, 37.24); they are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - CHK1-5: w1, w2, w3, a, b, c, d are nets of top with vpiNetType == vpiInterconnect (37.17 detail 12),
//     explicitly declared (not implicit) and without a declaration assignment (6.7.1)
//   - CHK4: 'interconnect a, b;' declares both a and b on line 44; c comes from its own declaration
//   - CHK5: d carries exactly the one delay value #5 (6.7.1: at most one delay); w1 has none
//   - CHK2: w2 keeps its packed range [3:0] (37.24 detail 1: equal to the connected wire [3:0])
//   - Connections: each sink instance's port p is connected to the right interconnect net;
//     w3[0] / w3[1] are element (bit) selects of the unpacked array w3
//   - Resolution: every interconnect merges with the sink's 'input wire p' -> vpiResolvedNetType
//     vpiWire (23.3.3.7.1, 37.17 detail 12)
//   - No errors: the whole file is legal
// Checked only in an elaborated design (m_design->getElaborated()):
//   - ResolvedNetTypeIsWire: vpiResolvedNetType is the simulated net's type (23.3.3.7.1, 37.17 detail 12)
// Not checked:
//   - Size/range of the unpacked dimension of w3 ([1:0]): HLDB has no interconnect-array object
//     (37.24 vpiSize/vpiLeftRange/vpiElement) and Net has no range accessor -- model gap
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: ic_decl_forms
//  19  :description: legal interconnect net declaration forms from 6.7.1
//  20  :tags: 6.6.8
//  21  */
//  22
//  23  // Checks in this file:
//  24  //   CHK1: scalar interconnect w1
//  25  //   CHK2: packed vector interconnect [3:0] w2
//  26  //   CHK3: packed + unpacked interconnect [3:0] w3 [1:0]
//  27  //   CHK4: two interconnect nets in one declaration: interconnect a, b (grammar allows at most two)
//  28  //   CHK5: interconnect with a single delay value #5
//  29
//  30  module sink1(input wire p);
//  31  endmodule
//  32
//  33  module sink4(input wire [3:0] p);
//  34  endmodule
//  35
//  36  module top;
//  37    // CHK1
//  38    interconnect w1;
//  39    // CHK2
//  40    interconnect [3:0] w2;
//  41    // CHK3
//  42    interconnect [3:0] w3 [1:0];
//  43    // CHK4
//  44    interconnect a, b;
//  45    interconnect c;
//  46    // CHK5
//  47    interconnect #5 d;
//  48
//  49    sink1 s_w1(w1);
//  50    sink4 s_w2(w2);
//  51    sink4 s_w30(w3[0]);
//  52    sink4 s_w31(w3[1]);
//  53    sink1 s_a(a);
//  54    sink1 s_b(b);
//  55    sink1 s_c(c);
//  56    sink1 s_d(d);
//  57  endmodule

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

class IcDeclFormsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.6.8--ic_decl_forms.hlc"}); }
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

TEST_F(IcDeclFormsTest, AllDeclaredNetsAreInterconnect) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module top not found";
  for (const char *const name : {"w1", "w2", "w3", "a", "b", "c", "d"}) {
    const hldb::Net *const net = getNet(top, name);
    ASSERT_NE(net, nullptr) << "interconnect '" << name << "' not found in top";
    EXPECT_EQ(net->getNetType(), vpiInterconnect) << name;
    EXPECT_FALSE(net->getImplicitDecl()) << name << " is explicitly declared";
    EXPECT_FALSE(net->getNetDeclAssign()) << name << ": interconnect has no declaration assignment";
  }
}

// CHK4: one declaration may name two nets (Syntax 6-1: net_identifier [ , net_identifier ]).
TEST_F(IcDeclFormsTest, TwoNamesInOneDeclaration) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const a = getNet(top, "a");
  const hldb::Net *const b = getNet(top, "b");
  const hldb::Net *const c = getNet(top, "c");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(b, nullptr);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(a->getStartLine(), 44u);
  EXPECT_EQ(b->getStartLine(), 44u) << "a and b come from the same declaration";
  EXPECT_EQ(c->getStartLine(), 45u);
}

// CHK5: interconnect #5 d -- one delay value is legal (6.7.1).
TEST_F(IcDeclFormsTest, SingleDelayValue) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const d = getNet(top, "d");
  ASSERT_NE(d, nullptr);
  ASSERT_NE(d->getDelay(), nullptr) << "d should carry its #5 delay";
  const hldb::Constant *const delay = any_cast<hldb::Constant>(d->getDelay());
  ASSERT_NE(delay, nullptr) << "#5 is a constant delay";
  EXPECT_EQ(delay->getDecompile(), std::string_view("5"));

  const hldb::Net *const w1 = getNet(top, "w1");
  ASSERT_NE(w1, nullptr);
  EXPECT_EQ(w1->getDelay(), nullptr) << "w1 is declared without a delay";
}

// CHK2: interconnect [3:0] w2 -- the packed range is [3:0].
TEST_F(IcDeclFormsTest, PackedVectorRange) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const w2 = getNet(top, "w2");
  ASSERT_NE(w2, nullptr);
  const hldb::LogicTypespec *const ts = hldb::getTypespec<hldb::LogicTypespec>(w2);
  ASSERT_NE(ts, nullptr) << "w2 should have a 4-state vector typespec";
  ASSERT_NE(ts->getRanges(), nullptr);
  ASSERT_EQ(ts->getRanges()->size(), 1u);
  const hldb::Range *const r = ts->getRanges()->at(0);
  const hldb::Constant *const left = any_cast<hldb::Constant>(r->getLeftExpr());
  const hldb::Constant *const right = any_cast<hldb::Constant>(r->getRightExpr());
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(left->getDecompile(), std::string_view("3"));
  EXPECT_EQ(right->getDecompile(), std::string_view("0"));
}

// Every sink instance is connected to the interconnect net named in the source.
TEST_F(IcDeclFormsTest, PortConnectionsBindToInterconnectNets) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const std::pair<const char *, const char *> conns[] = {{"s_w1", "w1"},  {"s_w2", "w2"}, {"s_w30", "w3"},
                                                         {"s_w31", "w3"}, {"s_a", "a"},   {"s_b", "b"},
                                                         {"s_c", "c"},    {"s_d", "d"}};
  for (const std::pair<const char *, const char *> &conn : conns) {
    const hldb::RefInstance *const inst = getInst(top, conn.first);
    ASSERT_NE(inst, nullptr) << "instance " << conn.first << " not found";
    const hldb::Any *const hc = getHighConn(inst, "p");
    ASSERT_NE(hc, nullptr) << conn.first << ".p is unconnected";
    EXPECT_EQ(getRootActual(hc), getNet(top, conn.second)) << conn.first << ".p should bind to " << conn.second;
  }
}

// CHK3: w3[0] and w3[1] are element selects of the unpacked interconnect array w3.
TEST_F(IcDeclFormsTest, UnpackedElementsAreSelected) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const std::pair<const char *, const char *> conns[] = {{"s_w30", "0"}, {"s_w31", "1"}};
  for (const std::pair<const char *, const char *> &conn : conns) {
    const hldb::Any *const hc = getHighConn(getInst(top, conn.first), "p");
    ASSERT_NE(hc, nullptr) << conn.first;
    const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(hc);
    ASSERT_NE(sel, nullptr) << conn.first << ".p should be an element select of w3";
    const hldb::Constant *const index = any_cast<hldb::Constant>(sel->getIndex());
    ASSERT_NE(index, nullptr);
    EXPECT_EQ(index->getDecompile(), std::string_view(conn.second));
  }
}

// Each net is connected to 'input wire p' of a sink module.
// 23.3.3.7.1: an interconnect merged with a typed net takes that net's type; 37.17 detail 12 exposes
// the result as vpiResolvedNetType. The resolved type exists only after elaboration (simulated net,
// 23.3.3.7), so it is checked only in an elaborated design.
TEST_F(IcDeclFormsTest, ResolvedNetTypeIsWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  for (const char *const name : {"w1", "w2", "w3", "a", "b", "c", "d"}) {
    const hldb::Net *const net = getNet(top, name);
    ASSERT_NE(net, nullptr) << name;
    if (m_design->getElaborated()) {
      EXPECT_EQ(net->getResolvedNetType(), vpiWire) << name;
    }
  }
}

// The file is legal SystemVerilog, so compilation must not report any error.
TEST_F(IcDeclFormsTest, CompilesWithoutErrors) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
