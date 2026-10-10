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

// Tests for tests/ExprEvalPartial/dut.sv:
//
//   module top;
//       logic [63:0] s1c;
//       assign s1c = ('1 << 8) + 1 + A + (2 + 3);
//   endmodule
//
// "A" is never declared anywhere in this file. Per IEEE 1800-2023 Sec 6.3,
// every identifier used in an expression must resolve to a declaration
// visible in scope; "A" cannot bind to anything, so the RefObj built for
// it must have a null "actual" and the compiler must report a
// COMP_FAILED_TO_BIND diagnostic naming "A". This is the "partial
// evaluation" case: unlike tests/ExprEvalBits (where every identifier
// resolves and the whole expression is well-formed), here elaboration can
// build the expression tree and evaluate the fully-constant sub-terms
// ('1 << 8, and 2 + 3), but the presence of the unresolved "A" means the
// overall continuous-assignment expression can never be reduced to a
// single constant value -- the evaluation is necessarily partial/failed
// once it reaches "A", even though the surrounding structure is otherwise
// legal SystemVerilog.
//
// Per Sec 11.3.1's operator-precedence table, binary '+' and '-' share one
// precedence level (lower than the shift operators, but here the shift is
// inside its own parens) and are left-associative, so, ignoring the
// explicit parens which do not change operator grouping beyond forcing
// their own sub-expression, the RHS parses as:
//   ((('1 << 8) + 1) + A) + (2 + 3)
// i.e. a top-level '+' Operation whose second operand is the parenthesized
// "(2 + 3)" '+' Operation, and whose first operand is a '+' Operation
// chain ending, at its own second operand, in the bare RefObj "A".
//
// Checked:
//   - module "top" exists (no ports)
//   - "top" declares exactly 1 Variable "s1c", unpacked as a plain
//     LogicTypespec with 1 packed Range "[63:0]"
//   - "top" has exactly 1 ContAssign; its lhs is a RefObj "s1c" resolving
//     to that Variable
//   - the ContAssign's rhs is a top-level '+' Operation (vpiAddOp) with 2
//     operands
//   - the second (rightmost) top-level operand is the parenthesized
//     "(2 + 3)": a '+' Operation with 2 Constant operands, "2" and "3"
//   - the first top-level operand is itself a '+' Operation whose second
//     operand is a bare RefObj named "A"
//   - RefObj "A" has a null getActual(): Sec 6.3, it never resolves to any
//     declaration
//   - the compiler reports COMP_FAILED_TO_BIND naming "A" (Sec 6.3); no
//     such diagnostic is reported for "s1c", which IS declared

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/Error.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
#include <hlc/ErrorReporting/ErrorDefinition.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/cont_assign.h>
#include <hldb/design.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ExprEvalPartialTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "ExprEvalPartial.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  }

  static const hldb::Variable *getS1c() {
    const hldb::Module *const top = getTop();
    if (top == nullptr) return nullptr;
    return hldb::findByName<hldb::Variable>("s1c", top->getVariables());
  }

  static const hldb::ContAssign *getContAssign() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getContAssigns() == nullptr || top->getContAssigns()->empty()) return nullptr;
    return top->getContAssigns()->at(0);
  }

  // The bare 'A' RefObj, reached by walking down the first operand of each
  // top-level '+' in the left-associative chain.
  static const hldb::RefObj *getARefObj() {
    const hldb::ContAssign *const ca = getContAssign();
    if (ca == nullptr) return nullptr;
    const hldb::Operation *const top = ca->getRhs<hldb::Operation>();
    if (top == nullptr || top->getOperands() == nullptr || top->getOperands()->empty()) return nullptr;
    const hldb::Operation *const chain = any_cast<hldb::Operation>(top->getOperands()->at(0));
    if (chain == nullptr || chain->getOperands() == nullptr || chain->getOperands()->size() < 2u) return nullptr;
    return any_cast<hldb::RefObj>(chain->getOperands()->at(1));
  }
};

// ---------------------------------------------------------------------------
// module / variable
// ---------------------------------------------------------------------------

TEST_F(ExprEvalPartialTest, ModuleTopExists) { ASSERT_NE(getTop(), nullptr); }

TEST_F(ExprEvalPartialTest, ModuleHasOneVariableS1c) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 1u);
  const hldb::Variable *const s1c = getS1c();
  ASSERT_NE(s1c, nullptr) << "'logic [63:0] s1c;' not found";
  EXPECT_EQ(s1c->getName(), "s1c");
}

TEST_F(ExprEvalPartialTest, S1cTypespecIsLogicWithRange63To0) {
  const hldb::Variable *const s1c = getS1c();
  ASSERT_NE(s1c, nullptr);
  ASSERT_NE(s1c->getTypespec(), nullptr);
  const hldb::LogicTypespec *const lt = s1c->getTypespec<hldb::RefTypespec>()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lt, nullptr) << "'logic [63:0]' should resolve to a LogicTypespec";
  ASSERT_NE(lt->getRanges(), nullptr);
  ASSERT_EQ(lt->getRanges()->size(), 1u);
  const hldb::Range *const r = lt->getRanges()->at(0);
  ASSERT_NE(r, nullptr);
  const hldb::Constant *const left = r->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "63");
  const hldb::Constant *const right = r->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
}

// ---------------------------------------------------------------------------
// assign s1c = ('1 << 8) + 1 + A + (2 + 3);
// ---------------------------------------------------------------------------

TEST_F(ExprEvalPartialTest, ModuleHasOneContAssign) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getContAssigns(), nullptr);
  EXPECT_EQ(top->getContAssigns()->size(), 1u);
}

TEST_F(ExprEvalPartialTest, ContAssignLhsIsS1c) {
  const hldb::ContAssign *const ca = getContAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::RefObj *const lhs = ca->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), "s1c");
  EXPECT_EQ(lhs->getActual<hldb::Variable>(), getS1c());
}

TEST_F(ExprEvalPartialTest, ContAssignRhsIsTopLevelAddOpWithTwoOperands) {
  const hldb::ContAssign *const ca = getContAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::Operation *const top = ca->getRhs<hldb::Operation>();
  ASSERT_NE(top, nullptr) << "Sec 11.3.1: outermost node of the left-associative '+' chain should be a "
                              "'+' Operation";
  EXPECT_EQ(top->getOpType(), vpiAddOp);
  ASSERT_NE(top->getOperands(), nullptr);
  ASSERT_EQ(top->getOperands()->size(), 2u);
}

TEST_F(ExprEvalPartialTest, TopAddOpRhsOperandIsParenthesizedTwoPlusThree) {
  const hldb::ContAssign *const ca = getContAssign();
  ASSERT_NE(ca, nullptr);
  const hldb::Operation *const top = ca->getRhs<hldb::Operation>();
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getOperands(), nullptr);
  ASSERT_EQ(top->getOperands()->size(), 2u);

  const hldb::Operation *const twoPlusThree = any_cast<hldb::Operation>(top->getOperands()->at(1));
  ASSERT_NE(twoPlusThree, nullptr) << "'(2 + 3)' should be a '+' Operation";
  EXPECT_EQ(twoPlusThree->getOpType(), vpiAddOp);
  ASSERT_NE(twoPlusThree->getOperands(), nullptr);
  ASSERT_EQ(twoPlusThree->getOperands()->size(), 2u);
  const hldb::Constant *const two = any_cast<hldb::Constant>(twoPlusThree->getOperands()->at(0));
  ASSERT_NE(two, nullptr);
  EXPECT_EQ(two->getDecompile(), "2");
  const hldb::Constant *const three = any_cast<hldb::Constant>(twoPlusThree->getOperands()->at(1));
  ASSERT_NE(three, nullptr);
  EXPECT_EQ(three->getDecompile(), "3");
}

TEST_F(ExprEvalPartialTest, ARefObjIsPresentInTheLeftChain) {
  const hldb::RefObj *const a = getARefObj();
  ASSERT_NE(a, nullptr) << "'A' should appear as a bare RefObj inside the left-associative '+' chain";
  EXPECT_EQ(a->getName(), "A");
}

// The crux of "partial evaluation": 'A' is never declared, so it cannot
// resolve to any actual -- the expression can never be fully constant
// folded, even though every other sub-term is a legal constant expression.
TEST_F(ExprEvalPartialTest, ARefObjDoesNotResolveToAnyDeclaration) {
  const hldb::RefObj *const a = getARefObj();
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->getActual(), nullptr) << "Sec 6.3: 'A' is never declared, so it must not resolve to any Any";
}

// ---------------------------------------------------------------------------
// compiler diagnostics
// ---------------------------------------------------------------------------

TEST_F(ExprEvalPartialTest, CompilerReportsFailedToBindForA) {
  ASSERT_NE(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "A"), nullptr)
      << "Sec 6.3: 'A' is used without ever being declared and must fail to bind";
}

TEST_F(ExprEvalPartialTest, CompilerDoesNotReportFailedToBindForS1c) {
  EXPECT_EQ(findError(ErrorDefinition::COMP_FAILED_TO_BIND, "s1c"), nullptr)
      << "'s1c' IS declared ('logic [63:0] s1c;') and must resolve without a binding error";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
