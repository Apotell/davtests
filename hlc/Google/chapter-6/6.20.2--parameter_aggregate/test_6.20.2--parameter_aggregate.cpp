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

// Spec-based validation of IEEE 1800-2017 §6.20.2 aggregate parameter.
// SV: tests/Google/chapter-6/6.20.2--parameter_aggregate.sv
//
//   module top();
//       parameter logic [31:0] p [3:0] = '{1, 2, 3, 4};
//   endmodule
//
// ── §6.20.2 constructs under test ────────────────────────────────────────────
//
// A parameter with an explicit aggregate type:
//   • `logic [31:0] p [3:0]` — static unpacked array of 4 elements, each a
//     32-bit logic vector. Surelog models this as ArrayTypespec whose
//     element typespec is LogicTypespec with packed range [31:0].
//   • `'{1, 2, 3, 4}` — assignment pattern literal (IEEE 1800-2017 §10.9.1).
//     Surelog models this as an Operation with vpiOpType = vpiAssignmentPatternOp (75)
//     and four Constant operands, one per element.
//   • Each element literal is an unsized unsigned integer (vpiUIntConst = 9).
//
// ── UHDM tree (from log) ──────────────────────────────────────────────────
//
//   Module name:work@top
//   ├── vpiParameter (1 item)
//   │   └── Parameter name:p
//   │       └── vpiTypespec  RefTypespec → actual: ArrayTypespec
//   │           ├── vpiArrayType: static (1)
//   │           ├── vpiRange  Range [3:0]  (unpacked dimension)
//   │           └── vpiElemTypespec  RefTypespec → actual: LogicTypespec
//   │               └── vpiRange (1 item)  Range [31:0]  (packed dimension)
//   └── vpiParamAssign (1 item)
//       └── ParamAssign
//           ├── vpiLhs  RefObj name:p → actual: Parameter name:p
//           └── vpiRhs  Operation
//               ├── vpiOpType: assign pattern (75)
//               └── vpiOperand (4 items)
//                   ├── Constant  vpiUIntConst(9)  value:"1"
//                   ├── Constant  vpiUIntConst(9)  value:"2"
//                   ├── Constant  vpiUIntConst(9)  value:"3"
//                   └── Constant  vpiUIntConst(9)  value:"4"
//
// ── VPI constants ─────────────────────────────────────────────────────────
//   vpiAssignmentPatternOp = 75
//   vpiStaticArray         =  1
//   vpiUIntConst           =  9

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/array_typespec.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/logic_typespec.h>
#include <uhdm/module.h>
#include <uhdm/operation.h>
#include <uhdm/param_assign.h>
#include <uhdm/parameter.h>
#include <uhdm/range.h>
#include <uhdm/ref_obj.h>
#include <uhdm/ref_typespec.h>

#include <string>

namespace SURELOG {

class ParameterAggregateTest : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.20.2--parameter_aggregate.hlc"});

    ASSERT_NE(m_session,  nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design,   nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const uhdm::Module *getTop(const uhdm::Design *d) {
  return uhdm::findByName<uhdm::Module>("work@top", d->getAllModules());
}

static const uhdm::Parameter *getParam(const uhdm::Design *d) {
  const uhdm::Module *m = getTop(d);
  if (!m || !m->getParameters()) return nullptr;
  return uhdm::findByName<uhdm::Parameter>("p", m->getParameters());
}

static const uhdm::ArrayTypespec *getArrayTypespec(const uhdm::Design *d) {
  const uhdm::Parameter *p = getParam(d);
  if (!p || !p->getTypespec()) return nullptr;
  return p->getTypespec()->getActual<uhdm::ArrayTypespec>();
}

static const uhdm::LogicTypespec *getElemLogicTypespec(const uhdm::Design *d) {
  const uhdm::ArrayTypespec *at = getArrayTypespec(d);
  if (!at || !at->getElemTypespec()) return nullptr;
  return at->getElemTypespec()->getActual<uhdm::LogicTypespec>();
}

static const uhdm::ParamAssign *getParamAssign(const uhdm::Design *d) {
  const uhdm::Module *m = getTop(d);
  if (!m || !m->getParamAssigns() || m->getParamAssigns()->empty())
    return nullptr;
  return (*m->getParamAssigns())[0];
}

static const uhdm::Operation *getRhsOp(const uhdm::Design *d) {
  const uhdm::ParamAssign *pa = getParamAssign(d);
  if (!pa) return nullptr;
  return pa->getRhs<uhdm::Operation>();
}

// ===========================================================================
// Module
// ===========================================================================

TEST_F(ParameterAggregateTest, ModuleExists) {
  EXPECT_NE(getTop(m_design), nullptr);
}

// ===========================================================================
// Parameter collection
// ===========================================================================

TEST_F(ParameterAggregateTest, Parameter_Collection_HasOneEntry) {
  const uhdm::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParameters(), nullptr);
  EXPECT_EQ(m->getParameters()->size(), 1u);
}

TEST_F(ParameterAggregateTest, Parameter_p_Exists) {
  EXPECT_NE(getParam(m_design), nullptr);
}

// ===========================================================================
// ArrayTypespec — 'logic [31:0] p [3:0]'
// ===========================================================================

// IEEE 1800-2017 §6.20.2: explicitly typed aggregate parameter resolves to
// an ArrayTypespec, not a LogicTypespec.
TEST_F(ParameterAggregateTest, Parameter_p_TypespecIsArrayTypespec) {
  const uhdm::Parameter *p = getParam(m_design);
  ASSERT_NE(p, nullptr);
  const uhdm::RefTypespec *rt = p->getTypespec();
  ASSERT_NE(rt, nullptr);
  EXPECT_NE(rt->getActual<uhdm::ArrayTypespec>(), nullptr)
      << "parameter 'logic [31:0] p [3:0]' must have ArrayTypespec";
}

// '[3:0]' is a fixed-size unpacked dimension → static array.
TEST_F(ParameterAggregateTest, Parameter_p_ArrayType_IsStatic) {
  const uhdm::ArrayTypespec *at = getArrayTypespec(m_design);
  ASSERT_NE(at, nullptr);
  EXPECT_EQ(at->getArrayType(), vpiStaticArray)
      << "'p [3:0]' must be a static array (vpiStaticArray = 1)";
}

// Unpacked dimension [3:0]: left bound = 3.
TEST_F(ParameterAggregateTest, Parameter_p_ArrayRange_LeftIs3) {
  const uhdm::ArrayTypespec *at = getArrayTypespec(m_design);
  ASSERT_NE(at, nullptr);
  const uhdm::Range *r = at->getRange();
  ASSERT_NE(r, nullptr) << "ArrayTypespec must have an unpacked range";
  const uhdm::Constant *left = r->getLeftExpr<uhdm::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(std::string(left->getValue()), "3")
      << "unpacked dimension left bound must be 3";
}

// Unpacked dimension [3:0]: right bound = 0.
TEST_F(ParameterAggregateTest, Parameter_p_ArrayRange_RightIs0) {
  const uhdm::ArrayTypespec *at = getArrayTypespec(m_design);
  ASSERT_NE(at, nullptr);
  const uhdm::Range *r = at->getRange();
  ASSERT_NE(r, nullptr);
  const uhdm::Constant *right = r->getRightExpr<uhdm::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(std::string(right->getValue()), "0")
      << "unpacked dimension right bound must be 0";
}

// Element type is 'logic [31:0]' → LogicTypespec.
TEST_F(ParameterAggregateTest, Parameter_p_ElemTypespec_IsLogicTypespec) {
  const uhdm::ArrayTypespec *at = getArrayTypespec(m_design);
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getElemTypespec(), nullptr);
  EXPECT_NE(at->getElemTypespec()->getActual<uhdm::LogicTypespec>(), nullptr)
      << "element typespec of 'logic [31:0] p [3:0]' must be LogicTypespec";
}

// §7.4.1: 'logic [31:0]' is a packed vector — explicit packed dimension makes
// the type a vector, not a scalar logic.
TEST_F(ParameterAggregateTest, Parameter_p_ElemTypespec_IsVector) {
  const uhdm::LogicTypespec *lts = getElemLogicTypespec(m_design);
  ASSERT_NE(lts, nullptr);
  EXPECT_TRUE(lts->getVector())
      << "'logic [31:0]' must be flagged as a vector type";
}

// 'logic [31:0]' has one packed dimension.
TEST_F(ParameterAggregateTest, Parameter_p_ElemTypespec_HasOnePackedRange) {
  const uhdm::LogicTypespec *lts = getElemLogicTypespec(m_design);
  ASSERT_NE(lts, nullptr);
  ASSERT_NE(lts->getRanges(), nullptr);
  EXPECT_EQ(lts->getRanges()->size(), 1u)
      << "'logic [31:0]' must have exactly one packed range";
}

// Packed dimension [31:0]: left bound = 31.
TEST_F(ParameterAggregateTest, Parameter_p_ElemTypespec_Range_LeftIs31) {
  const uhdm::LogicTypespec *lts = getElemLogicTypespec(m_design);
  ASSERT_NE(lts, nullptr);
  ASSERT_NE(lts->getRanges(), nullptr);
  const uhdm::Range *r = (*lts->getRanges())[0];
  ASSERT_NE(r, nullptr);
  const uhdm::Constant *left = r->getLeftExpr<uhdm::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(std::string(left->getValue()), "31")
      << "packed dimension left bound must be 31";
}

// Packed dimension [31:0]: right bound = 0.
TEST_F(ParameterAggregateTest, Parameter_p_ElemTypespec_Range_RightIs0) {
  const uhdm::LogicTypespec *lts = getElemLogicTypespec(m_design);
  ASSERT_NE(lts, nullptr);
  ASSERT_NE(lts->getRanges(), nullptr);
  const uhdm::Range *r = (*lts->getRanges())[0];
  ASSERT_NE(r, nullptr);
  const uhdm::Constant *right = r->getRightExpr<uhdm::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(std::string(right->getValue()), "0")
      << "packed dimension right bound must be 0";
}

// ===========================================================================
// ParamAssign collection
// ===========================================================================

TEST_F(ParameterAggregateTest, ParamAssign_Collection_HasOneEntry) {
  const uhdm::Module *m = getTop(m_design);
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getParamAssigns(), nullptr);
  EXPECT_EQ(m->getParamAssigns()->size(), 1u);
}

// ===========================================================================
// ParamAssign LHS
// ===========================================================================

TEST_F(ParameterAggregateTest, ParamAssign_Lhs_IsRefObj) {
  const uhdm::ParamAssign *pa = getParamAssign(m_design);
  ASSERT_NE(pa, nullptr);
  EXPECT_NE(pa->getLhs<uhdm::RefObj>(), nullptr);
}

TEST_F(ParameterAggregateTest, ParamAssign_Lhs_NameIsP) {
  const uhdm::ParamAssign *pa = getParamAssign(m_design);
  ASSERT_NE(pa, nullptr);
  const uhdm::RefObj *lhs = pa->getLhs<uhdm::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "p");
}

TEST_F(ParameterAggregateTest, ParamAssign_Lhs_ActualIsParameter) {
  const uhdm::ParamAssign *pa = getParamAssign(m_design);
  ASSERT_NE(pa, nullptr);
  const uhdm::RefObj *lhs = pa->getLhs<uhdm::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_NE(lhs->getActual<uhdm::Parameter>(), nullptr);
}

// ===========================================================================
// ParamAssign RHS — assignment pattern '{1, 2, 3, 4}'
// ===========================================================================

// IEEE 1800-2017 §10.9.1: '{...}' is an assignment pattern expression.
// Surelog models this as an Operation with vpiAssignmentPatternOp (75).
TEST_F(ParameterAggregateTest, ParamAssign_Rhs_IsOperation) {
  const uhdm::ParamAssign *pa = getParamAssign(m_design);
  ASSERT_NE(pa, nullptr);
  EXPECT_NE(pa->getRhs<uhdm::Operation>(), nullptr)
      << "RHS of assignment pattern must be an Operation node";
}

TEST_F(ParameterAggregateTest, ParamAssign_Rhs_OpType_IsAssignmentPattern) {
  const uhdm::Operation *op = getRhsOp(m_design);
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiAssignmentPatternOp)
      << "'{...}' must have vpiOpType = vpiAssignmentPatternOp (75)";
}

// '{1, 2, 3, 4}' has 4 operands, one per array element.
TEST_F(ParameterAggregateTest, ParamAssign_Rhs_OperandCount_IsFour) {
  const uhdm::Operation *op = getRhsOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 4u)
      << "assignment pattern '{1,2,3,4}' must have 4 operands";
}

// Each element literal is an unsized unsigned integer (§5.7.1).
TEST_F(ParameterAggregateTest, ParamAssign_Rhs_AllOperands_AreUnsignedIntConst) {
  const uhdm::Operation *op = getRhsOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  for (const uhdm::Any *elem : *op->getOperands()) {
    const auto *c = any_cast<const uhdm::Constant *>(elem);
    ASSERT_NE(c, nullptr) << "each operand must be a Constant";
    EXPECT_EQ(c->getConstType(), vpiUIntConst)
        << "each element literal must be vpiUIntConst (9)";
  }
}

// §6.20.2: unsized decimal literals use the host integer width (64 bits).
TEST_F(ParameterAggregateTest, ParamAssign_Rhs_AllOperands_SizeIs64) {
  const uhdm::Operation *op = getRhsOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  for (const uhdm::Any *elem : *op->getOperands()) {
    const auto *c = any_cast<const uhdm::Constant *>(elem);
    ASSERT_NE(c, nullptr);
    EXPECT_EQ(c->getSize(), 64)
        << "each unsized element literal must have host-int size (64)";
  }
}

// Individual element values from the source: '{1, 2, 3, 4}'.
TEST_F(ParameterAggregateTest, ParamAssign_Rhs_Operand0_ValueIs1) {
  const uhdm::Operation *op = getRhsOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 1u);
  const auto *c = any_cast<const uhdm::Constant *>((*op->getOperands())[0]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getValue()), "1");
}

TEST_F(ParameterAggregateTest, ParamAssign_Rhs_Operand1_ValueIs2) {
  const uhdm::Operation *op = getRhsOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 2u);
  const auto *c = any_cast<const uhdm::Constant *>((*op->getOperands())[1]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getValue()), "2");
}

TEST_F(ParameterAggregateTest, ParamAssign_Rhs_Operand2_ValueIs3) {
  const uhdm::Operation *op = getRhsOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 3u);
  const auto *c = any_cast<const uhdm::Constant *>((*op->getOperands())[2]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getValue()), "3");
}

TEST_F(ParameterAggregateTest, ParamAssign_Rhs_Operand3_ValueIs4) {
  const uhdm::Operation *op = getRhsOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_GE(op->getOperands()->size(), 4u);
  const auto *c = any_cast<const uhdm::Constant *>((*op->getOperands())[3]);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(std::string(c->getValue()), "4");
}

}  // namespace SURELOG
