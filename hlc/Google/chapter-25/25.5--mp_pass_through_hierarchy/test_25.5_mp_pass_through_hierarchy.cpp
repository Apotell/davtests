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

// Tests for tests/Google/chapter-25/25.5--mp_pass_through_hierarchy.sv
// Expected results follow IEEE 1800-2023 (25.5, 25.5.1-25.5.5, 25.10, 23.3.3, 37); they are
// not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - Ports: leaf_t.b = bus_if.target, leaf_i.b = bus_if.initiator, mid_full.b = bus_if,
//     mid_mp.b = bus_if.initiator, mid_generic.b = generic interface port
//   - CHK1-CHK3: each mid's u_leaf.b is connected to the mid's own port b, a reference to the interface
//     or modport declaration that port names (37.13 detail 3, 37.15): bus_if in mid_full, the modport
//     initiator in mid_mp
//   - CHK4: leaf_t assigns b.y = b.x (writes y, reads x); leaf_i assigns b.x
//   - CHK5: top connects u1.b and u2.b to b1 and u3.b to b2
//   - No errors: the whole file is legal
// Not checked:
//   - What mid_generic's u_leaf.b refers to: b is a generic interface port, so its interface is known
//     only from the connection in top (elaboration)
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: mp_pass_through_hierarchy
//  19  :description: modport views passed down through several levels of hierarchy
//  20  :tags: 25.5
//  21  */
//  22
//  23  // Checks in this file:
//  24  //   CHK1: top passes full interface, middle passes to leaf with modport header
//  25  //   CHK2: middle module has modport header, passes same view to leaf
//  26  //   CHK3: middle generic interface port forwarded to a named-modport leaf
//  27  //   CHK4: leaf reads input and writes output through the forwarded modport
//  28  //   CHK5: two different modports of one instance passed down different paths
//  29
//  30  interface bus_if;
//  31    logic x, y;
//  32    modport target (input x, output y);
//  33    modport initiator (output x, input y);
//  34  endinterface
//  35
//  36  // CHK4
//  37  module leaf_t(bus_if.target b);
//  38    assign b.y = b.x;
//  39  endmodule
//  40
//  41  module leaf_i(bus_if.initiator b);
//  42    assign b.x = 1'b1;
//  43  endmodule
//  44
//  45  // CHK1
//  46  module mid_full(bus_if b);
//  47    leaf_t u_leaf(.b(b));
//  48  endmodule
//  49
//  50  // CHK2
//  51  module mid_mp(bus_if.initiator b);
//  52    leaf_i u_leaf(.b(b));
//  53  endmodule
//  54
//  55  // CHK3
//  56  module mid_generic(interface b);
//  57    leaf_t u_leaf(.b(b));
//  58  endmodule
//  59
//  60  module top;
//  61    bus_if b1(), b2();
//  62    mid_full u1(.b(b1));
//  63    // CHK5
//  64    mid_mp u2(.b(b1));
//  65    mid_generic u3(.b(b2));
//  66  endmodule

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
#include <hldb/io_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/modport.h>
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

#include <initializer_list>
#include <string_view>
#include <vector>

namespace hlc {

class MpPassThroughHierarchyTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.5--mp_pass_through_hierarchy.hlc"}); }
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

  static const hldb::Modport *getModport(const hldb::Interface *ifc, std::string_view name) {
    return (ifc == nullptr) ? nullptr : hldb::findByName<hldb::Modport>(name, ifc->getModports());
  }

  // The modport lists exactly 'entries' (name, direction), in order (25.5 modport_ports_declaration).
  static void expectEntries(const hldb::Modport *mp, const std::vector<std::pair<const char *, int32_t>> &entries,
                            std::string_view what) {
    ASSERT_NE(mp, nullptr) << "modport " << what << " not found";
    ASSERT_NE(mp->getIODecls(), nullptr) << what;
    ASSERT_EQ(mp->getIODecls()->size(), entries.size()) << what;
    for (size_t i = 0; i < entries.size(); ++i) {
      const hldb::IODecl *const io = mp->getIODecls()->at(i);
      EXPECT_EQ(io->getName(), std::string_view(entries[i].first)) << what << " entry " << i;
      EXPECT_EQ(io->getDirection(), entries[i].second) << what << "." << entries[i].first;
    }
  }

  // The interface type of port 'port' of module/interface/program 'def' (element type for a port array).
  static const hldb::InterfaceTypespec *getPortInterface(const hldb::Any *def, std::string_view port) {
    const hldb::Port *const p = hldb::findByName<hldb::Port>(port, hldb::getPorts(def));
    if ((p == nullptr) || (p->getTypespec() == nullptr)) return nullptr;
    const hldb::Typespec *ts = p->getTypespec()->getActual();
    if (const hldb::ArrayTypespec *const at = any_cast<hldb::ArrayTypespec>(ts)) {
      ts = (at->getElemTypespec() == nullptr) ? nullptr : at->getElemTypespec()->getActual();
    }
    return any_cast<hldb::InterfaceTypespec>(ts);
  }

  // Port 'port' of 'def' has interface type 'ifc' and selects modport 'modport' ("" = no modport).
  static void expectInterfacePort(const hldb::Any *def, std::string_view port, std::string_view ifc,
                                  std::string_view modport) {
    const hldb::InterfaceTypespec *const its = getPortInterface(def, port);
    ASSERT_NE(its, nullptr) << port << " should be an interface port";
    if (modport.empty()) {
      EXPECT_EQ(its->getDefName(), ifc) << port;
      EXPECT_EQ(its->getModport(), nullptr) << port << " selects no modport";
    } else {
      // The def name of a typespec that represents a modport is the modport identifier
      // (IEEE 1800-2023 37.30), so the interface is checked through the selected modport here.
      ASSERT_NE(its->getModport(), nullptr) << port << " should select modport " << modport;
      EXPECT_EQ(its->getModport()->getName(), modport) << port;
      ASSERT_NE(its->getModport()->getInterface(), nullptr) << port;
      EXPECT_EQ(its->getModport()->getInterface()->getDefName(), ifc) << port;
    }
  }

  static const hldb::Program *getProgram(std::string_view name) {
    return hldb::findByDefName<hldb::Program>(name, m_design->getAllPrograms());
  }

  // True when an event of one of 'codes' is reported on a line in [first, last].
  bool anyEvent(std::initializer_list<uint32_t> codes, uint32_t first, uint32_t last) {
    for (const uint32_t code : codes) {
      for (uint32_t line = first; line <= last; ++line) {
        if (!findEvents(code, line).empty()) return true;
      }
    }
    return false;
  }
};

TEST_F(MpPassThroughHierarchyTest, PortModports) {
  expectInterfacePort(getModule("leaf_t"), "b", "bus_if", "target");
  expectInterfacePort(getModule("leaf_i"), "b", "bus_if", "initiator");
  expectInterfacePort(getModule("mid_full"), "b", "bus_if", "");
  expectInterfacePort(getModule("mid_mp"), "b", "bus_if", "initiator");
  const hldb::Module *const generic = getModule("mid_generic");
  ASSERT_NE(generic, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Port>("b", generic->getPorts()), nullptr) << "generic interface port b";
}

// CHK1-CHK3: leaf u_leaf(.b(b)) in every mid module.
TEST_F(MpPassThroughHierarchyTest, ForwardedPorts) {
  // u_leaf(.b(b)) passes the mid module's own interface port on. The connection is a ref obj whose
  // vpiActual is an interface or modport, never the port itself (IEEE 1800-2023 37.15); in the
  // definition model it is the declaration the port names (37.13 detail 3).
  const hldb::Interface *const bus = getInterface("bus_if");
  ASSERT_NE(bus, nullptr);
  const hldb::Modport *const initiator = getModport(bus, "initiator");
  ASSERT_NE(initiator, nullptr);
  struct Mid {
    const char *m_mid;
    const char *m_leaf;
    const hldb::Any *m_bound;  // nullptr: needs elaboration (generic interface port)
  };
  const Mid mids[] = {
      {"mid_full", "leaf_t", bus}, {"mid_mp", "leaf_i", initiator}, {"mid_generic", "leaf_t", nullptr}};
  for (const Mid &m : mids) {
    const hldb::RefInstance *const leaf = getInst(getModule(m.m_mid), "u_leaf");
    ASSERT_NE(leaf, nullptr) << m.m_mid;
    EXPECT_EQ(getInstDefName(leaf), std::string_view(m.m_leaf)) << m.m_mid;
    if (m.m_bound != nullptr) {
      EXPECT_EQ(getRootActual(getHighConn(leaf, "b")), m.m_bound) << m.m_mid << ": the declaration its port b names";
    }
  }
}

TEST_F(MpPassThroughHierarchyTest, LeafAccess) {
  const hldb::Interface *const bus = getInterface("bus_if");
  ASSERT_NE(bus, nullptr);
  const hldb::Module *const lt = getModule("leaf_t");
  const hldb::Module *const li = getModule("leaf_i");
  ASSERT_NE(lt, nullptr);
  ASSERT_NE(li, nullptr);
  ASSERT_NE(lt->getContAssigns(), nullptr);
  ASSERT_EQ(lt->getContAssigns()->size(), 1u);
  EXPECT_EQ(getRootActual(lt->getContAssigns()->at(0)->getLhs()), getVar(bus, "y"));
  EXPECT_EQ(getRootActual(lt->getContAssigns()->at(0)->getRhs()), getVar(bus, "x"));
  ASSERT_NE(li->getContAssigns(), nullptr);
  ASSERT_EQ(li->getContAssigns()->size(), 1u);
  EXPECT_EQ(getRootActual(li->getContAssigns()->at(0)->getLhs()), getVar(bus, "x"));
}

TEST_F(MpPassThroughHierarchyTest, TopConnections) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module top not found";
  EXPECT_EQ(getBoundName(getHighConn(getInst(top, "u1"), "b")), std::string_view("b1"));
  EXPECT_EQ(getBoundName(getHighConn(getInst(top, "u2"), "b")), std::string_view("b1"));
  EXPECT_EQ(getBoundName(getHighConn(getInst(top, "u3"), "b")), std::string_view("b2"));
}

// The file is legal SystemVerilog, so compilation must not report any error.
TEST_F(MpPassThroughHierarchyTest, CompilesWithoutErrors) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
