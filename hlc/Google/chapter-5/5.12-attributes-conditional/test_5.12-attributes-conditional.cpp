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

// Validates an attribute on a ternary-operator branch in UHDM:
//   a = b ? (* no_glitch *) c : d;
//
// UHDM structure:
//   Assignment (blocking)
//     LHS: RefObj "a"
//     RHS: Operation (vpiConditionOp = 32)
//            operand[0]: RefObj "b"        <- condition
//            operand[1]: RefObj "c"        <- true branch
//              vpiAttribute[0]: Attribute "no_glitch" (no value -- flag)
//            operand[2]: RefObj "d"        <- false branch
//
// Key fact: the (* ... *) attribute is attached to the branch Expr node,
// not to the ternary Operation itself.  Access via Expr::getAttributes().

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/attribute.h>
#include <hldb/begin.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/variable.h>

namespace hlc {

class AttributesConditional : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.12-attributes-conditional.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// Helper: the one Assignment inside initial begin.
static const hldb::Assignment *getAssignment(const hldb::Design *design) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", design->getAllModules());
  if (!top || !top->getProcesses()) return nullptr;
  for (const hldb::Process *const p : *top->getProcesses()) {
    if (const hldb::Initial *const i = any_cast<hldb::Initial>(p)) {
      const hldb::Begin *const blk = i->getStmt<hldb::Begin>();
      if (!blk || !blk->getStmts()) return nullptr;
      for (const hldb::Any *const s : *blk->getStmts()) {
        if (const hldb::Assignment *const a = any_cast<hldb::Assignment>(s)) return a;
      }
    }
  }
  return nullptr;
}

// ----
// Module and variables
// ----
TEST_F(AttributesConditional, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

TEST_F(AttributesConditional, FourBitVariablesExist) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 4u) << "expected 4 variables: a, b, c, d";

  bool hasA = false, hasB = false, hasC = false, hasD = false;
  for (const hldb::Variable *const n : *top->getVariables()) {
    if (n->getName() == "a") hasA = true;
    if (n->getName() == "b") hasB = true;
    if (n->getName() == "c") hasC = true;
    if (n->getName() == "d") hasD = true;
  }
  EXPECT_TRUE(hasA) << "variable 'a' missing";
  EXPECT_TRUE(hasB) << "variable 'b' missing";
  EXPECT_TRUE(hasC) << "variable 'c' missing";
  EXPECT_TRUE(hasD) << "variable 'd' missing";
}

// `bit a, b, c, d;` has no net-type keyword, so per IEEE 1800-2023 Sec
// 6.7/6.8 none of these must also appear as a Net.
TEST_F(AttributesConditional, VariablesAreNotDuplicatedAsNets) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  if (top->getNets() != nullptr) {
    EXPECT_EQ(hldb::findByName<hldb::Net>("a", top->getNets()), nullptr);
    EXPECT_EQ(hldb::findByName<hldb::Net>("b", top->getNets()), nullptr);
    EXPECT_EQ(hldb::findByName<hldb::Net>("c", top->getNets()), nullptr);
    EXPECT_EQ(hldb::findByName<hldb::Net>("d", top->getNets()), nullptr);
  }
}

// ----
// Assignment: a = b ? (* no_glitch *) c : d
// ----
TEST_F(AttributesConditional, AssignmentExists) {
  ASSERT_NE(getAssignment(m_design), nullptr) << "blocking assignment not found";
}

TEST_F(AttributesConditional, AssignmentIsBlocking) {
  const hldb::Assignment *const a = getAssignment(m_design);
  ASSERT_NE(a, nullptr);
  EXPECT_TRUE(a->getBlocking()) << "assignment should be blocking";
}

TEST_F(AttributesConditional, AssignmentLhsIsA) {
  const hldb::Assignment *const a = getAssignment(m_design);
  ASSERT_NE(a, nullptr);
  const hldb::RefObj *const lhs = a->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "LHS is not a RefObj";
  EXPECT_EQ(lhs->getName(), "a");
}

// ----
// RHS: ternary Operation (condition)
// ----
TEST_F(AttributesConditional, RhsIsConditionalOperation) {
  const hldb::Assignment *const a = getAssignment(m_design);
  ASSERT_NE(a, nullptr);
  const hldb::Operation *const rhs = a->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr) << "RHS is not an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiConditionOp) << "RHS opType should be vpiConditionOp (32)";
}

TEST_F(AttributesConditional, ConditionalHasThreeOperands) {
  const hldb::Assignment *const a = getAssignment(m_design);
  ASSERT_NE(a, nullptr);
  const hldb::Operation *const rhs = a->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  EXPECT_EQ(rhs->getOperands()->size(), 3u) << "ternary operator needs exactly 3 operands: condition, true, false";
}

TEST_F(AttributesConditional, OperandZeroIsConditionB) {
  const hldb::Assignment *const a = getAssignment(m_design);
  ASSERT_NE(a, nullptr);
  const hldb::Operation *const rhs = a->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 3u);

  const hldb::RefObj *const cond = any_cast<hldb::RefObj>((*rhs->getOperands())[0]);
  ASSERT_NE(cond, nullptr) << "operand[0] (condition) should be a RefObj";
  EXPECT_EQ(cond->getName(), "b");
}

TEST_F(AttributesConditional, OperandOneIsTrueBranchC) {
  const hldb::Assignment *const a = getAssignment(m_design);
  ASSERT_NE(a, nullptr);
  const hldb::Operation *const rhs = a->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 3u);

  const hldb::RefObj *const trueBranch = any_cast<hldb::RefObj>((*rhs->getOperands())[1]);
  ASSERT_NE(trueBranch, nullptr) << "operand[1] (true branch) should be a RefObj";
  EXPECT_EQ(trueBranch->getName(), "c");
}

TEST_F(AttributesConditional, OperandTwoIsFalseBranchD) {
  const hldb::Assignment *const a = getAssignment(m_design);
  ASSERT_NE(a, nullptr);
  const hldb::Operation *const rhs = a->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 3u);

  const hldb::RefObj *const falseBranch = any_cast<hldb::RefObj>((*rhs->getOperands())[2]);
  ASSERT_NE(falseBranch, nullptr) << "operand[2] (false branch) should be a RefObj";
  EXPECT_EQ(falseBranch->getName(), "d");
}

// ----
// (* no_glitch *) attribute on the true-branch operand
// ----
TEST_F(AttributesConditional, TrueBranchHasNoGlitchAttribute) {
  const hldb::Assignment *const a = getAssignment(m_design);
  ASSERT_NE(a, nullptr);
  const hldb::Operation *const rhs = a->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 3u);

  // Attribute is on the branch Expr node, not on the Operation itself.
  const hldb::RefObj *const trueBranch = any_cast<hldb::RefObj>((*rhs->getOperands())[1]);
  ASSERT_NE(trueBranch, nullptr);
  ASSERT_NE(trueBranch->getAttributes(), nullptr) << "true branch 'c' should have attributes";
  ASSERT_EQ(trueBranch->getAttributes()->size(), 1u);
  EXPECT_EQ((*trueBranch->getAttributes())[0]->getName(), "no_glitch");
}

TEST_F(AttributesConditional, NoGlitchIsFlagAttribute) {
  const hldb::Assignment *const a = getAssignment(m_design);
  ASSERT_NE(a, nullptr);
  const hldb::Operation *const rhs = a->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 3u);

  const hldb::RefObj *const trueBranch = any_cast<hldb::RefObj>((*rhs->getOperands())[1]);
  ASSERT_NE(trueBranch, nullptr);
  ASSERT_NE(trueBranch->getAttributes(), nullptr);
  ASSERT_EQ(trueBranch->getAttributes()->size(), 1u);

  // Flag attribute: no = expr, so getValue() is null
  const hldb::Attribute *const attr = (*trueBranch->getAttributes())[0];
  ASSERT_NE(attr, nullptr);
  EXPECT_EQ(attr->getValue(), nullptr) << "'no_glitch' is a flag attribute and should have no value";
}

TEST_F(AttributesConditional, ConditionAndFalseBranchHaveNoAttributes) {
  const hldb::Assignment *const a = getAssignment(m_design);
  ASSERT_NE(a, nullptr);
  const hldb::Operation *const rhs = a->getRhs<hldb::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 3u);

  const hldb::RefObj *const cond = any_cast<hldb::RefObj>((*rhs->getOperands())[0]);
  const hldb::RefObj *const falseBranch = any_cast<hldb::RefObj>((*rhs->getOperands())[2]);
  ASSERT_NE(cond, nullptr);
  ASSERT_NE(falseBranch, nullptr);

  EXPECT_TRUE(!cond->getAttributes() || cond->getAttributes()->empty()) << "condition 'b' should have no attributes";
  EXPECT_TRUE(!falseBranch->getAttributes() || falseBranch->getAttributes()->empty())
      << "false branch 'd' should have no attributes";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
