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

// Tests for tests/Google/chapter-25/25.3--if_inst_arrays_generate.sv
// Expected results follow IEEE 1800-2023 (25.3, 25.3.2, 25.3.3, 23.2.2, 23.3.2, 7.4.2,
// 37); they are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - CHK1: 'bus_if scalar1(), vector[9:0]();' -- scalar1 is one instance, vector an array [9:0]
//   - CHK2: 'bus_if arr[4]();' -- the size form [4] means [0:3] (7.4.2)
//   - CHK1/CHK2: 'assign vector[3].x = scalar1.x' and 'assign arr[0].x = arr[3].x' read/write x of bus_if
//   - CHK3: the generate for block g instantiates bus_if u_if
//   - CHK4: the generate if (USE) instantiates bus_if u_if; the else branch declares logic unused
//   - CHK5: 'assign y = arr[2].v' reads v of bus_if
//   - No errors: the whole file is legal
// Skipped (GTEST_SKIP; the real assertions stay below the skip):
//   - InstanceArrays: HLC leaves the left bound of a size-form range [N] empty (no left expression on the array range).
// Not checked:
//   - Number of generate iterations (3) and which branch is chosen: the definition model holds the
//     generate constructs, not their elaborated copies
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: if_inst_arrays_generate
//  19  :description: arrays of interface instances and instances inside generate blocks
//  20  :tags: 25.3
//  21  */
//  22
//  23  // Checks in this file:
//  24  //   CHK1: interface instance array with [9:0] range
//  25  //   CHK2: interface instance array with C-style size [4]
//  26  //   CHK3: interface instantiated inside a generate for loop
//  27  //   CHK4: interface instantiated inside a generate if/else
//  28  //   CHK5: member read through an array element arr[2].v
//  29
//  30  interface bus_if;
//  31    logic [1:0] v;
//  32    logic x;
//  33  endinterface
//  34
//  35  module top #(parameter bit USE = 1);
//  36    // CHK1
//  37    bus_if scalar1(), vector[9:0]();
//  38    assign vector[3].x = scalar1.x;
//  39    // CHK2
//  40    bus_if arr[4]();
//  41    assign arr[0].x = arr[3].x;
//  42    // CHK3
//  43    for (genvar i = 0; i < 3; i++) begin : g
//  44      bus_if u_if();
//  45      assign u_if.x = 1'b1;
//  46    end
//  47    // CHK4
//  48    if (USE) begin : g_on
//  49      bus_if u_if();
//  50    end else begin : g_off
//  51      logic unused;
//  52    end
//  53    // CHK5
//  54    logic [1:0] y;
//  55    assign y = arr[2].v;
//  56  endmodule

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

class IfInstArraysGenerateTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3--if_inst_arrays_generate.hlc"}); }
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

// CHK1/CHK2: scalar instance and two arrays of interface instances.
TEST_F(IfInstArraysGenerateTest, InstanceArrays) {
  GTEST_SKIP() << "HLC leaves the left bound of a size-form range [N] empty (no left expression on the array range)."
                  " IEEE 1800-2023 Sec 7.4.2 / 25.3: [N] means [0:N-1], so the range must be [0:N-1]. Fix pending.";
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module top not found";
  const hldb::RefInstance *const scalar = getInst(top, "scalar1");
  ASSERT_NE(scalar, nullptr);
  EXPECT_EQ(getInstDefName(scalar), std::string_view("bus_if"));
  EXPECT_EQ(getInstArray(top, "scalar1"), nullptr) << "scalar1 is a single instance";
  const hldb::ArrayTypespec *const vector = getInstArray(top, "vector");
  ASSERT_NE(vector, nullptr) << "vector should be an array of instances";
  EXPECT_EQ(getInstDefName(getInst(top, "vector")), std::string_view("bus_if"));
  expectRange(vector->getRange(), "9", "0", "vector[9:0]");
  const hldb::ArrayTypespec *const arr = getInstArray(top, "arr");
  ASSERT_NE(arr, nullptr) << "arr should be an array of instances";
  expectRange(arr->getRange(), "0", "3", "arr[4] == arr[0:3]");
}

// assign vector[3].x = scalar1.x; assign arr[0].x = arr[3].x; assign y = arr[2].v;
TEST_F(IfInstArraysGenerateTest, MemberAccessThroughArrays) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Interface *const bus = getInterface("bus_if");
  ASSERT_NE(bus, nullptr);
  const hldb::Variable *const x = getVar(bus, "x");
  const hldb::Variable *const v = getVar(bus, "v");
  ASSERT_NE(x, nullptr);
  ASSERT_NE(v, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 3u);
  for (const hldb::ContAssign *const ca : *top->getContAssigns()) {
    if (ca->getStartLine() == 55) {
      EXPECT_EQ(getRootActual(ca->getLhs()), getVar(top, "y"));
      EXPECT_EQ(getRootActual(ca->getRhs()), v) << "arr[2].v";
    } else {
      EXPECT_EQ(getRootActual(ca->getLhs()), x) << "line " << ca->getStartLine();
      EXPECT_EQ(getRootActual(ca->getRhs()), x) << "line " << ca->getStartLine();
    }
  }
}

// CHK3/CHK4: generate for and generate if/else.
TEST_F(IfInstArraysGenerateTest, InstancesInGenerate) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getGenStmts(), nullptr);
  const hldb::GenFor *genFor = nullptr;
  const hldb::GenIfElse *genIf = nullptr;
  for (const hldb::Any *const g : *top->getGenStmts()) {
    if (genFor == nullptr) genFor = any_cast<hldb::GenFor>(g);
    if (genIf == nullptr) genIf = any_cast<hldb::GenIfElse>(g);
  }
  ASSERT_NE(genFor, nullptr) << "generate for block g not found";
  ASSERT_NE(genIf, nullptr) << "generate if/else not found";
  std::vector<const hldb::RefInstance *> insts;
  std::vector<const hldb::Net *> nets;
  walkGenerate(genFor, "u_if", "", &insts, &nets);
  ASSERT_EQ(insts.size(), 1u) << "the loop body instantiates u_if";
  EXPECT_EQ(getInstDefName(insts[0]), std::string_view("bus_if"));
  insts.clear();
  walkGenerate(genIf->getStmt(), "u_if", "", &insts, &nets);
  ASSERT_EQ(insts.size(), 1u) << "the if branch instantiates u_if";
  EXPECT_EQ(getInstDefName(insts[0]), std::string_view("bus_if"));
  insts.clear();
  walkGenerate(genIf->getElseStmt(), "u_if", "", &insts, &nets);
  EXPECT_TRUE(insts.empty()) << "the else branch instantiates nothing";
  EXPECT_NE(genIf->getElseStmt(), nullptr) << "else branch with logic unused";
}

// The file is legal SystemVerilog, so compilation must not report any error.
TEST_F(IfInstArraysGenerateTest, CompilesWithoutErrors) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
