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
// tests/Google/chapter-16/16.2--assume-uvm-fail.sv
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
// :name: assume_test_fail_uvm
// :description: failing assume test with UVM
// :should_fail_because: adder returns wrong value and assume expects correct result (a+b)
// :type: simulation
// :tags: uvm uvm-assertions
// */
//
// import uvm_pkg::*;
// `include "uvm_macros.svh"
//
// module adder (
//     input clk,
//     input [7:0] a,
//     input [7:0] b,
//     output reg [8:0] c
// );
//     always @ (posedge clk) begin
//         c <= a + a;                 // bug: should be "a + b"
//     end
// endmodule: adder
//
// interface adder_if(
//     output bit clk,
//     output reg [7:0] a,
//     output reg [7:0] b,
//     input [8:0] c
// );
// endinterface: adder_if
//
// string label = "ASSERT_UVM";
//
// class env extends uvm_env;
//     virtual adder_if m_if;
//
//     function new(string name, uvm_component parent = null);
//         super.new(name, parent);
//     endfunction
//
//     function void connect_phase(uvm_phase phase);
//         `uvm_info(label, "Started connect phase", UVM_LOW);
//         assert(uvm_resource_db#(virtual adder_if)::read_by_name(
//             get_full_name(), "adder_if", m_if));
//         `uvm_info(label, "Finished connect phase", UVM_LOW);
//     endfunction: connect_phase
//
//     task run_phase(uvm_phase phase);
//         phase.raise_objection(this);
//         `uvm_info(label, "Started run phase", UVM_LOW);
//         begin
//             int a = 8'h35, b = 8'h79;
//             @(m_if.clk);
//             m_if.a <= a;
//             m_if.b <= b;
//
//             repeat(3) @(m_if.clk);
//                 assume (m_if.c == (a + b)) else `uvm_error(label, $sformatf("c(%0d) != a + b(%0d) :assert: (False)", m_if.c, a + b));
//         end
//         `uvm_info(label, "Finished run phase", UVM_LOW);
//         phase.drop_objection(this);
//     endtask: run_phase
// endclass
//
// module top;
//     env environment;
//     adder_if dif();
//     adder dut(.clk(dif.clk), .a(dif.a), .b(dif.b), .c(dif.c));
//
//     initial begin
//         environment = new("env");
//         uvm_resource_db#(virtual adder_if)::set("env", "adder_if", dif);
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
// IEEE 1800-2023 constructs under test (Sec 16.2 / Sec 16.3):
//   Structurally identical to test_16.2--assume-uvm.cpp: a plain immediate
//   assert in connect_phase, and a plain immediate assume in run_phase
//   checking "m_if.c == (a + b)" with an else-clause. The only difference
//   from that file is that this "adder" module has a deliberate bug
//   ("c <= a + a" instead of "c <= a + b", per the source's own
//   :should_fail_because: comment) -- a functional/simulation-time defect
//   that would make the assumption fail *at runtime*, not a difference in
//   the assertion's static syntax or shape. Since HLC is a compiler/
//   elaborator with no simulation, that bug is invisible at this level: the
//   compiled structure of the assumption is exactly the same as the
//   passing file's, so every assertion below is identical to
//   test_16.2--assume-uvm.cpp's by design, not by oversight.
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - class "env" exists (Design::getAllClasses()), with methods
//     "connect_phase" (a Function) and "run_phase" (a Task).
//   - "connect_phase" contains a plain ImmediateAssert (getIsDeferred() ==
//     false, getIsFinal() == false).
//   - "run_phase" contains an ImmediateAssume with getIsDeferred() == false
//     and getIsFinal() == false -- the plain immediate form (Sec 16.3).
//   - that ImmediateAssume's condition is the comparison "m_if.c ==
//     (a + b)" (Operation, vpiEqOp, two operands), it has an else-clause
//     (getElseStmt() non-null), and no pass action (getStmt() == null).
//   - the search for the ImmediateAssume descends only through explicit
//     Begin scopes, matching the source's one literal begin/end block in
//     run_phase -- see helper function FindImmediateAssume below.
//
// NOT CHECKED (out of scope; every assertion below states only what IEEE
// 1800-2023 requires -- none of it is based on reading a .log file or any
// other tool-output dump):
//   - The "adder" module's internal logic ("c <= a + a") is not inspected
//     at all -- confirming a design bug like this one requires either
//     simulation or a dataflow/value analysis of the always block, neither
//     of which is what this file's assertion-focused tests exercise.
//   - The exact statement layout produced for a task/function body with
//     multiple top-level statements and no enclosing begin/end (see
//     test_16.2--assert-uvm.cpp for the same reasoning).
//   - The "(a + b)" addition inside the assumption condition is only
//     confirmed to exist as the second operand; its own internal shape is
//     not asserted.
//   - The condition expression inside "connect_phase" is only checked for
//     presence, not for its internal call structure.
//   - Runtime pass/fail behavior of the assumption -- including whether it
//     actually fails because of the adder bug -- cannot be observed: HLC is
//     a compiler/elaborator with no simulation.
// ============================================================================

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/begin.h>
#include <hldb/class_defn.h>
#include <hldb/design.h>
#include <hldb/function.h>
#include <hldb/immediate_assert.h>
#include <hldb/immediate_assume.h>
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

// Same traversal as FindImmediateAssert, but for ImmediateAssume.
const hldb::ImmediateAssume *FindImmediateAssume(const hldb::Any *node) {
  if (node == nullptr) {
    return nullptr;
  }
  if (const hldb::ImmediateAssume *const found = any_cast<hldb::ImmediateAssume>(node)) {
    return found;
  }
  if (const hldb::Begin *const scope = any_cast<hldb::Begin>(node)) {
    if (scope->getStmts() != nullptr) {
      for (const hldb::Any *const stmt : *scope->getStmts()) {
        if (const hldb::ImmediateAssume *const found = FindImmediateAssume(stmt)) {
          return found;
        }
      }
    }
  }
  return nullptr;
}
}  // namespace

class AssumeUvmFailTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16.2--assume-uvm-fail.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to AssumeUvmFailTest go here!

TEST_F(AssumeUvmFailTest, ClassEnvExists) {
  const hldb::ClassDefn *const env = hldb::findByName<hldb::ClassDefn>("env", m_design->getAllClasses());
  ASSERT_NE(env, nullptr) << "class 'env' not found";
}

TEST_F(AssumeUvmFailTest, ClassEnvHasConnectPhaseFunctionAndRunPhaseTask) {
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

TEST_F(AssumeUvmFailTest, ConnectPhaseContainsPlainImmediateAssert) {
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

TEST_F(AssumeUvmFailTest, RunPhaseContainsPlainImmediateAssume) {
  const hldb::ClassDefn *const env = hldb::findByName<hldb::ClassDefn>("env", m_design->getAllClasses());
  ASSERT_NE(env, nullptr);
  const hldb::TaskFunc *const runPhase = hldb::findByName<hldb::TaskFunc>("run_phase", env->getMethods());
  ASSERT_NE(runPhase, nullptr);

  const hldb::ImmediateAssume *const assumeStmt = FindImmediateAssume(runPhase->getStmt());
  ASSERT_NE(assumeStmt, nullptr) << "'assume (m_if.c == (a + b)) else ...;' should be an ImmediateAssume "
                                     "somewhere in run_phase's body";

  EXPECT_FALSE(assumeStmt->getIsDeferred()) << "plain 'assume(...)' (no '#0'/'final') is not deferred (Sec 16.3)";
  EXPECT_FALSE(assumeStmt->getIsFinal());
  EXPECT_EQ(assumeStmt->getStmt(), nullptr) << "the source gives no explicit pass action";
  EXPECT_NE(assumeStmt->getElseStmt(), nullptr) << "'else `uvm_error(...)' should produce an else-clause";
}

TEST_F(AssumeUvmFailTest, RunPhaseAssumeConditionIsEqualComparison) {
  const hldb::ClassDefn *const env = hldb::findByName<hldb::ClassDefn>("env", m_design->getAllClasses());
  ASSERT_NE(env, nullptr);
  const hldb::TaskFunc *const runPhase = hldb::findByName<hldb::TaskFunc>("run_phase", env->getMethods());
  ASSERT_NE(runPhase, nullptr);
  const hldb::ImmediateAssume *const assumeStmt = FindImmediateAssume(runPhase->getStmt());
  ASSERT_NE(assumeStmt, nullptr);

  ASSERT_NE(assumeStmt->getExpr(), nullptr);
  const hldb::Operation *const cond = any_cast<hldb::Operation>(assumeStmt->getExpr());
  ASSERT_NE(cond, nullptr) << "'m_if.c == (a + b)' should be an Operation";
  EXPECT_EQ(cond->getOpType(), vpiEqOp);
  ASSERT_NE(cond->getOperands(), nullptr);
  EXPECT_EQ(cond->getOperands()->size(), 2u);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
