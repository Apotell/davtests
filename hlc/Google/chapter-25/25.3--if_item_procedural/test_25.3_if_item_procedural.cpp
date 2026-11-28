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

// Tests for tests/Google/chapter-25/25.3--if_item_procedural.sv
// Expected results follow IEEE 1800-2023 (25.3, 25.3.2, 25.3.3, 23.2.2, 23.3.2, 7.4.2,
// 37); they are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - CHK1: c1_if has 'assign y = a ^ b' (lhs y)
//   - CHK2: c2_if has an always_ff and an always_comb process
//   - CHK3: c3_if has an initial and a final process
//   - CHK4: c4_if has typedef state_t (an enum) and variable state of that type
//   - CHK5: c5_if declares program p
//   - No errors: the whole file is legal
// Not checked:
//   - Run-time behavior of the processes
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: if_item_procedural
//  19  :description: procedural and behavioral items inside an interface
//  20  :tags: 25.3
//  21  */
//  22
//  23  // Checks in this file:
//  24  //   CHK1: continuous assignment inside the interface
//  25  //   CHK2: always_ff and always_comb inside the interface
//  26  //   CHK3: initial and final procedures inside the interface
//  27  //   CHK4: enum typedef and initialized enum variable
//  28  //   CHK5: program declaration inside the interface
//  29
//  30  // CHK1
//  31  interface c1_if;
//  32    logic a, b, y;
//  33    assign y = a ^ b;
//  34  endinterface
//  35
//  36  // CHK2
//  37  interface c2_if(input logic clk);
//  38    logic d, q, n;
//  39    always_ff @(posedge clk) q <= d;
//  40    always_comb n = ~q;
//  41  endinterface
//  42
//  43  // CHK3
//  44  interface c3_if;
//  45    logic a;
//  46    initial a = 1'b0;
//  47    final $display("a=%b", a);
//  48  endinterface
//  49
//  50  // CHK4
//  51  interface c4_if;
//  52    typedef enum logic [1:0] {IDLE, BUSY, DONE} state_t;
//  53    state_t state = IDLE;
//  54  endinterface
//  55
//  56  // CHK5
//  57  interface c5_if;
//  58    logic a;
//  59    program p;
//  60      initial a = 1'b1;
//  61    endprogram
//  62  endinterface
//  63
//  64  module top;
//  65    logic clk;
//  66    c1_if u1();
//  67    c2_if u2(clk);
//  68    c3_if u3();
//  69    c4_if u4();
//  70    c5_if u5();
//  71  endmodule

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/always.h>
#include <hldb/array_typespec.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/enum.h>
#include <hldb/enum_typespec.h>
#include <hldb/expr.h>
#include <hldb/final_stmt.h>
#include <hldb/gen_block.h>
#include <hldb/gen_for.h>
#include <hldb/gen_if.h>
#include <hldb/gen_if_else.h>
#include <hldb/gen_region.h>
#include <hldb/initial.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/named_argument.h>
#include <hldb/net.h>
#include <hldb/port.h>
#include <hldb/process_stmt.h>
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

#include <string_view>
#include <vector>

namespace hlc {

class IfItemProceduralTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3--if_item_procedural.hlc"}); }
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
};

TEST_F(IfItemProceduralTest, ContinuousAssign) {
  const hldb::Interface *const c1 = getInterface("c1_if");
  ASSERT_NE(c1, nullptr);
  ASSERT_NE(c1->getContAssigns(), nullptr);
  ASSERT_EQ(c1->getContAssigns()->size(), 1u);
  EXPECT_EQ(getRootActual(c1->getContAssigns()->at(0)->getLhs()), getVar(c1, "y"));
}

TEST_F(IfItemProceduralTest, AlwaysProcesses) {
  const hldb::Interface *const c2 = getInterface("c2_if");
  ASSERT_NE(c2, nullptr);
  ASSERT_NE(c2->getProcesses(), nullptr);
  bool ff = false;
  bool comb = false;
  for (const hldb::Process *const p : *c2->getProcesses()) {
    if (const hldb::Always *const a = any_cast<hldb::Always>(p)) {
      ff = ff || (a->getAlwaysType() == vpiAlwaysFF);
      comb = comb || (a->getAlwaysType() == vpiAlwaysComb);
    }
  }
  EXPECT_TRUE(ff) << "always_ff";
  EXPECT_TRUE(comb) << "always_comb";
}

TEST_F(IfItemProceduralTest, InitialAndFinal) {
  const hldb::Interface *const c3 = getInterface("c3_if");
  ASSERT_NE(c3, nullptr);
  ASSERT_NE(c3->getProcesses(), nullptr);
  bool hasInitial = false;
  bool hasFinal = false;
  for (const hldb::Process *const p : *c3->getProcesses()) {
    hasInitial = hasInitial || (any_cast<hldb::Initial>(p) != nullptr);
    hasFinal = hasFinal || (any_cast<hldb::FinalStmt>(p) != nullptr);
  }
  EXPECT_TRUE(hasInitial) << "initial";
  EXPECT_TRUE(hasFinal) << "final";
}

// CHK4: typedef enum logic [1:0] {IDLE, BUSY, DONE} state_t; state_t state = IDLE;
TEST_F(IfItemProceduralTest, EnumTypedef) {
  const hldb::Interface *const c4 = getInterface("c4_if");
  ASSERT_NE(c4, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Typedef>("state_t", c4->getTypedefs()), nullptr) << "typedef state_t";
  const hldb::Variable *const state = getVar(c4, "state");
  ASSERT_NE(state, nullptr);
  const hldb::EnumTypespec *const et = any_cast<hldb::EnumTypespec>(getResolvedTypespec(state));
  ASSERT_NE(et, nullptr) << "state has the enum type state_t";
  ASSERT_NE(et->getEnum(), nullptr);
  ASSERT_NE(et->getEnum()->getEnumConsts(), nullptr);
  EXPECT_EQ(et->getEnum()->getEnumConsts()->size(), 3u) << "IDLE, BUSY, DONE";
}

// CHK5: program p; ... endprogram inside c5_if
TEST_F(IfItemProceduralTest, ProgramInsideInterface) {
  const hldb::Interface *const c5 = getInterface("c5_if");
  ASSERT_NE(c5, nullptr);
  const bool inInterface = hldb::findByDefName<hldb::Program>("p", c5->getPrograms()) != nullptr;
  const bool inDesign = hldb::findByDefName<hldb::Program>("p", m_design->getAllPrograms()) != nullptr;
  EXPECT_TRUE(inInterface || inDesign) << "program p declared inside c5_if";
}

// The file is legal SystemVerilog, so compilation must not report any error.
TEST_F(IfItemProceduralTest, CompilesWithoutErrors) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
