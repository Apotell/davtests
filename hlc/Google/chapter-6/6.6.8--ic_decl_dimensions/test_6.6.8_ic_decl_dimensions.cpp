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

// Tests for tests/Google/chapter-6/6.6.8--ic_decl_dimensions.sv
// Expected results follow IEEE 1800-2023 (6.6.8 generic interconnect, 6.7.1, 23.3.3.7.1,
// 37.17, 37.24); they are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - All declared nets (asc, p2, csz, two, e0, e1, l0, l1) are vpiInterconnect nets of top
//   - CHK1: asc keeps the ascending packed range [0:7] (left 0, right 7)
//   - CHK2: p2 keeps both packed dimensions [1:0][3:0], outermost first
//   - CHK3/CHK4: the generate loop holds the instances s_c, s_t0, s_t1, each connected through an
//     element select to csz / two
//   - CHK5: e0/e1 come from two declarations (lines 49, 50); l0/l1 from one (line 51); all their
//     element connections bind to the right net
//   - Resolution to vpiWire for every net (23.3.3.7.1, 37.17 detail 12)
//   - No errors: the whole file is legal
// Checked only in an elaborated design (m_design->getElaborated()):
//   - ResolvedNetTypeIsWire: vpiResolvedNetType is the simulated net's type (23.3.3.7.1, 37.17 detail 12)
// Not checked:
//   - Unpacked dimension sizes [4], [0:3][0:1], [2]: no interconnect-array object / range accessor
//     in HLDB (37.24) -- model gap
//   - Number of generate iterations (4): the definition model holds the loop once, not per iteration
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: ic_decl_dimensions
//  19  :description: interconnect nets with different packed and unpacked dimension styles
//  20  :tags: 6.6.8
//  21  */
//  22
//  23  // Checks in this file:
//  24  //   CHK1: ascending packed range interconnect [0:7]
//  25  //   CHK2: multiple packed dimensions interconnect [1:0][3:0]
//  26  //   CHK3: C-style unpacked size interconnect w [4]
//  27  //   CHK4: two unpacked dimensions interconnect w [0:3][0:1]
//  28  //   CHK5: each interconnect declared on its own line vs list with unpacked dims
//  29
//  30  module sink1(input wire p);
//  31  endmodule
//  32
//  33  module sink8(input wire [0:7] p);
//  34  endmodule
//  35
//  36  module sink24(input wire [1:0][3:0] p);
//  37  endmodule
//  38
//  39  module top;
//  40    // CHK1
//  41    interconnect [0:7] asc;
//  42    // CHK2
//  43    interconnect [1:0][3:0] p2;
//  44    // CHK3
//  45    interconnect csz [4];
//  46    // CHK4
//  47    interconnect two [0:3][0:1];
//  48    // CHK5
//  49    interconnect e0;
//  50    interconnect e1;
//  51    interconnect l0 [2], l1 [2];
//  52
//  53    sink8  s_asc(asc);
//  54    sink24 s_p2(p2);
//  55    for (genvar i = 0; i < 4; i++) begin : g
//  56      sink1 s_c(csz[i]);
//  57      sink1 s_t0(two[i][0]);
//  58      sink1 s_t1(two[i][1]);
//  59    end
//  60    sink1 s_e0(e0);
//  61    sink1 s_e1(e1);
//  62    sink1 s_l00(l0[0]);
//  63    sink1 s_l01(l0[1]);
//  64    sink1 s_l10(l1[0]);
//  65    sink1 s_l11(l1[1]);
//  66  endmodule

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

class IcDeclDimensionsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.6.8--ic_decl_dimensions.hlc"}); }
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

TEST_F(IcDeclDimensionsTest, AllDeclaredNetsAreInterconnect) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module top not found";
  for (const char *const name : {"asc", "p2", "csz", "two", "e0", "e1", "l0", "l1"}) {
    const hldb::Net *const net = getNet(top, name);
    ASSERT_NE(net, nullptr) << "interconnect '" << name << "' not found";
    EXPECT_EQ(net->getNetType(), vpiInterconnect) << name;
    EXPECT_FALSE(net->getImplicitDecl()) << name;
  }
}

static void expectRange(const hldb::Range *r, std::string_view left, std::string_view right) {
  ASSERT_NE(r, nullptr);
  const hldb::Constant *const l = any_cast<hldb::Constant>(r->getLeftExpr());
  const hldb::Constant *const rt = any_cast<hldb::Constant>(r->getRightExpr());
  ASSERT_NE(l, nullptr);
  ASSERT_NE(rt, nullptr);
  EXPECT_EQ(l->getDecompile(), left);
  EXPECT_EQ(rt->getDecompile(), right);
}

// CHK1: [0:7] is ascending; the declared direction must be kept (left 0, right 7).
TEST_F(IcDeclDimensionsTest, AscendingPackedRange) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const asc = getNet(top, "asc");
  ASSERT_NE(asc, nullptr);
  const hldb::LogicTypespec *const ts = hldb::getTypespec<hldb::LogicTypespec>(asc);
  ASSERT_NE(ts, nullptr);
  ASSERT_NE(ts->getRanges(), nullptr);
  ASSERT_EQ(ts->getRanges()->size(), 1u);
  expectRange(ts->getRanges()->at(0), "0", "7");
}

// CHK2: [1:0][3:0] -- two packed dimensions, outermost first.
TEST_F(IcDeclDimensionsTest, MultiplePackedDimensions) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const p2 = getNet(top, "p2");
  ASSERT_NE(p2, nullptr);
  const hldb::LogicTypespec *const ts = hldb::getTypespec<hldb::LogicTypespec>(p2);
  ASSERT_NE(ts, nullptr);
  ASSERT_NE(ts->getRanges(), nullptr);
  ASSERT_EQ(ts->getRanges()->size(), 2u);
  expectRange(ts->getRanges()->at(0), "1", "0");
  expectRange(ts->getRanges()->at(1), "3", "0");
}

// CHK3/CHK4: the loop body connects one element of csz and two elements of two.
TEST_F(IcDeclDimensionsTest, GenerateElementConnections) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const std::pair<const char *, const char *> groups[] = {{"s_c", "csz"}, {"s_t0", "two"}, {"s_t1", "two"}};
  for (const std::pair<const char *, const char *> &grp : groups) {
    std::vector<const hldb::RefInstance *> insts;
    std::vector<const hldb::Net *> unused;
    collectFromGenerate(top, grp.first, "", &insts, &unused);
    ASSERT_FALSE(insts.empty()) << grp.first << " not found inside the generate loop";
    for (const hldb::RefInstance *const inst : insts) {
      const hldb::Any *const hc = getHighConn(inst, "p");
      ASSERT_NE(hc, nullptr) << grp.first << ".p unconnected";
      EXPECT_NE(any_cast<hldb::Select>(hc), nullptr) << grp.first << ".p is an element select";
      EXPECT_EQ(getRootActual(hc), getNet(top, grp.second)) << grp.first << ".p should bind to " << grp.second;
    }
  }
}

// CHK5: separate declarations vs. one declaration with two unpacked arrays.
TEST_F(IcDeclDimensionsTest, SeparateAndListDeclarations) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Net *const e0 = getNet(top, "e0");
  const hldb::Net *const e1 = getNet(top, "e1");
  const hldb::Net *const l0 = getNet(top, "l0");
  const hldb::Net *const l1 = getNet(top, "l1");
  ASSERT_NE(e0, nullptr);
  ASSERT_NE(e1, nullptr);
  ASSERT_NE(l0, nullptr);
  ASSERT_NE(l1, nullptr);
  EXPECT_EQ(e0->getStartLine(), 49u);
  EXPECT_EQ(e1->getStartLine(), 50u);
  EXPECT_EQ(l0->getStartLine(), 51u);
  EXPECT_EQ(l1->getStartLine(), 51u) << "l0 and l1 come from the same declaration";
  const std::pair<const char *, const char *> conns[] = {{"s_e0", "e0"},  {"s_e1", "e1"},  {"s_l00", "l0"},
                                                         {"s_l01", "l0"}, {"s_l10", "l1"}, {"s_l11", "l1"}};
  for (const std::pair<const char *, const char *> &conn : conns) {
    const hldb::RefInstance *const inst = getInst(top, conn.first);
    ASSERT_NE(inst, nullptr) << conn.first;
    EXPECT_EQ(getRootActual(getHighConn(inst, "p")), getNet(top, conn.second)) << conn.first;
  }
}

// Each net is connected to 'input wire p' of its sinks.
// 23.3.3.7.1: an interconnect merged with a typed net takes that net's type; 37.17 detail 12 exposes
// the result as vpiResolvedNetType. The resolved type exists only after elaboration (simulated net,
// 23.3.3.7), so it is checked only in an elaborated design.
TEST_F(IcDeclDimensionsTest, ResolvedNetTypeIsWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  for (const char *const name : {"asc", "p2", "csz", "two", "e0", "e1", "l0", "l1"}) {
    const hldb::Net *const net = getNet(top, name);
    ASSERT_NE(net, nullptr) << name;
    if (m_design->getElaborated()) {
      EXPECT_EQ(net->getResolvedNetType(), vpiWire) << name;
    }
  }
}

// The file is legal SystemVerilog, so compilation must not report any error.
TEST_F(IcDeclDimensionsTest, CompilesWithoutErrors) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
