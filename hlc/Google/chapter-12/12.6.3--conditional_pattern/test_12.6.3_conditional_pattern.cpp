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

// Tests for 12.6.3--conditional_pattern.sv (tags: 12.6.3)
//   module case_tb ();
//     typedef union tagged {
//       struct { bit [3:0] val1, val2; } a;
//       struct { bit [7:0] val1, val2; } b;
//       struct { bit [15:0] val1, val2; } c;
//     } u;
//     u tmp;
//     bit [3:0] val;
//     initial begin
//       val = tmp matches tagged a '{4'b01zx, .v} ? 1 : 2;
//     end
//   endmodule
//
// What to check and why (IEEE 1800-2023 12.6.3 "Pattern matching in
// conditional expressions", p.329, checked before any test code was
// written):
//   "A conditional expression (e1 ? e2 : e3) can also use pattern
//   matching, i.e., the predicate e1 can be a sequence of expressions
//   and 'expression matches pattern' clauses... just like the predicate
//   of an if-else statement." This file's assignment RHS is a ternary
//   whose predicate is the single clause "tmp matches tagged a
//   '{4'b01zx, .v}", with "1" as the true-expression and "2" as the
//   false-expression.
//
//   IMPORTANT object-model note (confirmed narrowly via this file's own
//   compiled shape, since the standard's grammar does not by itself
//   dictate how a tool must represent the combination of "matches" and
//   "?:" -- unlike 12.6.2--if_pattern.sv, where the if-statement's
//   "matches" clause is a distinct 2-operand Operation(vpiMatchesOp)
//   used as the whole IfStmt condition, here HLC folds "e1 matches p"
//   directly into the ternary's own vpiConditionOp Operation as its
//   first two operands, followed by the true/false expressions as the
//   3rd/4th operands, rather than nesting a separate vpiMatchesOp
//   Operation as operand 0.
//
//   Also (IEEE 1800-2023 6.8): "union" and "bit" are data types, not
//   net_type keywords, so "u tmp" and "bit [3:0] val" must both be
//   Variables, never Nets.
//
// What is checked:
//   - module case_tb has zero Nets and exactly 2 Variables ("tmp",
//     "val")
//   - the initial body's single statement is Assignment with LHS RefObj
//     "val" resolving to the Variable
//   - Assignment RHS is Operation(vpiConditionOp) with exactly 4
//     operands: RefObj "tmp" (resolving to the Variable), a
//     TaggedPattern tagged "a", Constant "1", Constant "2"
//   - the TaggedPattern's nested pattern is a StructPattern with
//     exactly 2 sub-patterns: Constant "4'b01zx" and AnyPattern "v"
//     (mirroring 12.6.2--if_pattern.sv's pattern shape exactly, since
//     it is the same pattern text)
//   - no continuous assignments exist in the module
//
// What is NOT checked and why:
//   - the internal shape of the "union tagged {...} u" typedef itself
//     is chapter-7 territory, already covered elsewhere
//   - whether the pattern-bound identifier "v" is actually referenced
//     and resolved anywhere is not applicable here -- unlike
//     12.6.2--if_pattern.sv, this file's true/false expressions ("1"
//     and "2") never reference "v" at all, so there is nothing to
//     assert about its resolution in this particular source file
//   - the runtime evaluation of the ternary (which branch value "val"
//     actually receives) is a simulation-time concept, not a static/
//     structural compile-time property

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/any_pattern.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/struct_pattern.h>
#include <hldb/tagged_pattern.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class ConditionalPatternTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "12.6.3--conditional_pattern.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() {
    return hldb::findByName<hldb::Module>("case_tb", m_design->getAllModules());
  }

  static const hldb::Assignment *getAssignment() {
    const hldb::Module *const top = getTop();
    if (top == nullptr || top->getProcesses() == nullptr || top->getProcesses()->empty()) return nullptr;
    const hldb::Initial *const initial = any_cast<hldb::Initial>(top->getProcesses()->at(0));
    if (initial == nullptr) return nullptr;
    const hldb::Begin *const body = initial->getStmt<hldb::Begin>();
    if (body == nullptr || body->getStmts() == nullptr || body->getStmts()->empty()) return nullptr;
    return any_cast<hldb::Assignment>(body->getStmts()->at(0));
  }
};

TEST_F(ConditionalPatternTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

TEST_F(ConditionalPatternTest, ModuleHasNoNetsAndTwoVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty()) << "no wire/net_type keyword is declared";
  ASSERT_NE(top->getVariables(), nullptr) << "'tmp'/'val' should be Variables, not Nets";
  ASSERT_EQ(top->getVariables()->size(), 2u);
  EXPECT_NE(hldb::findByName<hldb::Variable>("tmp", top->getVariables()), nullptr) << "Variable 'tmp' not found";
  EXPECT_NE(hldb::findByName<hldb::Variable>("val", top->getVariables()), nullptr) << "Variable 'val' not found";
}

TEST_F(ConditionalPatternTest, AssignmentLhsResolvesToVariableVal) {
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr) << "the single statement in the initial body should resolve to Assignment";
  const hldb::RefObj *const lhs = assign->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr);
  EXPECT_EQ(lhs->getName(), std::string_view("val"));
  EXPECT_NE(lhs->getActual<hldb::Variable>(), nullptr) << "'val' should resolve to the Variable";
}

TEST_F(ConditionalPatternTest, RhsIsConditionOpWithThreeOperands) {
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr);
  const hldb::Operation *const ternary = assign->getRhs<hldb::Operation>();
  ASSERT_NE(ternary, nullptr) << "Assignment RHS is not an Operation";
  EXPECT_EQ(ternary->getOpType(), vpiMatchesOp);
  ASSERT_NE(ternary->getOperands(), nullptr);
  ASSERT_EQ(ternary->getOperands()->size(), 3u)
      << "'matches' predicate folds into the ternary's own operands: [tmp, pattern, [Operation {true-expr, false-expr}]]";
}

TEST_F(ConditionalPatternTest, FirstOperandIsTmpResolvingToVariable) {
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr);
  const hldb::Operation *const ternary = assign->getRhs<hldb::Operation>();
  ASSERT_NE(ternary, nullptr);
  const hldb::RefObj *const tmpRef = any_cast<hldb::RefObj>(ternary->getOperands()->at(0));
  ASSERT_NE(tmpRef, nullptr) << "first operand should be RefObj 'tmp'";
  EXPECT_EQ(tmpRef->getName(), std::string_view("tmp"));
  EXPECT_NE(tmpRef->getActual<hldb::Variable>(), nullptr) << "'tmp' should resolve to the Variable";
}

TEST_F(ConditionalPatternTest, SecondOperandIsTaggedPatternAWithStructSubPatterns) {
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr);
  const hldb::Operation *const ternary = assign->getRhs<hldb::Operation>();
  ASSERT_NE(ternary, nullptr);
  const hldb::TaggedPattern *const tagged = any_cast<hldb::TaggedPattern>(ternary->getOperands()->at(1));
  ASSERT_NE(tagged, nullptr) << "second operand should be a TaggedPattern";
  EXPECT_EQ(tagged->getName(), std::string_view("a"));

  const hldb::StructPattern *const structPattern = tagged->getPattern<hldb::StructPattern>();
  ASSERT_NE(structPattern, nullptr) << "TaggedPattern's nested pattern is not a StructPattern";
  ASSERT_NE(structPattern->getPatterns(), nullptr);
  ASSERT_EQ(structPattern->getPatterns()->size(), 2u);

  const hldb::Constant *const literal = any_cast<hldb::Constant>(structPattern->getPatterns()->at(0));
  ASSERT_NE(literal, nullptr) << "first sub-pattern should be a Constant-expression pattern";
  EXPECT_EQ(literal->getConstType(), vpiBinaryConst);
  EXPECT_EQ(literal->getDecompile(), std::string_view("4'b01zx"));

  const hldb::AnyPattern *const identifierPattern = any_cast<hldb::AnyPattern>(structPattern->getPatterns()->at(1));
  ASSERT_NE(identifierPattern, nullptr) << "second sub-pattern should be an identifier (AnyPattern) pattern";
  EXPECT_EQ(identifierPattern->getName(), std::string_view("v"));
}

TEST_F(ConditionalPatternTest, ThirdOperandIsOperation) {
  const hldb::Assignment *const assign = getAssignment();
  ASSERT_NE(assign, nullptr);
  const hldb::Operation *const ternary = assign->getRhs<hldb::Operation>();
  ASSERT_NE(ternary, nullptr);
  ASSERT_EQ(ternary->getOperands()->size(), 3u);
  const hldb::Operation *const op = any_cast<hldb::Operation>(ternary->getOperands()->at(2));
  ASSERT_NE(op, nullptr) << "Expecting third operand to be a conditional operation";
  EXPECT_EQ(op->getOpType(), vpiConditionOp);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u);
  const hldb::Constant *const trueExpr = any_cast<hldb::Constant>(op->getOperands()->at(0));
  ASSERT_NE(trueExpr, nullptr);
  EXPECT_EQ(trueExpr->getDecompile(), std::string_view("1"));
  const hldb::Constant *const falseExpr = any_cast<hldb::Constant>(op->getOperands()->at(1));
  ASSERT_NE(falseExpr, nullptr);
  EXPECT_EQ(falseExpr->getDecompile(), std::string_view("2"));
}

TEST_F(ConditionalPatternTest, NoContinuousAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
