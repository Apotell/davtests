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
// tests/Google/chapter-16/16.9--sequence-stable-uvm.sv
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
// :name: sequence_stable_test_uvm
// :description: sequence with "stable" task in UVM
// :type: simulation parsing
// :tags: uvm uvm-assertions
// :timeout: 60
// */
//
// import uvm_pkg::*;
// `include "uvm_macros.svh"
//
// module clk_gen(
//     input   clk,
//     output  out
// );
//     assign out = 0;
// endmodule: clk_gen
//
// interface clk_gen_if(
//     output bit clk,
//     input out
// );
// endinterface: clk_gen_if
//
// string label = "SEQUENCE_FUNC_UVM";
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
//             repeat(10) @(posedge m_if.clk);
//         end
//         `uvm_info(label, "Finished run phase", UVM_LOW);
//         phase.drop_objection(this);
//     endtask: run_phase
// endclass
//
// module top();
//     env environment;
//     int cycle = 0;
//     clk_gen_if dif();
//     clk_gen dut(.clk(dif.clk), .out(dif.out));
//
//     initial begin
//         environment = new("env");
//         uvm_resource_db#(virtual clk_gen_if)::set("env", "clk_gen_if", dif);
//         dif.clk = 0;
//         run_test();
//     end
//
//     sequence seq;
//         @(posedge dif.clk) $stable(dif.out);
//     endsequence
//
//     assert property (seq) else `uvm_info(label, $sformatf("$stable(dif.out) returned false :assert: (%d == 1)", cycle), UVM_LOW);
//
//     initial begin
//         forever begin
//             #(50) dif.clk = ~dif.clk;
//             cycle = cycle + 1;
//         end
//     end
// endmodule
// ============================================================================
//
// IEEE 1800-2023 constructs under test (Sec 16.9.3, "Sampled value system
// functions"): this file is the "$stable" counterpart of
// test_16.9--sequence-changed-uvm.cpp / -fell-uvm.cpp / -rose-uvm.cpp --
// same ClockedSeq/posedge-clocking-event shape, with $stable(dif.out) as
// the sequence body. $stable(expr) is true when expr did not change value
// on the most recent clock tick.
//
// Structural note (unlike the other three sampled-value-function files):
// this module's "clk_gen" DUT is a single continuous assignment
// ("assign out = 0;", no clocked always block, no internal counter), and
// module top increments "cycle" directly inside the clock-generation
// "forever" block rather than via a separate "always @(posedge dif.clk)"
// process. Neither difference affects the sequence/assertion shape under
// test, so every check below still applies unchanged.
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - module "clk_gen", module "top", and class "env" all exist; "env" has
//     "connect_phase" (a Function) and "run_phase" (a Task).
//   - sequence "seq" is declared in module top.
//   - SequenceDecl::getExpr() is a ClockedSeq; getClockingEvent() is an
//     Operation (opType == vpiPosedgeOp) referencing "clk".
//   - getSequenceExpr() is a SysFuncCall named "$stable" with exactly one
//     argument, referencing "out".
//   - `assert property (seq) else ...;` is reachable via
//     Scope::getConcurrentAssertions(), is an Assert, and has a non-null
//     getElseStmt().
//
// NOT CHECKED (out of scope; every assertion below states only what IEEE
// 1800-2023 requires -- none of it is based on reading a .log file or any
// other tool-output dump):
//   - `assert property (seq)`'s exact getProperty() shape is only checked
//     for presence.
//   - The "clk_gen" DUT's trivial "assign out = 0;", the "int cycle"
//     counter, the run_phase wait loop, macro expansions, and the
//     clock-generation/interface-connection structure are unrelated to the
//     sequence/assertion constructs under test.
//   - Runtime pass/fail behavior of the assertion cannot be observed: HLC
//     is a compiler/elaborator with no simulation.
// ============================================================================

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/assert_stmt.h>
#include <hldb/class_defn.h>
#include <hldb/clocked_seq.h>
#include <hldb/concurrent_assertions.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/sequence_decl.h>
#include <hldb/sys_func_call.h>
#include <hldb/task.h>
#include <hldb/task_func.h>

#include <hlc/Tests/Test.h>

namespace hlc {
namespace {
bool OperandsContainNamedRef(const hldb::Operation *op, std::string_view name) {
  return (op != nullptr) && (hldb::findByName<hldb::RefObj>(name, op->getOperands()) != nullptr);
}

bool ArgumentsContainNamedRef(const hldb::SysFuncCall *call, std::string_view name) {
  return (call != nullptr) && (hldb::findByName<hldb::RefObj>(name, call->getArguments()) != nullptr);
}
}  // namespace

class SequenceStableUvmTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16.9--sequence-stable-uvm.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to SequenceStableUvmTest go here!

TEST_F(SequenceStableUvmTest, ModuleClkGenAndTopAndClassEnvExist) {
  EXPECT_NE(hldb::findByName<hldb::Module>("clk_gen", m_design->getAllModules()), nullptr)
      << "module 'clk_gen' not found";
  EXPECT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr) << "module 'top' not found";
  EXPECT_NE(hldb::findByName<hldb::ClassDefn>("env", m_design->getAllClasses()), nullptr) << "class 'env' not found";
}

TEST_F(SequenceStableUvmTest, ClassEnvHasConnectPhaseFunctionAndRunPhaseTask) {
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

TEST_F(SequenceStableUvmTest, SequenceSeqDeclaredInModuleTop) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::SequenceDecl *const seq = hldb::findByName<hldb::SequenceDecl>("seq", top->getSequenceDecls());
  ASSERT_NE(seq, nullptr) << "sequence 'seq' not found in module top";
}

TEST_F(SequenceStableUvmTest, SequenceExprIsClockedSeqWithPosedgeClockingEvent) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::SequenceDecl *const seq = hldb::findByName<hldb::SequenceDecl>("seq", top->getSequenceDecls());
  ASSERT_NE(seq, nullptr);

  ASSERT_NE(seq->getExpr(), nullptr) << "'@(posedge dif.clk) ...;' is the sequence body and must be present";
  const hldb::ClockedSeq *const clocked = any_cast<hldb::ClockedSeq>(seq->getExpr());
  ASSERT_NE(clocked, nullptr) << "a sequence with a leading clocking event should be a ClockedSeq";

  ASSERT_NE(clocked->getClockingEvent(), nullptr) << "'@(posedge dif.clk)' is the clocking event";
  const hldb::Operation *const clockOp = any_cast<hldb::Operation>(clocked->getClockingEvent());
  ASSERT_NE(clockOp, nullptr) << "the clocking event should be an Operation";
  EXPECT_EQ(clockOp->getOpType(), vpiPosedgeOp);
  EXPECT_TRUE(OperandsContainNamedRef(clockOp, std::string_view("dif.clk"))) << "the posedge operand should reference 'clk'";
}

TEST_F(SequenceStableUvmTest, SequenceExprIsStableSysFuncCallOnOut) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::SequenceDecl *const seq = hldb::findByName<hldb::SequenceDecl>("seq", top->getSequenceDecls());
  ASSERT_NE(seq, nullptr);
  const hldb::ClockedSeq *const clocked = any_cast<hldb::ClockedSeq>(seq->getExpr());
  ASSERT_NE(clocked, nullptr);

  ASSERT_NE(clocked->getSequenceExpr(), nullptr) << "'$stable(dif.out)' is the sequence body";
  const hldb::SysFuncCall *const stable = any_cast<hldb::SysFuncCall>(clocked->getSequenceExpr());
  ASSERT_NE(stable, nullptr) << "'$stable(...)' returns a value, so this should be a SysFuncCall";
  EXPECT_EQ(stable->getName(), "$stable");

  ASSERT_NE(stable->getArguments(), nullptr);
  ASSERT_EQ(stable->getArguments()->size(), 1u) << "'$stable()' takes exactly one argument";
  EXPECT_TRUE(ArgumentsContainNamedRef(stable, std::string_view("dif.out"))) << "'dif.out' should be the argument";
}

TEST_F(SequenceStableUvmTest, AssertPropertyIsReachableAndReferencesSeq) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  ASSERT_NE(top->getConcurrentAssertions(), nullptr)
      << "'assert property (seq) else ...;' should be reachable via Scope::getConcurrentAssertions()";
  ASSERT_EQ(top->getConcurrentAssertions()->size(), 1u);

  const hldb::ConcurrentAssertions *const item = top->getConcurrentAssertions()->front();
  ASSERT_NE(item, nullptr);
  const hldb::Assert *const assertProp = any_cast<hldb::Assert>(item);
  ASSERT_NE(assertProp, nullptr) << "'assert property (...)' should be an Assert";

  EXPECT_NE(assertProp->getProperty(), nullptr) << "'seq' is the asserted property/sequence and must be present";
  EXPECT_NE(assertProp->getElseStmt(), nullptr) << "'else `uvm_info(...)' should still produce an else-clause";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
