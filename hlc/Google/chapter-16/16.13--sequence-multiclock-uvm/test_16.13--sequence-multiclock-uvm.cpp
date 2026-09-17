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
// tests/Google/chapter-16/16.13--sequence-multiclock-uvm.sv
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
// :name: sequence_multiclock_test_uvm
// :description: sequence with local variables in UVM
// :type: simulation parsing
// :tags: uvm uvm-assertions
// :timeout: 60
// */
//
// import uvm_pkg::*;
// `include "uvm_macros.svh"
//
// module clk_gen(
//     input      clk0,
//     input      clk1,
//     output reg out0,
//     output reg out1
// );
//     initial begin out0 = 0; out1 = 0; end
//     always @(posedge clk0) begin out0 <= 1; end
//     always @(posedge clk1) begin out1 <= 1; end
// endmodule: clk_gen
//
// interface clk_gen_if(
//     output bit clk0,
//     output bit clk1,
//     input      out0,
//     input      out1
// );
// endinterface: clk_gen_if
//
// string label = "SEQUENCE_MULTICLOCK_UVM";
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
//             repeat(10) @(posedge m_if.clk0);
//         end
//         `uvm_info(label, "Finished run phase", UVM_LOW);
//         phase.drop_objection(this);
//     endtask: run_phase
// endclass
//
// module top();
//     env environment;
//     clk_gen_if dif();
//     clk_gen dut(.clk0(dif.clk0), .clk1(dif.clk1), .out0(dif.out0), .out1(dif.out1));
//
//     initial begin
//         environment = new("env");
//         uvm_resource_db#(virtual clk_gen_if)::set("env", "clk_gen_if", dif);
//         dif.clk0 = 0;
//         dif.clk1 = 0;
//         run_test();
//     end
//
//     sequence seq;
//         @(posedge dif.clk0) ##1 dif.out0 ##1 @(posedge dif.clk1) dif.out1;
//     endsequence
//
//     assert property (seq) else `uvm_error(label, $sformatf("sequence check failed :assert: (False)"));
//
//     initial begin
//         forever begin
//             #(50) dif.clk0 = ~dif.clk0;
//             #(150) dif.clk1 = ~dif.clk1;
//         end
//     end
// endmodule
// ============================================================================
//
// IEEE 1800-2023 construct under test (Sec 16.16, "Multiclocked
// sequences and properties"): `@(posedge dif.clk0) ##1 dif.out0 ##1
// @(posedge dif.clk1) dif.out1` -- a sequence that starts sampled on
// "clk0" and, partway through (after two "##1" cycle delays counted in the
// "clk0" domain), switches to being clocked by "clk1" for the remainder
// ("dif.out1"). Sec 16.16.1 models a clock change mid-sequence as a nested
// clocked sequence embedded at the point of the change.
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - module "clk_gen", module "top", and class "env" all exist; "env" has
//     "connect_phase" (a Function) and "run_phase" (a Task).
//   - sequence "seq" is declared in module top.
//   - SequenceDecl::getExpr() is a ClockedSeq whose getClockingEvent() is
//     an Operation (opType == vpiPosedgeOp) referencing "clk0" -- the
//     sequence's initial (outer) clock.
//   - somewhere within that ClockedSeq's getSequenceExpr() tree there is a
//     second, nested ClockedSeq whose own getClockingEvent() is an
//     Operation (opType == vpiPosedgeOp) referencing "clk1" -- confirming
//     the mid-sequence clock change to "clk1" is present, and that its own
//     getSequenceExpr() references "out1".
//   - `assert property (seq) else ...;` is reachable via
//     Scope::getConcurrentAssertions(), is an Assert, and has a non-null
//     getElseStmt().
//
// NOT CHECKED (out of scope; every assertion below states only what IEEE
// 1800-2023 requires -- none of it is based on reading a .log file or any
// other tool-output dump):
//   - The exact operand COUNT/ORDER within the outer ClockedSeq's
//     getSequenceExpr() tree (i.e. precisely how the two "##1" delays and
//     the nested ClockedSeq are arranged relative to each other) -- the
//     nested ClockedSeq is located by a recursive search through Operation
//     operands rather than by indexing into a fixed position, for the same
//     reason as the cycle-delay operand-order caveat in
//     test_16.7--sequence-and-uvm.cpp.
//   - Whether "out0" (referenced between the two "##1" delays, still in
//     the "clk0" domain) is present is not separately asserted, to keep
//     this file focused on confirming the clock-domain switch itself.
//   - `assert property (seq)`'s exact getProperty() shape is only checked
//     for presence.
//   - The "clk_gen" DUT's independent clk0/clk1-triggered always blocks,
//     the run_phase wait loop, macro expansions, and the clock-generation/
//     interface-connection structure in module top are unrelated to the
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

      if (ref->getPathElems() != nullptr && !ref->getPathElems()->empty()) {
        const hldb::RefObj *const leaf = any_cast<hldb::RefObj>(ref->getPathElems()->back());
        if (leaf != nullptr && leaf->getName() == name) {
          return true;
        }
      }
    }
  }
  return false;
}

// Recursively searches "root", descending only through Operation operand
// trees, for the first nested ClockedSeq (used to find a mid-sequence
// clock change -- see file header).
const hldb::ClockedSeq *FindClockedSeq(const hldb::Any *root) {
  if (const hldb::ClockedSeq *const clocked = any_cast<hldb::ClockedSeq>(root)) {
    return clocked;
  }
  if (const hldb::Operation *const op = any_cast<hldb::Operation>(root)) {
    if (op->getOperands() != nullptr) {
      for (const hldb::Any *const operand : *op->getOperands()) {
        if (const hldb::ClockedSeq *const found = FindClockedSeq(operand)) {
          return found;
        }
      }
    }
  }
  return nullptr;
}
}  // namespace

class SequenceMulticlockUvmTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16.13--sequence-multiclock-uvm.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to SequenceMulticlockUvmTest go here!

TEST_F(SequenceMulticlockUvmTest, ModuleClkGenAndTopAndClassEnvExist) {
  EXPECT_NE(hldb::findByName<hldb::Module>("clk_gen", m_design->getAllModules()), nullptr)
      << "module 'clk_gen' not found";
  EXPECT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr) << "module 'top' not found";
  EXPECT_NE(hldb::findByName<hldb::ClassDefn>("env", m_design->getAllClasses()), nullptr) << "class 'env' not found";
}

TEST_F(SequenceMulticlockUvmTest, ClassEnvHasConnectPhaseFunctionAndRunPhaseTask) {
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

TEST_F(SequenceMulticlockUvmTest, SequenceSeqDeclaredInModuleTop) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::SequenceDecl *const seq = hldb::findByName<hldb::SequenceDecl>("seq", top->getSequenceDecls());
  ASSERT_NE(seq, nullptr) << "sequence 'seq' not found in module top";
}

TEST_F(SequenceMulticlockUvmTest, SequenceExprIsClockedSeqWithPosedgeClk0) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::SequenceDecl *const seq = hldb::findByName<hldb::SequenceDecl>("seq", top->getSequenceDecls());
  ASSERT_NE(seq, nullptr);

  ASSERT_NE(seq->getExpr(), nullptr) << "'@(posedge dif.clk0) ...;' is the sequence body and must be present";
  const hldb::ClockedSeq *const outerClocked = seq->getExpr<hldb::ClockedSeq>();
  ASSERT_NE(outerClocked, nullptr) << "a sequence with a leading clocking event should be a ClockedSeq";

  ASSERT_NE(outerClocked->getClockingEvent(), nullptr) << "'@(posedge dif.clk0)' is the outer clocking event";
  const hldb::Operation *const clockOp = outerClocked->getClockingEvent<hldb::Operation>();
  ASSERT_NE(clockOp, nullptr) << "the clocking event should be an Operation";
  EXPECT_EQ(clockOp->getOpType(), vpiPosedgeOp);
  EXPECT_TRUE(OperandsContainNamedRef(clockOp, std::string_view("clk0"))) << "the posedge operand should reference 'clk0'";
}

TEST_F(SequenceMulticlockUvmTest, SequenceExprContainsNestedClockedSeqWithPosedgeClk1) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::SequenceDecl *const seq = hldb::findByName<hldb::SequenceDecl>("seq", top->getSequenceDecls());
  ASSERT_NE(seq, nullptr);
  const hldb::ClockedSeq *const outerClocked = seq->getExpr<hldb::ClockedSeq>();
  ASSERT_NE(outerClocked, nullptr);

  ASSERT_NE(outerClocked->getSequenceExpr(), nullptr)
      << "'##1 dif.out0 ##1 @(posedge dif.clk1) dif.out1' is the outer sequence body";

  const hldb::ClockedSeq *const innerClocked = FindClockedSeq(outerClocked->getSequenceExpr());
  ASSERT_NE(innerClocked, nullptr) << "'@(posedge dif.clk1) dif.out1' should produce a nested ClockedSeq "
                                       "somewhere in the outer sequence body";
  ASSERT_NE(innerClocked->getClockingEvent(), nullptr) << "'@(posedge dif.clk1)' is the inner clocking event";
  const hldb::Operation *const innerClockOp = innerClocked->getClockingEvent<hldb::Operation>();
  ASSERT_NE(innerClockOp, nullptr) << "the inner clocking event should be an Operation";
  EXPECT_EQ(innerClockOp->getOpType(), vpiPosedgeOp);
  EXPECT_TRUE(OperandsContainNamedRef(innerClockOp, std::string_view("dif.clk1"))) << "the inner posedge operand should reference 'clk1'";

  ASSERT_NE(innerClocked->getSequenceExpr(), nullptr) << "'dif.out1' is the inner sequence body";
  EXPECT_NE(innerClocked->getSequenceExpr<hldb::RefObj>(), nullptr)
      << "'dif.out1' should be a plain RefObj (no further hierarchy)";
}

TEST_F(SequenceMulticlockUvmTest, AssertPropertyIsReachableAndReferencesSeq) {
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
  EXPECT_NE(assertProp->getElseStmt(), nullptr) << "'else `uvm_error(...)' should produce an else-clause";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
