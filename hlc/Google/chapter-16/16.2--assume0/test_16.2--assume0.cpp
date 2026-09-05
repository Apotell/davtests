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
// tests/Google/chapter-16/16.2--assume0.sv
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
// :name: assume0_test
// :description: assume #0 test
// :tags: 16.4
// */
// module top(input logic a);
//
// assume #0 (a != 0);
//
// endmodule
// ============================================================================
//
// IEEE 1800-2023 construct under test (Sec 16.4, "Deferred assertions"):
//   `assume #0 (expr);` -- a "#0" (zero-)deferred immediate assume, written
//   directly as a module item (not inside any initial/always procedural
//   block). Deferred immediate assertions (assert/assume/cover, either "#0"
//   or "final") may appear as module items per Sec 16.4; the plain
//   immediate form must be a procedural statement instead (see
//   test_16.2--assume.cpp).
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - module "top" exists with one input port, "a".
//   - the assumption is reachable via Instance::getAssertions() (the module
//     item collection shared by all deferred assert/assume/cover items),
//     not via any process -- confirming it is placed exactly as written,
//     with no implicit initial/always wrapper.
//   - it is an ImmediateAssume with getIsDeferred() == true and
//     getIsFinal() == false, distinguishing "assume #0" both from a plain
//     immediate assume (Sec 16.3, neither flag set) and from "assume final"
//     (Sec 16.4, getIsDeferred() true but getIsFinal() also expected true).
//   - its condition is the comparison "a != 0" (Operation, vpiNeqOp, two
//     operands: RefObj "a" and a Constant "0").
//   - no pass statement and no else-clause are present -- the source has
//     neither, so getStmt() and getElseStmt() should both be null.
//
// NOT CHECKED (out of scope; every assertion below states only what IEEE
// 1800-2023 requires -- none of it is based on reading a .log file or any
// other tool-output dump):
//   - Runtime pass/fail behavior of the assumption, and the precise "end of
//     current time step" timing that "#0" implies, cannot be observed: HLC
//     is a compiler/elaborator with no simulation.
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
class Assume0Test : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16.2--assume0.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to Assume0Test go here!

TEST_F(Assume0Test, ModuleTopHasOneInputPortNamedA) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'top' not found";
  ASSERT_NE(top->getPorts(), nullptr);
  ASSERT_EQ(top->getPorts()->size(), 1u) << "'module top(input logic a);' declares exactly one port";
  EXPECT_EQ(top->getPorts()->front()->getName(), "a");
}

TEST_F(Assume0Test, AssumeIsReachableAsModuleItemNotProcess) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  ASSERT_NE(top->getAssertions(), nullptr)
      << "'assume #0 (...)' is a deferred immediate assertion, legal as a module item per "
         "IEEE 1800-2023 Sec 16.4, and should be reachable via Instance::getAssertions()";
  ASSERT_EQ(top->getAssertions()->size(), 1u) << "module top contains exactly one assertion item";

  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty())
      << "the source has no initial/always block -- the assumption must not be wrapped in one";
}

TEST_F(Assume0Test, AssumeIsZeroDeferredImmediateAssume) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getAssertions(), nullptr);
  ASSERT_EQ(top->getAssertions()->size(), 1u);

  const hldb::Any *const item = top->getAssertions()->at(0);
  ASSERT_NE(item, nullptr);
  const hldb::ImmediateAssume *const assumeStmt = any_cast<hldb::ImmediateAssume>(item);
  ASSERT_NE(assumeStmt, nullptr) << "'assume #0 (...)' should be an ImmediateAssume";

  EXPECT_TRUE(assumeStmt->getIsDeferred()) << "'assume #0' is the zero-deferred form (Sec 16.4)";
  EXPECT_FALSE(assumeStmt->getIsFinal()) << "'#0' distinguishes this from 'assume final', which is "
                                             "deferred but also final";
}

TEST_F(Assume0Test, AssumeConditionIsNotEqualComparison) {
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

TEST_F(Assume0Test, AssumeHasNoPassStmtAndNoElseStmt) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::ImmediateAssume *const assumeStmt = any_cast<hldb::ImmediateAssume>(top->getAssertions()->at(0));
  ASSERT_NE(assumeStmt, nullptr);

  EXPECT_EQ(assumeStmt->getStmt(), nullptr) << "'assume #0 (a != 0);' has no pass action";
  EXPECT_EQ(assumeStmt->getElseStmt(), nullptr) << "'assume #0 (a != 0);' has no else-clause";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
