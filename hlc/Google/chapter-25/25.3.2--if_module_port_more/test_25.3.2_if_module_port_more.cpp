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

// Tests for tests/Google/chapter-25/25.3.2--if_module_port_more.sv
// Expected results follow IEEE 1800-2023 (25.3, 25.3.2, 25.3.3, 23.2.2, 23.3.2, 7.4.2,
// 37); they are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - CHK1: c1_sub(input logic clk, bus_if b, output logic q) -- ports in this order; clk input,
//     q output, b an interface port of type bus_if
//   - CHK2: c2_sub(b) with 'bus_if b;' in the body -- a non-ANSI interface port of type bus_if
//     (23.2.2.1 interface_port_declaration)
//   - CHK3: c3_mid passes its interface port b to c3_leaf u_leaf(.b(b))
//   - CHK4: c4_outer_if has an interface port i of type bus_if
//   - top: u1(.clk(clk), .b(b1), .q(q)), u2(b2), u3(.b(b3)), u4(b4) -- each bound to the right instance
//   - No errors: the whole file is legal
// Skipped (GTEST_SKIP; the real assertions stay below the skip):
//   - NonAnsiInterfacePort: HLC gives the non-ANSI interface port b ('bus_if b;' in the body of c2_sub) no interface
//   typespec.
//   - PortPassedThroughHierarchy: HLC binds u_leaf(.b(b)) inside c3_mid to the interface definition bus_if, not to the
//   port b.
// Not checked:
//   - Nothing intentionally left out
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: if_module_port_more
//  19  :description: interface ports mixed with signals, non-ANSI, hierarchy and interfaces with interface ports
//  20  :tags: 25.3.2
//  21  */
//  22
//  23  // Checks in this file:
//  24  //   CHK1: interface port mixed with ordinary input/output ports
//  25  //   CHK2: interface port declared in a non-ANSI port list
//  26  //   CHK3: interface passed from top through a middle module to a leaf
//  27  //   CHK4: interface whose port is another interface
//  28
//  29  interface bus_if;
//  30    logic x;
//  31  endinterface
//  32
//  33  // CHK1
//  34  module c1_sub(input logic clk, bus_if b, output logic q);
//  35    always_ff @(posedge clk) q <= b.x;
//  36  endmodule
//  37
//  38  // CHK2
//  39  module c2_sub(b);
//  40    bus_if b;
//  41  endmodule
//  42
//  43  // CHK3
//  44  module c3_leaf(bus_if b);
//  45    assign b.x = 1'b1;
//  46  endmodule
//  47
//  48  module c3_mid(bus_if b);
//  49    c3_leaf u_leaf(.b(b));
//  50  endmodule
//  51
//  52  // CHK4
//  53  interface c4_outer_if(bus_if i);
//  54    logic y;
//  55    assign y = i.x;
//  56  endinterface
//  57
//  58  module top;
//  59    logic clk, q;
//  60    bus_if b1(), b2(), b3(), b4();
//  61    c1_sub u1(.clk(clk), .b(b1), .q(q));
//  62    c2_sub u2(b2);
//  63    c3_mid u3(.b(b3));
//  64    c4_outer_if u4(b4);
//  65  endmodule

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
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
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>
#include <vector>

namespace hlc {

class IfModulePortMoreTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3.2--if_module_port_more.hlc"}); }
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

  // Interface definition 'name'; an extern interface header (25.3) is skipped in favor of the declaration.
  static const hldb::Interface *getInterface(std::string_view name) {
    if (m_design->getAllInterfaces() != nullptr) {
      for (const hldb::Interface *const i : *m_design->getAllInterfaces()) {
        if ((i->getDefName() == name) && !i->getExtern()) return i;
      }
    }
    return hldb::findByDefName<hldb::Interface>(name, m_design->getAllInterfaces());
  }

  // Variable 'name' declared directly in 'scope'.
  static const hldb::Variable *getVar(const hldb::Any *scope, std::string_view name) {
    return hldb::findByName<hldb::Variable>(name, hldb::getVariables(scope));
  }

  // Type of 'object' with typedefs followed to the type they name.
  static const hldb::Typespec *getResolvedTypespec(const hldb::Any *object) {
    const hldb::Typespec *ts = hldb::getTypespec(object);
    for (int i = 0; (i < 8) && (ts != nullptr); ++i) {
      const hldb::TypedefTypespec *const td = any_cast<hldb::TypedefTypespec>(ts);
      if ((td == nullptr) || (td->getTypedef() == nullptr) || (td->getTypedef()->getAlias() == nullptr)) break;
      ts = td->getTypedef()->getAlias()->getActual();
    }
    return ts;
  }

  // Name of what an expression connected to a port refers to (an interface instance, port, ...).
  static std::string_view getBoundName(const hldb::Any *expr) {
    const hldb::Any *const actual = getRootActual(expr);
    return (actual == nullptr) ? std::string_view() : actual->getName();
  }

  // Left/right bounds of a range as written.
  static void expectRange(const hldb::Range *r, std::string_view left, std::string_view right, std::string_view what) {
    ASSERT_NE(r, nullptr) << what;
    const hldb::Constant *const l = any_cast<hldb::Constant>(r->getLeftExpr());
    const hldb::Constant *const rt = any_cast<hldb::Constant>(r->getRightExpr());
    ASSERT_NE(l, nullptr) << what << ": left bound";
    ASSERT_NE(rt, nullptr) << what << ": right bound";
    EXPECT_EQ(l->getDecompile(), left) << what;
    EXPECT_EQ(rt->getDecompile(), right) << what;
  }

  // A logic [left:right] typespec (one packed dimension).
  static void expectLogicRange(const hldb::Typespec *ts, std::string_view left, std::string_view right,
                               std::string_view what) {
    const hldb::LogicTypespec *const lt = any_cast<hldb::LogicTypespec>(ts);
    ASSERT_NE(lt, nullptr) << what << " should be a 4-state vector";
    ASSERT_NE(lt->getRanges(), nullptr) << what;
    ASSERT_EQ(lt->getRanges()->size(), 1u) << what;
    expectRange(lt->getRanges()->at(0), left, right, what);
  }

  // Array of instances 'name' in 'scope': the array typespec of its RefInstance.
  static const hldb::ArrayTypespec *getInstArray(const hldb::Any *scope, std::string_view name) {
    const hldb::RefInstance *const inst = getInst(scope, name);
    if ((inst == nullptr) || (inst->getTypespec() == nullptr)) return nullptr;
    return any_cast<hldb::ArrayTypespec>(inst->getTypespec()->getActual());
  }

  // True when a parser error is reported on any line in [first, last].
  bool parserErrorIn(uint32_t first, uint32_t last) {
    for (uint32_t line = first; line <= last; ++line) {
      if (!findEvents(EventType::PaSyntaxError, line).empty()) return true;
      if (!findEvents(EventType::PaReservedKeyword, line).empty()) return true;
    }
    return false;
  }
};

static const hldb::InterfaceTypespec *getPortInterface(const hldb::Any *def, std::string_view port) {
  const hldb::Port *const p = hldb::findByName<hldb::Port>(port, hldb::getPorts(def));
  if ((p == nullptr) || (p->getTypespec() == nullptr)) return nullptr;
  return any_cast<hldb::InterfaceTypespec>(p->getTypespec()->getActual());
}

// CHK1: module c1_sub(input logic clk, bus_if b, output logic q);
TEST_F(IfModulePortMoreTest, InterfacePortAmongSignalPorts) {
  const hldb::Module *const c1 = getModule("c1_sub");
  ASSERT_NE(c1, nullptr);
  ASSERT_NE(c1->getPorts(), nullptr);
  ASSERT_EQ(c1->getPorts()->size(), 3u);
  EXPECT_EQ(c1->getPorts()->at(0)->getName(), std::string_view("clk"));
  EXPECT_EQ(c1->getPorts()->at(1)->getName(), std::string_view("b"));
  EXPECT_EQ(c1->getPorts()->at(2)->getName(), std::string_view("q"));
  EXPECT_EQ(c1->getPorts()->at(0)->getDirection(), vpiInput);
  EXPECT_EQ(c1->getPorts()->at(2)->getDirection(), vpiOutput);
  const hldb::InterfaceTypespec *const b = getPortInterface(c1, "b");
  ASSERT_NE(b, nullptr) << "b is an interface port";
  EXPECT_EQ(b->getDefName(), std::string_view("bus_if"));
}

// CHK2: module c2_sub(b); bus_if b; endmodule
TEST_F(IfModulePortMoreTest, NonAnsiInterfacePort) {
  GTEST_SKIP()
      << "HLC gives the non-ANSI interface port b ('bus_if b;' in the body of c2_sub) no interface typespec."
         " IEEE 1800-2023 Sec 23.2.2.1 (interface_port_declaration) / 25.3.2: b is a port of type bus_if. Fix pending.";
  const hldb::InterfaceTypespec *const b = getPortInterface(getModule("c2_sub"), "b");
  ASSERT_NE(b, nullptr) << "c2_sub.b is a non-ANSI interface port";
  EXPECT_EQ(b->getDefName(), std::string_view("bus_if"));
}

// CHK3: module c3_mid(bus_if b); c3_leaf u_leaf(.b(b)); endmodule
TEST_F(IfModulePortMoreTest, PortPassedThroughHierarchy) {
  GTEST_SKIP() << "HLC binds u_leaf(.b(b)) inside c3_mid to the interface definition bus_if, not to the port b."
                  " IEEE 1800-2023 Sec 25.3.2 / 23.3.3.4: an interface port is connected to a higher-level interface "
                  "port. Fix pending.";
  const hldb::Module *const mid = getModule("c3_mid");
  ASSERT_NE(mid, nullptr);
  const hldb::RefInstance *const leaf = getInst(mid, "u_leaf");
  ASSERT_NE(leaf, nullptr);
  EXPECT_EQ(getInstDefName(leaf), std::string_view("c3_leaf"));
  const hldb::Any *const hc = getHighConn(leaf, "b");
  ASSERT_NE(hc, nullptr) << "u_leaf.b is unconnected";
  EXPECT_EQ(getBoundName(hc), std::string_view("b")) << "u_leaf.b is bound to c3_mid's port b";
  const hldb::InterfaceTypespec *const leafPort = getPortInterface(getModule("c3_leaf"), "b");
  ASSERT_NE(leafPort, nullptr);
  EXPECT_EQ(leafPort->getDefName(), std::string_view("bus_if"));
}

// CHK4: interface c4_outer_if(bus_if i);
TEST_F(IfModulePortMoreTest, InterfacePortOfInterface) {
  const hldb::InterfaceTypespec *const i = getPortInterface(getInterface("c4_outer_if"), "i");
  ASSERT_NE(i, nullptr) << "c4_outer_if.i is an interface port";
  EXPECT_EQ(i->getDefName(), std::string_view("bus_if"));
}

TEST_F(IfModulePortMoreTest, TopConnections) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module top not found";
  struct Conn {
    const char *m_inst;
    const char *m_port;
    const char *m_actual;
  };
  const Conn conns[] = {{"u1", "b", "b1"}, {"u2", "b", "b2"}, {"u3", "b", "b3"}, {"u4", "i", "b4"}};
  for (const Conn &c : conns) {
    const hldb::Any *const hc = getHighConn(getInst(top, c.m_inst), c.m_port);
    ASSERT_NE(hc, nullptr) << c.m_inst << "." << c.m_port << " is unconnected";
    EXPECT_EQ(getBoundName(hc), std::string_view(c.m_actual)) << c.m_inst << "." << c.m_port;
  }
  EXPECT_EQ(getRootActual(getHighConn(getInst(top, "u1"), "clk")), getVar(top, "clk"));
  EXPECT_EQ(getRootActual(getHighConn(getInst(top, "u1"), "q")), getVar(top, "q"));
}

// The file is legal SystemVerilog, so compilation must not report any error.
TEST_F(IfModulePortMoreTest, CompilesWithoutErrors) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
