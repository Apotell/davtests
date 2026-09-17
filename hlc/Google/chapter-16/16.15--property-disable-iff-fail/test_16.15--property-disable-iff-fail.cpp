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
// tests/Google/chapter-16/16.15--property-disable-iff-fail.sv
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
// :name: property_disable_iff_fail_test
// :description: failing property with disable iff
// :should_fail_because: disable iff uses wrong reset polarity
// :type: simulation
// :tags: 16.15
// */
//
// module clk_gen(
//     input      rst,
//     input      clk,
//     output reg out
// );
//     initial begin out = 0; end
//     always @(posedge clk or posedge rst) begin
//         if (rst) out <= 0; else out <= 1;
//     end
// endmodule: clk_gen
//
// module top();
//     logic rst;
//     logic clk;
//     logic out;
//
//     clk_gen dut(.rst(rst), .clk(clk), .out(out));
//
//     initial begin clk = 0; rst = 1; end
//
//     property prop;
//         @(posedge clk) disable iff (~rst) out;
//     endproperty
//
//     assert property (prop) else $error($sformatf("property check failed :assert: (False)"));
//
//     initial begin
//         forever begin
//             #(50) clk = ~clk;
//         end
//     end
//
//     initial #1000 $finish;
// endmodule
// ============================================================================
//
// IEEE 1800-2023 construct under test: structurally almost identical to
// test_16.15--property-disable-iff.cpp -- same property "prop" with the
// same posedge clocking event and bare-reference property expression
// ("out") -- but here the disable condition is "~rst" (bitwise negation)
// instead of a bare "rst". Per the source's own :should_fail_because:
// comment, this inverts the reset polarity: the property is now disabled
// exactly when the DUT is NOT in reset (and active in the one moment it
// actually is being reset), the opposite of the intended behavior -- a
// functional/simulation-time defect, invisible to this compile-only
// object model (same reasoning as the other "-fail" files in this test
// suite). Unlike most "-fail" pairs, this one is a genuine difference in
// the property declaration itself, not just the DUT.
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - module "clk_gen" and module "top" both exist.
//   - property "prop" is declared in module top.
//   - PropertyDecl::getPropertySpec() is a PropertySpec whose
//     getClockingEvent() is an Operation (opType == vpiPosedgeOp)
//     referencing "clk".
//   - PropertySpec::getDisableCondition() is an Operation with opType ==
//     vpiBitNegOp ("~"), whose single operand references "rst" -- not a
//     bare RefObj this time, unlike the passing file.
//   - PropertySpec::getPropertyExpr() is a plain RefObj referencing "out".
//   - `assert property (prop) else $error(...);` is reachable via
//     Scope::getConcurrentAssertions(), is an Assert, with a non-null
//     getElseStmt().
//
// NOT CHECKED (out of scope; every assertion below states only what IEEE
// 1800-2023 requires -- none of it is based on reading a .log file or any
// other tool-output dump):
//   - The "clk_gen" DUT's reset/output logic, the clock-generation
//     "forever" block, and "initial #1000 $finish;" are unrelated to the
//     property construct under test.
//   - Runtime pass/fail behavior of the assertion -- including whether it
//     actually fails because of the inverted polarity -- cannot be
//     observed: HLC is a compiler/elaborator with no simulation.
// ============================================================================

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/assert_stmt.h>
#include <hldb/concurrent_assertions.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/property_decl.h>
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

class PropertyDisableIffFailTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16.15--property-disable-iff-fail.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to PropertyDisableIffFailTest go here!

TEST_F(PropertyDisableIffFailTest, ModuleClkGenAndTopExist) {
  EXPECT_NE(hldb::findByName<hldb::Module>("clk_gen", m_design->getAllModules()), nullptr)
      << "module 'clk_gen' not found";
  EXPECT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr) << "module 'top' not found";
}

TEST_F(PropertyDisableIffFailTest, PropertyPropDeclaredWithPosedgeClockingEventAndNegatedDisableCondition) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::PropertyDecl *const prop = hldb::findByName<hldb::PropertyDecl>("prop", top->getPropertyDecls());
  ASSERT_NE(prop, nullptr) << "property 'prop' not found in module top";

  ASSERT_NE(prop->getPropertySpec(), nullptr) << "'@(posedge clk) disable iff (~rst) out' should produce a "
                                                  "PropertySpec";
  const hldb::PropertySpec *const spec = prop->getPropertySpec();

  ASSERT_NE(spec->getClockingEvent(), nullptr) << "'@(posedge clk)' is the clocking event";
  const hldb::Operation *const clockOp = any_cast<hldb::Operation>(spec->getClockingEvent());
  ASSERT_NE(clockOp, nullptr) << "the clocking event should be an Operation";
  EXPECT_EQ(clockOp->getOpType(), vpiPosedgeOp);
  EXPECT_TRUE(OperandsContainNamedRef(clockOp, std::string_view("clk"))) << "the posedge operand should reference 'clk'";

  ASSERT_NE(spec->getDisableCondition(), nullptr) << "'disable iff (~rst)' should produce a disable condition";
  const hldb::Operation *const iffOp = spec->getDisableCondition<hldb::Operation>();
  ASSERT_NE(iffOp, nullptr) << "'~rst' should be an Operation, not a bare RefObj (unlike the passing file's "
                                "'disable iff (rst)')";

  EXPECT_EQ(iffOp->getOpType(), vpiIffOp);
  ASSERT_NE(iffOp->getOperands(), nullptr);
  ASSERT_EQ(iffOp->getOperands()->size(), 1u);

  const hldb::Operation *const negOp = any_cast<hldb::Operation>(iffOp->getOperands()->front());
  EXPECT_EQ(negOp->getOpType(), vpiBitNegOp) << "'~' should be vpiBitNegOp";
  ASSERT_NE(negOp->getOperands(), nullptr);
  ASSERT_EQ(negOp->getOperands()->size(), 1u) << "'~' is unary";
  EXPECT_TRUE(OperandsContainNamedRef(negOp, "rst")) << "'rst' should be the operand of '~'";
}

TEST_F(PropertyDisableIffFailTest, PropertyExprIsBareReferenceToOut) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::PropertyDecl *const prop = hldb::findByName<hldb::PropertyDecl>("prop", top->getPropertyDecls());
  ASSERT_NE(prop, nullptr);
  ASSERT_NE(prop->getPropertySpec(), nullptr);

  ASSERT_NE(prop->getPropertySpec()->getPropertyExpr(), nullptr) << "'out' is the property expression";
  const hldb::RefObj *const outRef = any_cast<hldb::RefObj>(prop->getPropertySpec()->getPropertyExpr());
  ASSERT_NE(outRef, nullptr) << "'out' should be a plain RefObj, not an Operation";
  EXPECT_EQ(outRef->getName(), "out");
}

TEST_F(PropertyDisableIffFailTest, AssertPropertyIsReachableAndReferencesProp) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);

  ASSERT_NE(top->getConcurrentAssertions(), nullptr)
      << "'assert property (prop) else $error(...);' should be reachable via Scope::getConcurrentAssertions()";
  ASSERT_EQ(top->getConcurrentAssertions()->size(), 1u);

  const hldb::ConcurrentAssertions *const item = top->getConcurrentAssertions()->front();
  ASSERT_NE(item, nullptr);
  const hldb::Assert *const assertProp = any_cast<hldb::Assert>(item);
  ASSERT_NE(assertProp, nullptr) << "'assert property (...)' should be an Assert";

  EXPECT_NE(assertProp->getProperty(), nullptr) << "'prop' is the asserted property and must be present";
  EXPECT_NE(assertProp->getElseStmt(), nullptr) << "'else $error(...)' should produce an else-clause";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
