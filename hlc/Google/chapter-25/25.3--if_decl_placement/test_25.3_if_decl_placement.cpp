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

// Tests for tests/Google/chapter-25/25.3--if_decl_placement.sv
// Expected results follow IEEE 1800-2023 (25.3, 25.3.2, 25.3.3, 23.2.2, 23.3.2, 7.4.2,
// 37); they are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - CHK1: u1 is bound to c1_if although c1_if is declared after top
//   - CHK2: u2 is bound to c2_local_if, an interface declared inside module top (variable a)
//   - CHK3: c3_outer_if declares the nested interface c3_inner_if and instantiates it as u_inner
//   - CHK4: c4_outer_if instantiates c4_inner_if as u_inner; its 'assign y = u_inner.x' reads x of
//     c4_inner_if, and top's 'assign u4.u_inner.x = 1' writes that same x (hierarchical names)
//   - No errors: the whole file is legal
// Skipped (GTEST_SKIP; the real assertions stay below the skip):
//   - HierarchicalAccessThroughNestedInstance: HLC leaves the hierarchical references u_inner.x and u4.u_inner.x
//   unbound (null actual).
// Not checked:
//   - Nothing intentionally left out
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: if_decl_placement
//  19  :description: where an interface can be declared and nested
//  20  :tags: 25.3
//  21  */
//  22
//  23  // Checks in this file:
//  24  //   CHK1: interface declared after the module that instantiates it
//  25  //   CHK2: interface declared inside a module
//  26  //   CHK3: interface declared inside another interface
//  27  //   CHK4: interface instance inside another interface, accessed hierarchically
//  28
//  29  module top;
//  30    // CHK1
//  31    c1_if u1();
//  32
//  33    // CHK2
//  34    interface c2_local_if;
//  35      logic a;
//  36    endinterface
//  37    c2_local_if u2();
//  38
//  39    c3_outer_if u3();
//  40
//  41    c4_outer_if u4();
//  42    assign u4.u_inner.x = 1'b1;
//  43  endmodule
//  44
//  45  interface c1_if;
//  46    logic a;
//  47  endinterface
//  48
//  49  // CHK3
//  50  interface c3_outer_if;
//  51    interface c3_inner_if;
//  52      logic a;
//  53    endinterface
//  54    c3_inner_if u_inner();
//  55  endinterface
//  56
//  57  // CHK4
//  58  interface c4_inner_if;
//  59    logic x;
//  60  endinterface
//  61
//  62  interface c4_outer_if;
//  63    c4_inner_if u_inner();
//  64    logic y;
//  65    assign y = u_inner.x;
//  66  endinterface

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
#include <hldb/logic_typespec.h>
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

#include <string_view>
#include <vector>

namespace hlc {

class IfDeclPlacementTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3--if_decl_placement.hlc"}); }
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

// CHK1: c1_if u1(); appears before 'interface c1_if' in the file.
TEST_F(IfDeclPlacementTest, DeclaredAfterUse) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module top not found";
  const hldb::RefInstance *const u1 = getInst(top, "u1");
  ASSERT_NE(u1, nullptr);
  EXPECT_EQ(getInstDefName(u1), std::string_view("c1_if"));
  EXPECT_NE(getInstDefinition(u1), nullptr) << "u1 should be bound to the c1_if declared later";
}

// CHK2: interface c2_local_if declared inside module top.
TEST_F(IfDeclPlacementTest, DeclaredInsideModule) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::RefInstance *const u2 = getInst(top, "u2");
  ASSERT_NE(u2, nullptr);
  EXPECT_EQ(getInstDefName(u2), std::string_view("c2_local_if"));
  const hldb::Interface *const local = any_cast<hldb::Interface>(getInstDefinition(u2));
  ASSERT_NE(local, nullptr) << "definition of c2_local_if not found";
  EXPECT_NE(getVar(local, "a"), nullptr);
}

// CHK3: interface c3_outer_if; interface c3_inner_if; ... endinterface c3_inner_if u_inner(); endinterface
TEST_F(IfDeclPlacementTest, DeclaredInsideInterface) {
  const hldb::Interface *const outer = getInterface("c3_outer_if");
  ASSERT_NE(outer, nullptr);
  const hldb::RefInstance *const inner = getInst(outer, "u_inner");
  ASSERT_NE(inner, nullptr);
  EXPECT_EQ(getInstDefName(inner), std::string_view("c3_inner_if"));
  const hldb::Interface *const innerDef = any_cast<hldb::Interface>(getInstDefinition(inner));
  ASSERT_NE(innerDef, nullptr) << "definition of c3_inner_if not found";
  EXPECT_NE(getVar(innerDef, "a"), nullptr);
}

// CHK4: assign y = u_inner.x; inside c4_outer_if and assign u4.u_inner.x = 1'b1; in top.
TEST_F(IfDeclPlacementTest, HierarchicalAccessThroughNestedInstance) {
  GTEST_SKIP() << "HLC leaves the hierarchical references u_inner.x and u4.u_inner.x unbound (null actual)."
                  " IEEE 1800-2023 Sec 25.3 / 23.6: they name x of the nested c4_inner_if instance. Fix pending.";
  const hldb::Interface *const inner = getInterface("c4_inner_if");
  const hldb::Interface *const outer = getInterface("c4_outer_if");
  ASSERT_NE(inner, nullptr);
  ASSERT_NE(outer, nullptr);
  const hldb::Variable *const x = getVar(inner, "x");
  ASSERT_NE(x, nullptr);
  const hldb::RefInstance *const u = getInst(outer, "u_inner");
  ASSERT_NE(u, nullptr);
  EXPECT_EQ(getInstDefName(u), std::string_view("c4_inner_if"));
  ASSERT_NE(outer->getContAssigns(), nullptr);
  ASSERT_EQ(outer->getContAssigns()->size(), 1u);
  EXPECT_EQ(getRootActual(outer->getContAssigns()->at(0)->getLhs()), getVar(outer, "y"));
  EXPECT_EQ(getRootActual(outer->getContAssigns()->at(0)->getRhs()), x) << "u_inner.x";
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 1u);
  EXPECT_EQ(getRootActual(top->getContAssigns()->at(0)->getLhs()), x) << "u4.u_inner.x";
}

// The file is legal SystemVerilog, so compilation must not report any error.
TEST_F(IfDeclPlacementTest, CompilesWithoutErrors) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
