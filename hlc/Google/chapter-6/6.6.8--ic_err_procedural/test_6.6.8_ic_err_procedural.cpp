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

// Tests for tests/Google/chapter-6/6.6.8--ic_err_procedural.sv
// Expected results follow IEEE 1800-2023 (6.6.8 generic interconnect, 6.7.1, 23.3.3.7.1,
// 37.17, 37.24); they are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - ERR1 line 43: $display(w1) -> CompIllegalInterconnectUse naming w1
//   - ERR2 line 45: @(w2) event control -> reported for w2
//   - ERR3 line 47: if (w3) condition -> reported for w3
//   - ERR4 line 49: @(posedge w4) -> reported for w4
//   - ERR5 line 51: id(w5) function argument -> reported for w5
//   - w1..w5 are still modeled as vpiInterconnect nets
// Not checked:
//   - The diagnostic code is HLC's choice; CompIllegalInterconnectUse is the code HLC defines for 6.6.8
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: ic_err_procedural
//  19  :description: interconnect nets used in procedural contexts
//  20  :should_fail_because: an interconnect net shall not be used in any procedural context
//  21  :tags: 6.6.8
//  22  */
//  23
//  24  // Errors in this file:
//  25  //   ERR1: $display of an interconnect net in an initial block (LRM)
//  26  //   ERR2: interconnect in an always event control @(w)
//  27  //   ERR3: interconnect as an if condition in a procedural block
//  28  //   ERR4: interconnect as an edge event @(posedge w)
//  29  //   ERR5: interconnect passed as a function argument in a procedural block
//  30
//  31  module leaf(inout wire p);
//  32  endmodule
//  33
//  34  module top;
//  35    interconnect w1, w2;
//  36    interconnect w3, w4;
//  37    interconnect w5;
//  38    logic r;
//  39    function automatic logic id(logic v);
//  40      return v;
//  41    endfunction
//  42    // ERR1
//  43    initial $display(w1);
//  44    // ERR2
//  45    always @(w2) r = 1'b0;
//  46    // ERR3
//  47    initial if (w3) r = 1'b1;
//  48    // ERR4
//  49    always @(posedge w4) r = 1'b0;
//  50    // ERR5
//  51    initial r = id(w5);
//  52    leaf l1(w1);
//  53    leaf l2(w2);
//  54    leaf l3(w3);
//  55    leaf l4(w4);
//  56    leaf l5(w5);
//  57  endmodule

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
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
#include <hldb/ref_instance.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/select.h>
#include <hldb/sv_vpi_user.h>
#include <hldb/vpi_user.h>

#include <string_view>
#include <vector>

namespace hlc {

class IcErrProceduralTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.6.8--ic_err_procedural.hlc"}); }
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

// ERR1: initial $display(w1);
TEST_F(IcErrProceduralTest, DisplayOfInterconnectIsRejected) {
  EXPECT_NE(findEvent(EventType::CompIllegalInterconnectUse, "w1", 43), nullptr)
      << "interconnect 'w1' at line 43 must be reported (IEEE 1800-2023 Sec 6.6.8)";
}

// ERR2: always @(w2) ...
TEST_F(IcErrProceduralTest, EventControlOnInterconnectIsRejected) {
  EXPECT_NE(findEvent(EventType::CompIllegalInterconnectUse, "w2", 45), nullptr)
      << "interconnect 'w2' at line 45 must be reported (IEEE 1800-2023 Sec 6.6.8)";
}

// ERR3: initial if (w3) ...
TEST_F(IcErrProceduralTest, IfConditionOnInterconnectIsRejected) {
  EXPECT_NE(findEvent(EventType::CompIllegalInterconnectUse, "w3", 47), nullptr)
      << "interconnect 'w3' at line 47 must be reported (IEEE 1800-2023 Sec 6.6.8)";
}

// ERR4: always @(posedge w4) ...
TEST_F(IcErrProceduralTest, EdgeEventOnInterconnectIsRejected) {
  EXPECT_NE(findEvent(EventType::CompIllegalInterconnectUse, "w4", 49), nullptr)
      << "interconnect 'w4' at line 49 must be reported (IEEE 1800-2023 Sec 6.6.8)";
}

// ERR5: initial r = id(w5);
TEST_F(IcErrProceduralTest, FunctionArgumentInterconnectIsRejected) {
  EXPECT_NE(findEvent(EventType::CompIllegalInterconnectUse, "w5", 51), nullptr)
      << "interconnect 'w5' at line 51 must be reported (IEEE 1800-2023 Sec 6.6.8)";
}

// The illegal uses must not change how the declarations are modeled.
TEST_F(IcErrProceduralTest, DeclarationsAreInterconnect) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  for (const char *const name : {"w1", "w2", "w3", "w4", "w5"}) {
    const hldb::Net *const net = getNet(top, name);
    ASSERT_NE(net, nullptr) << name;
    EXPECT_EQ(net->getNetType(), vpiInterconnect) << name;
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
