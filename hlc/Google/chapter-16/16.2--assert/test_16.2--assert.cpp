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

// Spec-based validation of IEEE 1800-2017 ss.16.2 simple immediate assertion.
// SV: tests/Google/chapter-16/16.2--assert.sv
//
//   module top();
//       logic a = 1;
//       initial assert (a != 0);
//   endmodule
//
// -- ss.16.2 constructs under test ------------------------------------------
//
// A simple immediate assertion (IEEE 1800-2017 ss.16.2):
//   * The assertion is procedural: it lives inside an initial block.
//   * The expression is a != 0 -- a binary logical-inequality comparison
//     (IEEE ss.11.4.5).
//   * The elaborator maps it to: Initial -> ImmediateAssert -> Operation.
//   * The operation has opType vpiNeqOp (not-equal) and two operands:
//       LHS: RefObj "a" resolving to the Net declaration.
//       RHS: Constant "0", vpiUIntConst, 64-bit unsized decimal literal.
//
// -- UHDM tree (from log) ---------------------------------------------------
//
//   Design name:unnamed
//   +-- vpiAllModules (1 item)
//       +-- Module name:work@top
//           +-- vpiNet (1 item)
//           |   +-- Net name:a
//           |       +-- vpiTypespec  RefTypespec -> actual: LogicTypespec
//           +-- vpiProcess (1 item)
//               +-- Initial
//                   +-- vpiStmt  ImmediateAssert
//                       +-- vpiExpr  Operation
//                           +-- vpiOpType: not equal (15) = vpiNeqOp
//                           +-- vpiOperand (2 items)
//                               +-- RefObj name:a -> actual: Net name:a
//                               +-- Constant
//                                   +-- vpiConstType: unsigned int (9) = vpiUIntConst
//                                   +-- vpiSize: 64
//                                   +-- vpiDecompile: "0"
//
// -- VPI constants ----------------------------------------------------------
//   vpiUIntConst = 9
//   vpiNeqOp     = 15

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/any.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/immediate_assert.h>
#include <hldb/initial.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>

#include <string>

namespace hlc {

class AssertTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "16.2--assert.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("work@top", d->getAllModules());
}

static const hldb::Net *getNetA(const hldb::Design *d) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getNets()) return nullptr;
  return hldb::findByName<hldb::Net>("a", m->getNets());
}

static const hldb::Initial *getInitial(const hldb::Design *d) {
  const hldb::Module *m = getTop(d);
  if (!m || !m->getProcesses() || m->getProcesses()->empty()) return nullptr;
  return any_cast<hldb::Initial>((*m->getProcesses())[0]);
}

static const hldb::ImmediateAssert *getImmAssert(const hldb::Design *d) {
  const hldb::Initial *init = getInitial(d);
  if (!init) return nullptr;
  return any_cast<hldb::ImmediateAssert>(init->getStmt());
}

static const hldb::Operation *getOp(const hldb::Design *d) {
  const hldb::ImmediateAssert *ia = getImmAssert(d);
  if (!ia) return nullptr;
  return any_cast<hldb::Operation>(ia->getExpr());
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(AssertTest, ModuleExists) { EXPECT_NE(getTop(m_design), nullptr); }

// ===========================================================================
// Net collection
// ===========================================================================

TEST_F(AssertTest, Net_Collection_HasOneEntry) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getNets(), nullptr);
  EXPECT_EQ(m->getNets()->size(), 1u);
}

TEST_F(AssertTest, Net_a_Exists) { EXPECT_NE(getNetA(m_design), nullptr); }

// IEEE 1800-2017 ss.6.6: 'logic' declares a net of logic type; HLDB
// represents this as LogicTypespec via RefTypespec.
TEST_F(AssertTest, Net_a_HasLogicTypespec) {
  const hldb::Net *n = getNetA(m_design);
  ASSERT_NE(n, nullptr);
  const hldb::RefTypespec *rt = n->getTypespec();
  ASSERT_NE(rt, nullptr) << "Net 'a' must have a RefTypespec";
  EXPECT_NE(rt->getActual<hldb::LogicTypespec>(), nullptr) << "RefTypespec must resolve to LogicTypespec for 'logic a'";
}

// ===========================================================================
// Process collection
// ===========================================================================

TEST_F(AssertTest, Process_Collection_HasOneEntry) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getProcesses(), nullptr);
  EXPECT_EQ(m->getProcesses()->size(), 1u);
}

// ===========================================================================
// Initial process
// ===========================================================================

// IEEE 1800-2017 ss.16.2: immediate assertions can appear inside procedural
// blocks. The source uses 'initial', so the process must be an Initial.
TEST_F(AssertTest, Process_IsInitial) {
  const hldb::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getProcesses(), nullptr);
  ASSERT_FALSE(m->getProcesses()->empty());
  EXPECT_NE(any_cast<hldb::Initial>((*m->getProcesses())[0]), nullptr) << "The one process must be an Initial block";
}

TEST_F(AssertTest, Initial_HasStatement) {
  const hldb::Initial *init = getInitial(m_design);
  ASSERT_NE(init, nullptr);
  EXPECT_NE(init->getStmt(), nullptr) << "Initial block must have a non-null body statement";
}

// ===========================================================================
// ImmediateAssert
// ===========================================================================

// IEEE 1800-2017 ss.16.2: 'assert (expression)' is a
// simple_immediate_assert_statement, represented as ImmediateAssert.
TEST_F(AssertTest, Initial_Stmt_IsImmediateAssert) {
  const hldb::Initial *init = getInitial(m_design);
  ASSERT_NE(init, nullptr);
  EXPECT_NE(any_cast<hldb::ImmediateAssert>(init->getStmt()), nullptr) << "Initial body must be an ImmediateAssert";
}

TEST_F(AssertTest, ImmediateAssert_HasExpr) {
  const hldb::ImmediateAssert *ia = getImmAssert(m_design);
  ASSERT_NE(ia, nullptr);
  EXPECT_NE(ia->getExpr(), nullptr) << "ImmediateAssert must have a non-null expression";
}

TEST_F(AssertTest, ImmediateAssert_Expr_IsOperation) {
  const hldb::ImmediateAssert *ia = getImmAssert(m_design);
  ASSERT_NE(ia, nullptr);
  EXPECT_NE(any_cast<hldb::Operation>(ia->getExpr()), nullptr) << "Assert expression must be an Operation";
}

// ===========================================================================
// Operation (a != 0)
// ===========================================================================

// IEEE 1800-2017 ss.11.4.5: != is the logical-inequality operator;
// VPI encodes it as vpiNeqOp (15).
TEST_F(AssertTest, Operation_OpType_IsNeq) {
  const hldb::Operation *op = getOp(m_design);
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiNeqOp) << "'a != 0' must have opType vpiNeqOp (15)";
}

TEST_F(AssertTest, Operation_HasTwoOperands) {
  const hldb::Operation *op = getOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u) << "Binary not-equal operation must have exactly 2 operands";
}

// ===========================================================================
// Operand 0: RefObj "a"
// ===========================================================================

TEST_F(AssertTest, Operand0_IsRefObj) {
  const hldb::Operation *op = getOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  EXPECT_NE(any_cast<hldb::RefObj>((*op->getOperands())[0]), nullptr) << "Left operand of 'a != 0' must be a RefObj";
}

TEST_F(AssertTest, Operand0_Name_IsA) {
  const hldb::Operation *op = getOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const hldb::RefObj *ref = any_cast<hldb::RefObj>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->getName(), "a");
}

// The LHS RefObj must resolve to the Net declaration of 'a'.
TEST_F(AssertTest, Operand0_ActualIsNet) {
  const hldb::Operation *op = getOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const hldb::RefObj *ref = any_cast<hldb::RefObj>((*op->getOperands())[0]);
  ASSERT_NE(ref, nullptr);
  EXPECT_NE(ref->getActual<hldb::Net>(), nullptr) << "RefObj 'a' must resolve to the Net declaration";
}

// ===========================================================================
// Operand 1: Constant "0"
// ===========================================================================

TEST_F(AssertTest, Operand1_IsConstant) {
  const hldb::Operation *op = getOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  EXPECT_NE(any_cast<hldb::Constant>((*op->getOperands())[1]), nullptr)
      << "Right operand of 'a != 0' must be a Constant";
}

// IEEE 1800-2017 ss.5.7.1: unsized decimal integer literals without a sign
// qualifier are unsigned; HLDB encodes this as vpiUIntConst (9).
TEST_F(AssertTest, Operand1_ConstType_IsUnsignedInt) {
  const hldb::Operation *op = getOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const hldb::Constant *c = any_cast<hldb::Constant>((*op->getOperands())[1]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getConstType(), vpiUIntConst) << "Constant '0' must have vpiConstType == vpiUIntConst (9)";
}

TEST_F(AssertTest, Operand1_Value_IsZero) {
  const hldb::Operation *op = getOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const hldb::Constant *c = any_cast<hldb::Constant>((*op->getOperands())[1]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getValue()), "0");
}

// IEEE 1800-2017 ss.5.7.1: an unsized decimal literal uses the host integer
// width; HLDB represents this as 64 bits.
TEST_F(AssertTest, Operand1_Size_Is64) {
  const hldb::Operation *op = getOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const hldb::Constant *c = any_cast<hldb::Constant>((*op->getOperands())[1]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getSize(), 64) << "unsized decimal literal '0' must have host-int size (64)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
