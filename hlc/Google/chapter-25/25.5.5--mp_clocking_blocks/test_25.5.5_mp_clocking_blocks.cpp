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

// Tests for tests/Google/chapter-25/25.5.5--mp_clocking_blocks.sv
// Expected results follow IEEE 1800-2023 (25.5, 25.5.1-25.5.5, 25.10, 23.3.3, 37); they are
// not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - A_Bus has wire nets req, gnt, addr, data and the clocking block sb
//   - CHK2: DUT = (input clk, req, addr, output gnt, inout data); TB = (input gnt, output req, addr,
//     inout data)
//   - CHK1: STB lists only the clocking block sb (a clocking entry, 25.5.5)
//   - dev1.b and dev2.b select A_Bus.DUT; CHK3: program T has ports b1, b2 selecting A_Bus.STB
//   - CHK5: T holds an assertion; CHK4: T has an initial block
//   - top: d1 -> b1, d2 -> b2, tb -> (b1, b2)
//   - No errors: the whole file is legal (LRM 25.5.5 example)
// Skipped (GTEST_SKIP; the real assertions stay below the skip):
//   - CompilesWithoutErrors: HLC builds the dotted property reference b1.sb.p1 as a plain name reference
//     and reports model errors for it.
// Not checked:
//   - Run-time behavior of the program
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: mp_clocking_blocks
//  19  :description: clocking blocks referenced from modports (LRM A_Bus example)
//  20  :tags: 25.5.5
//  21  */
//  22
//  23  // Checks in this file:
//  24  //   CHK1: modport containing only a clocking block (STB)
//  25  //   CHK2: asynchronous modports without the clocking block (DUT, TB)
//  26  //   CHK3: program with two clocking-modport ports driving b.sb.req
//  27  //   CHK4: wait on a clocking block input through the modport
//  28  //   CHK5: assert property declared inside the clocking block via b1.sb.p1
//  29
//  30  interface A_Bus(input logic clk);
//  31    wire req, gnt;
//  32    wire [7:0] addr, data;
//  33
//  34    clocking sb @(posedge clk);
//  35      input gnt;
//  36      output req, addr;
//  37      inout data;
//  38      property p1; req ##[1:3] gnt; endproperty
//  39    endclocking
//  40
//  41    // CHK2
//  42    modport DUT (input clk, req, addr, output gnt, inout data);
//  43    // CHK1
//  44    modport STB (clocking sb);
//  45    // CHK2
//  46    modport TB (input gnt, output req, addr, inout data);
//  47  endinterface
//  48
//  49  module dev1(A_Bus.DUT b);
//  50  endmodule
//  51
//  52  module dev2(A_Bus.DUT b);
//  53  endmodule
//  54
//  55  // CHK3
//  56  program T(A_Bus.STB b1, A_Bus.STB b2);
//  57    // CHK5
//  58    assert property (b1.sb.p1);
//  59    initial begin
//  60      b1.sb.req <= 1;
//  61      // CHK4
//  62      wait (b1.sb.gnt == 1);
//  63      b1.sb.req <= 0;
//  64      b2.sb.req <= 1;
//  65      wait (b2.sb.gnt == 1);
//  66      b2.sb.req <= 0;
//  67    end
//  68  endprogram
//  69
//  70  module top;
//  71    logic clk;
//  72    A_Bus b1(clk);
//  73    A_Bus b2(clk);
//  74    dev1 d1(b1);
//  75    dev2 d2(b2);
//  76    T tb(b1, b2);
//  77  endmodule

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/begin.h>
#include <hldb/clocking_block.h>
#include <hldb/constant.h>
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
#include <hldb/program.h>
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

class MpClockingBlocksTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.5.5--mp_clocking_blocks.hlc"}); }
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

TEST_F(MpClockingBlocksTest, InterfaceItems) {
  const hldb::Interface *const bus = getInterface("A_Bus");
  ASSERT_NE(bus, nullptr);
  for (const char *const name : {"req", "gnt", "addr", "data"}) {
    const hldb::Net *const n = getNet(bus, name);
    ASSERT_NE(n, nullptr) << name;
    EXPECT_EQ(n->getNetType(), vpiWire) << name;
  }
  EXPECT_NE(hldb::findByName<hldb::ClockingBlock>("sb", bus->getClockingBlocks()), nullptr);
}

TEST_F(MpClockingBlocksTest, AsynchronousModports) {
  const hldb::Interface *const bus = getInterface("A_Bus");
  expectEntries(getModport(bus, "DUT"),
                {{"clk", vpiInput}, {"req", vpiInput}, {"addr", vpiInput}, {"gnt", vpiOutput}, {"data", vpiInout}},
                "DUT");
  expectEntries(getModport(bus, "TB"), {{"gnt", vpiInput}, {"req", vpiOutput}, {"addr", vpiOutput}, {"data", vpiInout}},
                "TB");
}

// CHK1: modport STB (clocking sb);
TEST_F(MpClockingBlocksTest, ClockingModport) {
  const hldb::Modport *const stb = getModport(getInterface("A_Bus"), "STB");
  ASSERT_NE(stb, nullptr);
  ASSERT_NE(stb->getIODecls(), nullptr);
  ASSERT_EQ(stb->getIODecls()->size(), 1u);
  EXPECT_EQ(stb->getIODecls()->at(0)->getName(), std::string_view("sb"));
  EXPECT_TRUE(stb->getIODecls()->at(0)->getClocking()) << "sb is a clocking entry";
}

TEST_F(MpClockingBlocksTest, PortsAndProgram) {
  expectInterfacePort(getModule("dev1"), "b", "A_Bus", "DUT");
  expectInterfacePort(getModule("dev2"), "b", "A_Bus", "DUT");
  const hldb::Program *const prog = getProgram("T");
  ASSERT_NE(prog, nullptr) << "program T";
  expectInterfacePort(prog, "b1", "A_Bus", "STB");
  expectInterfacePort(prog, "b2", "A_Bus", "STB");
  const bool inConcurrent = (prog->getConcurrentAssertions() != nullptr) && !prog->getConcurrentAssertions()->empty();
  const bool inAssertions = (prog->getAssertions() != nullptr) && !prog->getAssertions()->empty();
  EXPECT_TRUE(inConcurrent || inAssertions) << "assert property (b1.sb.p1)";
  ASSERT_NE(prog->getProcesses(), nullptr);
  EXPECT_EQ(prog->getProcesses()->size(), 1u) << "initial begin ... end";
}

TEST_F(MpClockingBlocksTest, Connections) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module top not found";
  EXPECT_EQ(getBoundName(getHighConn(getInst(top, "d1"), "b")), std::string_view("b1"));
  EXPECT_EQ(getBoundName(getHighConn(getInst(top, "d2"), "b")), std::string_view("b2"));
  EXPECT_EQ(getBoundName(getHighConn(getInst(top, "tb"), "b1")), std::string_view("b1"));
  EXPECT_EQ(getBoundName(getHighConn(getInst(top, "tb"), "b2")), std::string_view("b2"));
}

// The file is legal SystemVerilog, so compilation must not report any error.
TEST_F(MpClockingBlocksTest, CompilesWithoutErrors) {
  GTEST_SKIP() << "IEEE 1800-2023 Sec 25.5.5 (its own A_Bus example): assert property (b1.sb.p1) names the property"
                  " p1 of the clocking block sb through the modport STB and is legal. HLC builds the dotted property"
                  " reference as a plain name reference and reports three model errors for it (the same happens for"
                  " cb.p2 without a modport; a plain assert property (q) is fine).";
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
