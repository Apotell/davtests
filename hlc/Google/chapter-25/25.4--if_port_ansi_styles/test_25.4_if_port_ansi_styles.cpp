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

// Tests for tests/Google/chapter-25/25.4--if_port_ansi_styles.sv
// Expected results follow IEEE 1800-2023 (25.4, 25.6, 25.7, 25.7.2-25.7.4, 23.2.2, 37); they
// are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - Port order and direction of c1_if..c5_if
//   - 23.2.2.3 port kind defaults: 'input logic x' is a net (wire), 'output logic y' is a variable;
//     'inout wire d' is a net; 'input var logic b' / 'output var logic y' are variables
//   - CHK1: c1_if(input a, input b, output y) one per line; CHK2: 'input logic a, b' -- b inherits input
//   - CHK3: 'input logic [3:0] a, b, c' -- b and c inherit input and [3:0]; y is [3:0]
//   - CHK4: c4_if has input clk, output q, inout d (wire)
//   - CHK5: c5_if: a is 'wire logic' (net), b and y are 'var logic' (variables)
//   - Each interface has its body statement (assign or always_ff)
//   - top connects u1 by name, u2..u5 by position, to the matching signals
//   - No errors: the whole file is legal
// Not checked:
//   - Nothing intentionally left out
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: if_port_ansi_styles
//  19  :description: ANSI interface port declaration styles
//  20  :tags: 25.4
//  21  */
//  22
//  23  // Checks in this file:
//  24  //   CHK1: one port per line: input a, / input b, / output y
//  25  //   CHK2: ports sharing one declaration: input logic a, b
//  26  //   CHK3: later ports inherit direction and type: input logic [3:0] a, b, c
//  27  //   CHK4: input, output and inout in one header
//  28  //   CHK5: explicit wire and var port kinds
//  29
//  30  // CHK1
//  31  interface c1_if(
//  32    input  logic a,
//  33    input  logic b,
//  34    output logic y
//  35  );
//  36    assign y = a & b;
//  37  endinterface
//  38
//  39  // CHK2
//  40  interface c2_if(input logic a, b, output logic y);
//  41    assign y = a | b;
//  42  endinterface
//  43
//  44  // CHK3
//  45  interface c3_if(input logic [3:0] a, b, c, output logic [3:0] y);
//  46    assign y = a + b + c;
//  47  endinterface
//  48
//  49  // CHK4
//  50  interface c4_if(input logic clk, output logic q, inout wire d);
//  51    always_ff @(posedge clk) q <= d;
//  52  endinterface
//  53
//  54  // CHK5
//  55  interface c5_if(input wire logic a, input var logic b, output var logic y);
//  56    assign y = a ^ b;
//  57  endinterface
//  58
//  59  module top;
//  60    logic a, b, y1, y2, y5, clk, q;
//  61    logic [3:0] a4, b4, c4, y4;
//  62    wire d;
//  63    c1_if u1(.a(a), .b(b), .y(y1));
//  64    c2_if u2(a, b, y2);
//  65    c3_if u3(a4, b4, c4, y4);
//  66    c4_if u4(clk, q, d);
//  67    c5_if u5(a, b, y5);
//  68  endmodule

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
#include <hldb/function.h>
#include <hldb/gen_block.h>
#include <hldb/gen_for.h>
#include <hldb/gen_if.h>
#include <hldb/gen_if_else.h>
#include <hldb/gen_region.h>
#include <hldb/hldb_vpi_user.h>
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
#include <hldb/task.h>
#include <hldb/task_func.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <initializer_list>
#include <string_view>
#include <vector>

namespace hlc {

class IfPortAnsiStylesTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.4--if_port_ansi_styles.hlc"}); }
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
    EXPECT_EQ(its->getDefName(), ifc) << port;
    if (modport.empty()) {
      EXPECT_EQ(its->getModport(), nullptr) << port << " selects no modport";
    } else {
      ASSERT_NE(its->getModport(), nullptr) << port << " should select modport " << modport;
      EXPECT_EQ(its->getModport()->getName(), modport) << port;
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

  // Port 'name' of 'def': direction and whether its internal object is a net or a variable.
  static void expectPortKind(const hldb::Any *def, std::string_view name, int32_t dir, bool isNet,
                             std::string_view what) {
    const hldb::Port *const p = hldb::findByName<hldb::Port>(name, hldb::getPorts(def));
    ASSERT_NE(p, nullptr) << what << "." << name << " not found";
    EXPECT_EQ(p->getDirection(), dir) << what << "." << name;
    if (isNet) {
      EXPECT_NE(getNet(def, name), nullptr) << what << "." << name << " should be a net";
    } else {
      EXPECT_NE(getVar(def, name), nullptr) << what << "." << name << " should be a variable";
    }
  }

  static const hldb::TaskFunc *getTaskFunc(const hldb::Any *scope, std::string_view name) {
    const hldb::Instance *const inst = any_cast<hldb::Instance>(scope);
    return (inst == nullptr) ? nullptr : hldb::findByName<hldb::TaskFunc>(name, inst->getTaskFuncs());
  }

  // Entry 'name' of modport 'mp' is an import (vpiImported) or an export (vpiExported) of a subroutine.
  static void expectSubroutineEntry(const hldb::Modport *mp, std::string_view name, int32_t kind,
                                    std::string_view what) {
    ASSERT_NE(mp, nullptr) << what;
    const hldb::IODecl *const io = hldb::findByName<hldb::IODecl>(name, mp->getIODecls());
    ASSERT_NE(io, nullptr) << what << " should list " << name;
    EXPECT_EQ(io->getImportExport(), kind) << what << "." << name;
  }
};

TEST_F(IfPortAnsiStylesTest, PortKindsAndDirections) {
  const hldb::Interface *const c1 = getInterface("c1_if");
  expectPortKind(c1, "a", vpiInput, true, "c1_if");
  expectPortKind(c1, "b", vpiInput, true, "c1_if");
  expectPortKind(c1, "y", vpiOutput, false, "c1_if");
  const hldb::Interface *const c2 = getInterface("c2_if");
  expectPortKind(c2, "a", vpiInput, true, "c2_if");
  expectPortKind(c2, "b", vpiInput, true, "c2_if");
  expectPortKind(c2, "y", vpiOutput, false, "c2_if");
  const hldb::Interface *const c4 = getInterface("c4_if");
  expectPortKind(c4, "clk", vpiInput, true, "c4_if");
  expectPortKind(c4, "q", vpiOutput, false, "c4_if");
  expectPortKind(c4, "d", vpiInout, true, "c4_if");
  const hldb::Interface *const c5 = getInterface("c5_if");
  expectPortKind(c5, "a", vpiInput, true, "c5_if");
  expectPortKind(c5, "b", vpiInput, false, "c5_if");
  expectPortKind(c5, "y", vpiOutput, false, "c5_if");
}

TEST_F(IfPortAnsiStylesTest, PortOrder) {
  const std::pair<const char *, std::vector<const char *>> orders[] = {{"c1_if", {"a", "b", "y"}},
                                                                       {"c2_if", {"a", "b", "y"}},
                                                                       {"c3_if", {"a", "b", "c", "y"}},
                                                                       {"c4_if", {"clk", "q", "d"}},
                                                                       {"c5_if", {"a", "b", "y"}}};
  for (const std::pair<const char *, std::vector<const char *>> &o : orders) {
    const hldb::Interface *const ifc = getInterface(o.first);
    ASSERT_NE(ifc, nullptr) << o.first;
    ASSERT_NE(ifc->getPorts(), nullptr) << o.first;
    ASSERT_EQ(ifc->getPorts()->size(), o.second.size()) << o.first;
    for (size_t i = 0; i < o.second.size(); ++i) {
      EXPECT_EQ(ifc->getPorts()->at(i)->getName(), std::string_view(o.second[i])) << o.first << " port " << i;
    }
  }
}

// CHK3: input logic [3:0] a, b, c, output logic [3:0] y
TEST_F(IfPortAnsiStylesTest, InheritedPackedRange) {
  const hldb::Interface *const c3 = getInterface("c3_if");
  ASSERT_NE(c3, nullptr);
  for (const char *const name : {"a", "b", "c"}) {
    expectPortKind(c3, name, vpiInput, true, "c3_if");
    expectLogicRange(hldb::getTypespec(getNet(c3, name)), "3", "0", std::string("c3_if.") + name);
  }
  expectPortKind(c3, "y", vpiOutput, false, "c3_if");
  expectLogicRange(hldb::getTypespec(getVar(c3, "y")), "3", "0", "c3_if.y");
}

TEST_F(IfPortAnsiStylesTest, BodyStatements) {
  for (const char *const name : {"c1_if", "c2_if", "c3_if", "c5_if"}) {
    const hldb::Interface *const ifc = getInterface(name);
    ASSERT_NE(ifc, nullptr) << name;
    ASSERT_NE(ifc->getContAssigns(), nullptr) << name;
    ASSERT_EQ(ifc->getContAssigns()->size(), 1u) << name;
    EXPECT_EQ(getRootActual(ifc->getContAssigns()->at(0)->getLhs()), getVar(ifc, "y")) << name << ": assign y";
  }
  const hldb::Interface *const c4 = getInterface("c4_if");
  ASSERT_NE(c4, nullptr);
  ASSERT_NE(c4->getProcesses(), nullptr);
  EXPECT_EQ(c4->getProcesses()->size(), 1u) << "always_ff";
}

TEST_F(IfPortAnsiStylesTest, Connections) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module top not found";
  struct Conn {
    const char *m_inst;
    const char *m_port;
    const char *m_signal;
  };
  const Conn conns[] = {{"u1", "a", "a"},  {"u1", "b", "b"},  {"u1", "y", "y1"}, {"u2", "y", "y2"},
                        {"u3", "a", "a4"}, {"u3", "c", "c4"}, {"u3", "y", "y4"}, {"u4", "clk", "clk"},
                        {"u4", "q", "q"},  {"u5", "y", "y5"}};
  for (const Conn &c : conns) {
    EXPECT_EQ(getRootActual(getHighConn(getInst(top, c.m_inst), c.m_port)), getVar(top, c.m_signal))
        << c.m_inst << "." << c.m_port;
  }
  EXPECT_EQ(getRootActual(getHighConn(getInst(top, "u4"), "d")), getNet(top, "d")) << "u4.d";
}

// The file is legal SystemVerilog, so compilation must not report any error.
TEST_F(IfPortAnsiStylesTest, CompilesWithoutErrors) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
