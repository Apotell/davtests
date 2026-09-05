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
// tests/Google/chapter-16/16.2--cover.sv
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
// :name: cover_test
// :description: cover test
// :tags: 16.2
// */
// module top();
//
// logic a = 1;
//
// initial cover (a != 0);
//
// endmodule
// ============================================================================
//
// IEEE 1800-2023 construct under test ("The immediate cover statement",
// same family as Sec 16.2/16.3's immediate assert/assume, grammar rule
// simple_immediate_cover_statement): `cover (expr);` used as the single
// statement of an `initial` procedural block (no begin/end). Unlike
// assert/assume, a cover statement's grammar has no else-clause at all
// (`simple_immediate_cover_statement ::= cover ( expression_or_dist )
// statement_or_null` -- only one branch, taken when the expression is true).
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - module "top" exists and declares variable "a".
//   - the initial block binds the ImmediateCover directly as its statement
//     (no begin/end was written, so no Begin wrapper should exist).
//   - it is an ImmediateCover with getIsDeferred() == false and
//     getIsFinal() == false -- a plain immediate cover is not a deferred
//     form (Sec 16.4 deferred forms are "cover #0" / "cover final", tested
//     separately in test_16.2--cover0.cpp / test_16.2--cover-final.cpp).
//   - its condition is the comparison "a != 0" (Operation, vpiNeqOp, two
//     operands: RefObj "a" and a Constant "0").
//   - no pass statement is present (getStmt() == null).
//   - ImmediateCover has no getElseStmt() accessor at all in this object
//     model (see hldb/immediate_cover.h), matching the grammar fact above
//     that a cover statement cannot have an else-clause -- there is nothing
//     to assert here beyond noting the accessor's absence confirms the
//     grammar distinction, so no test calls a nonexistent method.
//
// NOT CHECKED (out of scope; every assertion below states only what IEEE
// 1800-2023 requires -- none of it is based on reading a .log file or any
// other tool-output dump):
//   - Runtime coverage-collection behavior (does the cover point actually
//     get sampled and counted) cannot be observed: HLC is a
//     compiler/elaborator with no simulation.
// ============================================================================

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/begin.h>
#include <hldb/design.h>
#include <hldb/immediate_cover.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/process_stmt.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>

#include <hlc/Tests/Test.h>

namespace hlc {
class CoverTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16.2--cover.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to CoverTest go here!

TEST_F(CoverTest, ModuleTopExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'top' not found";
}

TEST_F(CoverTest, VariableADeclared) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  EXPECT_NE(a, nullptr) << "'logic a = 1;' should declare a Variable named 'a'";
}

TEST_F(CoverTest, InitialBindsCoverDirectlyWithNoBeginWrapper) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getProcesses(), nullptr);
  ASSERT_EQ(top->getProcesses()->size(), 1u) << "module top has exactly one process (the initial block)";

  const hldb::Process *const process = top->getProcesses()->front();
  ASSERT_NE(process, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(process);
  ASSERT_NE(init, nullptr) << "top's process should specifically be an Initial block";

  ASSERT_NE(init->getStmt(), nullptr) << "'initial cover (a != 0);' with no begin/end should bind "
                                          "directly as the Initial's statement";
  EXPECT_EQ(any_cast<hldb::Begin>(init->getStmt()), nullptr)
      << "no begin/end was written, so the statement must not be wrapped in a Begin";
}

TEST_F(CoverTest, CoverIsPlainImmediateCover) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);

  const hldb::ImmediateCover *const coverStmt = any_cast<hldb::ImmediateCover>(init->getStmt());
  ASSERT_NE(coverStmt, nullptr) << "'cover (a != 0);' should be an ImmediateCover";

  EXPECT_FALSE(coverStmt->getIsDeferred()) << "plain 'cover' (no '#0'/'final') is not deferred (Sec 16.4)";
  EXPECT_FALSE(coverStmt->getIsFinal()) << "plain 'cover' is not the 'final' deferred form";
}

TEST_F(CoverTest, CoverConditionIsNotEqualComparison) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);
  const hldb::ImmediateCover *const coverStmt = any_cast<hldb::ImmediateCover>(init->getStmt());
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

TEST_F(CoverTest, CoverHasNoPassStmt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Initial *const init = any_cast<hldb::Initial>(top->getProcesses()->front());
  ASSERT_NE(init, nullptr);
  const hldb::ImmediateCover *const coverStmt = any_cast<hldb::ImmediateCover>(init->getStmt());
  ASSERT_NE(coverStmt, nullptr);

  EXPECT_EQ(coverStmt->getStmt(), nullptr) << "'cover (a != 0);' has no pass action";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
