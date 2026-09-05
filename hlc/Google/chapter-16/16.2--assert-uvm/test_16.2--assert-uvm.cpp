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
// tests/Google/chapter-16/16.2--assert-uvm.sv
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
// :name: assert_test_uvm
// :description: assert test with UVM
// :type: simulation parsing
// :tags: uvm uvm-assertions
// :timeout: 60
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
//         c <= a + b;
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
//                 assert (m_if.c == (a + b)) else `uvm_error(label, $sformatf("c(%0d) != a + b(%0d) :assert: (False)", m_if.c, a + b));
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
// IEEE 1800-2023 constructs under test (Sec 16.2, "The immediate assert
// statement"):
//   - `assert(...);`                (in connect_phase) -- a plain immediate
//                                    assert, neither deferred nor final.
//   - `assert (m_if.c == (a + b)) else ...;` (in run_phase) -- this file's
//                                    focus: the plain immediate assert form,
//                                    with an else-clause but no explicit
//                                    pass action, following (not nested
//                                    inside) a `repeat(3) @(m_if.clk);`
//                                    statement in the same explicit
//                                    begin/end block.
//
// Unlike test_16.2--assert0-uvm.cpp / test_16.2--assert-final-uvm.cpp, this
// file's run_phase assertion uses neither "#0" nor "final" -- it is the
// plain immediate form, so both getIsDeferred() and getIsFinal() are
// expected false. The surrounding UVM machinery (phases, objections,
// `uvm_info`/`uvm_error`/`uvm_resource_db`, clock generation) is not itself
// IEEE 1800 assertion syntax and is out of scope here -- see NOT CHECKED
// below.
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - class "env" exists (Design::getAllClasses()), with methods
//     "connect_phase" (a Function) and "run_phase" (a Task).
//   - "connect_phase" contains a plain ImmediateAssert (getIsDeferred() ==
//     false, getIsFinal() == false).
//   - "run_phase" contains an ImmediateAssert with getIsDeferred() == false
//     and getIsFinal() == false -- the plain immediate form (Sec 16.2),
//     distinct from both deferred forms tested in the other two UVM files.
//   - that ImmediateAssert's condition is the comparison "m_if.c ==
//     (a + b)" (Operation, vpiEqOp, two operands), it has an else-clause
//     (getElseStmt() non-null, from "else `uvm_error(...)"), and no pass
//     action (getStmt() == null, since the source gives none).
//   - the search for each ImmediateAssert descends only through explicit
//     Begin scopes (matching the source's one literal begin/end block per
//     method), not into any other statement kind, and specifically does not
//     treat the preceding "repeat(3) @(m_if.clk);" loop statement as
//     containing the assertion -- they are separate, sibling statements in
//     the same begin/end block, not nested -- see helper function
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
//   - The "(a + b)" addition inside the assertion condition is only
//     confirmed to exist as the second operand; its own internal shape
//     (an Operation with vpiAddOp) is not asserted, to keep this file
//     focused on the assert construct itself rather than general
//     expression-tree coverage already exercised elsewhere (e.g. chapter 11).
//   - The condition expression inside "connect_phase" (a parameterized
//     static class method call, `uvm_resource_db#(...)::read_by_name(...)`)
//     is only checked for presence, not for its internal call structure --
//     that is UVM library usage, not IEEE assertion syntax.
//   - The "repeat(3) @(m_if.clk);" loop statement, the `uvm_info`/
//     `uvm_error` macro expansions, the `virtual adder_if` handle's
//     resolution, phases/objections, and the DUT/interface/clock-generation
//     structure in module top are all unrelated to the assertion construct
//     itself.
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

class AssertUvmTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16.2--assert-uvm.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to AssertUvmTest go here!

TEST_F(AssertUvmTest, ClassEnvExists) {
  const hldb::ClassDefn *const env = hldb::findByName<hldb::ClassDefn>("env", m_design->getAllClasses());
  ASSERT_NE(env, nullptr) << "class 'env' not found";
}

TEST_F(AssertUvmTest, ClassEnvHasConnectPhaseFunctionAndRunPhaseTask) {
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

TEST_F(AssertUvmTest, ConnectPhaseContainsPlainImmediateAssert) {
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

TEST_F(AssertUvmTest, RunPhaseContainsPlainImmediateAssert) {
  const hldb::ClassDefn *const env = hldb::findByName<hldb::ClassDefn>("env", m_design->getAllClasses());
  ASSERT_NE(env, nullptr);
  const hldb::TaskFunc *const runPhase = hldb::findByName<hldb::TaskFunc>("run_phase", env->getMethods());
  ASSERT_NE(runPhase, nullptr);

  const hldb::ImmediateAssert *const assertStmt = FindImmediateAssert(runPhase->getStmt());
  ASSERT_NE(assertStmt, nullptr) << "'assert (m_if.c == (a + b)) else ...;' should be an ImmediateAssert "
                                     "somewhere in run_phase's body";

  EXPECT_FALSE(assertStmt->getIsDeferred()) << "plain 'assert(...)' (no '#0'/'final') is not deferred (Sec 16.2)";
  EXPECT_FALSE(assertStmt->getIsFinal());
  EXPECT_EQ(assertStmt->getStmt(), nullptr) << "the source gives no explicit pass action";
  EXPECT_NE(assertStmt->getElseStmt(), nullptr) << "'else `uvm_error(...)' should produce an else-clause";
}

TEST_F(AssertUvmTest, RunPhaseAssertConditionIsEqualComparison) {
  const hldb::ClassDefn *const env = hldb::findByName<hldb::ClassDefn>("env", m_design->getAllClasses());
  ASSERT_NE(env, nullptr);
  const hldb::TaskFunc *const runPhase = hldb::findByName<hldb::TaskFunc>("run_phase", env->getMethods());
  ASSERT_NE(runPhase, nullptr);
  const hldb::ImmediateAssert *const assertStmt = FindImmediateAssert(runPhase->getStmt());
  ASSERT_NE(assertStmt, nullptr);

  ASSERT_NE(assertStmt->getExpr(), nullptr);
  const hldb::Operation *const cond = any_cast<hldb::Operation>(assertStmt->getExpr());
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
