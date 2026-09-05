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
// tests/Google/chapter-16/16.2--cover-final.sv
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
// :name: cover_final_test
// :description: cover final test
// :tags: 16.4
// */
// module top();
//
// logic a = 1;
//
// cover final (a != 0);
//
// endmodule
// ============================================================================
//
// IEEE 1800-2023 construct under test (Sec 16.4, "Deferred assertions"):
//   `cover final (expr);` -- a "final" deferred immediate cover, written
//   directly as a module item. "final" defers evaluation to the very end of
//   simulation, as opposed to "#0" which defers only to the end of the
//   current time step (see test_16.2--cover0.cpp).
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - module "top" exists and declares variable "a".
//   - the cover is reachable via Instance::getAssertions(), not via any
//     process.
//   - it is an ImmediateCover with getIsDeferred() == true and
//     getIsFinal() == true.
//   - its condition is the comparison "a != 0" (Operation, vpiNeqOp, two
//     operands: RefObj "a" and a Constant "0").
//   - no pass statement is present (getStmt() == null).
//   - ImmediateCover has no getElseStmt() accessor at all in this object
//     model, matching the grammar fact that a cover statement never has an
//     else-clause.
//
// NOT CHECKED (out of scope; every assertion below states only what IEEE
// 1800-2023 requires -- none of it is based on reading a .log file or any
// other tool-output dump):
//   - Runtime coverage-collection behavior cannot be observed: HLC is a
//     compiler/elaborator with no simulation.
//
// Note: this file's "final" flag check follows the same shape as
// test_16.2--assert-final.cpp, where getIsFinal() was found (via an actual
// build+test run, not a log read) to not currently come back true for
// "assert final". If the same turns out to hold for "cover final" here,
// that is a real, separately-confirmable finding for this construct, not an
// assumption carried over from the assert file.
// ============================================================================

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/design.h>
#include <hldb/immediate_cover.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>

#include <hlc/Tests/Test.h>

namespace hlc {
class CoverFinalTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16.2--cover-final.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to CoverFinalTest go here!

TEST_F(CoverFinalTest, ModuleTopExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'top' not found";
}

TEST_F(CoverFinalTest, VariableADeclared) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  EXPECT_NE(a, nullptr) << "'logic a = 1;' should declare a Variable named 'a'";
}

TEST_F(CoverFinalTest, CoverIsReachableAsModuleItemNotProcess) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  ASSERT_NE(top->getAssertions(), nullptr)
      << "'cover final (...)' is a deferred immediate assertion, legal as a module item per "
         "IEEE 1800-2023 Sec 16.4, and should be reachable via Instance::getAssertions()";
  ASSERT_EQ(top->getAssertions()->size(), 1u) << "module top contains exactly one assertion item";

  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty())
      << "the source has no initial/always block -- the cover must not be wrapped in one";
}

TEST_F(CoverFinalTest, CoverIsFinalDeferredImmediateCover) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getAssertions(), nullptr);
  ASSERT_EQ(top->getAssertions()->size(), 1u);

  const hldb::Any *const item = top->getAssertions()->at(0);
  ASSERT_NE(item, nullptr);
  const hldb::ImmediateCover *const coverStmt = any_cast<hldb::ImmediateCover>(item);
  ASSERT_NE(coverStmt, nullptr) << "'cover final (...)' should be an ImmediateCover";

  EXPECT_TRUE(coverStmt->getIsDeferred()) << "'cover final' is a deferred form (Sec 16.4)";
  EXPECT_TRUE(coverStmt->getIsFinal()) << "'final' distinguishes this from 'cover #0', which is "
                                           "deferred but not final";
}

TEST_F(CoverFinalTest, CoverConditionIsNotEqualComparison) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ImmediateCover *const coverStmt = any_cast<hldb::ImmediateCover>(top->getAssertions()->at(0));
  ASSERT_NE(coverStmt, nullptr);

  ASSERT_NE(coverStmt->getExpr(), nullptr) << "'a != 0' is the cover condition and must be present";
  const hldb::Operation *const cond = any_cast<hldb::Operation>(coverStmt->getExpr());
  ASSERT_NE(cond, nullptr) << "'a != 0' should be an Operation";
  EXPECT_EQ(cond->getOpType(), vpiNeqOp);

  ASSERT_NE(cond->getOperands(), nullptr);
  ASSERT_EQ(cond->getOperands()->size(), 2u);
  const hldb::RefObj *const lhs = any_cast<hldb::RefObj>(cond->getOperands()->at(0));
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "a");
}

TEST_F(CoverFinalTest, CoverHasNoPassStmt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ImmediateCover *const coverStmt = any_cast<hldb::ImmediateCover>(top->getAssertions()->at(0));
  ASSERT_NE(coverStmt, nullptr);

  EXPECT_EQ(coverStmt->getStmt(), nullptr) << "'cover final (a != 0);' has no pass action";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
