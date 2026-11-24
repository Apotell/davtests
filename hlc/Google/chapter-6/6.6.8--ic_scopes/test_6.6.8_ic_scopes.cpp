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

// Tests for tests/Google/chapter-6/6.6.8--ic_scopes.sv
// Expected results follow IEEE 1800-2023 (6.6.8 generic interconnect, 6.7.1, 23.3.3.7.1,
// 37.17, 37.24); they are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - CHK1: the generate for loop body (block g) declares an interconnect n and a leaf u bound to it
//   - CHK2: the generate if block g_en declares an interconnect n and a leaf u bound to it
//   - CHK3: nested module inner_mod (instance u_inner) declares an interconnect n bound to its leaf u
//   - CHK4: interface ic_if declares interconnect 'inner'; u_hier.p is bound to it via u_if.inner
//   - CHK5: interface port_if has an inout port ext whose net is an interconnect; u_pif connects wire w
//   - Resolution: the interconnects merge with 'inout wire p' / wire w -> vpiWire (23.3.3.7.1)
//   - No errors: the whole file is legal
// Checked only in an elaborated design (m_design->getElaborated()):
//   - ResolvedNetTypeIsWire: vpiResolvedNetType is the simulated net's type (23.3.3.7.1, 37.17 detail 12)
// Not checked:
//   - Number of loop iterations (2): the definition model holds the loop body once
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: ic_scopes
//  19  :description: interconnect nets declared in different scopes
//  20  :tags: 6.6.8
//  21  */
//  22
//  23  // Checks in this file:
//  24  //   CHK1: interconnect declared inside a generate for loop
//  25  //   CHK2: interconnect declared inside a generate if block
//  26  //   CHK3: interconnect declared inside a nested module
//  27  //   CHK4: interconnect declared inside an interface and connected by hierarchical name
//  28  //   CHK5: interconnect port on an interface
//  29
//  30  module leaf(inout wire p);
//  31  endmodule
//  32
//  33  // CHK4
//  34  interface ic_if;
//  35    interconnect inner;
//  36  endinterface
//  37
//  38  // CHK5
//  39  interface port_if(inout interconnect ext);
//  40  endinterface
//  41
//  42  module top #(parameter bit EN = 1);
//  43    // CHK1
//  44    for (genvar i = 0; i < 2; i++) begin : g
//  45      interconnect n;
//  46      leaf u(n);
//  47    end
//  48    // CHK2
//  49    if (EN) begin : g_en
//  50      interconnect n;
//  51      leaf u(n);
//  52    end
//  53    // CHK3
//  54    module inner_mod;
//  55      interconnect n;
//  56      leaf u(n);
//  57    endmodule
//  58    inner_mod u_inner();
//  59
//  60    ic_if u_if();
//  61    leaf u_hier(u_if.inner);
//  62
//  63    wire w;
//  64    port_if u_pif(w);
//  65  endmodule

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

class IcScopesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.6.8--ic_scopes.hlc"}); }
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

  // Generate statement of 'top' of the given kind (GenFor for the loop, GenIf for the if block).
  template <typename T>
  static const T *findGenerate(const hldb::Module *top) {
    if ((top == nullptr) || (top->getGenStmts() == nullptr)) return nullptr;
    for (const hldb::Any *const g : *top->getGenStmts()) {
      if (const T *const t = any_cast<T>(g)) return t;
    }
    return nullptr;
  }

  // CHK1/CHK2: a generate body has exactly one interconnect n, and its leaf u is bound to that n.
  static void checkGenerateBody(const hldb::Any *gen, std::string_view what) {
    ASSERT_NE(gen, nullptr) << what << " not found in top";
    std::vector<const hldb::RefInstance *> insts;
    std::vector<const hldb::Net *> nets;
    walkGenerate(gen, "u", "n", &insts, &nets);
    ASSERT_EQ(nets.size(), 1u) << what << " declares one interconnect n";
    ASSERT_EQ(insts.size(), 1u) << what << " instantiates one leaf u";
    EXPECT_EQ(nets[0]->getNetType(), vpiInterconnect) << what;
    EXPECT_EQ(getRootActual(getHighConn(insts[0], "p")), nets[0]) << what << ": u.p binds to the n of its own block";
  }
};

TEST_F(IcScopesTest, GenerateForLoop) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module top not found";
  checkGenerateBody(findGenerate<hldb::GenFor>(top), "generate for block g");
}

TEST_F(IcScopesTest, GenerateIfBlock) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Any *gen = findGenerate<hldb::GenIf>(top);
  if (gen == nullptr) gen = findGenerate<hldb::GenIfElse>(top);
  checkGenerateBody(gen, "generate if block g_en");
}

// CHK3: module inner_mod; interconnect n; leaf u(n); endmodule  inner_mod u_inner();
TEST_F(IcScopesTest, NestedModule) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::RefInstance *const ui = getInst(top, "u_inner");
  ASSERT_NE(ui, nullptr);
  EXPECT_EQ(getInstDefName(ui), std::string_view("inner_mod"));
  const hldb::Module *const inner = any_cast<hldb::Module>(getInstDefinition(ui));
  ASSERT_NE(inner, nullptr) << "definition of inner_mod not found";
  const hldb::Net *const n = getNet(inner, "n");
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(n->getNetType(), vpiInterconnect);
  EXPECT_EQ(getRootActual(getHighConn(getInst(inner, "u"), "p")), n);
}

// CHK4: interface ic_if; interconnect inner; endinterface ... leaf u_hier(u_if.inner);
TEST_F(IcScopesTest, InterfaceMemberByHierarchicalName) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::RefInstance *const ui = getInst(top, "u_if");
  ASSERT_NE(ui, nullptr);
  EXPECT_EQ(getInstDefName(ui), std::string_view("ic_if"));
  const hldb::Interface *const ifc = hldb::findByDefName<hldb::Interface>("ic_if", m_design->getAllInterfaces());
  ASSERT_NE(ifc, nullptr);
  const hldb::Net *const inner = getNet(ifc, "inner");
  ASSERT_NE(inner, nullptr);
  EXPECT_EQ(inner->getNetType(), vpiInterconnect);
  const hldb::Any *const hc = getHighConn(getInst(top, "u_hier"), "p");
  ASSERT_NE(hc, nullptr) << "u_hier.p is unconnected";
  EXPECT_EQ(getRootActual(hc), inner) << "u_if.inner binds to the interface's interconnect";
}

// CHK5: interface port_if(inout interconnect ext); ... port_if u_pif(w);
TEST_F(IcScopesTest, InterconnectPortOnInterface) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Interface *const pif = hldb::findByDefName<hldb::Interface>("port_if", m_design->getAllInterfaces());
  ASSERT_NE(pif, nullptr);
  const hldb::Port *const ext = hldb::findByName<hldb::Port>("ext", pif->getPorts());
  ASSERT_NE(ext, nullptr);
  EXPECT_EQ(ext->getDirection(), vpiInout);
  const hldb::Net *const net = getNet(pif, "ext");
  ASSERT_NE(net, nullptr) << "the interconnect port has a net inside the interface";
  EXPECT_EQ(net->getNetType(), vpiInterconnect);
  const hldb::RefInstance *const u = getInst(top, "u_pif");
  ASSERT_NE(u, nullptr);
  EXPECT_EQ(getInstDefName(u), std::string_view("port_if"));
  EXPECT_EQ(getRootActual(getHighConn(u, "ext")), getNet(top, "w"));
}

// Every interconnect in this file merges with a wire (leaf's 'inout wire p' or top's wire w).
// The resolved type exists only after elaboration (simulated net, 23.3.3.7), so it is checked only
// in an elaborated design.
TEST_F(IcScopesTest, ResolvedNetTypeIsWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  std::vector<const hldb::RefInstance *> insts;
  std::vector<const hldb::Net *> nets;
  walkGenerate(findGenerate<hldb::GenFor>(top), "u", "n", &insts, &nets);
  const hldb::Any *genIf = findGenerate<hldb::GenIf>(top);
  if (genIf == nullptr) genIf = findGenerate<hldb::GenIfElse>(top);
  walkGenerate(genIf, "u", "n", &insts, &nets);
  // inner_mod is nested in top, so it is reached through its instance, not getAllModules().
  const hldb::Module *const inner = any_cast<hldb::Module>(getInstDefinition(getInst(top, "u_inner")));
  if (inner != nullptr) nets.emplace_back(getNet(inner, "n"));
  const hldb::Interface *const ifc = hldb::findByDefName<hldb::Interface>("ic_if", m_design->getAllInterfaces());
  if (ifc != nullptr) nets.emplace_back(getNet(ifc, "inner"));
  const hldb::Interface *const pif = hldb::findByDefName<hldb::Interface>("port_if", m_design->getAllInterfaces());
  if (pif != nullptr) nets.emplace_back(getNet(pif, "ext"));
  ASSERT_EQ(nets.size(), 5u) << "n (for), n (if), inner_mod.n, ic_if.inner, port_if.ext";
  for (const hldb::Net *const n : nets) {
    ASSERT_NE(n, nullptr);
    if (m_design->getElaborated()) {
      EXPECT_EQ(n->getResolvedNetType(), vpiWire) << n->getName() << " at line " << n->getStartLine();
    }
  }
}

// The file is legal SystemVerilog, so compilation must not report any error.
TEST_F(IcScopesTest, CompilesWithoutErrors) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
