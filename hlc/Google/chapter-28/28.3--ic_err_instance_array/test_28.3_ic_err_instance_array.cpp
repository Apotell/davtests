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

// Tests for tests/Google/chapter-28/28.3--ic_err_instance_array.sv
// Expected results follow IEEE 1800-2023 (28.3.6 arrays of instances, 6.6.8, 23.3.3.5,
// 23.3.3.7.1, 37.17); they are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - 28.3.6: 'If the instance-array port expression is an interconnect port or interconnect net
//     expression, the bit-length of the port expression shall be the same as the instance array
//     length.' -- each ERR is reported as CompInterconnectInstanceArrayWidth naming the net:
//   - ERR1 line 50: n1 (8 bits) on leaf2 a1[3:0] (4 instances) -- no part-select split for interconnect
//   - ERR2 line 52: n2 (3 bits) on leaf1 a2[3:0] (4 instances)
//   - ERR3 line 54: n3 (6 bits) on leaf3 a3[1:0] (2 instances)
//   - ERR4 line 41: port q (8 bits) on leaf2 u[3:0] (4 instances) inside fwd
//   - Model: n1..n4 are interconnect nets, fwd.q is an inout interconnect port, the arrays a1, a2,
//     a3 (top) and u (fwd) exist with their modules and ranges
// Not checked:
//   - The diagnostic code is HLC's choice; CompInterconnectInstanceArrayWidth is the code HLC
//     defines for exactly this rule
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: ic_err_instance_array
//  19  :description: interconnect expressions whose bit length differs from the instance array length
//  20  :should_fail_because: an interconnect port expression on an instance array must have bit length equal to the array length
//  21  :tags: 28.3
//  22  */
//  23
//  24  // Errors in this file:
//  25  //   ERR1: interconnect [7:0] connected to four 2-bit instances (no part-select split for interconnect)
//  26  //   ERR2: interconnect [2:0] connected to an array of four instances
//  27  //   ERR3: interconnect [5:0] connected to an array of two instances
//  28  //   ERR4: interconnect port of width 8 forwarded to an array of four instances
//  29
//  30  module leaf1(inout wire p);
//  31  endmodule
//  32
//  33  module leaf2(inout wire [1:0] p);
//  34  endmodule
//  35
//  36  module leaf3(inout wire [2:0] p);
//  37  endmodule
//  38
//  39  module fwd(inout interconnect [7:0] q);
//  40    // ERR4
//  41    leaf2 u[3:0](q);
//  42  endmodule
//  43
//  44  module top;
//  45    interconnect [7:0] n1;
//  46    interconnect [2:0] n2;
//  47    interconnect [5:0] n3;
//  48    interconnect [7:0] n4;
//  49    // ERR1
//  50    leaf2 a1[3:0](n1);
//  51    // ERR2
//  52    leaf1 a2[3:0](n2);
//  53    // ERR3
//  54    leaf3 a3[1:0](n3);
//  55    fwd u4(n4);
//  56  endmodule

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

class IcErrInstanceArrayTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "28.3--ic_err_instance_array.hlc"}); }
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

  // Array of instances 'name' in 'mod': a RefInstance whose typespec is an array typespec (the array
  // range) whose element is the module typespec (IEEE 1800-2023 Sec 28.3.6, 37.11 module array).
  static const hldb::ArrayTypespec *getArrayTypespec(const hldb::Module *mod, std::string_view name) {
    const hldb::RefInstance *const inst = getInst(mod, name);
    if ((inst == nullptr) || (inst->getTypespec() == nullptr)) return nullptr;
    return any_cast<hldb::ArrayTypespec>(inst->getTypespec()->getActual());
  }

  // The expression connected to port 'port' of the instance array 'name' in 'mod'.
  static const hldb::Any *getArrayConn(const hldb::Module *mod, std::string_view name, std::string_view port) {
    return getHighConn(getInst(mod, name), port);
  }

  // The instance array 'name' in 'mod' instantiates module 'def' with range [left:right].
  static void expectArrayRange(const hldb::Module *mod, std::string_view name, std::string_view def,
                               std::string_view left, std::string_view right) {
    const hldb::ArrayTypespec *const arr = getArrayTypespec(mod, name);
    ASSERT_NE(arr, nullptr) << name << " should be an array of instances";
    EXPECT_EQ(getInstDefName(getInst(mod, name)), def) << name;
    ASSERT_NE(arr->getRange(), nullptr) << name;
    const hldb::Constant *const l = any_cast<hldb::Constant>(arr->getRange()->getLeftExpr());
    const hldb::Constant *const r = any_cast<hldb::Constant>(arr->getRange()->getRightExpr());
    ASSERT_NE(l, nullptr) << name;
    ASSERT_NE(r, nullptr) << name;
    EXPECT_EQ(l->getDecompile(), left) << name;
    EXPECT_EQ(r->getDecompile(), right) << name;
  }
};

// ERR1: leaf2 a1[3:0](n1); with interconnect [7:0] n1
TEST_F(IcErrInstanceArrayTest, EightBitsOnFourInstancesIsRejected) {
  EXPECT_NE(findEvent(EventType::CompInterconnectInstanceArrayWidth, "n1", 50), nullptr)
      << "line 50: 8-bit interconnect on 4 instances (IEEE 1800-2023 Sec 28.3.6)";
}

// ERR2: leaf1 a2[3:0](n2); with interconnect [2:0] n2
TEST_F(IcErrInstanceArrayTest, ThreeBitsOnFourInstancesIsRejected) {
  EXPECT_NE(findEvent(EventType::CompInterconnectInstanceArrayWidth, "n2", 52), nullptr)
      << "line 52: 3-bit interconnect on 4 instances (IEEE 1800-2023 Sec 28.3.6)";
}

// ERR3: leaf3 a3[1:0](n3); with interconnect [5:0] n3
TEST_F(IcErrInstanceArrayTest, SixBitsOnTwoInstancesIsRejected) {
  EXPECT_NE(findEvent(EventType::CompInterconnectInstanceArrayWidth, "n3", 54), nullptr)
      << "line 54: 6-bit interconnect on 2 instances (IEEE 1800-2023 Sec 28.3.6)";
}

// ERR4: leaf2 u[3:0](q); with inout interconnect [7:0] q
TEST_F(IcErrInstanceArrayTest, InterconnectPortOnArrayIsRejected) {
  EXPECT_NE(findEvent(EventType::CompInterconnectInstanceArrayWidth, "q", 41), nullptr)
      << "line 41: 8-bit interconnect port on 4 instances (IEEE 1800-2023 Sec 28.3.6)";
}

// The illegal widths must not change how the declarations and arrays are modeled.
TEST_F(IcErrInstanceArrayTest, DeclarationsAndArrays) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module top not found";
  for (const char *const name : {"n1", "n2", "n3", "n4"}) {
    const hldb::Net *const net = getNet(top, name);
    ASSERT_NE(net, nullptr) << name;
    EXPECT_EQ(net->getNetType(), vpiInterconnect) << name;
  }
  const hldb::Module *const fwd = getModule("fwd");
  ASSERT_NE(fwd, nullptr);
  const hldb::Net *const q = getNet(fwd, "q");
  ASSERT_NE(q, nullptr);
  EXPECT_EQ(q->getNetType(), vpiInterconnect);
  expectArrayRange(top, "a1", "leaf2", "3", "0");
  expectArrayRange(top, "a2", "leaf1", "3", "0");
  expectArrayRange(top, "a3", "leaf3", "1", "0");
  expectArrayRange(fwd, "u", "leaf2", "3", "0");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
