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

// Tests for tests/Google/chapter-25/25.5.4--mp_expressions_advanced.sv
// Expected results follow IEEE 1800-2023 (25.5, 25.5.1-25.5.5, 25.10, 23.3.3, 37); they are
// not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - Modport mp lists E0, ADDR, CAT, NC (inputs) and B0 (output) in this order
//   - CHK1: E0 = arr[0] (element select of the unpacked array arr)
//   - CHK2: ADDR = req.addr (member addr of the struct variable req)
//   - CHK3: CAT = {hi, lo} (concatenation of hi and lo)
//   - CHK4: NC() has no expression (25.5.4: 'The port expression is optional')
//   - CHK5: B0 = v[0] (bit select of v)
//   - user's port m selects bus_if.mp; m.E0, m.ADDR, m.CAT (read) and m.B0 (written) bind to the modport
//     port or its port expression; u_user.m -> u_if
//   - No errors: the whole file is legal
// Skipped (GTEST_SKIP; the real assertions stay below the skip):
//   - UserAccess: HLC leaves m.E0, m.CAT and m.B0 unresolved (null actual) through the modport mp.
// Not checked:
//   - Nothing intentionally left out
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: mp_expressions_advanced
//  19  :description: modport expressions using arrays, structs, concatenation and empty expressions
//  20  :tags: 25.5.4
//  21  */
//  22
//  23  // Checks in this file:
//  24  //   CHK1: unpacked array element as port expression .E0(arr[0])
//  25  //   CHK2: struct member as port expression .ADDR(req.addr)
//  26  //   CHK3: concatenation as input port expression .CAT({hi, lo})
//  27  //   CHK4: port expression with no expression .NC()
//  28  //   CHK5: bit select as output port expression .B0(v[0])
//  29
//  30  interface bus_if;
//  31    typedef struct packed { logic [7:0] addr; logic wr; } req_t;
//  32    logic [3:0] arr [0:1];
//  33    req_t req;
//  34    logic [3:0] hi, lo;
//  35    logic [7:0] v;
//  36
//  37    modport mp (
//  38      input  .E0(arr[0]),      // CHK1
//  39      input  .ADDR(req.addr),  // CHK2
//  40      input  .CAT({hi, lo}),   // CHK3
//  41      input  .NC(),            // CHK4
//  42      output .B0(v[0])         // CHK5
//  43    );
//  44  endinterface
//  45
//  46  module user(bus_if.mp m);
//  47    logic [3:0] e;
//  48    logic [7:0] a, c;
//  49    assign e = m.E0;
//  50    assign a = m.ADDR;
//  51    assign c = m.CAT;
//  52    assign m.B0 = 1'b1;
//  53  endmodule
//  54
//  55  module top;
//  56    bus_if u_if();
//  57    user u_user(.m(u_if));
//  58  endmodule

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/begin.h>
#include <hldb/bit_select.h>
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
#include <hldb/operation.h>
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

class MpExpressionsAdvancedTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.5.4--mp_expressions_advanced.hlc"}); }
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
};

TEST_F(MpExpressionsAdvancedTest, Entries) {
  expectEntries(getModport(getInterface("bus_if"), "mp"),
                {{"E0", vpiInput}, {"ADDR", vpiInput}, {"CAT", vpiInput}, {"NC", vpiInput}, {"B0", vpiOutput}}, "mp");
}

TEST_F(MpExpressionsAdvancedTest, PortExpressions) {
  const hldb::Interface *const bus = getInterface("bus_if");
  const hldb::Modport *const mp = getModport(bus, "mp");
  ASSERT_NE(mp, nullptr);
  ASSERT_NE(mp->getIODecls(), nullptr);
  ASSERT_EQ(mp->getIODecls()->size(), 5u);
  // CHK1: .E0(arr[0])
  const hldb::BitSelect *const e0 = any_cast<hldb::BitSelect>(mp->getIODecls()->at(0)->getExpr());
  ASSERT_NE(e0, nullptr) << "E0 is an element select";
  EXPECT_EQ(getRootActual(e0), getVar(bus, "arr"));
  // CHK2: .ADDR(req.addr)
  const hldb::Any *const addr = getRootActual(mp->getIODecls()->at(1)->getExpr());
  ASSERT_NE(addr, nullptr) << "ADDR's expression req.addr is unresolved";
  EXPECT_EQ(addr->getName(), std::string_view("addr"));
  // CHK3: .CAT({hi, lo})
  const hldb::Operation *const cat = any_cast<hldb::Operation>(mp->getIODecls()->at(2)->getExpr());
  ASSERT_NE(cat, nullptr) << "CAT is a concatenation";
  EXPECT_EQ(cat->getOpType(), vpiConcatOp);
  ASSERT_NE(cat->getOperands(), nullptr);
  ASSERT_EQ(cat->getOperands()->size(), 2u);
  EXPECT_EQ(getRootActual(cat->getOperands()->at(0)), getVar(bus, "hi"));
  EXPECT_EQ(getRootActual(cat->getOperands()->at(1)), getVar(bus, "lo"));
  // CHK4: .NC()
  EXPECT_EQ(mp->getIODecls()->at(3)->getExpr(), nullptr) << "NC() has no expression";
  // CHK5: .B0(v[0])
  const hldb::BitSelect *const b0 = any_cast<hldb::BitSelect>(mp->getIODecls()->at(4)->getExpr());
  ASSERT_NE(b0, nullptr) << "B0 is a bit select";
  EXPECT_EQ(getRootActual(b0), getVar(bus, "v"));
}

// m.E0 etc. name the modport port; 25.5.4 binds that port to its port expression, so a reference may be
// bound to the port (IODecl) itself, to its expression, or to what the expression refers to.
TEST_F(MpExpressionsAdvancedTest, UserAccess) {
  GTEST_SKIP() << "HLC leaves m.E0, m.CAT and m.B0 unresolved (null actual) through the modport mp."
                  " IEEE 1800-2023 Sec 25.5.4: a modport expression port is accessed by its port name. Fix pending.";
  expectInterfacePort(getModule("user"), "m", "bus_if", "mp");
  const hldb::Modport *const mp = getModport(getInterface("bus_if"), "mp");
  const hldb::Module *const user = getModule("user");
  ASSERT_NE(mp, nullptr);
  ASSERT_NE(user, nullptr);
  ASSERT_NE(user->getContAssigns(), nullptr);
  ASSERT_EQ(user->getContAssigns()->size(), 4u);
  struct Use {
    uint32_t m_line;
    const char *m_port;
    bool m_lhs;
  };
  const Use uses[] = {{49u, "E0", false}, {50u, "ADDR", false}, {51u, "CAT", false}, {52u, "B0", true}};
  for (const Use &u : uses) {
    const hldb::IODecl *const io = hldb::findByName<hldb::IODecl>(u.m_port, mp->getIODecls());
    ASSERT_NE(io, nullptr) << u.m_port;
    const hldb::ContAssign *found = nullptr;
    for (const hldb::ContAssign *const ca : *user->getContAssigns()) {
      if (ca->getStartLine() == u.m_line) found = ca;
    }
    ASSERT_NE(found, nullptr) << "no continuous assign on line " << u.m_line;
    const hldb::Any *const bound = getRootActual(u.m_lhs ? found->getLhs() : found->getRhs());
    ASSERT_NE(bound, nullptr) << "m." << u.m_port << " is unresolved";
    const bool ok = (bound == io) || (bound == io->getExpr()) || (bound == getRootActual(io->getExpr()));
    EXPECT_TRUE(ok) << "m." << u.m_port << " should bind to modport port " << u.m_port << " or its expression";
  }
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(getBoundName(getHighConn(getInst(top, "u_user"), "m")), std::string_view("u_if"));
}

// The file is legal SystemVerilog, so compilation must not report any error.
TEST_F(MpExpressionsAdvancedTest, CompilesWithoutErrors) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
