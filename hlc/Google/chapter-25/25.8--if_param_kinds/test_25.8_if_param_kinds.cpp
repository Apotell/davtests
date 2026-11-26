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

// Tests for tests/Google/chapter-25/25.8--if_param_kinds.sv
// Expected results follow IEEE 1800-2023 (25.8, 25.9, 25.9.2, 25.10, 6.20, 37); they are
// not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - CHK1: c1_if has the type parameter T; u1 overrides it with int (the override is an int type)
//   - CHK2: c2_if declares 'parameter int W = 4' in its body (not local); u2 overrides it with 12
//   - CHK3: top has one defparam whose value is 4 (defparam u3.W = 4)
//   - CHK4: c4_sub's 'initial w = b.W' reads the parameter W through the port b; u4.b -> b4 (#(12))
//   - CHK5: scalar1 and the array vector[9:0] both get W = 100
//   - No errors: the whole file is legal
// Not checked:
//   - The value each instance ends up with (elaboration)
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: if_param_kinds
//  19  :description: type parameters, body parameters, defparam and parameter access through a port
//  20  :tags: 25.8
//  21  */
//  22
//  23  // Checks in this file:
//  24  //   CHK1: type parameter overridden with int
//  25  //   CHK2: parameter declared in the body and overridden
//  26  //   CHK3: parameter changed with defparam
//  27  //   CHK4: module reads the parameter through its interface port b.W in a
//  28  //         non-constant (procedural) expression
//  29  //   CHK5: one override applied to several instances and an array (LRM #(100) example)
//  30
//  31  // CHK1
//  32  interface c1_if #(parameter type T = logic [7:0]);
//  33    T data;
//  34  endinterface
//  35
//  36  // CHK2
//  37  interface c2_if;
//  38    parameter int W = 4;
//  39    logic [W-1:0] data;
//  40  endinterface
//  41
//  42  interface w_if #(parameter int W = 8);
//  43    logic [W-1:0] data;
//  44  endinterface
//  45
//  46  // CHK4
//  47  module c4_sub(w_if b);
//  48    int w;
//  49    initial w = b.W;
//  50  endmodule
//  51
//  52  module top;
//  53    c1_if #(.T(int)) u1();
//  54    int x;
//  55    assign x = u1.data;
//  56    c2_if #(.W(12)) u2();
//  57    // CHK3
//  58    w_if u3();
//  59    defparam u3.W = 4;
//  60    w_if #(12) b4();
//  61    c4_sub u4(.b(b4));
//  62    // CHK5
//  63    w_if #(100) scalar1(), vector[9:0]();
//  64  endmodule

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
#include <hldb/cont_assign.h>
#include <hldb/def_param.h>
#include <hldb/design.h>
#include <hldb/expr.h>
#include <hldb/gen_block.h>
#include <hldb/gen_for.h>
#include <hldb/gen_if.h>
#include <hldb/gen_if_else.h>
#include <hldb/gen_region.h>
#include <hldb/if_stmt.h>
#include <hldb/int_typespec.h>
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
#include <hldb/type_parameter.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/unit.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <initializer_list>
#include <string_view>
#include <vector>

namespace hlc {

class IfParamKindsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.8--if_param_kinds.hlc"}); }
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

static const hldb::InterfaceTypespec *getInstInterface(const hldb::Module *top, std::string_view name) {
  if (top == nullptr) return nullptr;
  const hldb::RefInstance *const inst = hldb::findByName<hldb::RefInstance>(name, top->getRefInstances());
  if ((inst == nullptr) || (inst->getTypespec() == nullptr)) return nullptr;
  const hldb::Typespec *ts = inst->getTypespec()->getActual();
  if (const hldb::ArrayTypespec *const at = any_cast<hldb::ArrayTypespec>(ts)) {
    ts = (at->getElemTypespec() == nullptr) ? nullptr : at->getElemTypespec()->getActual();
  }
  return any_cast<hldb::InterfaceTypespec>(ts);
}

// CHK1: interface c1_if #(parameter type T = logic [7:0]); c1_if #(.T(int)) u1();
TEST_F(IfParamKindsTest, TypeParameter) {
  const hldb::Interface *const c1 = getInterface("c1_if");
  ASSERT_NE(c1, nullptr);
  EXPECT_NE(hldb::findByName<hldb::TypeParameter>("T", c1->getParameters()), nullptr) << "type parameter T";
  const hldb::InterfaceTypespec *const its = getInstInterface(getTop(), "u1");
  ASSERT_NE(its, nullptr);
  ASSERT_NE(its->getParamAssigns(), nullptr);
  ASSERT_EQ(its->getParamAssigns()->size(), 1u);
  const hldb::Any *rhs = its->getParamAssigns()->at(0)->getRhs();
  if (const hldb::RefTypespec *const rt = any_cast<hldb::RefTypespec>(rhs)) rhs = rt->getActual();
  ASSERT_NE(rhs, nullptr);
  EXPECT_EQ(rhs->getAnyType(), hldb::AnyType::IntTypespec) << "T is overridden with int";
}

// CHK2: parameter int W = 4; in the body of c2_if, overridden by #(.W(12)).
TEST_F(IfParamKindsTest, BodyParameter) {
  const hldb::Interface *const c2 = getInterface("c2_if");
  ASSERT_NE(c2, nullptr);
  const hldb::Parameter *const w = hldb::findByName<hldb::Parameter>("W", c2->getParameters());
  ASSERT_NE(w, nullptr);
  EXPECT_FALSE(w->getLocalParam())
      << "a body parameter of an interface without a parameter port list can be overridden";
  EXPECT_EQ(getOverride(getInstInterface(getTop(), "u2"), "W"), std::string_view("12"));
}

// CHK3: defparam u3.W = 4;
TEST_F(IfParamKindsTest, Defparam) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module top not found";
  ASSERT_NE(top->getDefParams(), nullptr);
  ASSERT_EQ(top->getDefParams()->size(), 1u);
  const hldb::Constant *const v = any_cast<hldb::Constant>(top->getDefParams()->at(0)->getRhs());
  ASSERT_NE(v, nullptr);
  EXPECT_EQ(v->getDecompile(), std::string_view("4"));
}

// CHK4: module c4_sub(w_if b); initial w = b.W;  ...  c4_sub u4(.b(b4));
TEST_F(IfParamKindsTest, ParameterThroughPort) {
  const hldb::Assignment *const a = getAssignmentAt(getModule("c4_sub"), 49);
  ASSERT_NE(a, nullptr) << "w = b.W";
  const hldb::Any *const w = getRootActual(a->getRhs());
  ASSERT_NE(w, nullptr) << "b.W is unresolved";
  EXPECT_EQ(w->getName(), std::string_view("W"));
  EXPECT_EQ(w->getAnyType(), hldb::AnyType::Parameter) << "b.W is the parameter W of w_if";
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(getOverride(getInstInterface(top, "b4"), "W"), std::string_view("12"));
  EXPECT_EQ(getBoundName(getHighConn(getInst(top, "u4"), "b")), std::string_view("b4"));
}

// CHK5: w_if #(100) scalar1(), vector[9:0]();
TEST_F(IfParamKindsTest, OneOverrideForSeveralInstances) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(getOverride(getInstInterface(top, "scalar1"), "W"), std::string_view("100")) << "scalar1";
  EXPECT_EQ(getOverride(getInstInterface(top, "vector"), "W"), std::string_view("100")) << "vector";
  const hldb::ArrayTypespec *const vec = getInstArray(top, "vector");
  ASSERT_NE(vec, nullptr) << "vector is an array of instances";
  expectRange(vec->getRange(), "9", "0", "vector[9:0]");
}

// The file is legal SystemVerilog, so compilation must not report any error.
TEST_F(IfParamKindsTest, CompilesWithoutErrors) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
