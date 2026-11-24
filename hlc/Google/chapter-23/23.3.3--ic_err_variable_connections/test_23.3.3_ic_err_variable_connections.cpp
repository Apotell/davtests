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

// Tests for tests/Google/chapter-23/23.3.3--ic_err_variable_connections.sv
// Expected results follow IEEE 1800-2023 (6.6.8, 23.2.2, 23.3.3, 23.3.3.4, 23.3.3.7,
// 23.3.3.7.1, 23.3.3.7.2, 37.17); they are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - ERR1 line 53: logic variable v on an interconnect port -> CompIllegalVariableOnInterconnect
//     (23.3.3.2: it shall be illegal to connect a port variable to an interconnect port or net)
//   - ERR2 line 55: int variable iv on an inout interconnect port -> CompIllegalVariableOnInterconnect
//     or CompIllegalVariableOnInoutPort (an inout port also needs a net, 23.3.3.3)
//   - ERR3 line 57: interconnect n3 on the output variable port o -> CompIllegalVariableOnInterconnect
//   - ERR4 line 59: interconnect n4 on 'output var logic o' -> CompIllegalVariableOnInterconnect
//   - ERR5 line 61: interconnect n5 on a ref port -> CompIllegalVariableOnInterconnect or
//     CompIllegalNetOnRefPort (a ref port shall be connected to a variable, 23.3.3.2)
//   - Model: v and iv are variables, n3..n5 are interconnect nets; the port kinds are as declared
// Not checked:
//   - The diagnostic code is HLC's choice; where two rules are broken, either code is accepted
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: ic_err_variable_connections
//  19  :description: variables connected to interconnect ports or interconnect nets
//  20  :should_fail_because: it shall be illegal to connect a port variable to an interconnect port or interconnect net
//  21  :tags: 23.3.3
//  22  */
//  23
//  24  // Errors in this file:
//  25  //   ERR1: logic variable connected to an input interconnect port
//  26  //   ERR2: int variable connected to an inout interconnect port
//  27  //   ERR3: interconnect net connected to an output logic variable port
//  28  //   ERR4: interconnect net connected to an output var logic port
//  29  //   ERR5: interconnect net connected to a ref port
//  30
//  31  module ic_in(input interconnect p);
//  32  endmodule
//  33
//  34  module ic_io(inout interconnect p);
//  35  endmodule
//  36
//  37  module var_out(output logic o);
//  38    always_comb o = 1'b0;
//  39  endmodule
//  40
//  41  module var_out2(output var logic o);
//  42  endmodule
//  43
//  44  module ref_port(ref logic r);
//  45  endmodule
//  46
//  47  module top;
//  48    logic v;
//  49    int   iv;
//  50    interconnect n3, n4;
//  51    interconnect n5;
//  52    // ERR1
//  53    ic_in u1(v);
//  54    // ERR2
//  55    ic_io u2(iv);
//  56    // ERR3
//  57    var_out u3(n3);
//  58    // ERR4
//  59    var_out2 u4(n4);
//  60    // ERR5
//  61    ref_port u5(n5);
//  62  endmodule

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
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>
#include <vector>

namespace hlc {

class IcErrVariableConnectionsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "23.3.3--ic_err_variable_connections.hlc"}); }
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

  // Port 'name' of 'mod' has direction 'dir' and its internal net (lowconn) is an interconnect.
  static void expectInterconnectPort(const hldb::Module *mod, std::string_view name, int32_t dir) {
    ASSERT_NE(mod, nullptr);
    const hldb::Port *const port = hldb::findByName<hldb::Port>(name, mod->getPorts());
    ASSERT_NE(port, nullptr) << "port " << name << " not found in " << mod->getDefName();
    EXPECT_EQ(port->getDirection(), dir) << name;
    const hldb::Net *const net = getNet(mod, name);
    ASSERT_NE(net, nullptr) << "no net for port " << name;
    EXPECT_EQ(net->getNetType(), vpiInterconnect) << name << " is an interconnect port";
    EXPECT_EQ(getRootActual(port->getLowConn()), net) << name << ": the port's internal connection is its net";
  }

  static const hldb::Module *getModule(std::string_view name) {
    return hldb::findByDefName<hldb::Module>(name, m_design->getAllModules());
  }
};

// ERR1: ic_in u1(v);
TEST_F(IcErrVariableConnectionsTest, LogicVariableOnInterconnectPortIsRejected) {
  EXPECT_FALSE(findEvents(EventType::CompIllegalVariableOnInterconnect, 53).empty())
      << "line 53: a variable cannot connect to an interconnect port";
}

// ERR2: ic_io u2(iv);
// Either diagnostic names a rule the line breaks, so either one is accepted.
TEST_F(IcErrVariableConnectionsTest, IntVariableOnInoutInterconnectPortIsRejected) {
  const bool first = !findEvents(EventType::CompIllegalVariableOnInterconnect, 55).empty();
  const bool second = !findEvents(EventType::CompIllegalVariableOnInoutPort, 55).empty();
  EXPECT_TRUE(first || second) << "line 55: a variable cannot connect to an inout interconnect port";
}

// ERR3: var_out u3(n3);
TEST_F(IcErrVariableConnectionsTest, InterconnectOnOutputVariablePortIsRejected) {
  EXPECT_FALSE(findEvents(EventType::CompIllegalVariableOnInterconnect, 57).empty())
      << "line 57: an interconnect cannot connect to a variable port";
}

// ERR4: var_out2 u4(n4);
TEST_F(IcErrVariableConnectionsTest, InterconnectOnVarOutputPortIsRejected) {
  EXPECT_FALSE(findEvents(EventType::CompIllegalVariableOnInterconnect, 59).empty())
      << "line 59: an interconnect cannot connect to a variable port";
}

// ERR5: ref_port u5(n5);
// Either diagnostic names a rule the line breaks, so either one is accepted.
TEST_F(IcErrVariableConnectionsTest, InterconnectOnRefPortIsRejected) {
  const bool first = !findEvents(EventType::CompIllegalVariableOnInterconnect, 61).empty();
  const bool second = !findEvents(EventType::CompIllegalNetOnRefPort, 61).empty();
  EXPECT_TRUE(first || second) << "line 61: an interconnect net cannot connect to a ref port";
}

// The objects on both sides keep their declared kinds.
TEST_F(IcErrVariableConnectionsTest, DeclaredKinds) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module top not found";
  EXPECT_NE(hldb::findByName<hldb::Variable>("v", top->getVariables()), nullptr) << "v is a variable";
  EXPECT_NE(hldb::findByName<hldb::Variable>("iv", top->getVariables()), nullptr) << "iv is a variable";
  for (const char *const name : {"n3", "n4", "n5"}) {
    const hldb::Net *const net = getNet(top, name);
    ASSERT_NE(net, nullptr) << name;
    EXPECT_EQ(net->getNetType(), vpiInterconnect) << name;
  }
  expectInterconnectPort(getModule("ic_in"), "p", vpiInput);
  expectInterconnectPort(getModule("ic_io"), "p", vpiInout);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
