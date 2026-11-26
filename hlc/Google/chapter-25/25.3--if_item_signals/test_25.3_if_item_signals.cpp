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

// Tests for tests/Google/chapter-25/25.3--if_item_signals.sv
// Expected results follow IEEE 1800-2023 (25.3, 25.3.2, 25.3.3, 23.2.2, 23.3.2, 7.4.2,
// 37); they are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - CHK1: 'logic a, b, c;' -- three variables from one declaration (line 32)
//   - CHK2: a, b, c each on their own line (37, 38, 39)
//   - CHK3: addr/data are logic [7:0]; packed2d has packed dims [3:0][7:0]; mem is an unpacked array
//     [0:15] of logic [7:0]; queue_q is a queue (vpiQueueArray)
//   - CHK4: w1, w2 are wire nets and t is a tri [3:0] net (vpiNetType), with 'assign w1 = 1'
//   - CHK5: typedef req_t is a packed struct with members addr and wr; req has that type
//   - top writes u1.a, u2.a, u3.addr, u5.req.wr through the instances
//   - No errors: the whole file is legal
// Not checked:
//   - Nothing intentionally left out
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: if_item_signals
//  19  :description: data and net declarations inside an interface
//  20  :tags: 25.3
//  21  */
//  22
//  23  // Checks in this file:
//  24  //   CHK1: several variables on one line: logic a, b, c;
//  25  //   CHK2: each variable on its own line
//  26  //   CHK3: packed vectors, unpacked arrays and a queue
//  27  //   CHK4: wire and tri nets with a continuous assign
//  28  //   CHK5: typedef struct declared in the interface and used for a signal
//  29
//  30  // CHK1
//  31  interface c1_if;
//  32    logic a, b, c;
//  33  endinterface
//  34
//  35  // CHK2
//  36  interface c2_if;
//  37    logic a;
//  38    logic b;
//  39    logic c;
//  40  endinterface
//  41
//  42  // CHK3
//  43  interface c3_if;
//  44    logic [7:0] addr, data;
//  45    logic [3:0][7:0] packed2d;
//  46    logic [7:0] mem [0:15];
//  47    int queue_q[$];
//  48  endinterface
//  49
//  50  // CHK4
//  51  interface c4_if;
//  52    wire w1, w2;
//  53    tri [3:0] t;
//  54    assign w1 = 1'b1;
//  55  endinterface
//  56
//  57  // CHK5
//  58  interface c5_if;
//  59    typedef struct packed {
//  60      logic [7:0] addr;
//  61      logic       wr;
//  62    } req_t;
//  63    req_t req;
//  64  endinterface
//  65
//  66  module top;
//  67    c1_if u1();
//  68    c2_if u2();
//  69    c3_if u3();
//  70    c4_if u4();
//  71    c5_if u5();
//  72    assign u1.a = u1.b & u1.c;
//  73    assign u2.a = u2.b | u2.c;
//  74    assign u3.addr = u3.mem[3];
//  75    assign u5.req.wr = 1'b0;
//  76  endmodule

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
#include <hldb/struct.h>
#include <hldb/struct_typespec.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>
#include <vector>

namespace hlc {

class IfItemSignalsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3--if_item_signals.hlc"}); }
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

TEST_F(IfItemSignalsTest, OneLineAndSeparateLines) {
  const std::pair<const char *, uint32_t> c1[] = {{"a", 32u}, {"b", 32u}, {"c", 32u}};
  const std::pair<const char *, uint32_t> c2[] = {{"a", 37u}, {"b", 38u}, {"c", 39u}};
  for (const std::pair<const char *, uint32_t> &v : c1) {
    const hldb::Variable *const var = getVar(getInterface("c1_if"), v.first);
    ASSERT_NE(var, nullptr) << "c1_if." << v.first;
    EXPECT_EQ(var->getStartLine(), v.second) << "c1_if." << v.first;
  }
  for (const std::pair<const char *, uint32_t> &v : c2) {
    const hldb::Variable *const var = getVar(getInterface("c2_if"), v.first);
    ASSERT_NE(var, nullptr) << "c2_if." << v.first;
    EXPECT_EQ(var->getStartLine(), v.second) << "c2_if." << v.first;
  }
}

// CHK3: vectors, packed 2-D, unpacked array, queue.
TEST_F(IfItemSignalsTest, VectorsAndArrays) {
  const hldb::Interface *const c3 = getInterface("c3_if");
  ASSERT_NE(c3, nullptr);
  expectLogicRange(hldb::getTypespec(getVar(c3, "addr")), "7", "0", "addr");
  expectLogicRange(hldb::getTypespec(getVar(c3, "data")), "7", "0", "data");
  const hldb::LogicTypespec *const p2 = any_cast<hldb::LogicTypespec>(hldb::getTypespec(getVar(c3, "packed2d")));
  ASSERT_NE(p2, nullptr) << "packed2d is a packed logic array";
  ASSERT_NE(p2->getRanges(), nullptr);
  ASSERT_EQ(p2->getRanges()->size(), 2u);
  expectRange(p2->getRanges()->at(0), "3", "0", "packed2d outer");
  expectRange(p2->getRanges()->at(1), "7", "0", "packed2d inner");
  const hldb::ArrayTypespec *const mem = any_cast<hldb::ArrayTypespec>(hldb::getTypespec(getVar(c3, "mem")));
  ASSERT_NE(mem, nullptr) << "mem is an unpacked array";
  expectRange(mem->getRange(), "0", "15", "mem [0:15]");
  ASSERT_NE(mem->getElemTypespec(), nullptr);
  expectLogicRange(mem->getElemTypespec()->getActual(), "7", "0", "mem element");
  const hldb::ArrayTypespec *const q = any_cast<hldb::ArrayTypespec>(hldb::getTypespec(getVar(c3, "queue_q")));
  ASSERT_NE(q, nullptr) << "queue_q is a queue";
  EXPECT_EQ(q->getArrayType(), vpiQueueArray);
}

// CHK4: wire w1, w2; tri [3:0] t; assign w1 = 1'b1;
TEST_F(IfItemSignalsTest, Nets) {
  const hldb::Interface *const c4 = getInterface("c4_if");
  ASSERT_NE(c4, nullptr);
  for (const char *const name : {"w1", "w2"}) {
    const hldb::Net *const n = getNet(c4, name);
    ASSERT_NE(n, nullptr) << name;
    EXPECT_EQ(n->getNetType(), vpiWire) << name;
  }
  const hldb::Net *const tnet = getNet(c4, "t");
  ASSERT_NE(tnet, nullptr);
  EXPECT_EQ(tnet->getNetType(), vpiTri);
  expectLogicRange(hldb::getTypespec(tnet), "3", "0", "t");
  ASSERT_NE(c4->getContAssigns(), nullptr);
  ASSERT_EQ(c4->getContAssigns()->size(), 1u);
  EXPECT_EQ(getRootActual(c4->getContAssigns()->at(0)->getLhs()), getNet(c4, "w1"));
}

// CHK5: typedef struct packed { logic [7:0] addr; logic wr; } req_t; req_t req;
TEST_F(IfItemSignalsTest, StructTypedef) {
  const hldb::Interface *const c5 = getInterface("c5_if");
  ASSERT_NE(c5, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Typedef>("req_t", c5->getTypedefs()), nullptr);
  const hldb::StructTypespec *const st = any_cast<hldb::StructTypespec>(getResolvedTypespec(getVar(c5, "req")));
  ASSERT_NE(st, nullptr) << "req has the struct type req_t";
  ASSERT_NE(st->getStruct(), nullptr);
  EXPECT_TRUE(st->getStruct()->getPacked());
  ASSERT_NE(st->getStruct()->getMembers(), nullptr);
  ASSERT_EQ(st->getStruct()->getMembers()->size(), 2u);
  EXPECT_EQ(st->getStruct()->getMembers()->at(0)->getName(), std::string_view("addr"));
  EXPECT_EQ(st->getStruct()->getMembers()->at(1)->getName(), std::string_view("wr"));
}

// top: assign u1.a = ...; assign u2.a = ...; assign u3.addr = u3.mem[3]; assign u5.req.wr = 1'b0;
TEST_F(IfItemSignalsTest, AccessThroughInstances) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module top not found";
  ASSERT_NE(top->getContAssigns(), nullptr);
  ASSERT_EQ(top->getContAssigns()->size(), 4u);
  const std::pair<uint32_t, const hldb::Variable *> lhs[] = {{72u, getVar(getInterface("c1_if"), "a")},
                                                             {73u, getVar(getInterface("c2_if"), "a")},
                                                             {74u, getVar(getInterface("c3_if"), "addr")}};
  for (const std::pair<uint32_t, const hldb::Variable *> &l : lhs) {
    ASSERT_NE(l.second, nullptr) << "line " << l.first;
    const hldb::ContAssign *found = nullptr;
    for (const hldb::ContAssign *const ca : *top->getContAssigns()) {
      if (ca->getStartLine() == l.first) found = ca;
    }
    ASSERT_NE(found, nullptr) << "no continuous assign on line " << l.first;
    EXPECT_EQ(getRootActual(found->getLhs()), l.second) << "line " << l.first;
  }
  // assign u5.req.wr = 1'b0; -- the target is the member wr of the struct variable req.
  const hldb::ContAssign *wr = nullptr;
  for (const hldb::ContAssign *const ca : *top->getContAssigns()) {
    if (ca->getStartLine() == 75u) wr = ca;
  }
  ASSERT_NE(wr, nullptr) << "no continuous assign on line 75";
  const hldb::Any *const member = getRootActual(wr->getLhs());
  ASSERT_NE(member, nullptr) << "u5.req.wr is unresolved";
  EXPECT_EQ(member->getAnyType(), hldb::AnyType::TypespecMember) << "u5.req.wr is a struct member";
  EXPECT_EQ(member->getName(), std::string_view("wr"));
}

// The file is legal SystemVerilog, so compilation must not report any error.
TEST_F(IfItemSignalsTest, CompilesWithoutErrors) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
