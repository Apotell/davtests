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

// Tests for tests/Google/chapter-6/6.6.8--ic_net_alias.sv
// Expected results follow IEEE 1800-2023 (6.6.8 generic interconnect, 6.7.1, 23.3.3.7.1,
// 37.17, 37.24); they are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - top has 5 alias statements (lines 45, 47, 49, 51, 53)
//   - CHK1: alias a = b -- lhs a, one rhs b, both interconnect
//   - CHK2: alias v1 = v2 -- packed vectors, both interconnect
//   - CHK3: alias v3 = {hi, lo} -- rhs is a concatenation of hi and lo
//   - CHK4: alias v1[1:0] = lo -- lhs is the part select [1:0] of v1
//   - CHK5: alias x = y = z -- lhs x and two rhs y, z
//   - Every net named in the aliases is vpiInterconnect (6.6.8 allows alias only between interconnects)
//   - No errors: all aliases here are legal
// Not checked:
//   - Which nets end up merged into one simulated net after aliasing (no HLDB accessor for alias groups)
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: ic_net_alias
//  19  :description: net_alias statements whose net_lvalues are all interconnect nets
//  20  :tags: 6.6.8
//  21  */
//  22
//  23  // Checks in this file:
//  24  //   CHK1: alias between two scalar interconnect nets
//  25  //   CHK2: alias between two packed interconnect vectors
//  26  //   CHK3: alias between an interconnect vector and a concatenation of interconnects
//  27  //   CHK4: alias of part selects of interconnect vectors
//  28  //   CHK5: three interconnect nets in one alias statement
//  29
//  30  module leaf1(inout wire p);
//  31  endmodule
//  32
//  33  module leaf4(inout wire [3:0] p);
//  34  endmodule
//  35
//  36  module top;
//  37    interconnect a, b;
//  38    interconnect x, y;
//  39    interconnect z;
//  40    interconnect [3:0] v1, v2;
//  41    interconnect [3:0] v3;
//  42    interconnect [1:0] hi, lo;
//  43
//  44    // CHK1
//  45    alias a = b;
//  46    // CHK2
//  47    alias v1 = v2;
//  48    // CHK3
//  49    alias v3 = {hi, lo};
//  50    // CHK4
//  51    alias v1[1:0] = lo;
//  52    // CHK5
//  53    alias x = y = z;
//  54
//  55    leaf1 l_a(a);
//  56    leaf4 l_v1(v1);
//  57    leaf4 l_v3(v3);
//  58    leaf1 l_x(x);
//  59  endmodule

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/alias_stmt.h>
#include <hldb/begin.h>
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
#include <hldb/module.h>
#include <hldb/module_typespec.h>
#include <hldb/named_argument.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/part_select.h>
#include <hldb/port.h>
#include <hldb/range.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/select.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

#include <string_view>
#include <vector>

namespace hlc {

class IcNetAliasTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.6.8--ic_net_alias.hlc"}); }
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
  static const hldb::Typespec *getInstTypespec(const hldb::RefInstance *inst) {
    return ((inst == nullptr) || (inst->getTypespec() == nullptr)) ? nullptr : inst->getTypespec()->getActual();
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

  // The expression the parent connects to port 'port' of 'inst' (vpiHighConn). All connections in
  // these sources are positional, so the argument position is the port's position in the definition.
  static const hldb::Any *getHighConn(const hldb::RefInstance *inst, std::string_view port) {
    if ((inst == nullptr) || (inst->getArguments() == nullptr)) return nullptr;
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
};

// Alias statements have no name; each is found by the line it starts on.
static const hldb::Alias *getAliasAt(const hldb::Module *top, uint32_t line) {
  if (top == nullptr || top->getAliases() == nullptr) return nullptr;
  for (const hldb::Alias *const a : *top->getAliases()) {
    if (a->getStartLine() == line) return a;
  }
  return nullptr;
}

static void expectRefTo(const hldb::Any *expr, const hldb::Net *net, std::string_view what) {
  const hldb::RefObj *const ref = any_cast<hldb::RefObj>(expr);
  ASSERT_NE(ref, nullptr) << what << " should be a plain net reference";
  ASSERT_NE(net, nullptr) << what;
  EXPECT_EQ(ref->getActual(), net) << what;
  EXPECT_EQ(net->getNetType(), vpiInterconnect) << what;
}

TEST_F(IcNetAliasTest, FiveAliasStatements) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "top not elaborated";
  ASSERT_NE(top->getAliases(), nullptr);
  EXPECT_EQ(top->getAliases()->size(), 5u);
}

// CHK1: alias a = b;
TEST_F(IcNetAliasTest, ScalarAlias) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Alias *const al = getAliasAt(top, 45);
  ASSERT_NE(al, nullptr);
  expectRefTo(al->getLhs(), getNet(top, "a"), "lhs a");
  ASSERT_NE(al->getRhs(), nullptr);
  ASSERT_EQ(al->getRhs()->size(), 1u);
  expectRefTo(al->getRhs()->at(0), getNet(top, "b"), "rhs b");
}

// CHK2: alias v1 = v2;
TEST_F(IcNetAliasTest, VectorAlias) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Alias *const al = getAliasAt(top, 47);
  ASSERT_NE(al, nullptr);
  expectRefTo(al->getLhs(), getNet(top, "v1"), "lhs v1");
  ASSERT_NE(al->getRhs(), nullptr);
  ASSERT_EQ(al->getRhs()->size(), 1u);
  expectRefTo(al->getRhs()->at(0), getNet(top, "v2"), "rhs v2");
}

// CHK3: alias v3 = {hi, lo};
TEST_F(IcNetAliasTest, ConcatenationAlias) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Alias *const al = getAliasAt(top, 49);
  ASSERT_NE(al, nullptr);
  expectRefTo(al->getLhs(), getNet(top, "v3"), "lhs v3");
  ASSERT_NE(al->getRhs(), nullptr);
  ASSERT_EQ(al->getRhs()->size(), 1u);
  const hldb::Operation *const cat = any_cast<hldb::Operation>(al->getRhs()->at(0));
  ASSERT_NE(cat, nullptr) << "rhs should be a concatenation";
  EXPECT_EQ(cat->getOpType(), vpiConcatOp);
  ASSERT_NE(cat->getOperands(), nullptr);
  ASSERT_EQ(cat->getOperands()->size(), 2u);
  expectRefTo(cat->getOperands()->at(0), getNet(top, "hi"), "first operand hi");
  expectRefTo(cat->getOperands()->at(1), getNet(top, "lo"), "second operand lo");
}

// CHK4: alias v1[1:0] = lo;
TEST_F(IcNetAliasTest, PartSelectAlias) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Alias *const al = getAliasAt(top, 51);
  ASSERT_NE(al, nullptr);
  const hldb::PartSelect *const ps = any_cast<hldb::PartSelect>(al->getLhs());
  ASSERT_NE(ps, nullptr) << "lhs should be the part select v1[1:0]";
  expectRefTo(ps->getPrefix(), getNet(top, "v1"), "part-select prefix v1");
  ASSERT_NE(ps->getRange(), nullptr);
  const hldb::Constant *const l = any_cast<hldb::Constant>(ps->getRange()->getLeftExpr());
  const hldb::Constant *const r = any_cast<hldb::Constant>(ps->getRange()->getRightExpr());
  ASSERT_NE(l, nullptr);
  ASSERT_NE(r, nullptr);
  EXPECT_EQ(l->getDecompile(), std::string_view("1"));
  EXPECT_EQ(r->getDecompile(), std::string_view("0"));
  ASSERT_NE(al->getRhs(), nullptr);
  ASSERT_EQ(al->getRhs()->size(), 1u);
  expectRefTo(al->getRhs()->at(0), getNet(top, "lo"), "rhs lo");
}

// CHK5: alias x = y = z; -- one lhs, two rhs in order.
TEST_F(IcNetAliasTest, ThreeNetAlias) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Alias *const al = getAliasAt(top, 53);
  ASSERT_NE(al, nullptr);
  expectRefTo(al->getLhs(), getNet(top, "x"), "lhs x");
  ASSERT_NE(al->getRhs(), nullptr);
  ASSERT_EQ(al->getRhs()->size(), 2u);
  expectRefTo(al->getRhs()->at(0), getNet(top, "y"), "rhs y");
  expectRefTo(al->getRhs()->at(1), getNet(top, "z"), "rhs z");
}

// The file is legal SystemVerilog, so compilation must not report any error.
TEST_F(IcNetAliasTest, CompilesWithoutErrors) {
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
