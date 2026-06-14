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
//            operand[0]: RefObj "b"        ← condition
//            operand[1]: RefObj "c"        ← true branch
//              vpiAttribute[0]: Attribute "no_glitch" (no value — flag)
//            operand[2]: RefObj "d"        ← false branch
//
// Key fact: the (* ... *) attribute is attached to the branch Expr node,
// not to the ternary Operation itself.  Access via Expr::getAttributes().

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/assignment.h>
#include <uhdm/attribute.h>
#include <uhdm/begin.h>
#include <uhdm/design.h>
#include <uhdm/initial.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/operation.h>
#include <uhdm/ref_obj.h>

namespace SURELOG {

class AttributesConditional : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.12-attributes-conditional.hlc"});

    ASSERT_NE(m_session, nullptr) << "Session is null";
    ASSERT_NE(m_compiler, nullptr) << "Compiler is null";
    ASSERT_NE(m_design, nullptr) << "Design is null";
  }

  static void TearDownTestSuite() {
    m_design = nullptr;
    delete m_compiler;
    m_compiler = nullptr;
    delete m_session;
    m_session = nullptr;
  }
};

// Helper: the one Assignment inside initial begin.
static const uhdm::Assignment *getAssignment(const uhdm::Design *design) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", design->getAllModules());
  if (!top || !top->getProcesses()) return nullptr;
  for (const uhdm::Process *const p : *top->getProcesses()) {
    if (const uhdm::Initial *const i = any_cast<uhdm::Initial>(p)) {
      const uhdm::Begin *const blk = i->getStmt<uhdm::Begin>();
      if (!blk || !blk->getStmts()) return nullptr;
      for (const uhdm::Any *const s : *blk->getStmts()) {
        if (const uhdm::Assignment *const a = any_cast<uhdm::Assignment>(s))
          return a;
      }
    }
  }
  return nullptr;
}

// ---------------------------------------------------------------------------
// Module and nets
// ---------------------------------------------------------------------------
TEST_F(AttributesConditional, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

TEST_F(AttributesConditional, FourBitNetsExist) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 4u) << "expected 4 nets: a, b, c, d";

  bool hasA = false, hasB = false, hasC = false, hasD = false;
  for (const uhdm::Net *const n : *top->getNets()) {
    if (n->getName() == "a") hasA = true;
    if (n->getName() == "b") hasB = true;
    if (n->getName() == "c") hasC = true;
    if (n->getName() == "d") hasD = true;
  }
  EXPECT_TRUE(hasA) << "net 'a' missing";
  EXPECT_TRUE(hasB) << "net 'b' missing";
  EXPECT_TRUE(hasC) << "net 'c' missing";
  EXPECT_TRUE(hasD) << "net 'd' missing";
}

// ---------------------------------------------------------------------------
// Assignment: a = b ? (* no_glitch *) c : d
// ---------------------------------------------------------------------------
TEST_F(AttributesConditional, AssignmentExists) {
  ASSERT_NE(getAssignment(m_design), nullptr) << "blocking assignment not found";
}

TEST_F(AttributesConditional, AssignmentIsBlocking) {
  const uhdm::Assignment *const a = getAssignment(m_design);
  ASSERT_NE(a, nullptr);
  EXPECT_TRUE(a->getBlocking()) << "assignment should be blocking";
}

TEST_F(AttributesConditional, AssignmentLhsIsA) {
  const uhdm::Assignment *const a = getAssignment(m_design);
  ASSERT_NE(a, nullptr);
  const uhdm::RefObj *const lhs = a->getLhs<uhdm::RefObj>();
  ASSERT_NE(lhs, nullptr) << "LHS is not a RefObj";
  EXPECT_EQ(lhs->getName(), "a");
}

// ---------------------------------------------------------------------------
// RHS: ternary Operation (condition)
// ---------------------------------------------------------------------------
TEST_F(AttributesConditional, RhsIsConditionalOperation) {
  const uhdm::Assignment *const a = getAssignment(m_design);
  ASSERT_NE(a, nullptr);
  const uhdm::Operation *const rhs = a->getRhs<uhdm::Operation>();
  ASSERT_NE(rhs, nullptr) << "RHS is not an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiConditionOp)
      << "RHS opType should be vpiConditionOp (32)";
}

TEST_F(AttributesConditional, ConditionalHasThreeOperands) {
  const uhdm::Assignment *const a = getAssignment(m_design);
  ASSERT_NE(a, nullptr);
  const uhdm::Operation *const rhs = a->getRhs<uhdm::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  EXPECT_EQ(rhs->getOperands()->size(), 3u)
      << "ternary operator needs exactly 3 operands: condition, true, false";
}

TEST_F(AttributesConditional, OperandZeroIsConditionB) {
  const uhdm::Assignment *const a = getAssignment(m_design);
  ASSERT_NE(a, nullptr);
  const uhdm::Operation *const rhs = a->getRhs<uhdm::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 3u);

  const uhdm::RefObj *const cond =
      any_cast<uhdm::RefObj>((*rhs->getOperands())[0]);
  ASSERT_NE(cond, nullptr) << "operand[0] (condition) should be a RefObj";
  EXPECT_EQ(cond->getName(), "b");
}

TEST_F(AttributesConditional, OperandOneIsTrueBranchC) {
  const uhdm::Assignment *const a = getAssignment(m_design);
  ASSERT_NE(a, nullptr);
  const uhdm::Operation *const rhs = a->getRhs<uhdm::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 3u);

  const uhdm::RefObj *const trueBranch =
      any_cast<uhdm::RefObj>((*rhs->getOperands())[1]);
  ASSERT_NE(trueBranch, nullptr) << "operand[1] (true branch) should be a RefObj";
  EXPECT_EQ(trueBranch->getName(), "c");
}

TEST_F(AttributesConditional, OperandTwoIsFalseBranchD) {
  const uhdm::Assignment *const a = getAssignment(m_design);
  ASSERT_NE(a, nullptr);
  const uhdm::Operation *const rhs = a->getRhs<uhdm::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 3u);

  const uhdm::RefObj *const falseBranch =
      any_cast<uhdm::RefObj>((*rhs->getOperands())[2]);
  ASSERT_NE(falseBranch, nullptr) << "operand[2] (false branch) should be a RefObj";
  EXPECT_EQ(falseBranch->getName(), "d");
}

// ---------------------------------------------------------------------------
// (* no_glitch *) attribute on the true-branch operand
// ---------------------------------------------------------------------------
TEST_F(AttributesConditional, TrueBranchHasNoGlitchAttribute) {
  const uhdm::Assignment *const a = getAssignment(m_design);
  ASSERT_NE(a, nullptr);
  const uhdm::Operation *const rhs = a->getRhs<uhdm::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 3u);

  // Attribute is on the branch Expr node, not on the Operation itself.
  const uhdm::RefObj *const trueBranch =
      any_cast<uhdm::RefObj>((*rhs->getOperands())[1]);
  ASSERT_NE(trueBranch, nullptr);
  ASSERT_NE(trueBranch->getAttributes(), nullptr)
      << "true branch 'c' should have attributes";
  ASSERT_EQ(trueBranch->getAttributes()->size(), 1u);
  EXPECT_EQ((*trueBranch->getAttributes())[0]->getName(), "no_glitch");
}

TEST_F(AttributesConditional, NoGlitchIsFlagAttribute) {
  const uhdm::Assignment *const a = getAssignment(m_design);
  ASSERT_NE(a, nullptr);
  const uhdm::Operation *const rhs = a->getRhs<uhdm::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 3u);

  const uhdm::RefObj *const trueBranch =
      any_cast<uhdm::RefObj>((*rhs->getOperands())[1]);
  ASSERT_NE(trueBranch, nullptr);
  ASSERT_NE(trueBranch->getAttributes(), nullptr);
  ASSERT_EQ(trueBranch->getAttributes()->size(), 1u);

  // Flag attribute: no = expr, so getValue() is null
  const uhdm::Attribute *const attr = (*trueBranch->getAttributes())[0];
  ASSERT_NE(attr, nullptr);
  EXPECT_EQ(attr->getValue(), nullptr)
      << "'no_glitch' is a flag attribute and should have no value";
}

TEST_F(AttributesConditional, ConditionAndFalseBranchHaveNoAttributes) {
  const uhdm::Assignment *const a = getAssignment(m_design);
  ASSERT_NE(a, nullptr);
  const uhdm::Operation *const rhs = a->getRhs<uhdm::Operation>();
  ASSERT_NE(rhs, nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 3u);

  const uhdm::RefObj *const cond  =
      any_cast<uhdm::RefObj>((*rhs->getOperands())[0]);
  const uhdm::RefObj *const falseBranch =
      any_cast<uhdm::RefObj>((*rhs->getOperands())[2]);
  ASSERT_NE(cond,        nullptr);
  ASSERT_NE(falseBranch, nullptr);

  EXPECT_TRUE(!cond->getAttributes() || cond->getAttributes()->empty())
      << "condition 'b' should have no attributes";
  EXPECT_TRUE(!falseBranch->getAttributes() || falseBranch->getAttributes()->empty())
      << "false branch 'd' should have no attributes";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
