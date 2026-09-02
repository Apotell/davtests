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
// tests/Google/chapter-16/16.2--assert0-uvm.sv
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
// :name: assert0_test_uvm
// :description: assert0 test with UVM
// :type: simulation parsing
// :tags: uvm uvm-assertions
// :timeout: 60
// */
//
// import uvm_pkg::*;
// `include "uvm_macros.svh"
//
// module inverter (
//     input [7:0] a,
//     output [7:0] b
// );
//     assign b = !a;
// endmodule: inverter
//
// interface inverter_if(
//     output reg [7:0] a,
//     input [7:0] b
// );
// endinterface: inverter_if
//
// string label = "ASSERT0_UVM";
//
// class env extends uvm_env;
//     virtual inverter_if m_if;
//
//     function new(string name, uvm_component parent = null);
//         super.new(name, parent);
//     endfunction
//
//     function void connect_phase(uvm_phase phase);
//         `uvm_info(label, "Started connect phase", UVM_LOW);
//         assert(uvm_resource_db#(virtual inverter_if)::read_by_name(
//             get_full_name(), "inverter_if", m_if));
//         `uvm_info(label, "Finished connect phase", UVM_LOW);
//     endfunction: connect_phase
//
//     task run_phase(uvm_phase phase);
//         phase.raise_objection(this);
//         `uvm_info(label, "Started run phase", UVM_LOW);
//         begin
//             int a = 8'h35;
//             m_if.a <= a;
//
//             assert #0 (m_if.a != m_if.b) else `uvm_error(label, $sformatf("assert failed :assert: (False)"));
//         end
//         `uvm_info(label, "Finished run phase", UVM_LOW);
//         phase.drop_objection(this);
//     endtask: run_phase
// endclass
//
// module top;
//     env environment;
//     inverter_if dif();
//     inverter dut(.a(dif.a), .b(dif.b));
//
//     initial begin
//         environment = new("env");
//         uvm_resource_db#(virtual inverter_if)::set("env", "inverter_if", dif);
//         run_test();
//     end
// endmodule
// ============================================================================
//
// IEEE 1800-2023 constructs under test (Sec 16.2 / Sec 16.4):
//   - `assert(...);`      (in connect_phase) -- a plain immediate assert,
//                         neither deferred nor final.
//   - `assert #0 (...) else ...;` (in run_phase) -- this file's focus: a "#0"
//                         deferred immediate assertion (Sec 16.4), which
//                         defers evaluation to the end of the current time
//                         step. Written inside an explicit begin/end block
//                         nested in the task, with an else-clause but no
//                         explicit pass action.
//
// This file wraps the assertion construct in a realistic UVM testbench
// (uvm_env subclass, phases, a virtual interface). The surrounding UVM
// machinery (phases, objections, `uvm_info`/`uvm_error`/`uvm_resource_db`,
// the DUT/interface instantiation) is not itself IEEE 1800 assertion syntax
// and is out of scope here -- see NOT CHECKED below.
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - class "env" exists (Design::getAllClasses()), with methods
//     "connect_phase" (a Function) and "run_phase" (a Task).
//   - "connect_phase" contains a plain ImmediateAssert (getIsDeferred() ==
//     false, getIsFinal() == false).
//   - "run_phase" contains an ImmediateAssert with getIsDeferred() == true
//     and getIsFinal() == false -- the "#0" deferred form, distinct from
//     both the plain form above and from "assert final" (Sec 16.4).
//   - that ImmediateAssert's condition is the comparison "m_if.a != m_if.b"
//     (Operation, vpiNeqOp, two operands), it has an else-clause
//     (getElseStmt() non-null, from "else `uvm_error(...)"), and no pass
//     action (getStmt() == null, since the source gives none).
//   - the search for each ImmediateAssert descends only through explicit
//     Begin scopes (matching the source's one literal begin/end block per
//     method), not into any other statement kind -- see helper function
//     FindImmediateAssert below.
//
// NOT CHECKED (out of scope; every assertion below states only what IEEE
// 1800-2023 requires -- none of it is based on reading a .log file or any
// other tool-output dump):
//   - The exact statement layout produced for a task/function body with
//     multiple top-level statements and no enclosing begin/end (Sec 13.3
//     permits this for tasks/functions, unlike initial/always): this
//     object model's TaskFunc::getStmt() holds exactly one Any*, so more
//     than one top-level statement must resolve to some single grouping
//     node. This file does not assert what that node's type is; it only
//     descends into it if it happens to be a Begin, which is sufficient to
//     reach the explicit nested begin/end block that actually contains
//     each assertion.
//   - The condition expression inside "connect_phase" (a parameterized
//     static class method call, `uvm_resource_db#(...)::read_by_name(...)`)
//     is only checked for presence, not for its internal call structure --
//     that is UVM library usage, not IEEE assertion syntax.
//   - The contents of the `uvm_info`/`uvm_error` macro expansions, the
//     `virtual inverter_if` handle's resolution, phases/objections, and the
//     DUT/interface instantiation in module top are all UVM-library and
//     structural concerns unrelated to the assertion construct itself.
//   - Runtime pass/fail behavior of the assertion cannot be observed: HLC
//     is a compiler/elaborator with no simulation.
// ============================================================================

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/immediate_assert.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/task.h>
#include <hldb/task_func.h>

#include <hlc/Tests/Test.h>

namespace hlc {
namespace {
// Descends only through explicit Begin scopes to find the first
// ImmediateAssert nested anywhere underneath "node" -- see the file header
// for why the search is limited to this one node kind.
const hldb::ImmediateAssert *FindImmediateAssert(const hldb::Any *node) {
  if (node == nullptr) {
    return nullptr;
  }
  if (const hldb::ImmediateAssert *const found = any_cast<hldb::ImmediateAssert>(node)) {
    return found;
  }
  if (const hldb::Begin *const scope = any_cast<hldb::Begin>(node)) {
    if (scope->getStmts() != nullptr) {
      for (const hldb::Any *const stmt : *scope->getStmts()) {
        if (const hldb::ImmediateAssert *const found = FindImmediateAssert(stmt)) {
          return found;
        }
      }
    }
  }
  return nullptr;
}
}  // namespace

class Assert0UvmTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16.2--assert0-uvm.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to Assert0UvmTest go here!

TEST_F(Assert0UvmTest, ClassEnvExists) {
  const hldb::ClassDefn *const env = hldb::findByName<hldb::ClassDefn>("env", m_design->getAllClasses());
  ASSERT_NE(env, nullptr) << "class 'env' not found";
}

TEST_F(Assert0UvmTest, ClassEnvHasConnectPhaseFunctionAndRunPhaseTask) {
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

TEST_F(Assert0UvmTest, ConnectPhaseContainsPlainImmediateAssert) {
  const hldb::ClassDefn *const env = hldb::findByName<hldb::ClassDefn>("env", m_design->getAllClasses());
  ASSERT_NE(env, nullptr);
  const hldb::TaskFunc *const connectPhase = hldb::findByName<hldb::TaskFunc>("connect_phase", env->getMethods());
  ASSERT_NE(connectPhase, nullptr);

  const hldb::ImmediateAssert *const assertStmt = FindImmediateAssert(connectPhase->getStmt());
  ASSERT_NE(assertStmt, nullptr) << "'assert(uvm_resource_db#(...)::read_by_name(...));' should be an "
                                     "ImmediateAssert somewhere in connect_phase's body";
  EXPECT_FALSE(assertStmt->getIsDeferred()) << "plain 'assert(...)' (no '#0'/'final') is not deferred";
  EXPECT_FALSE(assertStmt->getIsFinal());
  EXPECT_NE(assertStmt->getExpr(), nullptr) << "the read_by_name(...) call is the assertion condition";
}

TEST_F(Assert0UvmTest, RunPhaseContainsZeroDeferredImmediateAssert) {
  const hldb::ClassDefn *const env = hldb::findByName<hldb::ClassDefn>("env", m_design->getAllClasses());
  ASSERT_NE(env, nullptr);
  const hldb::TaskFunc *const runPhase = hldb::findByName<hldb::TaskFunc>("run_phase", env->getMethods());
  ASSERT_NE(runPhase, nullptr);

  const hldb::ImmediateAssert *const assertStmt = FindImmediateAssert(runPhase->getStmt());
  ASSERT_NE(assertStmt, nullptr) << "'assert #0 (m_if.a != m_if.b) else ...;' should be an ImmediateAssert "
                                     "somewhere in run_phase's body";

  EXPECT_TRUE(assertStmt->getIsDeferred()) << "'assert #0' is the zero-deferred form (Sec 16.4)";
  EXPECT_FALSE(assertStmt->getIsFinal()) << "'#0' distinguishes this from 'assert final', which is "
                                             "deferred but also final";
  EXPECT_EQ(assertStmt->getStmt(), nullptr) << "the source gives no explicit pass action";
  EXPECT_NE(assertStmt->getElseStmt(), nullptr) << "'else `uvm_error(...)' should produce an else-clause";
}

TEST_F(Assert0UvmTest, RunPhaseAssertConditionIsNotEqualComparison) {
  const hldb::ClassDefn *const env = hldb::findByName<hldb::ClassDefn>("env", m_design->getAllClasses());
  ASSERT_NE(env, nullptr);
  const hldb::TaskFunc *const runPhase = hldb::findByName<hldb::TaskFunc>("run_phase", env->getMethods());
  ASSERT_NE(runPhase, nullptr);
  const hldb::ImmediateAssert *const assertStmt = FindImmediateAssert(runPhase->getStmt());
  ASSERT_NE(assertStmt, nullptr);

  ASSERT_NE(assertStmt->getExpr(), nullptr);
  const hldb::Operation *const cond = any_cast<hldb::Operation>(assertStmt->getExpr());
  ASSERT_NE(cond, nullptr) << "'m_if.a != m_if.b' should be an Operation";
  EXPECT_EQ(cond->getOpType(), vpiNeqOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  EXPECT_EQ(cond->getOperands()->size(), 2u);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
