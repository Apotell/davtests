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

// Tests for tests/Google/chapter-25/25.5--mp_multiple_ports_one_module.sv
// Expected results follow IEEE 1800-2023 (25.5, 25.5.1-25.5.5, 25.10, 23.3.3, 37); they are
// not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - CHK1-CHK3: bridge ports in order: clk (input), up (bus_if.target), down (bus_if.initiator),
//     ctl (ctl_if.sink), busy (output)
//   - CHK4: bridge has an always_ff, 'assign up.y = down.y' and 'assign busy = ctl.en'
//   - CHK5: top's bridge u_br(.*) connects every port to the same-named object of top
//   - No errors: the whole file is legal
// Not checked:
//   - Nothing intentionally left out
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: mp_multiple_ports_one_module
//  19  :description: one module with several modport ports and mixed ordinary ports
//  20  :tags: 25.5
//  21  */
//  22
//  23  // Checks in this file:
//  24  //   CHK1: two ports of the same interface with different modports
//  25  //   CHK2: ports of two different interfaces with modports
//  26  //   CHK3: modport ports mixed with ordinary input/output ports
//  27  //   CHK4: data moved from one modport view to another inside the module
//  28  //   CHK5: connection by .* with matching instance names
//  29
//  30  interface bus_if;
//  31    logic x, y;
//  32    modport target (input x, output y);
//  33    modport initiator (output x, input y);
//  34  endinterface
//  35
//  36  interface ctl_if;
//  37    logic en;
//  38    modport sink (input en);
//  39  endinterface
//  40
//  41  // CHK1 + CHK2 + CHK3
//  42  module bridge(input logic clk,
//  43                bus_if.target up,
//  44                bus_if.initiator down,
//  45                ctl_if.sink ctl,
//  46                output logic busy);
//  47    // CHK4
//  48    always_ff @(posedge clk) if (ctl.en) down.x <= up.x;
//  49    assign up.y = down.y;
//  50    assign busy = ctl.en;
//  51  endmodule
//  52
//  53  module top;
//  54    logic clk, busy;
//  55    bus_if up(), down();
//  56    ctl_if ctl();
//  57    // CHK5
//  58    bridge u_br(.*);
//  59  endmodule

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

class MpMultiplePortsOneModuleTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.5--mp_multiple_ports_one_module.hlc"}); }
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

TEST_F(MpMultiplePortsOneModuleTest, PortList) {
  const hldb::Module *const br = getModule("bridge");
  ASSERT_NE(br, nullptr);
  ASSERT_NE(br->getPorts(), nullptr);
  ASSERT_EQ(br->getPorts()->size(), 5u);
  const char *const order[] = {"clk", "up", "down", "ctl", "busy"};
  for (size_t i = 0; i < 5; ++i) EXPECT_EQ(br->getPorts()->at(i)->getName(), std::string_view(order[i])) << i;
  EXPECT_EQ(br->getPorts()->at(0)->getDirection(), vpiInput);
  EXPECT_EQ(br->getPorts()->at(4)->getDirection(), vpiOutput);
  expectInterfacePort(br, "up", "bus_if", "target");
  expectInterfacePort(br, "down", "bus_if", "initiator");
  expectInterfacePort(br, "ctl", "ctl_if", "sink");
}

TEST_F(MpMultiplePortsOneModuleTest, BodyAccess) {
  const hldb::Module *const br = getModule("bridge");
  ASSERT_NE(br, nullptr);
  ASSERT_NE(br->getProcesses(), nullptr);
  EXPECT_EQ(br->getProcesses()->size(), 1u) << "always_ff";
  ASSERT_NE(br->getContAssigns(), nullptr);
  ASSERT_EQ(br->getContAssigns()->size(), 2u);
  const hldb::Interface *const bus = getInterface("bus_if");
  const hldb::Interface *const ctl = getInterface("ctl_if");
  ASSERT_NE(bus, nullptr);
  ASSERT_NE(ctl, nullptr);
  for (const hldb::ContAssign *const ca : *br->getContAssigns()) {
    if (ca->getStartLine() == 49u) {
      EXPECT_EQ(getRootActual(ca->getLhs()), getVar(bus, "y")) << "up.y";
      EXPECT_EQ(getRootActual(ca->getRhs()), getVar(bus, "y")) << "down.y";
    } else {
      EXPECT_EQ(getRootActual(ca->getRhs()), getVar(ctl, "en")) << "ctl.en";
    }
  }
}

// CHK5: bridge u_br(.*);
TEST_F(MpMultiplePortsOneModuleTest, WildcardConnection) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module top not found";
  const hldb::RefInstance *const br = getInst(top, "u_br");
  ASSERT_NE(br, nullptr);
  for (const char *const name : {"up", "down", "ctl"}) {
    EXPECT_EQ(getBoundName(getHighConn(br, name)), std::string_view(name)) << name;
  }
  EXPECT_EQ(getRootActual(getHighConn(br, "clk")), getVar(top, "clk"));
  EXPECT_EQ(getRootActual(getHighConn(br, "busy")), getVar(top, "busy"));
}

// The file is legal SystemVerilog, so compilation must not report any error.
TEST_F(MpMultiplePortsOneModuleTest, CompilesWithoutErrors) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
