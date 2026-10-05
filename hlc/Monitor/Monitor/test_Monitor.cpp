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

// Tests for tests/Monitor/design.sv + testbench.sv (tags: Monitor)
// The .hlc compiles these together with UVM, SVUnit and SVAUnit; only the
// design/testbench constructs are tested here.
//
// design.sv:
//   interface my_interface(input wire clock, input wire select,
//                          input wire [3:0] data);
//     clocking cb @(posedge clock);
//       input select, data;
//     endclocking
//   endinterface
//
//   class my_monitor extends uvm_monitor;
//     virtual my_interface _if;
//     uvm_analysis_port #(int) m_ap;
//     `uvm_component_utils(my_monitor)
//     function new (string name, uvm_component parent = null); ...
//     task run_phase(uvm_phase phase);
//       forever begin
//         @_if.cb;                                   // line 32
//         if (_if.cb.select) begin                   // line 33
//           int pkt = _if.cb.data;                   // line 34
//           m_ap.write(pkt);
//         end
//       end
//     endtask
//   endclass
//
// testbench.sv (excerpt):
//   class mock_scoreboard extends uvm_scoreboard;
//     int write_count; int last_pkt;
//     uvm_analysis_imp #(int, mock_scoreboard) m_imp;
//     function void write(int pkt); ...
//   endclass
//   module my_monitor_unit_test;
//     logic clock; logic select; logic [3:0] data;
//     my_interface my_interface(.*);
//     ...
//   endmodule
//
// What is checked (IEEE 1800-2023):
//   - 25.4: my_interface has three input ports clock, select, data.
//   - 14.3: my_interface declares clocking block "cb" whose clocking event is
//     @(posedge clock) (vpiPosedgeOp) and which has two clocking input
//     signals select and data (vpiInput).
//   - 8.13: my_monitor extends uvm_monitor; mock_scoreboard extends
//     uvm_scoreboard (the base class resolves to the UVM ClassDefn).
//   - 25.9: "virtual my_interface _if;" is a class property whose type is a
//     virtual interface of my_interface.
//   - 25.9 / 14.3: through a virtual interface, the members of the
//     interface, including its clocking blocks and their clockvars, are
//     accessible: "@_if.cb", "_if.cb.select" and "_if.cb.data" must resolve
//     -- no COMP_FAILED_TO_BIND for "cb", "select" or "data" at lines
//     32..34 of design.sv.
//   - 8.6 / 13.3: my_monitor has methods "new" (function) and "run_phase"
//     (task) whose body is a forever loop (12.7.2).
//   - 8.3: mock_scoreboard has int properties write_count and last_pkt and a
//     function method "write".
//   - 6.8: in my_monitor_unit_test, "logic clock/select/data" are variables;
//     "my_interface my_interface(.*)" instantiates the interface (25.3).
//
// What is NOT checked and why:
//   - anything inside UVM/SVUnit/SVAUnit (third-party library code);
//     macro-generated members (`uvm_component_utils, `SVTEST...).
//   - the exact HLDB shape of the ".*" implicit port connections (23.3.2.4):
//     the HLDB model for wildcard connections is not fixed by the standard.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/class_typespec.h>
#include <hldb/clocking_block.h>
#include <hldb/clocking_io_decl.h>
#include <hldb/design.h>
#include <hldb/event_control.h>
#include <hldb/extends.h>
#include <hldb/forever_stmt.h>
#include <hldb/function.h>
#include <hldb/interface.h>
#include <hldb/interface_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/port.h>
#include <hldb/ref_instance.h>
#include <hldb/ref_typespec.h>
#include <hldb/task.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <string_view>

namespace hlc {

class MonitorTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "Monitor.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getIface() {
    return hldb::findByName<hldb::Interface>("my_interface", m_design->getAllInterfaces());
  }

  static const hldb::ClassDefn *getClass(std::string_view name) {
    return hldb::findByName<hldb::ClassDefn>(name, m_design->getAllClasses());
  }

  static const hldb::ClockingBlock *getCb() {
    const hldb::Interface *const iface = getIface();
    if (iface == nullptr) return nullptr;
    return hldb::findByName<hldb::ClockingBlock>("cb", iface->getClockingBlocks());
  }

  static void expectExtends(std::string_view cls, std::string_view base) {
    const hldb::ClassDefn *const c = getClass(cls);
    ASSERT_NE(c, nullptr) << cls;
    ASSERT_NE(c->getExtends(), nullptr) << cls;
    ASSERT_NE(c->getExtends()->getClassTypespecs(), nullptr) << cls;
    ASSERT_EQ(c->getExtends()->getClassTypespecs()->size(), 1u) << cls;
    const hldb::RefTypespec *const rt = c->getExtends()->getClassTypespecs()->at(0);
    ASSERT_NE(rt, nullptr) << cls;
    const hldb::ClassTypespec *const ct = rt->getActual<hldb::ClassTypespec>();
    ASSERT_NE(ct, nullptr) << cls;
    ASSERT_NE(ct->getClassDefn(), nullptr) << cls << ": base class must resolve";
    EXPECT_EQ(ct->getClassDefn()->getName(), base) << cls;
  }
};

// ===========================================================================
// interface my_interface + clocking block cb
// ===========================================================================

TEST_F(MonitorTest, InterfaceHasThreeInputPorts) {
  const hldb::Interface *const iface = getIface();
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getPorts(), nullptr);
  ASSERT_EQ(iface->getPorts()->size(), 3u);
  const char *const names[] = {"clock", "select", "data"};
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_EQ(iface->getPorts()->at(i)->getName(), names[i]);
    EXPECT_EQ(iface->getPorts()->at(i)->getDirection(), vpiInput) << names[i];
  }
}

TEST_F(MonitorTest, ClockingBlockCbOnPosedgeClock) {
  const hldb::ClockingBlock *const cb = getCb();
  ASSERT_NE(cb, nullptr) << "14.3: clocking block 'cb' in my_interface";
  const hldb::EventControl *const ev = cb->getClockingEvent();
  ASSERT_NE(ev, nullptr);
  const hldb::Operation *const op = ev->getCondition<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "'posedge clock' should be an Operation";
  EXPECT_EQ(op->getOpType(), vpiPosedgeOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 1u);
  EXPECT_EQ(op->getOperands()->at(0)->getName(), "clock");
}

TEST_F(MonitorTest, ClockingBlockHasTwoInputSignals) {
  const hldb::ClockingBlock *const cb = getCb();
  ASSERT_NE(cb, nullptr);
  ASSERT_NE(cb->getClockingIODecls(), nullptr);
  ASSERT_EQ(cb->getClockingIODecls()->size(), 2u);
  EXPECT_EQ(cb->getClockingIODecls()->at(0)->getName(), "select");
  EXPECT_EQ(cb->getClockingIODecls()->at(1)->getName(), "data");
  for (const hldb::ClockingIODecl *const io : *cb->getClockingIODecls()) {
    EXPECT_EQ(io->getDirection(), vpiInput) << io->getName();
  }
}

// ===========================================================================
// class my_monitor
// ===========================================================================

TEST_F(MonitorTest, MyMonitorExtendsUvmMonitor) { expectExtends("my_monitor", "uvm_monitor"); }

TEST_F(MonitorTest, MockScoreboardExtendsUvmScoreboard) { expectExtends("mock_scoreboard", "uvm_scoreboard"); }

TEST_F(MonitorTest, IfPropertyIsVirtualInterface) {
  const hldb::ClassDefn *const c = getClass("my_monitor");
  ASSERT_NE(c, nullptr);
  const hldb::Variable *const v = hldb::findByName<hldb::Variable>("_if", c->getVariables());
  ASSERT_NE(v, nullptr);
  ASSERT_NE(v->getTypespec(), nullptr);
  const hldb::InterfaceTypespec *const its = v->getTypespec()->getActual<hldb::InterfaceTypespec>();
  ASSERT_NE(its, nullptr) << "25.9: '_if' is a virtual interface variable";
  EXPECT_TRUE(its->getVirtual());
  EXPECT_EQ(its->getInterface(), getIface());
}

TEST_F(MonitorTest, ClockingBlockAccessThroughVirtualInterfaceBinds) {
  for (uint32_t line : {32u, 33u, 34u}) {
    EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "cb", line), nullptr)
        << "25.9/14.3: '_if.cb' names the clocking block of my_interface (line " << line << ")";
  }
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "select", 33), nullptr)
      << "14.3: 'select' is a clockvar of cb";
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "data", 34), nullptr) << "14.3: 'data' is a clockvar of cb";
}

TEST_F(MonitorTest, MyMonitorHasNewAndRunPhase) {
  const hldb::ClassDefn *const c = getClass("my_monitor");
  ASSERT_NE(c, nullptr);
  ASSERT_NE(c->getMethods(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Function>("new", c->getMethods()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Task>("run_phase", c->getMethods()), nullptr);
}

TEST_F(MonitorTest, RunPhaseBodyIsForever) {
  const hldb::ClassDefn *const c = getClass("my_monitor");
  ASSERT_NE(c, nullptr);
  const hldb::Task *const t = hldb::findByName<hldb::Task>("run_phase", c->getMethods());
  ASSERT_NE(t, nullptr);
  const hldb::Any *body = t->getStmt();
  ASSERT_NE(body, nullptr);
  // A task body of a single statement may be modeled directly or inside an
  // implicit block; accept both.
  if (const hldb::Begin *const blk = any_cast<hldb::Begin>(body)) {
    ASSERT_NE(blk->getStmts(), nullptr);
    ASSERT_EQ(blk->getStmts()->size(), 1u);
    body = blk->getStmts()->at(0);
  }
  const hldb::ForeverStmt *const fe = any_cast<hldb::ForeverStmt>(body);
  ASSERT_NE(fe, nullptr) << "12.7.2: run_phase body is 'forever'";
  const hldb::Begin *const inner = fe->getStmt<hldb::Begin>();
  ASSERT_NE(inner, nullptr);
  ASSERT_NE(inner->getStmts(), nullptr);
  ASSERT_EQ(inner->getStmts()->size(), 2u);
  EXPECT_EQ(inner->getStmts()->at(0)->getAnyType(), hldb::AnyType::EventControl) << "'@_if.cb;'";
}

// ===========================================================================
// class mock_scoreboard
// ===========================================================================

TEST_F(MonitorTest, MockScoreboardMembers) {
  const hldb::ClassDefn *const c = getClass("mock_scoreboard");
  ASSERT_NE(c, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("write_count", c->getVariables()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("last_pkt", c->getVariables()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("m_imp", c->getVariables()), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Function>("write", c->getMethods()), nullptr);
}

// ===========================================================================
// module my_monitor_unit_test
// ===========================================================================

TEST_F(MonitorTest, UnitTestModuleVariablesAndInterfaceInstance) {
  const hldb::Module *const m = hldb::findByName<hldb::Module>("my_monitor_unit_test", m_design->getAllModules());
  ASSERT_NE(m, nullptr);
  for (std::string_view name : {"clock", "select", "data"}) {
    EXPECT_NE(hldb::findByName<hldb::Variable>(name, m->getVariables()), nullptr)
        << "6.8: 'logic " << name << "' is a variable";
  }
  const hldb::RefInstance *const ri = hldb::findByName<hldb::RefInstance>("my_interface", m->getRefInstances());
  ASSERT_NE(ri, nullptr);
  ASSERT_NE(ri->getTypespec(), nullptr);
  const hldb::InterfaceTypespec *const its = ri->getTypespec()->getActual<hldb::InterfaceTypespec>();
  ASSERT_NE(its, nullptr);
  EXPECT_EQ(its->getInterface(), getIface());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
