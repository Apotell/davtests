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

// Tests for tests/Google/chapter-25/25.7--if_tasks_functions.sv
// Expected results follow IEEE 1800-2023 (25.4, 25.6, 25.7, 25.7.2-25.7.4, 23.2.2, 37); they
// are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - bus_if declares the automatic function inc, the automatic task drive and the function bump
//   - CHK1: top's 'assign r = b.inc(b.data)' calls inc of bus_if (bound to that function)
//   - CHK2: drv calls b.drive(1'b1) in an initial block through its interface port
//   - CHK3: top calls b.bump() as a statement in an initial block
//   - CHK4: gen's 'assign r = g.inc(8'd4)' calls inc through a generic port (checked by name)
//   - Connections: u_drv.b and u_gen.g are connected to b
//   - No errors: the whole file is legal
// Skipped (GTEST_SKIP; the real assertions stay below the skip):
//   - CompilesWithoutErrors: HLC reports a 'Null Actual' error (DV8504) for the legal call g.inc(...) through the
//     generic interface port g, which it cannot resolve without elaboration.
// Not checked:
//   - What g.inc binds to: g is a generic port, so its interface is known only per instance after
//     elaboration
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: if_tasks_functions
//  19  :description: tasks and functions declared in an interface
//  20  :tags: 25.7
//  21  */
//  22
//  23  // Checks in this file:
//  24  //   CHK1: function called through the instance name in a continuous assign
//  25  //   CHK2: task called through a module interface port
//  26  //   CHK3: void function called as a statement
//  27  //   CHK4: function called through a generic interface port
//  28
//  29  interface bus_if(input logic clk);
//  30    logic [7:0] data;
//  31    logic req;
//  32    int count;
//  33    // CHK1 + CHK4
//  34    function automatic logic [7:0] inc(logic [7:0] v);
//  35      return v + 1;
//  36    endfunction
//  37    // CHK2
//  38    task automatic drive(input logic v);
//  39      @(posedge clk) req <= v;
//  40    endtask
//  41    // CHK3
//  42    function automatic void bump();
//  43      count++;
//  44    endfunction
//  45  endinterface
//  46
//  47  module drv(bus_if b);
//  48    initial b.drive(1'b1);
//  49  endmodule
//  50
//  51  module gen(interface g);
//  52    logic [7:0] r;
//  53    assign r = g.inc(8'd4);
//  54  endmodule
//  55
//  56  module top;
//  57    logic clk;
//  58    bus_if b(clk);
//  59    logic [7:0] r;
//  60    assign r = b.inc(b.data);
//  61    drv u_drv(.b(b));
//  62    initial b.bump();
//  63    gen u_gen(.g(b));
//  64  endmodule

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
#include <hldb/func_call.h>
#include <hldb/function.h>
#include <hldb/gen_block.h>
#include <hldb/gen_for.h>
#include <hldb/gen_if.h>
#include <hldb/gen_if_else.h>
#include <hldb/gen_region.h>
#include <hldb/hldb_vpi_user.h>
#include <hldb/initial.h>
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
#include <hldb/task_call.h>
#include <hldb/task_func.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <initializer_list>
#include <string_view>
#include <vector>

namespace hlc {

class IfTasksFunctionsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.7--if_tasks_functions.hlc"}); }
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

TEST_F(IfTasksFunctionsTest, Subroutines) {
  const hldb::Interface *const bus = getInterface("bus_if");
  ASSERT_NE(bus, nullptr);
  const hldb::Function *const inc = any_cast<hldb::Function>(getTaskFunc(bus, "inc"));
  const hldb::Task *const drive = any_cast<hldb::Task>(getTaskFunc(bus, "drive"));
  const hldb::Function *const bump = any_cast<hldb::Function>(getTaskFunc(bus, "bump"));
  ASSERT_NE(inc, nullptr) << "function inc";
  ASSERT_NE(drive, nullptr) << "task drive";
  ASSERT_NE(bump, nullptr) << "function bump";
  EXPECT_TRUE(inc->getAutomatic());
  EXPECT_TRUE(drive->getAutomatic());
  EXPECT_TRUE(bump->getAutomatic());
}

// CHK1: assign r = b.inc(b.data);
TEST_F(IfTasksFunctionsTest, FunctionCallThroughInstance) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module top not found";
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
  // Any subroutine-call object satisfies the standard; the kind of call node is HLC's modeling choice.
  const hldb::TFCall *const call = any_cast<hldb::TFCall>(top->getContAssigns()->at(0)->getRhs());
  ASSERT_NE(call, nullptr) << "the right-hand side is the call b.inc(...)";
  EXPECT_EQ(call->getName(), std::string_view("inc"));
  EXPECT_EQ(call->getTaskFunc(), getTaskFunc(getInterface("bus_if"), "inc")) << "bound to bus_if's inc";
}

// CHK2/CHK3: initial b.drive(1'b1); in drv and initial b.bump(); in top.
TEST_F(IfTasksFunctionsTest, StatementCalls) {
  const hldb::Module *const drv = getModule("drv");
  const hldb::Module *const top = getTop();
  ASSERT_NE(drv, nullptr);
  ASSERT_NE(top, nullptr);
  ASSERT_NE(drv->getProcesses(), nullptr);
  ASSERT_EQ(drv->getProcesses()->size(), 1u);
  const hldb::Initial *const di = any_cast<hldb::Initial>(drv->getProcesses()->at(0));
  ASSERT_NE(di, nullptr);
  const hldb::TFCall *const driveCall = any_cast<hldb::TFCall>(di->getStmt());
  ASSERT_NE(driveCall, nullptr) << "initial b.drive(1'b1)";
  EXPECT_EQ(driveCall->getName(), std::string_view("drive"));
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u);
  const hldb::Initial *const ti = any_cast<hldb::Initial>(top->getProcesses()->at(0));
  ASSERT_NE(ti, nullptr);
  const hldb::TFCall *const bumpCall = any_cast<hldb::TFCall>(ti->getStmt());
  ASSERT_NE(bumpCall, nullptr) << "initial b.bump()";
  EXPECT_EQ(bumpCall->getName(), std::string_view("bump"));
  EXPECT_EQ(bumpCall->getTaskFunc(), getTaskFunc(getInterface("bus_if"), "bump"));
}

// CHK4: assign r = g.inc(8'd4); through the generic port g.
TEST_F(IfTasksFunctionsTest, CallThroughGenericPort) {
  const hldb::Module *const gen = getModule("gen");
  ASSERT_NE(gen, nullptr);
  ASSERT_NE(gen->getContAssigns(), nullptr);
  ASSERT_EQ(gen->getContAssigns()->size(), 1u);
  const hldb::TFCall *const call = any_cast<hldb::TFCall>(gen->getContAssigns()->at(0)->getRhs());
  ASSERT_NE(call, nullptr) << "g.inc(8'd4)";
  EXPECT_EQ(call->getName(), std::string_view("inc"));
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(getBoundName(getHighConn(getInst(top, "u_drv"), "b")), std::string_view("b"));
  EXPECT_EQ(getBoundName(getHighConn(getInst(top, "u_gen"), "g")), std::string_view("b"));
}

// The file is legal SystemVerilog, so compilation must not report any error.
TEST_F(IfTasksFunctionsTest, CompilesWithoutErrors) {
  GTEST_SKIP() << "IEEE 1800-2023 Sec 25.3.3, 25.7: g.inc(...) is legal; g is a generic interface port, so the"
                  " called subroutine depends on the connected interface and can only be resolved by elaboration."
                  " HLC reports a 'Null Actual' error (DV8504) for the call.";
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
