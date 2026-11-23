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

// Tests for tests/Google/chapter-23/23.3.3.4--if_err_module_port_connections.sv
// Expected results follow IEEE 1800-2023 (6.6.8, 23.2.2, 23.3.3, 23.3.3.4, 23.3.3.7,
// 23.3.3.7.1, 23.3.3.7.2, 37.17); they are not taken from any HLC log. HLC builds the definition model
// (getAllModules(), RefInstance for instances), so the tests navigate module definitions.
//
// What to check: see the CHK/ERR list in the source below.
// Checked:
//   - ERR1 line 45: sub u1(); leaves the interface port b unconnected -> CompIllegalInterfacePortConnection
//     or CompMissingArgument ('Missing argument for formal b')
//     (23.3.3.4: an interface port cannot be left unconnected)
//   - ERR2 line 47: sub u2(.b()); explicit empty connection -> CompIllegalInterfacePortConnection or
//     CompMissingArgument
//   - ERR3 line 49: .b(ib) connects an if_b instance to an if_a port -> CompIllegalInterfacePortConnection
//     (23.3.3.4: a named interface port shall be connected to an instance of the identical type)
//   - ERR4 line 51: .b(not_an_if) connects a logic variable -> CompIllegalInterfacePortConnection
//   - Model: sub has one port b; the four instances of sub exist in top
// Not checked:
//   - The diagnostic code is HLC's choice; CompIllegalInterfacePortConnection is the code HLC defines
//     for illegal interface port connections
//
// Source under test (line numbers match the .sv file):
//  17  /*
//  18  :name: if_err_module_port_connections
//  19  :description: illegal connections to module interface ports
//  20  :should_fail_because: an interface port shall always be connected to an interface instance of the identical type
//  21  :tags: 23.3.3.4
//  22  */
//  23
//  24  // Errors in this file:
//  25  //   ERR1: interface port left unconnected
//  26  //   ERR2: interface port connected with empty .b()
//  27  //   ERR3: named interface port connected to a different interface type
//  28  //   ERR4: interface port connected to an ordinary logic signal
//  29
//  30  interface if_a;
//  31    logic x;
//  32  endinterface
//  33
//  34  interface if_b;
//  35    logic x;
//  36  endinterface
//  37
//  38  module sub(if_a b);
//  39  endmodule
//  40
//  41  module top;
//  42    if_b ib();
//  43    logic not_an_if;
//  44    // ERR1
//  45    sub u1();
//  46    // ERR2
//  47    sub u2(.b());
//  48    // ERR3
//  49    sub u3(.b(ib));
//  50    // ERR4
//  51    sub u4(.b(not_an_if));
//  52  endmodule

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

class IfErrModulePortConnectionsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "23.3.3.4--if_err_module_port_connections.hlc"}); }
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

// ERR1: sub u1();
// Either diagnostic names a rule the line breaks, so either one is accepted.
TEST_F(IfErrModulePortConnectionsTest, UnconnectedInterfacePortIsRejected) {
  const bool first = !findEvents(EventType::CompIllegalInterfacePortConnection, 45).empty();
  const bool second = !findEvents(EventType::CompMissingArgument, 45).empty();
  EXPECT_TRUE(first || second) << "line 45: an interface port cannot be left unconnected";
}

// ERR2: sub u2(.b());
// Either diagnostic names a rule the line breaks, so either one is accepted.
TEST_F(IfErrModulePortConnectionsTest, EmptyInterfacePortConnectionIsRejected) {
  const bool first = !findEvents(EventType::CompIllegalInterfacePortConnection, 47).empty();
  const bool second = !findEvents(EventType::CompMissingArgument, 47).empty();
  EXPECT_TRUE(first || second) << "line 47: an interface port cannot be connected to nothing";
}

// ERR3: sub u3(.b(ib)); with if_b ib
TEST_F(IfErrModulePortConnectionsTest, WrongInterfaceTypeIsRejected) {
  EXPECT_FALSE(findEvents(EventType::CompIllegalInterfacePortConnection, 49).empty())
      << "line 49: if_a port needs an if_a instance";
}

// ERR4: sub u4(.b(not_an_if));
TEST_F(IfErrModulePortConnectionsTest, SignalOnInterfacePortIsRejected) {
  EXPECT_FALSE(findEvents(EventType::CompIllegalInterfacePortConnection, 51).empty())
      << "line 51: an interface port needs an interface instance";
}

TEST_F(IfErrModulePortConnectionsTest, InstancesExist) {
  const hldb::Module *const sub = getModule("sub");
  ASSERT_NE(sub, nullptr);
  ASSERT_NE(sub->getPorts(), nullptr);
  ASSERT_EQ(sub->getPorts()->size(), 1u);
  EXPECT_EQ(sub->getPorts()->at(0)->getName(), std::string_view("b"));
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr) << "module top not found";
  for (const char *const name : {"u1", "u2", "u3", "u4"}) {
    const hldb::RefInstance *const inst = getInst(top, name);
    ASSERT_NE(inst, nullptr) << name;
    EXPECT_EQ(getInstDefName(inst), std::string_view("sub")) << name;
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
