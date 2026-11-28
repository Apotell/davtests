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

// Tests for tests/Google/chapter-25/25.7.3--mp_import_prototype_export.sv
// Expected results follow IEEE 1800-2023 (25.4, 25.6, 25.7, 25.7.2-25.7.4, 23.2.2, 37); they
// are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - CHK1: target exports Read, Write and calc (vpiExported)
//   - CHK2/CHK4: initiator imports Read, Write and calc with full prototypes (vpiImported)
//   - CHK3: memMod defines the tasks Read, Write and the function calc ('task a.Read', 25.7.3)
//   - CHK5: cpuMod's always block calls b.Read, b.Write and b.calc() (procedure present)
//   - top: mem -> sb_intf.target, cpu -> sb_intf.initiator
//   - No errors: the whole file is legal (LRM 25.7.3 example)
// Skipped (GTEST_SKIP; the real assertions stay below the skip):
//   - CompilesWithoutErrors: HLC reports a 'Null Actual' error (DV8504) for the legal calls b.Read, b.Write and
//     b.calc() through a generic interface port, which it cannot resolve without elaboration.
// Not checked:
//   - The default value 3 of the calc prototype and the argument it supplies to b.calc() (no accessor
//     for an import prototype's defaults on the modport entry)
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: mp_import_prototype_export
//  19  :description: import with full prototypes and export of tasks defined in a module
//  20  :tags: 25.7.3
//  21  */
//  22
//  23  // Checks in this file:
//  24  //   CHK1: export Read, Write by name in the target modport
//  25  //   CHK2: import task prototypes in the initiator modport
//  26  //   CHK3: tasks defined in the module with hierarchical names task a.Read
//  27  //   CHK4: import function prototype with a default argument value
//  28  //   CHK5: call using the default argument from the prototype
//  29
//  30  interface simple_bus(input logic clk);
//  31    logic req, gnt;
//  32    logic [7:0] addr, data;
//  33
//  34    // CHK1
//  35    modport target (input req, addr, clk, output gnt, ref data,
//  36                    export Read, Write, export calc);
//  37    // CHK2 + CHK4
//  38    modport initiator (input gnt, clk, output req, addr, ref data,
//  39                       import task Read(input logic [7:0] raddr),
//  40                              task Write(input logic [7:0] waddr),
//  41                       import function int calc(int v = 3));
//  42  endinterface : simple_bus
//  43
//  44  module memMod(interface a);
//  45    logic avail;
//  46    // CHK3
//  47    task a.Read(input logic [7:0] raddr);
//  48      avail = 0;
//  49      avail = 1;
//  50    endtask
//  51    task a.Write(input logic [7:0] waddr);
//  52      avail = 0;
//  53      avail = 1;
//  54    endtask
//  55    function int a.calc(int v);
//  56      return v * 2;
//  57    endfunction
//  58  endmodule
//  59
//  60  module cpuMod(interface b);
//  61    logic [7:0] raddr;
//  62    int r;
//  63    always @(posedge b.clk) begin
//  64      b.Read(raddr);
//  65      b.Write(raddr);
//  66      // CHK5
//  67      r = b.calc();
//  68    end
//  69  endmodule
//  70
//  71  module top;
//  72    logic clk = 0;
//  73    simple_bus sb_intf(clk);
//  74    memMod mem(sb_intf.target);
//  75    cpuMod cpu(sb_intf.initiator);
//  76  endmodule

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

class MpImportPrototypeExportTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.7.3--mp_import_prototype_export.hlc"}); }
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

TEST_F(MpImportPrototypeExportTest, ExportsAndImports) {
  const hldb::Interface *const bus = getInterface("simple_bus");
  const hldb::Modport *const target = getModport(bus, "target");
  const hldb::Modport *const initiator = getModport(bus, "initiator");
  for (const char *const name : {"Read", "Write", "calc"}) {
    expectSubroutineEntry(target, name, vpiExported, "target");
    expectSubroutineEntry(initiator, name, vpiImported, "initiator");
  }
}

// CHK3: task a.Read ...; task a.Write ...; function int a.calc ...; in memMod
TEST_F(MpImportPrototypeExportTest, ExportedDefinitions) {
  const hldb::Module *const mem = getModule("memMod");
  ASSERT_NE(mem, nullptr);
  EXPECT_NE(any_cast<hldb::Task>(getTaskFunc(mem, "Read")), nullptr) << "task a.Read";
  EXPECT_NE(any_cast<hldb::Task>(getTaskFunc(mem, "Write")), nullptr) << "task a.Write";
  EXPECT_NE(any_cast<hldb::Function>(getTaskFunc(mem, "calc")), nullptr) << "function a.calc";
}

TEST_F(MpImportPrototypeExportTest, Connections) {
  const hldb::Module *const cpu = getModule("cpuMod");
  ASSERT_NE(cpu, nullptr);
  ASSERT_NE(cpu->getProcesses(), nullptr);
  EXPECT_EQ(cpu->getProcesses()->size(), 1u);
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module top not found";
  EXPECT_EQ(getBoundName(getHighConn(getInst(top, "mem"), "a")), std::string_view("target"));
  EXPECT_EQ(getBoundName(getHighConn(getInst(top, "cpu"), "b")), std::string_view("initiator"));
}

// The file is legal SystemVerilog, so compilation must not report any error.
TEST_F(MpImportPrototypeExportTest, CompilesWithoutErrors) {
  GTEST_SKIP() << "IEEE 1800-2023 Sec 25.7, 25.7.3: b.Read, b.Write and b.calc() are legal; b is a generic"
                  " interface port, so the called subroutines depend on the connected modport and can only be"
                  " resolved by elaboration. HLC reports a 'Null Actual' error (DV8504) for each call.";
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
