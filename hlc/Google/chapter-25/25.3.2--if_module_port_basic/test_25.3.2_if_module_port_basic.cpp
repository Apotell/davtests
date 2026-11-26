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

// Tests for tests/Google/chapter-25/25.3.2--if_module_port_basic.sv
// Expected results follow IEEE 1800-2023 (25.3, 25.3.2, 25.3.3, 23.2.2, 23.3.2, 7.4.2,
// 37); they are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - test_bus has end label test_bus and variables req, gnt, x
//   - mem has one port a, sub one port iface, bridge two ports src, dst -- each port's type is the
//     interface test_bus (named interface port, 25.3.2)
//   - Inside mem, 'assign a.gnt = a.req' accesses gnt/req of test_bus through the port
//   - top declares sb, sb2, iface, s1, s2 as test_bus instances in one statement (line 46)
//   - CHK1: u1(.a(sb)); CHK2: u2(sb2) by position; CHK3: u3(.iface) implicit name; CHK4: u4(.*);
//     CHK5: u5(.src(s1), .dst(s2)) -- each port bound to the right interface instance
//   - No errors: the whole file is legal
// Not checked:
//   - Nothing intentionally left out
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: if_module_port_basic
//  19  :description: module ports of a named interface type
//  20  :tags: 25.3.2
//  21  */
//  22
//  23  // Checks in this file:
//  24  //   CHK1: interface port connected by name
//  25  //   CHK2: interface port connected by position
//  26  //   CHK3: interface port connected with .name (LRM test_bus example)
//  27  //   CHK4: interface port connected with .*
//  28  //   CHK5: module with two ports of the same interface type
//  29
//  30  interface test_bus;
//  31    logic req, gnt, x;
//  32  endinterface : test_bus
//  33
//  34  module mem(test_bus a);
//  35    assign a.gnt = a.req;
//  36  endmodule
//  37
//  38  module sub(test_bus iface);
//  39  endmodule
//  40
//  41  module bridge(test_bus src, test_bus dst);
//  42    assign dst.x = src.x;
//  43  endmodule
//  44
//  45  module top;
//  46    test_bus sb(), sb2(), iface(), s1(), s2();
//  47    // CHK1
//  48    mem u1(.a(sb));
//  49    // CHK2
//  50    mem u2(sb2);
//  51    // CHK3
//  52    sub u3(.iface);
//  53    // CHK4
//  54    sub u4(.*);
//  55    // CHK5
//  56    bridge u5(.src(s1), .dst(s2));
//  57  endmodule

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
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

class IfModulePortBasicTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3.2--if_module_port_basic.hlc"}); }
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

static const hldb::InterfaceTypespec *getPortInterface(const hldb::Module *mod, std::string_view port) {
  if (mod == nullptr) return nullptr;
  const hldb::Port *const p = hldb::findByName<hldb::Port>(port, mod->getPorts());
  if ((p == nullptr) || (p->getTypespec() == nullptr)) return nullptr;
  return any_cast<hldb::InterfaceTypespec>(p->getTypespec()->getActual());
}

TEST_F(IfModulePortBasicTest, InterfaceDeclaration) {
  const hldb::Interface *const bus = getInterface("test_bus");
  ASSERT_NE(bus, nullptr);
  EXPECT_EQ(bus->getEndLabel(), std::string_view("test_bus"));
  for (const char *const name : {"req", "gnt", "x"}) EXPECT_NE(getVar(bus, name), nullptr) << name;
}

// mem(test_bus a), sub(test_bus iface), bridge(test_bus src, test_bus dst)
TEST_F(IfModulePortBasicTest, NamedInterfacePorts) {
  const std::pair<const char *, const char *> ports[] = {
      {"mem", "a"}, {"sub", "iface"}, {"bridge", "src"}, {"bridge", "dst"}};
  for (const std::pair<const char *, const char *> &p : ports) {
    const hldb::InterfaceTypespec *const its = getPortInterface(getModule(p.first), p.second);
    ASSERT_NE(its, nullptr) << p.first << "." << p.second << " should be an interface port";
    EXPECT_EQ(its->getDefName(), std::string_view("test_bus")) << p.first << "." << p.second;
  }
  const hldb::Module *const bridge = getModule("bridge");
  ASSERT_NE(bridge, nullptr);
  ASSERT_NE(bridge->getPorts(), nullptr);
  EXPECT_EQ(bridge->getPorts()->size(), 2u);
}

// assign a.gnt = a.req; inside mem
TEST_F(IfModulePortBasicTest, AccessThroughPort) {
  const hldb::Module *const mem = getModule("mem");
  const hldb::Interface *const bus = getInterface("test_bus");
  ASSERT_NE(mem, nullptr);
  ASSERT_NE(bus, nullptr);
  ASSERT_NE(mem->getContAssigns(), nullptr);
  ASSERT_EQ(mem->getContAssigns()->size(), 1u);
  EXPECT_EQ(getRootActual(mem->getContAssigns()->at(0)->getLhs()), getVar(bus, "gnt")) << "a.gnt";
  EXPECT_EQ(getRootActual(mem->getContAssigns()->at(0)->getRhs()), getVar(bus, "req")) << "a.req";
}

// CHK1-CHK5: every connection style binds the interface port to the right instance.
TEST_F(IfModulePortBasicTest, PortConnections) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module top not found";
  for (const char *const name : {"sb", "sb2", "iface", "s1", "s2"}) {
    const hldb::RefInstance *const inst = getInst(top, name);
    ASSERT_NE(inst, nullptr) << name;
    EXPECT_EQ(getInstDefName(inst), std::string_view("test_bus")) << name;
    EXPECT_EQ(inst->getStartLine(), 46u) << name << " is declared in the statement on line 46";
  }
  struct Conn {
    const char *m_inst;
    const char *m_port;
    const char *m_actual;
  };
  const Conn conns[] = {{"u1", "a", "sb"},        {"u2", "a", "sb2"},  {"u3", "iface", "iface"},
                        {"u4", "iface", "iface"}, {"u5", "src", "s1"}, {"u5", "dst", "s2"}};
  for (const Conn &c : conns) {
    const hldb::RefInstance *const inst = getInst(top, c.m_inst);
    ASSERT_NE(inst, nullptr) << c.m_inst;
    const hldb::Any *const hc = getHighConn(inst, c.m_port);
    ASSERT_NE(hc, nullptr) << c.m_inst << "." << c.m_port << " is unconnected";
    EXPECT_EQ(getBoundName(hc), std::string_view(c.m_actual)) << c.m_inst << "." << c.m_port;
  }
}

// The file is legal SystemVerilog, so compilation must not report any error.
TEST_F(IfModulePortBasicTest, CompilesWithoutErrors) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
