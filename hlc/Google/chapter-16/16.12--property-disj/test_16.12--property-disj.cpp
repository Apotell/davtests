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
// tests/Google/chapter-16/16.12--property-disj.sv
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
// :name: property_disj_test
// :description: property || test
// :tags: 16.12
// */
// module top();
//
// logic clk;
// logic a;
// logic b;
//
// assert property ( @(posedge clk) a || b );
//
// endmodule
// ============================================================================
//
// IEEE 1800-2023 construct under test (Sec 16.12, "Declaring properties"):
// `assert property (@(posedge clk) a || b);` -- a property_spec written
// inline, combining a clocking event with the property disjunction
// operator "||" (Sec 16.12.10, "property || property").
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - module "top" exists.
//   - `assert property (...)` is reachable via
//     Scope::getConcurrentAssertions(), is an Assert, with no else-clause
//     (the source gives none).
//   - Assert::getProperty() is a PropertySpec whose getClockingEvent() is
//     an Operation (opType == vpiPosedgeOp) referencing "clk", and whose
//     getDisableCondition() is null (no "disable iff" here).
//   - PropertySpec::getPropertyExpr() is an Operation with opType ==
//     vpiLogOrOp ("||"), with two operands referencing "a" and "b".
//
// NOT CHECKED (out of scope; every assertion below states only what IEEE
// 1800-2023 requires -- none of it is based on reading a .log file or any
// other tool-output dump):
//   - The exact operand COUNT/ORDER within each Operation.
//   - Runtime pass/fail behavior of the assertion cannot be observed: HLC
//     is a compiler/elaborator with no simulation.
// ============================================================================

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/assert_stmt.h>
#include <hldb/concurrent_assertions.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/property_spec.h>
#include <hldb/ref_obj.h>

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
    }
  }
  return false;
}
}  // namespace

class PropertyDisjTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16.12--property-disj.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to PropertyDisjTest go here!

TEST_F(PropertyDisjTest, ModuleTopExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'top' not found";
}

TEST_F(PropertyDisjTest, AssertPropertyIsReachable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  ASSERT_NE(top->getConcurrentAssertions(), nullptr)
      << "'assert property (...)' should be reachable via Scope::getConcurrentAssertions()";
  ASSERT_EQ(top->getConcurrentAssertions()->size(), 1u);

  const hldb::ConcurrentAssertions *const item = top->getConcurrentAssertions()->front();
  ASSERT_NE(item, nullptr);
  const hldb::Assert *const assertProp = any_cast<hldb::Assert>(item);
  ASSERT_NE(assertProp, nullptr) << "'assert property (...)' should be an Assert";
  EXPECT_EQ(assertProp->getElseStmt(), nullptr) << "the source gives no else-clause";
}

TEST_F(PropertyDisjTest, PropertySpecHasPosedgeClockingEventAndNoDisableCondition) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Assert *const assertProp = any_cast<hldb::Assert>(top->getConcurrentAssertions()->front());
  ASSERT_NE(assertProp, nullptr);

  ASSERT_NE(assertProp->getProperty(), nullptr) << "the property_spec is the asserted property and must be present";
  const hldb::PropertySpec *const spec = any_cast<hldb::PropertySpec>(assertProp->getProperty());
  ASSERT_NE(spec, nullptr) << "'@(posedge clk) a || b' should directly be a PropertySpec";

  ASSERT_NE(spec->getClockingEvent(), nullptr) << "'@(posedge clk)' is the clocking event";
  const hldb::Operation *const clockOp = any_cast<hldb::Operation>(spec->getClockingEvent());
  ASSERT_NE(clockOp, nullptr) << "the clocking event should be an Operation";
  EXPECT_EQ(clockOp->getOpType(), vpiPosedgeOp);
  EXPECT_TRUE(OperandsContainNamedRef(clockOp, std::string_view("clk"))) << "the posedge operand should reference 'clk'";

  EXPECT_EQ(spec->getDisableCondition(), nullptr) << "no 'disable iff' was written";
}

TEST_F(PropertyDisjTest, PropertyExprIsLogicalOrOfAAndB) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Assert *const assertProp = any_cast<hldb::Assert>(top->getConcurrentAssertions()->front());
  ASSERT_NE(assertProp, nullptr);
  const hldb::PropertySpec *const spec = any_cast<hldb::PropertySpec>(assertProp->getProperty());
  ASSERT_NE(spec, nullptr);

  ASSERT_NE(spec->getPropertyExpr(), nullptr) << "'a || b' is the property expression";
  const hldb::Operation *const orOp = any_cast<hldb::Operation>(spec->getPropertyExpr());
  ASSERT_NE(orOp, nullptr) << "'||' should be an Operation";
  EXPECT_EQ(orOp->getOpType(), vpiLogOrOp) << "'||' should be vpiLogOrOp";

  ASSERT_NE(orOp->getOperands(), nullptr);
  ASSERT_EQ(orOp->getOperands()->size(), 2u) << "'||' takes exactly two operands";
  EXPECT_TRUE(OperandsContainNamedRef(orOp, "a")) << "'a' should be an operand of '||'";
  EXPECT_TRUE(OperandsContainNamedRef(orOp, "b")) << "'b' should be an operand of '||'";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
