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
// tests/Google/chapter-16/16.14--assume-property.sv
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
// :name: assume_property_test
// :description: assume property test
// :tags: 16.14
// */
// module top();
//
// logic clk;
// logic a;
//
// assume property ( @(posedge clk) (a == 1));
//
// endmodule
// ============================================================================
//
// IEEE 1800-2023 construct under test (Sec 16.14, "The assume property
// statement"): `assume property (@(posedge clk) (a == 1));` -- a
// property_spec written inline as a concurrent assumption (as opposed to
// "assert property", which checks; "assume" tells the tool the property is
// a constraint to rely on). Per the VPI object model, this is modeled by
// Assume (Assert's sibling class under ConcurrentAssertions).
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - module "top" exists.
//   - `assume property (...)` is reachable via
//     Scope::getConcurrentAssertions(), is an Assume, with no else-clause
//     (the source gives none).
//   - Assume::getProperty() is a PropertySpec whose getClockingEvent() is
//     an Operation (opType == vpiPosedgeOp) referencing "clk", and whose
//     getDisableCondition() is null.
//   - PropertySpec::getPropertyExpr() is an Operation with opType ==
//     vpiEqOp ("=="), with two operands: RefObj "a" and a Constant "1".
//
// NOT CHECKED (out of scope; every assertion below states only what IEEE
// 1800-2023 requires -- none of it is based on reading a .log file or any
// other tool-output dump):
//   - The exact operand COUNT/ORDER within each Operation.
//   - Runtime behavior of the assumption cannot be observed: HLC is a
//     compiler/elaborator with no simulation.
// ============================================================================

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/assume.h>
#include <hldb/concurrent_assertions.h>
#include <hldb/constant.h>
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

bool OperandsContainConstant(const hldb::Operation *op, std::string_view value) {
  if (op == nullptr || op->getOperands() == nullptr) {
    return false;
  }
  for (const hldb::Any *const operand : *op->getOperands()) {
    if (const hldb::Constant *const constant = any_cast<hldb::Constant>(operand)) {
      if (constant->getDecompile() == value) {
        return true;
      }
    }
  }
  return false;
}
}  // namespace

class AssumePropertyTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16.14--assume-property.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to AssumePropertyTest go here!

TEST_F(AssumePropertyTest, ModuleTopExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr) << "module 'top' not found";
}

TEST_F(AssumePropertyTest, AssumePropertyIsReachable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  ASSERT_NE(top->getConcurrentAssertions(), nullptr)
      << "'assume property (...)' should be reachable via Scope::getConcurrentAssertions()";
  ASSERT_EQ(top->getConcurrentAssertions()->size(), 1u);

  const hldb::ConcurrentAssertions *const item = top->getConcurrentAssertions()->front();
  ASSERT_NE(item, nullptr);
  const hldb::Assume *const assumeProp = any_cast<hldb::Assume>(item);
  ASSERT_NE(assumeProp, nullptr) << "'assume property (...)' should be an Assume";
  EXPECT_EQ(assumeProp->getElseStmt(), nullptr) << "the source gives no else-clause";
}

TEST_F(AssumePropertyTest, PropertySpecHasPosedgeClockingEventAndNoDisableCondition) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Assume *const assumeProp = any_cast<hldb::Assume>(top->getConcurrentAssertions()->front());
  ASSERT_NE(assumeProp, nullptr);

  ASSERT_NE(assumeProp->getProperty(), nullptr) << "the property_spec is the asserted property and must be present";
  const hldb::PropertySpec *const spec = any_cast<hldb::PropertySpec>(assumeProp->getProperty());
  ASSERT_NE(spec, nullptr) << "'@(posedge clk) (a == 1)' should directly be a PropertySpec";

  ASSERT_NE(spec->getClockingEvent(), nullptr) << "'@(posedge clk)' is the clocking event";
  const hldb::Operation *const clockOp = any_cast<hldb::Operation>(spec->getClockingEvent());
  ASSERT_NE(clockOp, nullptr) << "the clocking event should be an Operation";
  EXPECT_EQ(clockOp->getOpType(), vpiPosedgeOp);
  EXPECT_TRUE(OperandsContainNamedRef(clockOp, std::string_view("clk"))) << "the posedge operand should reference 'clk'";

  EXPECT_EQ(spec->getDisableCondition(), nullptr) << "no 'disable iff' was written";
}

TEST_F(AssumePropertyTest, PropertyExprIsEqualityOfAAndOne) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Assume *const assumeProp = any_cast<hldb::Assume>(top->getConcurrentAssertions()->front());
  ASSERT_NE(assumeProp, nullptr);
  const hldb::PropertySpec *const spec = any_cast<hldb::PropertySpec>(assumeProp->getProperty());
  ASSERT_NE(spec, nullptr);

  ASSERT_NE(spec->getPropertyExpr(), nullptr) << "'a == 1' is the property expression";
  const hldb::Operation *const eqOp = any_cast<hldb::Operation>(spec->getPropertyExpr());
  ASSERT_NE(eqOp, nullptr) << "'a == 1' should be an Operation";
  EXPECT_EQ(eqOp->getOpType(), vpiEqOp) << "'==' should be vpiEqOp";

  ASSERT_NE(eqOp->getOperands(), nullptr);
  ASSERT_EQ(eqOp->getOperands()->size(), 2u) << "'==' takes exactly two operands";
  EXPECT_TRUE(OperandsContainNamedRef(eqOp, "a")) << "'a' should be an operand of '=='";
  EXPECT_TRUE(OperandsContainConstant(eqOp, "1")) << "'1' should be an operand of '=='";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
