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

// Spec-based validation of IEEE 1800-2017 ss.6.20.2 parameter declarations:
// untyped dependent parameters and explicitly-typed parameters.
// SV: tests/Google/chapter-6/6.20.2--parameter_dep.sv
//
//   module top();
//       parameter        p1 = 123;
//       parameter        p2 = p1 * 3;
//       parameter int    p3 = 0;
//       parameter byte   p4 = 1;
//       parameter integer p5 = 3;
//       parameter longint p6 = 8;
//   endmodule
//
// -- ss.6.20.2 rules under test -----------------------------------------------
//
// (A) Untyped parameters (p1, p2):
//   * Default value is a constant_expression (ss.11.2.1).
//   * A constant_expression may reference previously-declared parameters in
//     the same scope (ss.6.20.2).
//   * 'parameter p2 = p1 * 3' is a dependent parameter: UHDM preserves the
//     expression tree as Operation{vpiMultOp(25)}, not a folded constant.
//
// (B) Typed parameters (p3-p6) -- ss.6.20.2 + ss.6.11:
//   * A parameter may carry an explicit data type (ss.6.20.2).
//   * The declared type is recorded as a typespec accessible via
//     Parameter::getTypespec() -> RefTypespec -> getActual<T>().
//   * ss.6.11.2 sizes and signedness (2-state types):
//       int     -- 32-bit signed  (IntTypespec)
//       byte    --  8-bit signed  (ByteTypespec)
//       longint -- 64-bit signed  (LongIntTypespec)
//   * ss.6.11.1 sizes and signedness (4-state type):
//       integer -- 32-bit signed  (IntegerTypespec)
//   * All four types are signed by default per the spec.
//   * The type identity (e.g. IntTypespec vs ByteTypespec) is the proof that
//     the correct bit-width was understood: each class maps to exactly one
//     width mandated by ss.6.11.
//
// -- UHDM tree ----------------------------------------------------------------
//
//   Module name:work@top
//   +-- getParameters() (AnyCollection, 6 items)
//   |   +-- [0] Parameter name:"p1"  localParam: false  (no typespec)
//   |   +-- [1] Parameter name:"p2"  localParam: false  (no typespec)
//   |   +-- [2] Parameter name:"p3"  localParam: false
//   |   |       typespec: RefTypespec -> IntTypespec      (32-bit signed 2-state)
//   |   +-- [3] Parameter name:"p4"  localParam: false
//   |   |       typespec: RefTypespec -> ByteTypespec     ( 8-bit signed 2-state)
//   |   +-- [4] Parameter name:"p5"  localParam: false
//   |   |       typespec: RefTypespec -> IntegerTypespec  (32-bit signed 4-state)
//   |   +-- [5] Parameter name:"p6"  localParam: false
//   |           typespec: RefTypespec -> LongIntTypespec  (64-bit signed 2-state)
//   +-- getParamAssigns() (ParamAssignCollection, 6 items)
//       +-- [0] ParamAssign  lhs:"p1"  rhs: Constant { decompile:"123" }
//       +-- [1] ParamAssign  lhs:"p2"  rhs: Operation { vpiMultOp(25) }
//       |           +-- operands[0]: RefObj { name:"p1" }
//       |           +-- operands[1]: Constant { decompile:"3" }
//       +-- [2] ParamAssign  lhs:"p3"  rhs: Constant { decompile:"0" }
//       +-- [3] ParamAssign  lhs:"p4"  rhs: Constant { decompile:"1" }
//       +-- [4] ParamAssign  lhs:"p5"  rhs: Constant { decompile:"3" }
//       +-- [5] ParamAssign  lhs:"p6"  rhs: Constant { decompile:"8" }
//
// NOTE: Surelog stores parameter default expressions via ParamAssign nodes
// (Scope::getParamAssigns()), not directly in Parameter::getExpr(). Access
// the default value by finding the ParamAssign by LHS name and calling
// getRhs<T>().
//
// -- VPI constants ------------------------------------------------------------
//   vpiMultOp    = 25   (binary multiplication, vpi_user.h)
//   vpiUIntConst = 9    (unsigned int constant, vpi_user.h)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/byte_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/integer_typespec.h>
#include <hldb/long_int_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>

#include <string>

namespace hlc {

class ParameterDepTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.20.2--parameter_dep.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@top", d->getAllModules());
}

static const hldb::Parameter *getParam(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getParameters()) return nullptr;
  return hldb::findByName<hldb::Parameter>(name, m->getParameters());
}

// Returns the ParamAssign for the named parameter (matched via getLhs name).
static const hldb::ParamAssign *getParamAssign(const hldb::Design *d, std::string_view name) {
  const hldb::Module *m = getTop(d);
  if (!m) return nullptr;
  return hldb::findByName<hldb::ParamAssign>(name, m->getParamAssigns());
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(ParameterDepTest, ModuleExists) { ASSERT_NE(getTop(m_design), nullptr) << "module 'work@top' not found"; }

// ===========================================================================
// Parameters -- collection
// ===========================================================================

TEST_F(ParameterDepTest, ParameterCollectionExists) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  EXPECT_NE(m->getParameters(), nullptr) << "module 'top' must have a parameter collection";
}

TEST_F(ParameterDepTest, ParameterCount) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 6u) << "top declares exactly six parameters: p1 through p6";
}

// ===========================================================================
// Parameter p1 = 123  (untyped)
// ===========================================================================

TEST_F(ParameterDepTest, P1_Exists) { EXPECT_NE(getParam(m_design, "p1"), nullptr) << "'p1' not found"; }

TEST_F(ParameterDepTest, P1_IsNotLocalParam) {
  const hldb::Parameter *p = getParam(m_design, "p1");
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->getLocalParam()) << "'parameter p1' must not be marked as a localparam";
}

TEST_F(ParameterDepTest, P1_RhsIsConstant) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p1");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p1' not found";
  EXPECT_NE(pa->getRhs<hldb::Constant>(), nullptr) << "'p1 = 123': RHS must be a Constant";
}

TEST_F(ParameterDepTest, P1_RhsDecompile) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p1");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p1' not found";
  const hldb::Constant *c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "'p1 = 123': RHS must be a Constant";
  EXPECT_EQ(std::string(c->getDecompile()), "123") << "'p1 = 123': decompile must be \"123\"";
}

// ===========================================================================
// Parameter p2 = p1 * 3  (untyped, dependent)
// ===========================================================================

TEST_F(ParameterDepTest, P2_Exists) { EXPECT_NE(getParam(m_design, "p2"), nullptr) << "'p2' not found"; }

TEST_F(ParameterDepTest, P2_IsNotLocalParam) {
  const hldb::Parameter *p = getParam(m_design, "p2");
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->getLocalParam()) << "'parameter p2' must not be marked as a localparam";
}

TEST_F(ParameterDepTest, P2_RhsIsOperation) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p2");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p2' not found";
  EXPECT_NE(pa->getRhs<hldb::Operation>(), nullptr)
      << "'p2 = p1 * 3': RHS must be an Operation (expression tree preserved)";
}

TEST_F(ParameterDepTest, P2_OpType_IsMultiply) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p2");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p2' not found";
  const hldb::Operation *op = pa->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "'p2 = p1 * 3': RHS must be an Operation";
  EXPECT_EQ(op->getOpType(), vpiMultOp) << "'p1 * 3' must produce vpiMultOp (25)";
}

TEST_F(ParameterDepTest, P2_OperandsCount) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p2");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p2' not found";
  const hldb::Operation *op = pa->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "'p2 = p1 * 3': RHS must be an Operation";
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u) << "'p1 * 3': binary multiply must have exactly two operands";
}

TEST_F(ParameterDepTest, P2_LeftOperand_IsRefObj) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p2");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p2' not found";
  const hldb::Operation *op = pa->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "'p2 = p1 * 3': RHS must be an Operation";
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  EXPECT_NE(any_cast<const hldb::RefObj *>((*op->getOperands())[0]), nullptr)
      << "'p1 * 3': left operand must be RefObj (reference to p1)";
}

TEST_F(ParameterDepTest, P2_LeftOperand_RefersToP1) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p2");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p2' not found";
  const hldb::Operation *op = pa->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "'p2 = p1 * 3': RHS must be an Operation";
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const hldb::RefObj *ref = any_cast<const hldb::RefObj *>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr) << "'p1 * 3': left operand must be RefObj";
  EXPECT_EQ(ref->getName(), "p1") << "'p1 * 3': left operand RefObj must name \"p1\"";
}

TEST_F(ParameterDepTest, P2_RightOperand_IsConstant) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p2");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p2' not found";
  const hldb::Operation *op = pa->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "'p2 = p1 * 3': RHS must be an Operation";
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  EXPECT_NE(any_cast<const hldb::Constant *>((*op->getOperands())[1]), nullptr)
      << "'p1 * 3': right operand must be Constant (literal 3)";
}

TEST_F(ParameterDepTest, P2_RightOperand_Decompile) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p2");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p2' not found";
  const hldb::Operation *op = pa->getRhs<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "'p2 = p1 * 3': RHS must be an Operation";
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const hldb::Constant *c = any_cast<const hldb::Constant *>((*op->getOperands())[1]);
  ASSERT_NE(c, nullptr) << "'p1 * 3': right operand must be Constant";
  EXPECT_EQ(std::string(c->getDecompile()), "3") << "'p1 * 3': right constant must decompile to \"3\"";
}

// ===========================================================================
// Parameter int p3 = 0
// ss.6.11.2: 'int' is a 2-state signed 32-bit integer.
// IntTypespec identity verifies both type recognition and 32-bit size.
// ===========================================================================

TEST_F(ParameterDepTest, P3_Exists) { EXPECT_NE(getParam(m_design, "p3"), nullptr) << "'p3' not found"; }

TEST_F(ParameterDepTest, P3_IsNotLocalParam) {
  const hldb::Parameter *p = getParam(m_design, "p3");
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->getLocalParam()) << "'parameter int p3' must not be marked as a localparam";
}

// ss.6.20.2: a parameter with an explicit type must carry a typespec.
TEST_F(ParameterDepTest, P3_TypespecExists) {
  const hldb::Parameter *p = getParam(m_design, "p3");
  ASSERT_NE(p, nullptr);
  EXPECT_NE(p->getTypespec(), nullptr) << "'parameter int p3': must have a non-null typespec";
}

// ss.6.11.2: 'int' maps to IntTypespec (32-bit 2-state).
TEST_F(ParameterDepTest, P3_Typespec_IsIntTypespec) {
  const hldb::Parameter *p = getParam(m_design, "p3");
  ASSERT_NE(p, nullptr);
  const hldb::RefTypespec *rt = p->getTypespec();
  ASSERT_NE(rt, nullptr) << "'parameter int p3': typespec must be non-null";
  EXPECT_NE(rt->getActual<hldb::IntTypespec>(), nullptr) << "ss.6.11.2: 'int' must resolve to IntTypespec (32-bit)";
}

// ss.6.11.2: 'int' is signed by default.
TEST_F(ParameterDepTest, P3_Typespec_IsSigned) {
  const hldb::Parameter *p = getParam(m_design, "p3");
  ASSERT_NE(p, nullptr);
  const hldb::RefTypespec *rt = p->getTypespec();
  ASSERT_NE(rt, nullptr);
  const hldb::IntTypespec *ts = rt->getActual<hldb::IntTypespec>();
  ASSERT_NE(ts, nullptr);
  EXPECT_TRUE(ts->getSigned()) << "ss.6.11.2: 'int' is a signed type";
}

TEST_F(ParameterDepTest, P3_RhsIsConstant) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p3");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p3' not found";
  EXPECT_NE(pa->getRhs<hldb::Constant>(), nullptr) << "'p3 = 0': RHS must be a Constant";
}

TEST_F(ParameterDepTest, P3_RhsDecompile) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p3");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p3' not found";
  const hldb::Constant *c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "'p3 = 0': RHS must be a Constant";
  EXPECT_EQ(std::string(c->getDecompile()), "0") << "'p3 = 0': decompile must be \"0\"";
}

// ===========================================================================
// Parameter byte p4 = 1
// ss.6.11.2: 'byte' is a 2-state signed 8-bit integer.
// ByteTypespec identity verifies both type recognition and 8-bit size.
// ===========================================================================

TEST_F(ParameterDepTest, P4_Exists) { EXPECT_NE(getParam(m_design, "p4"), nullptr) << "'p4' not found"; }

TEST_F(ParameterDepTest, P4_IsNotLocalParam) {
  const hldb::Parameter *p = getParam(m_design, "p4");
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->getLocalParam()) << "'parameter byte p4' must not be marked as a localparam";
}

TEST_F(ParameterDepTest, P4_TypespecExists) {
  const hldb::Parameter *p = getParam(m_design, "p4");
  ASSERT_NE(p, nullptr);
  EXPECT_NE(p->getTypespec(), nullptr) << "'parameter byte p4': must have a non-null typespec";
}

// ss.6.11.2: 'byte' maps to ByteTypespec (8-bit 2-state).
TEST_F(ParameterDepTest, P4_Typespec_IsByteTypespec) {
  const hldb::Parameter *p = getParam(m_design, "p4");
  ASSERT_NE(p, nullptr);
  const hldb::RefTypespec *rt = p->getTypespec();
  ASSERT_NE(rt, nullptr) << "'parameter byte p4': typespec must be non-null";
  EXPECT_NE(rt->getActual<hldb::ByteTypespec>(), nullptr) << "ss.6.11.2: 'byte' must resolve to ByteTypespec (8-bit)";
}

// ss.6.11.2: 'byte' is signed by default.
TEST_F(ParameterDepTest, P4_Typespec_IsSigned) {
  const hldb::Parameter *p = getParam(m_design, "p4");
  ASSERT_NE(p, nullptr);
  const hldb::RefTypespec *rt = p->getTypespec();
  ASSERT_NE(rt, nullptr);
  const hldb::ByteTypespec *ts = rt->getActual<hldb::ByteTypespec>();
  ASSERT_NE(ts, nullptr);
  EXPECT_TRUE(ts->getSigned()) << "ss.6.11.2: 'byte' is a signed type";
}

TEST_F(ParameterDepTest, P4_RhsIsConstant) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p4");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p4' not found";
  EXPECT_NE(pa->getRhs<hldb::Constant>(), nullptr) << "'p4 = 1': RHS must be a Constant";
}

TEST_F(ParameterDepTest, P4_RhsDecompile) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p4");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p4' not found";
  const hldb::Constant *c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "'p4 = 1': RHS must be a Constant";
  EXPECT_EQ(std::string(c->getDecompile()), "1") << "'p4 = 1': decompile must be \"1\"";
}

// ===========================================================================
// Parameter integer p5 = 3
// ss.6.11.1: 'integer' is a 4-state signed 32-bit integer.
// IntegerTypespec identity verifies both type recognition and 32-bit size.
// Unlike 'int' (2-state), 'integer' is a 4-state type supporting X and Z.
// ===========================================================================

TEST_F(ParameterDepTest, P5_Exists) { EXPECT_NE(getParam(m_design, "p5"), nullptr) << "'p5' not found"; }

TEST_F(ParameterDepTest, P5_IsNotLocalParam) {
  const hldb::Parameter *p = getParam(m_design, "p5");
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->getLocalParam()) << "'parameter integer p5' must not be marked as a localparam";
}

TEST_F(ParameterDepTest, P5_TypespecExists) {
  const hldb::Parameter *p = getParam(m_design, "p5");
  ASSERT_NE(p, nullptr);
  EXPECT_NE(p->getTypespec(), nullptr) << "'parameter integer p5': must have a non-null typespec";
}

// ss.6.11.1: 'integer' maps to IntegerTypespec (32-bit 4-state), NOT IntTypespec.
TEST_F(ParameterDepTest, P5_Typespec_IsIntegerTypespec) {
  const hldb::Parameter *p = getParam(m_design, "p5");
  ASSERT_NE(p, nullptr);
  const hldb::RefTypespec *rt = p->getTypespec();
  ASSERT_NE(rt, nullptr) << "'parameter integer p5': typespec must be non-null";
  EXPECT_NE(rt->getActual<hldb::IntegerTypespec>(), nullptr)
      << "ss.6.11.1: 'integer' must resolve to IntegerTypespec (32-bit 4-state)";
}

// ss.6.11.1: 'integer' is signed by default.
TEST_F(ParameterDepTest, P5_Typespec_IsSigned) {
  const hldb::Parameter *p = getParam(m_design, "p5");
  ASSERT_NE(p, nullptr);
  const hldb::RefTypespec *rt = p->getTypespec();
  ASSERT_NE(rt, nullptr);
  const hldb::IntegerTypespec *ts = rt->getActual<hldb::IntegerTypespec>();
  ASSERT_NE(ts, nullptr);
  EXPECT_TRUE(ts->getSigned()) << "ss.6.11.1: 'integer' is a signed type";
}

TEST_F(ParameterDepTest, P5_RhsIsConstant) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p5");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p5' not found";
  EXPECT_NE(pa->getRhs<hldb::Constant>(), nullptr) << "'p5 = 3': RHS must be a Constant";
}

TEST_F(ParameterDepTest, P5_RhsDecompile) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p5");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p5' not found";
  const hldb::Constant *c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "'p5 = 3': RHS must be a Constant";
  EXPECT_EQ(std::string(c->getDecompile()), "3") << "'p5 = 3': decompile must be \"3\"";
}

// ===========================================================================
// Parameter longint p6 = 8
// ss.6.11.2: 'longint' is a 2-state signed 64-bit integer.
// LongIntTypespec identity verifies both type recognition and 64-bit size.
// ===========================================================================

TEST_F(ParameterDepTest, P6_Exists) { EXPECT_NE(getParam(m_design, "p6"), nullptr) << "'p6' not found"; }

TEST_F(ParameterDepTest, P6_IsNotLocalParam) {
  const hldb::Parameter *p = getParam(m_design, "p6");
  ASSERT_NE(p, nullptr);
  EXPECT_FALSE(p->getLocalParam()) << "'parameter longint p6' must not be marked as a localparam";
}

TEST_F(ParameterDepTest, P6_TypespecExists) {
  const hldb::Parameter *p = getParam(m_design, "p6");
  ASSERT_NE(p, nullptr);
  EXPECT_NE(p->getTypespec(), nullptr) << "'parameter longint p6': must have a non-null typespec";
}

// ss.6.11.2: 'longint' maps to LongIntTypespec (64-bit 2-state).
TEST_F(ParameterDepTest, P6_Typespec_IsLongIntTypespec) {
  const hldb::Parameter *p = getParam(m_design, "p6");
  ASSERT_NE(p, nullptr);
  const hldb::RefTypespec *rt = p->getTypespec();
  ASSERT_NE(rt, nullptr) << "'parameter longint p6': typespec must be non-null";
  EXPECT_NE(rt->getActual<hldb::LongIntTypespec>(), nullptr)
      << "ss.6.11.2: 'longint' must resolve to LongIntTypespec (64-bit)";
}

// ss.6.11.2: 'longint' is signed by default.
TEST_F(ParameterDepTest, P6_Typespec_IsSigned) {
  const hldb::Parameter *p = getParam(m_design, "p6");
  ASSERT_NE(p, nullptr);
  const hldb::RefTypespec *rt = p->getTypespec();
  ASSERT_NE(rt, nullptr);
  const hldb::LongIntTypespec *ts = rt->getActual<hldb::LongIntTypespec>();
  ASSERT_NE(ts, nullptr);
  EXPECT_TRUE(ts->getSigned()) << "ss.6.11.2: 'longint' is a signed type";
}

TEST_F(ParameterDepTest, P6_RhsIsConstant) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p6");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p6' not found";
  EXPECT_NE(pa->getRhs<hldb::Constant>(), nullptr) << "'p6 = 8': RHS must be a Constant";
}

TEST_F(ParameterDepTest, P6_RhsDecompile) {
  const hldb::ParamAssign *pa = getParamAssign(m_design, "p6");
  ASSERT_NE(pa, nullptr) << "ParamAssign for 'p6' not found";
  const hldb::Constant *c = pa->getRhs<hldb::Constant>();
  ASSERT_NE(c, nullptr) << "'p6 = 8': RHS must be a Constant";
  EXPECT_EQ(std::string(c->getDecompile()), "8") << "'p6 = 8': decompile must be \"8\"";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
