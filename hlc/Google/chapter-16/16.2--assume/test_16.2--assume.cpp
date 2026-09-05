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
// tests/Google/chapter-16/16.2--assume.sv
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
// :name: assume_test
// :description: assume test
// :tags: 16.2
// */
// module top(input logic a);
//
// initial assume (a != 0);
//
// endmodule
// ============================================================================
//
// IEEE 1800-2023 construct under test (Sec 16.2/16.3, "The immediate assert
// statement" / "The immediate assume statement"):
//   `assume (expr);` -- a plain immediate assume statement, used as the
//   single statement of an `initial` procedural block (no begin/end, no
//   pass/else clauses). Sec 16.3: "assume" has the same syntax as "assert"
//   (Sec 16.2) but tells the tool the expression is a constraint to rely on
//   rather than a property to verify.
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - module "top" exists and declares one input port, "a".
//   - the initial block binds the ImmediateAssume directly as its statement
//     (no begin/end was written, so no Begin wrapper should exist).
//   - it is an ImmediateAssume with getIsDeferred() == false and
//     getIsFinal() == false -- a plain immediate assume is not a deferred
//     form (Sec 16.4 deferred assertions apply to "assert"/"assume #0" and
//     "assert final"/"assume final", not to the plain immediate form used
//     here).
//   - its condition is the comparison "a != 0" (Operation, vpiNeqOp, two
//     operands: RefObj "a" and a Constant "0").
//   - no pass statement and no else-clause are present -- the source has
//     neither, so getStmt() and getElseStmt() should both be null.
//
// NOT CHECKED (out of scope; every assertion below states only what IEEE
// 1800-2023 requires -- none of it is based on reading a .log file or any
// other tool-output dump):
//   - Runtime pass/fail behavior of the assumption cannot be observed: HLC
//     is a compiler/elaborator with no simulation.
//   - Any tool-specific handling of "assume" as a formal-verification
//     constraint (e.g. how a formal tool would use it) is outside the scope
//     of what this compiler-level object model represents.
// ============================================================================

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/begin.h>
#include <hldb/design.h>
#include <hldb/immediate_assume.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ports.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_obj.h>

#include <hlc/Tests/Test.h>

namespace hlc {
class AssumeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16.2--assume.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to AssumeTest go here!

TEST_F(AssumeTest, ModuleTopExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'top' not found";
}

TEST_F(AssumeTest, ModuleTopHasOneInputPortNamedA) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 1u) << "'module top(input logic a);' declares exactly one port";
  EXPECT_EQ(top->getPorts()->front()->getName(), "a");
}

TEST_F(AssumeTest, InitialBindsAssumeDirectlyWithNoBeginWrapper) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u) << "module top has exactly one process (the initial block)";

  const hldb::Process *const process = top->getProcesses()->front();
  ASSERT_NE(process, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(process);
  ASSERT_NE(init, nullptr) << "top's process should specifically be an Initial block";

  ASSERT_NE(init->getStmt(), nullptr) << "'initial assume (a != 0);' with no begin/end should bind "
                                          "directly as the Initial's statement";
  EXPECT_EQ(any_cast<hldb::Begin>(init->getStmt()), nullptr)
      << "no begin/end was written, so the statement must not be wrapped in a Begin";
}

TEST_F(AssumeTest, AssumeIsPlainImmediateAssume) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);

  const hldb::ImmediateAssume *const assumeStmt = any_cast<hldb::ImmediateAssume>(init->getStmt());
  ASSERT_NE(assumeStmt, nullptr) << "'assume (a != 0);' should be an ImmediateAssume";

  EXPECT_FALSE(assumeStmt->getIsDeferred()) << "plain 'assume' (no '#0'/'final') is not deferred (Sec 16.3/16.4)";
  EXPECT_FALSE(assumeStmt->getIsFinal()) << "plain 'assume' is not the 'final' deferred form";
}

TEST_F(AssumeTest, AssumeConditionIsNotEqualComparison) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);
  const hldb::ImmediateAssume *const assumeStmt = any_cast<hldb::ImmediateAssume>(init->getStmt());
  ASSERT_NE(assumeStmt, nullptr);

  ASSERT_NE(assumeStmt->getExpr(), nullptr) << "'a != 0' is the assumption condition and must be present";
  const hldb::Operation *const cond = any_cast<hldb::Operation>(assumeStmt->getExpr());
  ASSERT_NE(cond, nullptr) << "'a != 0' should be an Operation";
  EXPECT_EQ(cond->getOpType(), vpiNeqOp);

  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "a");
}

TEST_F(AssumeTest, AssumeHasNoPassStmtAndNoElseStmt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);
  const hldb::ImmediateAssume *const assumeStmt = any_cast<hldb::ImmediateAssume>(init->getStmt());
  ASSERT_NE(assumeStmt, nullptr);

  EXPECT_EQ(assumeStmt->getStmt(), nullptr) << "'assume (a != 0);' has no pass action";
  EXPECT_EQ(assumeStmt->getElseStmt(), nullptr) << "'assume (a != 0);' has no else-clause";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
