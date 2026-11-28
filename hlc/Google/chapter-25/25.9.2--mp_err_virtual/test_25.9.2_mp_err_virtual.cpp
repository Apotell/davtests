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

// Tests for tests/Google/chapter-25/25.9.2--mp_err_virtual.sv
// Expected results follow IEEE 1800-2023 (25.8, 25.9, 25.9.2, 25.10, 6.20, 37); they are
// not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - 25.9: a modport selected in a virtual interface declaration is part of its type:
//   - ERR1 line 54: v_tb (A_Bus.TB) = v_dut (A_Bus.DUT) -- different modports -> CompIllegalVirtualInterface
//   - ERR2 line 49: 'virtual A_Bus.NOPE' -- A_Bus has no modport NOPE -> CompUndefinedModport or
//     UnsupportedTypespec (unknown type A_Bus::NOPE)
//   - ERR3 line 56: v_plain (no modport) = v_tb (modport TB) -> CompIllegalVirtualInterface (only the
//     opposite direction is allowed)
//   - ERR4 line 58: v_tb (A_Bus.TB) = b.TB (B_Bus) -> CompIllegalVirtualInterface
//   - ERR5 line 60: v_tb.hidden -- hidden is not in modport TB -> CompModportMemberNotListed
// Not checked:
//   - The diagnostic codes are HLC's choice; each test uses the code HLC defines for the rule
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: mp_err_virtual
//  19  :description: illegal assignments between virtual interfaces with modports
//  20  :should_fail_because: virtual interface modports are part of the type and must match on assignment and access
//  21  :tags: 25.9.2
//  22  */
//  23
//  24  // Errors in this file:
//  25  //   ERR1: virtual with modport TB assigned from a virtual with modport DUT
//  26  //   ERR2: virtual interface declared with a modport that does not exist
//  27  //   ERR3: virtual with modport assigned to a virtual without modport
//  28  //   ERR4: virtual with modport of interface A assigned a modport of interface B
//  29  //   ERR5: access through a virtual modport to a signal that the modport hides
//  30
//  31  interface A_Bus;
//  32    logic req, gnt, hidden;
//  33    modport DUT (input req, output gnt);
//  34    modport TB (output req, input gnt);
//  35  endinterface
//  36
//  37  interface B_Bus;
//  38    logic req, gnt;
//  39    modport TB (output req, input gnt);
//  40  endinterface
//  41
//  42  module top;
//  43    A_Bus a();
//  44    B_Bus b();
//  45    virtual A_Bus.TB  v_tb;
//  46    virtual A_Bus.DUT v_dut;
//  47    virtual A_Bus     v_plain;
//  48    // ERR2
//  49    virtual A_Bus.NOPE v_bad;
//  50    logic y;
//  51    initial begin
//  52      v_dut = a;
//  53      // ERR1
//  54      v_tb = v_dut;
//  55      // ERR3
//  56      v_plain = v_tb;
//  57      // ERR4
//  58      v_tb = b.TB;
//  59      // ERR5
//  60      y = v_tb.hidden;
//  61    end
//  62  endmodule

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/expr.h>
#include <hldb/gen_block.h>
#include <hldb/gen_for.h>
#include <hldb/gen_if.h>
#include <hldb/gen_if_else.h>
#include <hldb/gen_region.h>
#include <hldb/if_stmt.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/io_decl.h>
#include <hldb/logic_typespec.h>
#include <hldb/modport.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/named_argument.h>
#include <hldb/net.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/port.h>
#include <hldb/process_stmt.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/select.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/unit.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <initializer_list>
#include <string_view>
#include <vector>

namespace hlc {

class MpErrVirtualTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.9.2--mp_err_virtual.hlc"}); }
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

  // Interface type of variable 'name' in 'scope' (for a virtual interface, 25.9); unpacked arrays give
  // their element type.
  static const hldb::InterfaceTypespec *getVarInterface(const hldb::Any *scope, std::string_view name) {
    const hldb::Variable *const v = getVar(scope, name);
    if ((v == nullptr) || (v->getTypespec() == nullptr)) return nullptr;
    const hldb::Typespec *ts = v->getTypespec()->getActual();
    if (const hldb::ArrayTypespec *const at = any_cast<hldb::ArrayTypespec>(ts)) {
      ts = (at->getElemTypespec() == nullptr) ? nullptr : at->getElemTypespec()->getActual();
    }
    return any_cast<hldb::InterfaceTypespec>(ts);
  }

  // Variable 'name' is a virtual interface of 'ifc' with modport 'modport' ("" = none).
  static void expectVirtual(const hldb::Any *scope, std::string_view name, std::string_view ifc,
                            std::string_view modport) {
    const hldb::InterfaceTypespec *const its = getVarInterface(scope, name);
    ASSERT_NE(its, nullptr) << name << " should have an interface type";
    EXPECT_TRUE(its->getVirtual()) << name << " is a virtual interface";
    EXPECT_EQ(its->getDefName(), ifc) << name;
    if (modport.empty()) {
      EXPECT_EQ(its->getModport(), nullptr) << name << " selects no modport";
    } else {
      ASSERT_NE(its->getModport(), nullptr) << name << " should select modport " << modport;
      EXPECT_EQ(its->getModport()->getName(), modport) << name;
    }
  }

  // Value given to parameter 'param' by the overrides of an interface typespec (by name or position).
  static std::string_view getOverride(const hldb::InterfaceTypespec *its, std::string_view param) {
    if ((its == nullptr) || (its->getParamAssigns() == nullptr)) return {};
    for (const hldb::ParamAssign *const pa : *its->getParamAssigns()) {
      if ((pa->getLhs() != nullptr) && (pa->getLhs()->getName() == param)) {
        const hldb::Constant *const c = any_cast<hldb::Constant>(pa->getRhs());
        return (c == nullptr) ? std::string_view() : c->getDecompile();
      }
    }
    return {};
  }

  // Every procedural assignment inside the processes of 'mod', keyed by the line it starts on.
  static void collectAssignments(const hldb::Any *stmt, std::vector<const hldb::Assignment *> *out) {
    if (stmt == nullptr) return;
    if (const hldb::Assignment *const a = any_cast<hldb::Assignment>(stmt)) {
      out->emplace_back(a);
    } else if (const hldb::Begin *const b = any_cast<hldb::Begin>(stmt)) {
      if (b->getStmts() != nullptr) {
        for (const hldb::Any *const s : *b->getStmts()) collectAssignments(s, out);
      }
    } else if (const hldb::IfStmt *const i = any_cast<hldb::IfStmt>(stmt)) {
      collectAssignments(i->getStmt(), out);
    }
  }

  // Class 'name': listed by the design or by a compilation unit (classes declared outside a module).
  static const hldb::ClassDefn *getClass(std::string_view name) {
    if (const hldb::ClassDefn *const c = hldb::findByDefName<hldb::ClassDefn>(name, m_design->getAllClasses()))
      return c;
    if (m_design->getUnits() == nullptr) return nullptr;
    for (const hldb::Unit *const u : *m_design->getUnits()) {
      if (const hldb::ClassDefn *const c = hldb::findByDefName<hldb::ClassDefn>(name, u->getClassDefns())) return c;
    }
    return nullptr;
  }

  // A select reached directly or through a hierarchical reference (u_if.data[0] is a reference whose
  // actual is the select data[0]).
  template <typename T>
  static const T *getSelect(const hldb::Any *expr) {
    if (const T *const t = any_cast<T>(expr)) return t;
    if (const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr)) return any_cast<T>(ref->getActual());
    return nullptr;
  }

  static const hldb::Assignment *getAssignmentAt(const hldb::Module *mod, uint32_t line) {
    if ((mod == nullptr) || (mod->getProcesses() == nullptr)) return nullptr;
    std::vector<const hldb::Assignment *> all;
    for (const hldb::Process *const p : *mod->getProcesses()) collectAssignments(p->getStmt(), &all);
    for (const hldb::Assignment *const a : all) {
      if (a->getStartLine() == line) return a;
    }
    return nullptr;
  }
};

// ERR1: v_tb = v_dut;
TEST_F(MpErrVirtualTest, DifferentModportIsRejected) {
  EXPECT_TRUE(anyEvent({EventType::CompIllegalVirtualInterface}, 54, 54))
      << "line 54: TB and DUT are different modports";
}

// ERR2: virtual A_Bus.NOPE v_bad;
TEST_F(MpErrVirtualTest, UnknownModportIsRejected) {
  EXPECT_TRUE(anyEvent({EventType::CompUndefinedModport, EventType::UnsupportedTypespec}, 49, 49))
      << "line 49: A_Bus has no modport NOPE";
}

// ERR3: v_plain = v_tb;
TEST_F(MpErrVirtualTest, ModportToPlainIsRejected) {
  EXPECT_TRUE(anyEvent({EventType::CompIllegalVirtualInterface}, 56, 56))
      << "line 56: a virtual with a modport cannot be assigned to one without";
}

// ERR4: v_tb = b.TB;
TEST_F(MpErrVirtualTest, OtherInterfaceModportIsRejected) {
  EXPECT_TRUE(anyEvent({EventType::CompIllegalVirtualInterface}, 58, 58)) << "line 58: B_Bus.TB is not A_Bus.TB";
}

// ERR5: y = v_tb.hidden;
TEST_F(MpErrVirtualTest, HiddenMemberIsRejected) {
  EXPECT_TRUE(anyEvent({EventType::CompModportMemberNotListed}, 60, 60)) << "line 60: hidden is not in modport TB";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
