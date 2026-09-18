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

// ============================================================================
// SystemVerilog source under test:
// tests/Google/chapter-16/16.12--property-interface-prec-uvm.sv
// ----------------------------------------------------------------------------
// // Copyright (C) 2019-2021  The SymbiFlow Authors.
// //
// // Use of this source code is governed by a ISC-style
// // license that can be found in the LICENSE file or at
// // https://opensource.org/licenses/ISC
// //
// // SPDX-License-Identifier: ISC
//
// /*
// :name: property_interface_prec_test_uvm
// :description: interface property with precondition in UVM
// :type: simulation parsing
// :tags: uvm uvm-assertions
// :timeout: 60
// */
//
// import uvm_pkg::*;
// `include "uvm_macros.svh"
//
// module mem_ctrl (
//     input            clk,
//     output reg       read,
//     output reg       write,
//     output reg [7:0] addr,
//     output reg [7:0] dout,
//     input      [7:0] din
// );
//     reg       phase;
//     reg [7:0] addr_i;
//     initial begin phase = 0; read = 0; write = 0; end
//     always @(posedge clk) begin
//         read <= 0; write <= 0; addr <= addr_i;
//         if(phase) begin read <= 1; end else begin write <= 1; end
//         dout <= din; addr_i <= addr_i + 1; phase <= ~phase;
//     end
// endmodule: mem_ctrl
//
// string label = "PROPERTY_UVM";
//
// interface mem_ctrl_if(
//     output bit clk,
//     input read,
//     input write,
//     input [7:0] addr,
//     input [7:0] dout,
//     output reg [7:0] din
// );
//     assert property (@(posedge clk) read |=> write ) else `uvm_error(label, $sformatf("write after read :assert: (False)"));
// endinterface: mem_ctrl_if
//
// class env extends uvm_env;
//     virtual mem_ctrl_if m_if;
//
//     function new(string name, uvm_component parent = null);
//         super.new(name, parent);
//     endfunction
//
//     function void connect_phase(uvm_phase phase);
//         `uvm_info(label, "Started connect phase", UVM_LOW);
//         assert(uvm_resource_db#(virtual mem_ctrl_if)::read_by_name(
//             get_full_name(), "mem_ctrl_if", m_if));
//         `uvm_info(label, "Finished connect phase", UVM_LOW);
//     endfunction: connect_phase
//
//     task run_phase(uvm_phase phase);
//         phase.raise_objection(this);
//         `uvm_info(label, "Started run phase", UVM_LOW);
//         begin
//             repeat(10) @(m_if.clk);
//         end
//         `uvm_info(label, "Finished run phase", UVM_LOW);
//         phase.drop_objection(this);
//     endtask: run_phase
// endclass
//
// module top();
//     env environment;
//     mem_ctrl_if dif();
//     mem_ctrl dut(.clk(dif.clk), .read(dif.read), .write(dif.write), .addr(dif.addr), .dout(dif.dout), .din(dif.din));
//
//     initial begin
//         environment = new("env");
//         uvm_resource_db#(virtual mem_ctrl_if)::set("env", "mem_ctrl_if", dif);
//         dif.clk = 0;
//         run_test();
//     end
//
//     initial begin
//         forever begin
//             #(50) dif.clk = ~dif.clk;
//         end
//     end
// endmodule
// ============================================================================
//
// IEEE 1800-2023 constructs under test: `assert property (@(posedge clk)
// read |=> write) else ...;` declared inside the "mem_ctrl_if" interface --
// the same non-overlapped implication ("|=>") shape as
// test_16.12--property-prec-uvm.cpp, but declared in the interface instead
// of module top (matching how test_16.12--property-interface-uvm.cpp moves
// the "!(read & write)" property from module top into the interface).
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - module "mem_ctrl", module "top", and class "env" all exist; "env"
//     has "connect_phase" (a Function) and "run_phase" (a Task).
//   - interface "mem_ctrl_if" exists, and its port list contains "clk",
//     "read", and "write".
//   - `assert property (...) else ...;` is reachable via
//     Scope::getConcurrentAssertions() on the interface object, is an
//     Assert, with a non-null getElseStmt().
//   - Assert::getProperty() is a PropertySpec whose getClockingEvent() is
//     an Operation (opType == vpiPosedgeOp) referencing "clk", and whose
//     getDisableCondition() is null.
//   - PropertySpec::getPropertyExpr() is an Operation with opType ==
//     vpiNonOverlapImplyOp ("|=>"), with two operands referencing "read"
//     and "write".
//
// NOT CHECKED (out of scope; every assertion below states only what IEEE
// 1800-2023 requires -- none of it is based on reading a .log file or any
// other tool-output dump):
//   - The exact operand COUNT/ORDER within each Operation.
//   - The "mem_ctrl" DUT's read/write phase logic, the run_phase wait
//     loop, macro expansions, and the interface-instantiation structure in
//     module top are unrelated to the property construct under test.
//   - Runtime pass/fail behavior of the assertion cannot be observed: HLC
//     is a compiler/elaborator with no simulation.
// ============================================================================

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/assert_stmt.h>
#include <hldb/class_defn.h>
#include <hldb/concurrent_assertions.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/interface.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ports.h>
#include <hldb/property_spec.h>
#include <hldb/ref_obj.h>
#include <hldb/task.h>
#include <hldb/task_func.h>

#include <hlc/Tests/Test.h>

namespace hlc {
namespace {
bool OperandsContainNamedRef(const hldb::Operation *op, std::string_view name) {
  if (op == nullptr || op->getOperands() == nullptr) {
    return false;
  }
  for (const hldb::Any *const operand : *op->getOperands()) {
    if (const hldb::RefObj *const ref = any_cast<hldb::RefObj>(operand)) {
      if (ref->getName() == name) {
        return true;
      }
    }
  }
  return false;
}
}  // namespace

class PropertyInterfacePrecUvmTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16.12--property-interface-prec-uvm.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to PropertyInterfacePrecUvmTest go here!

TEST_F(PropertyInterfacePrecUvmTest, ModuleMemCtrlAndTopAndClassEnvExist) {
  EXPECT_NE(hldb::findByName<hldb::Module>("mem_ctrl", m_design->getAllModules()), nullptr)
      << "module 'mem_ctrl' not found";
  EXPECT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr) << "module 'top' not found";
  EXPECT_NE(hldb::findByName<hldb::ClassDefn>("env", m_design->getAllClasses()), nullptr) << "class 'env' not found";
}

TEST_F(PropertyInterfacePrecUvmTest, InterfaceMemCtrlIfExistsWithClkReadWritePorts) {
  const hldb::Interface *const iface = hldb::findByName<hldb::Interface>("mem_ctrl_if", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr) << "interface 'mem_ctrl_if' not found";
  ASSERT_NE(iface->getPorts(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Ports>("clk", iface->getPorts()), nullptr) << "port 'clk' not found";
  EXPECT_NE(hldb::findByName<hldb::Ports>("read", iface->getPorts()), nullptr) << "port 'read' not found";
  EXPECT_NE(hldb::findByName<hldb::Ports>("write", iface->getPorts()), nullptr) << "port 'write' not found";
}

TEST_F(PropertyInterfacePrecUvmTest, ClassEnvHasConnectPhaseFunctionAndRunPhaseTask) {
  const hldb::ClassDefn *const env = hldb::findByName<hldb::ClassDefn>("env", m_design->getAllClasses());
  ASSERT_NE(env, nullptr);
  ASSERT_NE(env->getMethods(), nullptr);

  const hldb::TaskFunc *const connectPhase = hldb::findByName<hldb::TaskFunc>("connect_phase", env->getMethods());
  ASSERT_NE(connectPhase, nullptr) << "'connect_phase' method not found";
  EXPECT_NE(any_cast<hldb::Function>(connectPhase), nullptr)
      << "'function void connect_phase(...)' should be a Function";

  const hldb::TaskFunc *const runPhase = hldb::findByName<hldb::TaskFunc>("run_phase", env->getMethods());
  ASSERT_NE(runPhase, nullptr) << "'run_phase' method not found";
  EXPECT_NE(any_cast<hldb::Task>(runPhase), nullptr) << "'task run_phase(...)' should be a Task";
}

TEST_F(PropertyInterfacePrecUvmTest, AssertPropertyIsReachableOnInterface) {
  const hldb::Interface *const iface = hldb::findByName<hldb::Interface>("mem_ctrl_if", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);

  ASSERT_NE(iface->getConcurrentAssertions(), nullptr)
      << "'assert property (...) else ...;' inside the interface should be reachable via "
         "Scope::getConcurrentAssertions()";
  ASSERT_EQ(iface->getConcurrentAssertions()->size(), 1u);

  const hldb::ConcurrentAssertions *const item = iface->getConcurrentAssertions()->front();
  ASSERT_NE(item, nullptr);
  const hldb::Assert *const assertProp = any_cast<hldb::Assert>(item);
  ASSERT_NE(assertProp, nullptr) << "'assert property (...)' should be an Assert";
  EXPECT_NE(assertProp->getElseStmt(), nullptr) << "'else `uvm_error(...)' should produce an else-clause";
}

TEST_F(PropertyInterfacePrecUvmTest, PropertySpecHasPosedgeClockingEvent) {
  const hldb::Interface *const iface = hldb::findByName<hldb::Interface>("mem_ctrl_if", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  const hldb::Assert *const assertProp = any_cast<hldb::Assert>(iface->getConcurrentAssertions()->front());
  ASSERT_NE(assertProp, nullptr);

  ASSERT_NE(assertProp->getProperty(), nullptr) << "the property_spec is the asserted property and must be present";
  const hldb::PropertySpec *const spec = any_cast<hldb::PropertySpec>(assertProp->getProperty());
  ASSERT_NE(spec, nullptr) << "'@(posedge clk) read |=> write' should directly be a PropertySpec";

  ASSERT_NE(spec->getClockingEvent(), nullptr) << "'@(posedge clk)' is the clocking event";
  const hldb::Operation *const clockOp = any_cast<hldb::Operation>(spec->getClockingEvent());
  ASSERT_NE(clockOp, nullptr) << "the clocking event should be an Operation";
  EXPECT_EQ(clockOp->getOpType(), vpiPosedgeOp);
  EXPECT_TRUE(OperandsContainNamedRef(clockOp, std::string_view("clk"))) << "the posedge operand should reference 'clk'";

  EXPECT_EQ(spec->getDisableCondition(), nullptr) << "no 'disable iff' was written";
}

TEST_F(PropertyInterfacePrecUvmTest, PropertyExprIsNonOverlappedImplicationOfReadAndWrite) {
  const hldb::Interface *const iface = hldb::findByName<hldb::Interface>("mem_ctrl_if", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  const hldb::Assert *const assertProp = any_cast<hldb::Assert>(iface->getConcurrentAssertions()->front());
  ASSERT_NE(assertProp, nullptr);
  const hldb::PropertySpec *const spec = any_cast<hldb::PropertySpec>(assertProp->getProperty());
  ASSERT_NE(spec, nullptr);

  ASSERT_NE(spec->getPropertyExpr(), nullptr) << "'read |=> write' is the property expression";
  const hldb::Operation *const implyOp = any_cast<hldb::Operation>(spec->getPropertyExpr());
  ASSERT_NE(implyOp, nullptr) << "'|=>' should be an Operation";
  EXPECT_EQ(implyOp->getOpType(), vpiNonOverlapImplyOp) << "'|=>' should be vpiNonOverlapImplyOp";

  ASSERT_NE(implyOp->getOperands(), nullptr);
  ASSERT_EQ(implyOp->getOperands()->size(), 2u) << "'|=>' takes exactly two operands";
  EXPECT_TRUE(OperandsContainNamedRef(implyOp, "read")) << "'read' should be an operand of '|=>'";
  EXPECT_TRUE(OperandsContainNamedRef(implyOp, "write")) << "'write' should be an operand of '|=>'";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
