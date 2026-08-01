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

// Spec-based validation of IEEE 1800-2017 ss.6.20.5 specparam scope rule
// (invalid use of specparam name in a parameter constant_expression).
// SV: tests/Google/chapter-6/6.20.5--specparam_inv.sv
//
//   module top();
//       specparam delay = 50;
//       parameter p = delay + 2;   // INVALID -- see below
//   endmodule
//
// -- ss.6.20.5 rules under test -----------------------------------------------
//
// Specparam scope restriction (ss.6.20.5):
//   * A specparam name can only be used inside specify blocks and as arguments
//     to system timing checks (e.g. $setup, $hold).
//   * Using a specparam name in a parameter constant_expression is INVALID.
//     'parameter p = delay + 2' violates this rule.
//   * The compiler must reject this construct. Surelog emits:
//     [ERR:EL0535] Illegal implicit net "id:13, name:delay"
//     because 'delay' is not visible as a constant in the parameter expression
//     context; Surelog resolves it as an implicit net (RefObj) instead.
//
// Error-recovery representation (structural reference, not spec-mandated):
//   * Surelog still produces a partial HLDB despite the error:
//       - 'delay' lives in getSpecParams() as a SpecParam (correctly isolated).
//       - 'p' lives in getParameters() as a Parameter (parsed despite the error).
//       - The ParamAssign RHS for 'p' is an add Operation whose left operand
//         is a RefObj named "delay" (Surelog's implicit-net substitution) and
//         whose right operand is Constant "2".
//   * This documents that specparam names do NOT resolve as SpecParam nodes in
//     parameter expressions -- they become unresolved implicit-net references.
//
// -- UHDM tree ----------------------------------------------------------------
//
//   Module name:top  [ERROR: EL0535 on line 19]
//   +-- getParameters() (AnyCollection, 1 item)
//   |   +-- [0] Parameter name:"p"  localParam: false
//   |           typespec: RefTypespec -> LogicTypespec  (implicit, no keyword)
//   +-- getParamAssigns() (ParamAssignCollection, 1 item)
//   |   +-- [0] ParamAssign
//   |           lhs: RefObj name:"p"
//   |           rhs: Operation { opType: vpiAddOp (24) }
//   |               operands[0]: RefObj name:"delay"  (implicit net, NOT SpecParam)
//   |               operands[1]: Constant { vpiDecompile: "2" }
//   +-- getSpecParams() (SpecParamCollection, 1 item)
//       +-- [0] SpecParam name:"delay"
//               getExprs()[0]: Constant { vpiDecompile: "50" }
//
// -- VPI constants ------------------------------------------------------------
//   vpiAddOp = 24  (binary addition, vpi_user.h)

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/spec_param.h>
#include <hldb/vpi_user.h>

#include <string>

namespace hlc {

class SpecparamInvTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.20.5--specparam_inv.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("top", d->getAllModules());
}

static const hldb::SpecParam *getSpecParam(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getSpecParams()) return nullptr;
  return hldb::findByName<hldb::SpecParam>(name, m->getSpecParams());
}

static const hldb::Parameter *getParam(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getParameters()) return nullptr;
  return hldb::findByName<hldb::Parameter>(name, m->getParameters());
}

static const hldb::ParamAssign *getParamAssign(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m) return nullptr;
  return hldb::findByName<hldb::ParamAssign>(name, m->getParamAssigns());
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(SpecparamInvTest, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'top' not found"; }

// ===========================================================================
// specparam delay = 50  (valid on its own)
// ===========================================================================

// ss.6.20.5: 'specparam delay' is syntactically valid; the specparam
// collection must exist.
TEST_F(SpecparamInvTest, SpecParamCollectionExists) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(m->getSpecParams(), nullptr) << "ss.6.20.5: 'specparam delay' must produce a non-null specparam collection";
}

// ss.6.20.5: exactly one specparam is declared.
TEST_F(SpecparamInvTest, SpecParamCount) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getSpecParams(), nullptr);
  EXPECT_EQ(m->getSpecParams()->size(), 1u) << "module 'top' declares exactly one specparam: delay";
}

// ss.6.20.5: the specparam 'delay' must appear in getSpecParams().
TEST_F(SpecparamInvTest, Delay_Exists) {
  EXPECT_NE(getSpecParam(m_design, "delay"), nullptr) << "ss.6.20.5: 'specparam delay' must appear in getSpecParams()";
}

// ===========================================================================
// parameter p = delay + 2  (invalid -- specparam used in param expression)
// ===========================================================================

// The parameter 'p' is syntactically parsed and appears in getParameters()
// despite the error on its RHS expression.
TEST_F(SpecparamInvTest, ParameterCollectionExists) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(m->getParameters(), nullptr) << "module 'top' must have a parameter collection (parameter p is declared)";
}

// Exactly one parameter 'p' is declared.
TEST_F(SpecparamInvTest, ParameterCount) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 1u) << "module 'top' declares exactly one parameter: p";
}

TEST_F(SpecparamInvTest, P_Exists) { EXPECT_NE(getParam(m_design, "p"), nullptr) << "'p' not found in parameters"; }

// 'parameter' (without localparam) must not be marked as a localparam.
TEST_F(SpecparamInvTest, P_IsNotLocalParam) {
  const hldb::Parameter *p = getParam(m_design, "p");
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->getLocalParam()) << "ss.6.20.2: 'parameter p' must NOT be marked as a localparam";
}

// The ParamAssign for 'p' must exist.
TEST_F(SpecparamInvTest, P_ParamAssignExists) {
  EXPECT_NE(getParamAssign(m_design, "p"), nullptr) << "ParamAssign for 'p' not found";
}

// The RHS of 'p' is the compound expression 'delay + 2'; it must be an
// Operation node (not a plain Constant).
TEST_F(SpecparamInvTest, P_RhsIsOperation) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p");
  ASSERT_NE(pa, nullptr);
  EXPECT_NE(pa->getRhs<hldb::Operation>(), nullptr) << "'delay + 2' must be represented as an Operation node";
}

// The '+' operator must have opType vpiAddOp (24).
TEST_F(SpecparamInvTest, P_Rhs_IsAddOperation) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *op = pa->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiAddOp) << "'+' must have opType vpiAddOp (24)";
}

// The add operation has exactly two operands: the 'delay' reference and '2'.
TEST_F(SpecparamInvTest, P_Rhs_HasTwoOperands) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *op = pa->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u) << "'delay + 2' must have exactly two operands";
}

// ss.6.20.5: 'delay' used in the parameter expression is NOT resolved as the
// SpecParam node. Surelog treats it as an implicit net (RefObj) and emits
// EL0535. This test documents that specparam names have no visibility in
// parameter constant_expressions.
TEST_F(SpecparamInvTest, P_Rhs_LeftOperand_IsRefObj) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *op = pa->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  EXPECT_NE(any_cast<hldb::RefObj>((*op->getOperands())[0]), nullptr)
      << "ss.6.20.5: 'delay' in parameter expression must NOT resolve as a "
         "SpecParam -- it becomes a RefObj (implicit net) and triggers EL0535";
}

// The right operand '2' must be a Constant.
TEST_F(SpecparamInvTest, P_Rhs_RightOperand_IsConstant) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *op = pa->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  EXPECT_NE(any_cast<hldb::Constant>((*op->getOperands())[1]), nullptr) << "'2' in 'delay + 2' must be a Constant node";
}

// The right operand must decompile to "2".
TEST_F(SpecparamInvTest, P_Rhs_RightOperand_Is2) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p");
  ASSERT_NE(pa, nullptr);
  const hldb::Operation *op = pa->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const hldb::Constant *c = any_cast<hldb::Constant>((*op->getOperands())[1]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getDecompile()), "2") << "'2' in 'delay + 2' must decompile to \"2\"";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
