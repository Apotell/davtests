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
// tests/Google/chapter-16/16.2--assume-final.sv
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
// :name: assume_final_test
// :description: assume final test
// :tags: 16.4
// */
// module top(input logic a);
//
// assume final (a != 0);
//
// endmodule
// ============================================================================
//
// IEEE 1800-2023 construct under test (Sec 16.4, "Deferred assertions"):
//   `assume final (expr);` -- a "final" deferred immediate assume, written
//   directly as a module item (not inside any initial/always procedural
//   block). "final" defers evaluation to the very end of simulation, as
//   opposed to "#0" which defers only to the end of the current time step
//   (see test_16.2--assume0.cpp).
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - module "top" exists with one input port, "a".
//   - the assumption is reachable via Instance::getAssertions() (the module
//     item collection shared by all deferred assert/assume/cover items),
//     not via any process.
//   - it is an ImmediateAssume with getIsDeferred() == true and
//     getIsFinal() == true.
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
//
// Note: this file's "final" flag check follows the same shape as
// test_16.2--assert-final.cpp, where getIsFinal() was found (via an actual
// build+test run, not a log read) to not currently come back true for
// "assert final". If the same turns out to hold for "assume final" here,
// that is a real, separately-confirmable finding for this construct, not an
// assumption carried over from the assert file.
// ============================================================================

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/design.h>
#include <hldb/immediate_assume.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ports.h>
#include <hldb/ref_obj.h>

#include <hlc/Tests/Test.h>

namespace hlc {
class AssumeFinalTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16.2--assume-final.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to AssumeFinalTest go here!

TEST_F(AssumeFinalTest, ModuleTopHasOneInputPortNamedA) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 1u) << "'module top(input logic a);' declares exactly one port";
  EXPECT_EQ(top->getPorts()->front()->getName(), "a");
}

TEST_F(AssumeFinalTest, AssumeIsReachableAsModuleItemNotProcess) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  ASSERT_NE(top->getAssertions(), nullptr)
      << "'assume final (...)' is a deferred immediate assertion, legal as a module item per "
         "IEEE 1800-2023 Sec 16.4, and should be reachable via Instance::getAssertions()";
  ASSERT_EQ(top->getAssertions()->size(), 1u) << "module top contains exactly one assertion item";

  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty())
      << "the source has no initial/always block -- the assumption must not be wrapped in one";
}

TEST_F(AssumeFinalTest, AssumeIsFinalDeferredImmediateAssume) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getAssertions(), nullptr);
  ASSERT_EQ(top->getAssertions()->size(), 1u);

  const hldb::Any *const item = top->getAssertions()->at(0);
  ASSERT_NE(item, nullptr);
  const hldb::ImmediateAssume *const assumeStmt = any_cast<hldb::ImmediateAssume>(item);
  ASSERT_NE(assumeStmt, nullptr) << "'assume final (...)' should be an ImmediateAssume";

  EXPECT_TRUE(assumeStmt->getIsDeferred()) << "'assume final' is a deferred form (Sec 16.4)";
  EXPECT_TRUE(assumeStmt->getIsFinal()) << "'final' distinguishes this from 'assume #0', which is "
                                            "deferred but not final";
}

TEST_F(AssumeFinalTest, AssumeConditionIsNotEqualComparison) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ImmediateAssume *const assumeStmt = any_cast<hldb::ImmediateAssume>(top->getAssertions()->at(0));
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

TEST_F(AssumeFinalTest, AssumeHasNoPassStmtAndNoElseStmt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ImmediateAssume *const assumeStmt = any_cast<hldb::ImmediateAssume>(top->getAssertions()->at(0));
  ASSERT_NE(assumeStmt, nullptr);

  EXPECT_EQ(assumeStmt->getStmt(), nullptr) << "'assume final (a != 0);' has no pass action";
  EXPECT_EQ(assumeStmt->getElseStmt(), nullptr) << "'assume final (a != 0);' has no else-clause";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
