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

// Tests for tests/Google/chapter-28/28.3--ic_instance_array.sv
// Expected results follow IEEE 1800-2023 (28.3.6 arrays of instances, 6.6.8, 23.3.3.5,
// 23.3.3.7.1, 37.17); they are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - n4, n8, arr, aBus, f in top are vpiInterconnect nets; fwd.q is an inout interconnect port
//   - Each array of instances exists with its module and range (28.3.6):
//     a4 bit_leaf [3:0], a8 bit_leaf [0:7], a2 bit_leaf [1:0], driverArray driver [0:3],
//     cmpArray cmp [0:3], and u bit_leaf [3:0] inside fwd
//   - 28.3.6 interconnect rule holds in the source: bit length of the interconnect expression ==
//     number of instances (n4: 4 bits / 4, n8: 8 / 8, arr[1]: 2 / 2, q: 4 / 4)
//   - CHK1: a4.p <- n4; CHK2: a8.p <- n8; CHK3: a2.p <- arr[1] (element select, index 1)
//   - CHK4: driverArray.out <- aBus; cmpArray.inA <- aBus, cmpArray.rst <- rst, cmpArray.equal <- dBus
//   - CHK5: u_fwd.q <- f; inside fwd, u.p <- q
//   - Resolution: every interconnect merges with the wire ports it reaches -> vpiWire
//   - No errors: the whole file is legal
// Checked only in an elaborated design (m_design->getElaborated()):
//   - ResolvedNetTypeIsWire: vpiResolvedNetType is the simulated net's type (23.3.3.7.1, 37.17 detail 12)
// Not checked:
//   - Which bit each single instance receives (a4[0].p = n4[0], ...): the definition model has no
//     expanded per-instance objects (that needs elaboration)
//   - Unpacked sizes of arr [0:2] and aBus [0:3][0:1]: no interconnect-array object in HLDB (37.24)
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: ic_instance_array
//  19  :description: interconnect expressions connected to arrays of instances
//  20  :tags: 28.3
//  21  */
//  22
//  23  // Checks in this file:
//  24  //   CHK1: interconnect [3:0] to four instances with 1-bit ports (length equals array length)
//  25  //   CHK2: interconnect [0:7] to an instance array declared [0:7]
//  26  //   CHK3: element of an unpacked interconnect array to an instance array
//  27  //   CHK4: LRM aBus[0:3][0:1] connected to driver and cmp instance arrays
//  28  //   CHK5: interconnect port of a module forwarded to an instance array
//  29
//  30  module bit_leaf(inout wire p);
//  31  endmodule
//  32
//  33  module driver(output wire out[0:1]);
//  34  endmodule
//  35
//  36  module cmp(input wire inA[0:1], input logic rst, output logic equal);
//  37  endmodule
//  38
//  39  module fwd(inout interconnect [3:0] q);
//  40    // CHK5
//  41    bit_leaf u[3:0](q);
//  42  endmodule
//  43
//  44  module top;
//  45    // CHK1
//  46    interconnect [3:0] n4;
//  47    bit_leaf a4[3:0](n4);
//  48    // CHK2
//  49    interconnect [0:7] n8;
//  50    bit_leaf a8[0:7](n8);
//  51    // CHK3
//  52    interconnect [1:0] arr [0:2];
//  53    bit_leaf a2[1:0](arr[1]);
//  54    // CHK4
//  55    interconnect aBus[0:3][0:1];
//  56    logic [0:3] dBus;
//  57    logic rst;
//  58    driver driverArray[0:3](aBus);
//  59    cmp    cmpArray[0:3](aBus, rst, dBus);
//  60    // CHK5
//  61    interconnect [3:0] f;
//  62    fwd u_fwd(f);
//  63  endmodule

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
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
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>
#include <vector>

namespace hlc {

class IcInstanceArrayTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "28.3--ic_instance_array.hlc"}); }
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
  // For an array of instances the typespec is an array typespec around the module typespec; the
  // element is returned so that each instance's definition and ports are found.
  static const hldb::Typespec *getInstTypespec(const hldb::RefInstance *inst) {
    const hldb::Typespec *ts =
        ((inst == nullptr) || (inst->getTypespec() == nullptr)) ? nullptr : inst->getTypespec()->getActual();
    if (const hldb::ArrayTypespec *const at = any_cast<hldb::ArrayTypespec>(ts)) {
      ts = (at->getElemTypespec() == nullptr) ? nullptr : at->getElemTypespec()->getActual();
    }
    return ts;
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

  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }

  // Array of instances 'name' in 'mod': a RefInstance whose typespec is an array typespec (the array
  // range) whose element is the module typespec (IEEE 1800-2023 Sec 28.3.6, 37.11 module array).
  static const hldb::ArrayTypespec *getArrayTypespec(const hldb::Module *mod, std::string_view name) {
    const hldb::RefInstance *const inst = getInst(mod, name);
    if ((inst == nullptr) || (inst->getTypespec() == nullptr)) return nullptr;
    return any_cast<hldb::ArrayTypespec>(inst->getTypespec()->getActual());
  }

  // The expression connected to port 'port' of the instance array 'name' in 'mod'.
  static const hldb::Any *getArrayConn(const hldb::Module *mod, std::string_view name, std::string_view port) {
    return getHighConn(getInst(mod, name), port);
  }

  // The instance array 'name' in 'mod' instantiates module 'def' with range [left:right].
  static void expectArrayRange(const hldb::Module *mod, std::string_view name, std::string_view def,
                               std::string_view left, std::string_view right) {
    const hldb::ArrayTypespec *const arr = getArrayTypespec(mod, name);
    ASSERT_NE(arr, nullptr) << name << " should be an array of instances";
    EXPECT_EQ(getInstDefName(getInst(mod, name)), def) << name;
    ASSERT_NE(arr->getRange(), nullptr) << name;
    const hldb::Constant *const l = any_cast<hldb::Constant>(arr->getRange()->getLeftExpr());
    const hldb::Constant *const r = any_cast<hldb::Constant>(arr->getRange()->getRightExpr());
    ASSERT_NE(l, nullptr) << name;
    ASSERT_NE(r, nullptr) << name;
    EXPECT_EQ(l->getDecompile(), left) << name;
    EXPECT_EQ(r->getDecompile(), right) << name;
  }
};

TEST_F(IcInstanceArrayTest, InterconnectDeclarations) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module top not found";
  for (const char *const name : {"n4", "n8", "arr", "aBus", "f"}) {
    const hldb::Net *const net = getNet(top, name);
    ASSERT_NE(net, nullptr) << name;
    EXPECT_EQ(net->getNetType(), vpiInterconnect) << name;
  }
  const hldb::Module *const fwd = getModule("fwd");
  ASSERT_NE(fwd, nullptr);
  const hldb::Port *const q = hldb::findByName<hldb::Port>("q", fwd->getPorts());
  ASSERT_NE(q, nullptr);
  EXPECT_EQ(q->getDirection(), vpiInout);
  const hldb::Net *const qnet = getNet(fwd, "q");
  ASSERT_NE(qnet, nullptr);
  EXPECT_EQ(qnet->getNetType(), vpiInterconnect);
}

// Each array of instances is declared with its module and range.
TEST_F(IcInstanceArrayTest, ArraysOfInstances) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  expectArrayRange(top, "a4", "bit_leaf", "3", "0");
  expectArrayRange(top, "a8", "bit_leaf", "0", "7");
  expectArrayRange(top, "a2", "bit_leaf", "1", "0");
  expectArrayRange(top, "driverArray", "driver", "0", "3");
  expectArrayRange(top, "cmpArray", "cmp", "0", "3");
  expectArrayRange(getModule("fwd"), "u", "bit_leaf", "3", "0");
}

// 28.3.6: for an interconnect expression the bit length shall equal the array length. Here the packed
// width of each interconnect matches its array: [3:0] -> 4 instances, [0:7] -> 8, [1:0] -> 2.
TEST_F(IcInstanceArrayTest, InterconnectWidthMatchesArrayLength) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  struct Case {
    const hldb::Module *m_mod;
    const char *m_net;
    const char *m_left;
    const char *m_right;
  };
  const Case cases[] = {
      {top, "n4", "3", "0"}, {top, "n8", "0", "7"}, {top, "arr", "1", "0"}, {getModule("fwd"), "q", "3", "0"}};
  for (const Case &c : cases) {
    // arr [1:0] [0:2] is an unpacked array: its packed width is on the element typespec.
    const hldb::Typespec *ts0 = hldb::getTypespec(getNet(c.m_mod, c.m_net));
    if (const hldb::ArrayTypespec *const at = any_cast<hldb::ArrayTypespec>(ts0)) {
      ts0 = (at->getElemTypespec() == nullptr) ? nullptr : at->getElemTypespec()->getActual();
    }
    const hldb::LogicTypespec *const ts = any_cast<hldb::LogicTypespec>(ts0);
    ASSERT_NE(ts, nullptr) << c.m_net << " should have a packed vector typespec";
    ASSERT_NE(ts->getRanges(), nullptr) << c.m_net;
    ASSERT_EQ(ts->getRanges()->size(), 1u) << c.m_net;
    const hldb::Constant *const l = any_cast<hldb::Constant>(ts->getRanges()->at(0)->getLeftExpr());
    const hldb::Constant *const r = any_cast<hldb::Constant>(ts->getRanges()->at(0)->getRightExpr());
    ASSERT_NE(l, nullptr) << c.m_net;
    ASSERT_NE(r, nullptr) << c.m_net;
    EXPECT_EQ(l->getDecompile(), std::string_view(c.m_left)) << c.m_net;
    EXPECT_EQ(r->getDecompile(), std::string_view(c.m_right)) << c.m_net;
  }
}

// CHK1-CHK4: every array port is connected to the right expression.
TEST_F(IcInstanceArrayTest, ArrayConnections) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  struct Conn {
    const char *m_array;
    const char *m_port;
    const hldb::Any *m_expected;
  };
  const Conn conns[] = {{"a4", "p", getNet(top, "n4")},
                        {"a8", "p", getNet(top, "n8")},
                        {"a2", "p", getNet(top, "arr")},
                        {"driverArray", "out", getNet(top, "aBus")},
                        {"cmpArray", "inA", getNet(top, "aBus")},
                        {"cmpArray", "rst", hldb::findByName<hldb::Variable>("rst", top->getVariables())},
                        {"cmpArray", "equal", hldb::findByName<hldb::Variable>("dBus", top->getVariables())}};
  for (const Conn &c : conns) {
    ASSERT_NE(c.m_expected, nullptr) << c.m_array << "." << c.m_port << ": expected object not found";
    const hldb::Any *const hc = getArrayConn(top, c.m_array, c.m_port);
    ASSERT_NE(hc, nullptr) << c.m_array << "." << c.m_port << " is unconnected";
    EXPECT_EQ(getRootActual(hc), c.m_expected) << c.m_array << "." << c.m_port;
  }
}

// CHK3: bit_leaf a2[1:0](arr[1]); -- an element select of the unpacked interconnect array.
TEST_F(IcInstanceArrayTest, ElementOfUnpackedArray) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::BitSelect *const sel = any_cast<hldb::BitSelect>(getArrayConn(top, "a2", "p"));
  ASSERT_NE(sel, nullptr) << "a2.p should be the element select arr[1]";
  const hldb::Constant *const idx = any_cast<hldb::Constant>(sel->getIndex());
  ASSERT_NE(idx, nullptr);
  EXPECT_EQ(idx->getDecompile(), std::string_view("1"));
}

// CHK5: fwd u_fwd(f); and, inside fwd, bit_leaf u[3:0](q);
TEST_F(IcInstanceArrayTest, InterconnectPortForwardedToArray) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::RefInstance *const ufwd = getInst(top, "u_fwd");
  ASSERT_NE(ufwd, nullptr);
  EXPECT_EQ(getInstDefName(ufwd), std::string_view("fwd"));
  EXPECT_EQ(getRootActual(getHighConn(ufwd, "q")), getNet(top, "f"));
  const hldb::Module *const fwd = getModule("fwd");
  ASSERT_NE(fwd, nullptr);
  const hldb::Any *const hc = getArrayConn(fwd, "u", "p");
  ASSERT_NE(hc, nullptr) << "fwd.u.p is unconnected";
  EXPECT_EQ(getRootActual(hc), getNet(fwd, "q"));
}

// 23.3.3.7.1: each interconnect merges with the wire ports it reaches (bit_leaf 'inout wire p',
// driver 'output wire out', cmp 'input wire inA'), so the simulated net is a wire. The resolved type
// exists only after elaboration (simulated net, 23.3.3.7), so it is checked only in an elaborated design.
TEST_F(IcInstanceArrayTest, ResolvedNetTypeIsWire) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  for (const char *const name : {"n4", "n8", "arr", "aBus", "f"}) {
    const hldb::Net *const net = getNet(top, name);
    ASSERT_NE(net, nullptr) << name;
    if (m_design->getElaborated()) {
      EXPECT_EQ(net->getResolvedNetType(), vpiWire) << name;
    }
  }
  const hldb::Net *const q = getNet(getModule("fwd"), "q");
  ASSERT_NE(q, nullptr);
  if (m_design->getElaborated()) {
    EXPECT_EQ(q->getResolvedNetType(), vpiWire) << "fwd.q";
  }
}

// The file is legal SystemVerilog, so compilation must not report any error.
TEST_F(IcInstanceArrayTest, CompilesWithoutErrors) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
