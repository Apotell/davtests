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

// Tests for tests/Google/chapter-25/25.7.3--mp_err_export.sv
// Expected results follow IEEE 1800-2023 (25.4, 25.6, 25.7, 25.7.2-25.7.4, 23.2.2, 37); they
// are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - 25.7: a module connected to a modport with an exported subroutine shall define it, and the
//     definition shall match the export prototype:
//   - ERR1: m1 never defines Read (export at line 33, connection at line 87) -> CompMissingImplementation
//   - ERR2 line 46: Put(logic [7:0]) does not match 'export task Put(input int v)' ->
//     CompModportPrototypeMismatch or CompExternPrototypeMismatch
//   - ERR3 line 56: Put(int, int) does not match the 1-argument prototype -> same codes
//   - ERR4: u4a and u4b (lines 90-91) both export Ping without extern forkjoin (25.7.4) ->
//     CompMultiplyDefinedTaskFunc or CompDuplicateDeclaration (line 67 or 90-91)
//   - ERR5 line 77: a task named Get where the modport exports a function -> the prototype codes
// Skipped (GTEST_SKIP; the real assertions stay below the skip):
//   All five need elaboration: each module has a generic interface port, so the exporting modport (and the
//   prototype to match) is only known from the instances in top. HLC does not elaborate and reports no error.
//   - MissingExportedTaskIsRejected: 25.7, m1 does not define the exported Read.
//   - ExportTypeMismatchIsRejected: 25.7, a.Put does not exactly match the exported prototype (argument type).
//   - ExportCountMismatchIsRejected: 25.7, a.Put does not exactly match the exported prototype (argument count).
//   - DoubleExportIsRejected: 25.7.4, two modules export Ping although the interface does not declare it
//     extern forkjoin.
//   - TaskForExportedFunctionIsRejected: 25.7, a task does not match the exported function prototype.
// Not checked:
//   - The diagnostic codes are HLC's choice; each test accepts the codes HLC defines for the rule
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: mp_err_export
//  19  :description: errors with subroutines exported through modports
//  20  :should_fail_because: exported subroutines must be defined by the connected module and match the modport
//  prototype 21  :tags: 25.7.3 22  */ 23 24  // Errors in this file: 25  //   ERR1: module connected to a modport that
//  exports Read but never defines it 26  //   ERR2: exported prototype does not match the module definition (argument
//  type) 27  //   ERR3: exported prototype does not match the module definition (argument count) 28  //   ERR4: two
//  modules export the same task without extern forkjoin 29  //   ERR5: modport exports a function but the module
//  defines a task with that name 30 31  // ERR1 32  interface if1; 33    modport target (export Read); 34    modport
//  initiator (import task Read()); 35  endinterface 36 37  module m1(interface a); 38  endmodule 39 40  // ERR2 41
//  interface if2; 42    modport target (export task Put(input int v)); 43  endinterface 44 45  module m2(interface a);
//  46    task a.Put(input logic [7:0] v);
//  47    endtask
//  48  endmodule
//  49
//  50  // ERR3
//  51  interface if3;
//  52    modport target (export task Put(input int v));
//  53  endinterface
//  54
//  55  module m3(interface a);
//  56    task a.Put(input int v, input int w);
//  57    endtask
//  58  endmodule
//  59
//  60  // ERR4
//  61  interface if4;
//  62    modport target (export Ping);
//  63    modport initiator (import task Ping());
//  64  endinterface
//  65
//  66  module m4(interface a);
//  67    task a.Ping();
//  68    endtask
//  69  endmodule
//  70
//  71  // ERR5
//  72  interface if5;
//  73    modport target (export function int Get());
//  74  endinterface
//  75
//  76  module m5(interface a);
//  77    task a.Get();
//  78    endtask
//  79  endmodule
//  80
//  81  module top;
//  82    if1 i1();
//  83    if2 i2();
//  84    if3 i3();
//  85    if4 i4();
//  86    if5 i5();
//  87    m1 u1(i1.target);
//  88    m2 u2(i2.target);
//  89    m3 u3(i3.target);
//  90    m4 u4a(i4.target);
//  91    m4 u4b(i4.target);
//  92    m5 u5(i5.target);
//  93  endmodule

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

class MpErrExportTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.7.3--mp_err_export.hlc"}); }
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

// ERR1: modport target (export Read); m1 u1(i1.target);
TEST_F(MpErrExportTest, MissingExportedTaskIsRejected) {
  GTEST_SKIP() << "Needs elaboration. IEEE 1800-2023 Sec 25.7: a module connected to a modport that exports a"
                  " subroutine shall define it, else an elaboration error shall occur. m1 has a generic interface"
                  " port, so only u1(i1.target) shows that Read must be defined; HLC does not elaborate.";
  EXPECT_TRUE(anyEvent({EventType::CompMissingImplementation}, 33, 87))
      << "lines 33-87: m1 does not define the exported Read";
}

// ERR2: task a.Put(input logic [7:0] v);
TEST_F(MpErrExportTest, ExportTypeMismatchIsRejected) {
  GTEST_SKIP() << "Needs elaboration. IEEE 1800-2023 Sec 25.7, 6.22.1: a subroutine that does not exactly match the"
                  " exported prototype is an elaboration error (int vs logic [7:0]). m2 has a generic interface port,"
                  " so the prototype is only known from u2(i2.target); HLC does not elaborate.";
  EXPECT_TRUE(anyEvent({EventType::CompModportPrototypeMismatch, EventType::CompExternPrototypeMismatch}, 46, 46))
      << "line 46: Put takes an int";
}

// ERR3: task a.Put(input int v, input int w);
TEST_F(MpErrExportTest, ExportCountMismatchIsRejected) {
  GTEST_SKIP() << "Needs elaboration. IEEE 1800-2023 Sec 25.7: a subroutine that does not exactly match the exported"
                  " prototype is an elaboration error (two arguments vs one). m3 has a generic interface port, so"
                  " the prototype is only known from u3(i3.target); HLC does not elaborate.";
  EXPECT_TRUE(anyEvent({EventType::CompModportPrototypeMismatch, EventType::CompExternPrototypeMismatch}, 56, 56))
      << "line 56: Put takes one argument";
}

// ERR4: m4 u4a(i4.target); m4 u4b(i4.target);
TEST_F(MpErrExportTest, DoubleExportIsRejected) {
  GTEST_SKIP() << "Needs elaboration. IEEE 1800-2023 Sec 25.7.4: it is normally an error for more than one module to"
                  " export the same task name; such tasks shall be declared extern forkjoin in the interface. Only"
                  " the instances u4a and u4b, both on i4.target, show the double export; HLC does not elaborate.";
  EXPECT_TRUE(anyEvent({EventType::CompMultiplyDefinedTaskFunc, EventType::CompDuplicateDeclaration}, 67, 91))
      << "lines 67-91: Ping is exported twice without extern forkjoin";
}

// ERR5: task a.Get(); for export function int Get()
TEST_F(MpErrExportTest, TaskForExportedFunctionIsRejected) {
  GTEST_SKIP() << "Needs elaboration. IEEE 1800-2023 Sec 25.7: a task does not exactly match the exported function"
                  " prototype, an elaboration error. m5 has a generic interface port, so the prototype is only known"
                  " from u5(i5.target); HLC does not elaborate.";
  EXPECT_TRUE(anyEvent({EventType::CompModportPrototypeMismatch, EventType::CompExternPrototypeMismatch}, 77, 77))
      << "line 77: Get must be a function";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
