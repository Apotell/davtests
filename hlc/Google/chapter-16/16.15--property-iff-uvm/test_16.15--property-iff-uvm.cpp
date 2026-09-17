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
// tests/Google/chapter-16/16.15--property-iff-uvm.sv
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
// :name: property_disable_iff_test_uvm
// :description: property with disable iff test with UVM
// :type: simulation parsing
// :tags: uvm uvm-assertions
// :timeout: 60
// */
//
// import uvm_pkg::*;
// `include "uvm_macros.svh"
//
// module clk_gen(
//     input      rst,
//     input      clk,
//     output reg out
// );
//     initial begin out = 0; end
//     always @(posedge clk or posedge rst) begin
//         if (rst) out <= 0; else out <= 1;
//     end
// endmodule: clk_gen
//
// interface clk_gen_if(
//     output bit rst,
//     output bit clk,
//     input out
// );
// endinterface: clk_gen_if
//
// string label = "IFF_UVM";
//
// class env extends uvm_env;
//     virtual clk_gen_if m_if;
//
//     function new(string name, uvm_component parent = null);
//         super.new(name, parent);
//     endfunction
//
//     function void connect_phase(uvm_phase phase);
//         `uvm_info(label, "Started connect phase", UVM_LOW);
//         assert(uvm_resource_db#(virtual clk_gen_if)::read_by_name(
//             get_full_name(), "clk_gen_if", m_if));
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
//     clk_gen_if dif();
//     clk_gen dut(.clk(dif.clk), .rst(dif.rst), .out(dif.out));
//
//     initial begin
//         environment = new("env");
//         uvm_resource_db#(virtual clk_gen_if)::set("env", "clk_gen_if", dif);
//         dif.clk = 0;
//         dif.rst = 1;
//         run_test();
//     end
//
//     property prop;
//         @(posedge dif.clk) disable iff (dif.rst) dif.out;
//     endproperty
//
//     assert property (prop) else $error($sformatf("property check failed :assert: (False)"));
//
//     initial begin
//         forever begin
//             #(50) dif.clk = ~dif.clk;
//         end
//     end
// endmodule
// ============================================================================
//
// IEEE 1800-2023 construct under test: the same "disable iff" property
// shape as test_16.15--property-disable-iff.cpp ("@(posedge clk)
// disable iff (rst) out"), embedded in a UVM testbench the same way as the
// other chapter-16 UVM files, referencing the interface's "dif.clk"/
// "dif.rst"/"dif.out" signals instead of plain module signals.
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - module "clk_gen", module "top", and class "env" all exist; "env" has
//     "connect_phase" (a Function) and "run_phase" (a Task).
//   - property "prop" is declared in module top.
//   - PropertyDecl::getPropertySpec() is a PropertySpec whose
//     getClockingEvent() is an Operation (opType == vpiPosedgeOp)
//     referencing "clk".
//   - PropertySpec::getDisableCondition() is a plain RefObj referencing
//     "rst".
//   - PropertySpec::getPropertyExpr() is a plain RefObj referencing "out".
//   - `assert property (prop) else $error(...);` is reachable via
//     Scope::getConcurrentAssertions(), is an Assert, with a non-null
//     getElseStmt().
//
// NOT CHECKED (out of scope; every assertion below states only what IEEE
// 1800-2023 requires -- none of it is based on reading a .log file or any
// other tool-output dump):
//   - The "clk_gen" DUT's reset/output logic, the run_phase wait loop,
//     macro expansions, and the clock-generation/interface-connection
//     structure are unrelated to the property construct under test.
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
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/property_decl.h>
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

class PropertyIffUvmTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16.15--property-iff-uvm.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to PropertyIffUvmTest go here!

TEST_F(PropertyIffUvmTest, ModuleClkGenAndTopAndClassEnvExist) {
  EXPECT_NE(hldb::findByName<hldb::Module>("clk_gen", m_design->getAllModules()), nullptr)
      << "module 'clk_gen' not found";
  EXPECT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr) << "module 'top' not found";
  EXPECT_NE(hldb::findByName<hldb::ClassDefn>("env", m_design->getAllClasses()), nullptr) << "class 'env' not found";
}

TEST_F(PropertyIffUvmTest, ClassEnvHasConnectPhaseFunctionAndRunPhaseTask) {
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

TEST_F(PropertyIffUvmTest, PropertyPropDeclaredWithPosedgeClockingEventAndDisableCondition) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::PropertyDecl *const prop = hldb::findByName<hldb::PropertyDecl>("prop", top->getPropertyDecls());
  ASSERT_NE(prop, nullptr) << "property 'prop' not found in module top";

  ASSERT_NE(prop->getPropertySpec(), nullptr) << "'@(posedge dif.clk) disable iff (dif.rst) dif.out' should "
                                                  "produce a PropertySpec";
  const hldb::PropertySpec *const spec = prop->getPropertySpec();

  ASSERT_NE(spec->getClockingEvent(), nullptr) << "'@(posedge dif.clk)' is the clocking event";
  const hldb::Operation *const clockOp = spec->getClockingEvent<hldb::Operation>();
  ASSERT_NE(clockOp, nullptr) << "the clocking event should be an Operation";
  EXPECT_EQ(clockOp->getOpType(), vpiPosedgeOp);
  EXPECT_TRUE(OperandsContainNamedRef(clockOp, std::string_view("dif.clk"))) << "the posedge operand should reference 'clk'";

  ASSERT_NE(spec->getDisableCondition(), nullptr) << "'disable iff (dif.rst)' should produce a disable condition";
  const hldb::Operation *const op = spec->getDisableCondition<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiIffOp);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 1u);
  
  const hldb::RefObj *const disableRef = any_cast<hldb::RefObj>(op->getOperands()->front());
  ASSERT_NE(disableRef, nullptr) << "'dif.rst' should be a plain RefObj";
  EXPECT_EQ(disableRef->getName(), std::string_view("dif.rst"));
}

TEST_F(PropertyIffUvmTest, PropertyExprIsBareReferenceToOut) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::PropertyDecl *const prop = hldb::findByName<hldb::PropertyDecl>("prop", top->getPropertyDecls());
  ASSERT_NE(prop, nullptr);
  ASSERT_NE(prop->getPropertySpec(), nullptr);

  ASSERT_NE(prop->getPropertySpec()->getPropertyExpr(), nullptr) << "'dif.out' is the property expression";
  const hldb::RefObj *const outRef = prop->getPropertySpec()->getPropertyExpr<hldb::RefObj>();
  ASSERT_NE(outRef, nullptr) << "'dif.out' should be a plain RefObj, not an Operation";
  EXPECT_EQ(outRef->getName(), std::string_view("dif.out"));
}

TEST_F(PropertyIffUvmTest, AssertPropertyIsReachableAndReferencesProp) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  ASSERT_NE(top->getConcurrentAssertions(), nullptr)
      << "'assert property (prop) else $error(...);' should be reachable via Scope::getConcurrentAssertions()";
  ASSERT_EQ(top->getConcurrentAssertions()->size(), 1u);

  const hldb::ConcurrentAssertions *const item = top->getConcurrentAssertions()->front();
  ASSERT_NE(item, nullptr);
  const hldb::Assert *const assertProp = any_cast<hldb::Assert>(item);
  ASSERT_NE(assertProp, nullptr) << "'assert property (...)' should be an Assert";

  EXPECT_NE(assertProp->getProperty(), nullptr) << "'prop' is the asserted property and must be present";
  EXPECT_NE(assertProp->getElseStmt(), nullptr) << "'else $error(...)' should produce an else-clause";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
