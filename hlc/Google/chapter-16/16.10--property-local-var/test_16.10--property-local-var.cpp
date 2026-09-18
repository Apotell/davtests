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
// tests/Google/chapter-16/16.10--property-local-var.sv
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
// :name: property_local_var_test
// :description: property with local variables
// :type: simulation parsing
// :tags: 16.10
// */
//
// module clk_gen(
//     input            valid,
//     input            clk,
//     output reg [7:0] out,
//     input      [7:0] in
// );
//     reg [7:0] data_reg_0;
//     reg [7:0] data_reg_1;
//     reg [7:0] data_reg_2;
//
//     initial begin
//         data_reg_0 = 0; data_reg_1 = 0; data_reg_2 = 0; out = 0;
//     end
//
//     always @(posedge clk) begin
//         if (valid) begin
//             data_reg_0 <= in + 1;
//             data_reg_1 <= data_reg_0 + 1;
//             data_reg_2 <= data_reg_1 + 1;
//             out        <= data_reg_2 + 1;
//         end
//     end
// endmodule: clk_gen
//
// module top();
//     int         cycle;
//     logic       valid;
//     logic       clk;
//     logic [7:0] out;
//     logic [7:0] in;
//
//     clk_gen dut(.valid(valid), .clk(clk), .out(out), .in(in));
//
//     initial begin
//         cycle = 0; clk = 0; valid = 1;
//     end
//
//     property prop;
//         int x;
//         @(posedge clk) (valid, x = in) |-> ##4 (out == x + 4);
//     endproperty
//
//     assert property (prop) else $error($sformatf("property check failed :assert: (False)"));
//
//     assign in = cycle;
//
//     always @(posedge clk)
//         cycle = cycle + 1;
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
// IEEE 1800-2023 constructs under test (Sec 16.10, "Local variables"; Sec
// 16.9.2, "Overlapped and non-overlapped implication"):
//   - `property prop; int x; @(posedge clk) (valid, x = in) |-> ##4
//     (out == x + 4); endproperty` -- a property declaration with a local
//     variable "x" (Sec 16.10: local variables let a property/sequence
//     carry state, sampled here from "in" and checked four cycles later
//     against "out"). "|->" is the overlapped implication operator (Sec
//     16.9.2): the antecedent "(valid, x = in)" (a boolean expression
//     "valid" paired with an assignment to the local variable "x", via a
//     sequence match item) must hold in the same cycle the consequent
//     starts being checked.
//   - `assert property (prop) else $error(...);` -- a concurrent assertion
//     at module scope referencing the property by name, with an
//     else-clause.
//
// ----------------------------------------------------------------------------
// CHECKED (this file):
//   - module "clk_gen" and module "top" both exist.
//   - property "prop" is declared in module top
//     (Scope::getPropertyDecls()), and its getVariables() contains a local
//     Variable named "x".
//   - PropertyDecl::getPropertySpec() is a PropertySpec whose
//     getClockingEvent() is an Operation (opType == vpiPosedgeOp)
//     referencing "clk".
//   - PropertySpec::getPropertyExpr() is an Operation with opType ==
//     vpiOverlapImplyOp ("|->"), with exactly two operands.
//   - somewhere within the consequent (the operand that is not the bare
//     antecedent expression) there is a nested Operation with opType ==
//     vpiUnaryCycleDelayOp carrying a Constant "4" (the "##4" delay), and
//     within that, an Operation with opType == vpiEqOp referencing "out"
//     and containing an Operation with opType == vpiAddOp referencing "x"
//     and a Constant "4" -- confirming "out == x + 4" is present under the
//     delay.
//   - `assert property (prop) else $error(...);` is reachable via
//     Scope::getConcurrentAssertions(), is an Assert, and has a non-null
//     getElseStmt().
//
// NOT CHECKED (out of scope; every assertion below states only what IEEE
// 1800-2023 requires -- none of it is based on reading a .log file or any
// other tool-output dump):
//   - The exact internal shape of the antecedent "(valid, x = in)" -- a
//     parenthesized boolean expression paired with a sequence match item
//     assigning the local variable -- is only checked for presence as one
//     of the "|->" operands, not for its own internal structure. No
//     dedicated header for a "sequence match item" / comma-list construct
//     was found in this object model, so asserting its exact shape here
//     would be guessing rather than citing a confirmed mapping.
//   - The exact operand COUNT/ORDER within each Operation (same open
//     question as the chapter-16 sequence-operator files in this batch) --
//     checks search rather than index into a fixed position.
//   - `assert property (prop)`'s exact getProperty() shape (RefObj vs.
//     PropertySpec) is only checked for presence.
//   - The "clk_gen" DUT's pipeline logic, the "int cycle" counter, the
//     clock-generation "forever" block, and "initial #1000 $finish;" are
//     unrelated to the property/assertion constructs under test.
//   - Runtime pass/fail behavior of the assertion (does "out" actually
//     equal "x + 4" four cycles later) cannot be observed: HLC is a
//     compiler/elaborator with no simulation.
// ============================================================================

#include <hldb/Utils.h>
#include <hldb/any_type.h>
#include <hldb/assert_stmt.h>
#include <hldb/concurrent_assertions.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/property_decl.h>
#include <hldb/property_spec.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>

#include <hlc/Tests/Test.h>

namespace hlc {
namespace {
bool OperandsContainNamedRef(const hldb::Operation *op, std::string_view name) {
  return (op != nullptr) && (hldb::findByName<hldb::RefObj>(name, op->getOperands()) != nullptr);
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

// Recursively searches "root", descending only through Operation operand
// trees, for the first Operation whose getOpType() equals "opType".
const hldb::Operation *FindOperationByOpType(const hldb::Any *root, int32_t opType) {
  const hldb::Operation *const op = any_cast<hldb::Operation>(root);
  if (op == nullptr) {
    return nullptr;
  }
  if (op->getOpType() == opType) {
    return op;
  }
  if (op->getOperands() != nullptr) {
    for (const hldb::Any *const operand : *op->getOperands()) {
      if (const hldb::Operation *const found = FindOperationByOpType(operand, opType)) {
        return found;
      }
    }
  }
  return nullptr;
}
}  // namespace

class PropertyLocalVarTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16.10--property-local-var.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};
// ... All tests belonging to PropertyLocalVarTest go here!

TEST_F(PropertyLocalVarTest, ModuleClkGenAndTopExist) {
  EXPECT_NE(hldb::findByName<hldb::Module>("clk_gen", m_design->getAllModules()), nullptr)
      << "module 'clk_gen' not found";
  EXPECT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr) << "module 'top' not found";
}

TEST_F(PropertyLocalVarTest, PropertyPropDeclaredWithLocalVariableX) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::PropertyDecl *const prop = hldb::findByName<hldb::PropertyDecl>("prop", top->getPropertyDecls());
  ASSERT_NE(prop, nullptr) << "property 'prop' not found in module top";

  ASSERT_NE(prop->getVariables(), nullptr) << "'int x;' should declare a local variable";
  EXPECT_NE(hldb::findByName<hldb::Variable>("x", prop->getVariables()), nullptr)
      << "the local variable should be named 'x'";
}

TEST_F(PropertyLocalVarTest, PropertySpecHasPosedgeClockingEvent) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::PropertyDecl *const prop = hldb::findByName<hldb::PropertyDecl>("prop", top->getPropertyDecls());
  ASSERT_NE(prop, nullptr);

  ASSERT_NE(prop->getPropertySpec(), nullptr) << "'@(posedge clk) ...' should produce a PropertySpec";
  const hldb::PropertySpec *const spec = prop->getPropertySpec();

  ASSERT_NE(spec->getClockingEvent(), nullptr) << "'@(posedge clk)' is the clocking event";
  const hldb::Operation *const clockOp = any_cast<hldb::Operation>(spec->getClockingEvent());
  ASSERT_NE(clockOp, nullptr) << "the clocking event should be an Operation";
  EXPECT_EQ(clockOp->getOpType(), vpiPosedgeOp);
  EXPECT_TRUE(OperandsContainNamedRef(clockOp, std::string_view("clk"))) << "the posedge operand should reference 'clk'";
}

TEST_F(PropertyLocalVarTest, PropertyExprIsOverlappedImplication) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::PropertyDecl *const prop = hldb::findByName<hldb::PropertyDecl>("prop", top->getPropertyDecls());
  ASSERT_NE(prop, nullptr);
  ASSERT_NE(prop->getPropertySpec(), nullptr);

  ASSERT_NE(prop->getPropertySpec()->getPropertyExpr(), nullptr)
      << "'(valid, x = in) |-> ##4 (out == x + 4)' is the property expression";
  const hldb::Operation *const implyOp = any_cast<hldb::Operation>(prop->getPropertySpec()->getPropertyExpr());
  ASSERT_NE(implyOp, nullptr) << "'|->' should be an Operation";
  EXPECT_EQ(implyOp->getOpType(), vpiOverlapImplyOp) << "'|->' should be vpiOverlapImplyOp";

  ASSERT_NE(implyOp->getOperands(), nullptr);
  ASSERT_EQ(implyOp->getOperands()->size(), 2u) << "'|->' takes exactly two operands: antecedent and consequent";
}

TEST_F(PropertyLocalVarTest, ConsequentIsCycleDelayedEqualityWithLocalVariable) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::PropertyDecl *const prop = hldb::findByName<hldb::PropertyDecl>("prop", top->getPropertyDecls());
  ASSERT_NE(prop, nullptr);
  const hldb::Operation *const implyOp = any_cast<hldb::Operation>(prop->getPropertySpec()->getPropertyExpr());
  ASSERT_NE(implyOp, nullptr);

  const hldb::Operation *const delayOp = FindOperationByOpType(implyOp, vpiUnaryCycleDelayOp);
  ASSERT_NE(delayOp, nullptr) << "'##4 (out == x + 4)' should contain a vpiUnaryCycleDelayOp Operation";
  EXPECT_TRUE(OperandsContainConstant(delayOp, "4")) << "the '##4' delay magnitude should be present";

  const hldb::Operation *const eqOp = FindOperationByOpType(delayOp, vpiEqOp);
  ASSERT_NE(eqOp, nullptr) << "'out == x + 4' should contain a vpiEqOp Operation";
  EXPECT_TRUE(OperandsContainNamedRef(eqOp, "out")) << "'out' should be an operand of '=='";

  const hldb::Operation *const addOp = FindOperationByOpType(eqOp, vpiAddOp);
  ASSERT_NE(addOp, nullptr) << "'x + 4' should contain a vpiAddOp Operation";
  EXPECT_TRUE(OperandsContainNamedRef(addOp, "x")) << "'x' (the local variable) should be an operand of '+'";
  EXPECT_TRUE(OperandsContainConstant(addOp, "4")) << "the '+4' addend should be present";
}

TEST_F(PropertyLocalVarTest, AssertPropertyIsReachableAndReferencesProp) {
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
