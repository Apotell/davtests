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

// Validates an attribute on a binary-operator operand in UHDM:
//   a = b + (* mode = "cla" *) c;
//
// UHDM structure:
//   Assignment (blocking)
//     LHS: RefObj "a"
//     RHS: Operation (vpiAddOp = 24)
//            operand[0]: RefObj "b"   (no attributes)
//            operand[1]: RefObj "c"
//              vpiAttribute[0]: Attribute "mode"
//                vpiValue: Constant, vpiConstType=string(6), getValue()="cla"
//
// Key facts:
//   - The (* ... *) attribute attaches to the Expr operand node (RefObj "c"),
//     not to the Operation itself — same rule as for the ternary operator.
//   - String-valued attributes: getConstType() == 6 (vpiStringConst),
//     getValue() returns the raw string without surrounding quotes.

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/assignment.h>
#include <uhdm/attribute.h>
#include <uhdm/begin.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/initial.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/operation.h>
#include <uhdm/ref_obj.h>

namespace SURELOG {

class AttributesOperator : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.12-attributes-operator.hlc"});

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

static const uhdm::Assignment *getAssignment(const uhdm::Design *design) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", design->getAllModules());
  if (!top || !top->getProcesses()) return nullptr;
  for (const uhdm::Process *const p : *top->getProcesses()) {
    if (const uhdm::Initial *const i = any_cast<uhdm::Initial>(p)) {
      const uhdm::Begin *const blk = i->getStmt<uhdm::Begin>();
      if (!blk || !blk->getStmts()) return nullptr;
      for (const uhdm::Any *const s : *blk->getStmts())
        if (const uhdm::Assignment *const a = any_cast<uhdm::Assignment>(s))
          return a;
    }
  }
  return nullptr;
}

// Helper: get the add-operation RHS from the assignment.
static const uhdm::Operation *getAddOp(const uhdm::Design *design) {
  const uhdm::Assignment *const a = getAssignment(design);
  if (!a) return nullptr;
  return a->getRhs<uhdm::Operation>();
}

// ---------------------------------------------------------------------------
// Module and nets
// ---------------------------------------------------------------------------
TEST_F(AttributesOperator, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

TEST_F(AttributesOperator, ThreeNetsExist) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 3u);

  bool hasA = false, hasB = false, hasC = false;
  for (const uhdm::Net *const n : *top->getNets()) {
    if (n->getName() == "a") hasA = true;
    if (n->getName() == "b") hasB = true;
    if (n->getName() == "c") hasC = true;
  }
  EXPECT_TRUE(hasA) << "net 'a' missing";
  EXPECT_TRUE(hasB) << "net 'b' missing";
  EXPECT_TRUE(hasC) << "net 'c' missing";
}

// ---------------------------------------------------------------------------
// Assignment: a = b + (* mode = "cla" *) c
// ---------------------------------------------------------------------------
TEST_F(AttributesOperator, AssignmentLhsIsA) {
  const uhdm::Assignment *const a = getAssignment(m_design);
  ASSERT_NE(a, nullptr);
  const uhdm::RefObj *const lhs = a->getLhs<uhdm::RefObj>();
  ASSERT_NE(lhs, nullptr) << "LHS is not a RefObj";
  EXPECT_EQ(lhs->getName(), "a");
}

// ---------------------------------------------------------------------------
// RHS: add Operation
// ---------------------------------------------------------------------------
TEST_F(AttributesOperator, RhsIsAddOperation) {
  const uhdm::Operation *const op = getAddOp(m_design);
  ASSERT_NE(op, nullptr) << "RHS is not an Operation";
  EXPECT_EQ(op->getOpType(), vpiAddOp) << "expected vpiAddOp (24)";
}

TEST_F(AttributesOperator, AddOperationHasTwoOperands) {
  const uhdm::Operation *const op = getAddOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u);
}

TEST_F(AttributesOperator, LeftOperandIsB) {
  const uhdm::Operation *const op = getAddOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);

  const uhdm::RefObj *const left =
      any_cast<uhdm::RefObj>((*op->getOperands())[0]);
  ASSERT_NE(left, nullptr) << "operand[0] should be a RefObj";
  EXPECT_EQ(left->getName(), "b");
}

TEST_F(AttributesOperator, RightOperandIsC) {
  const uhdm::Operation *const op = getAddOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);

  const uhdm::RefObj *const right =
      any_cast<uhdm::RefObj>((*op->getOperands())[1]);
  ASSERT_NE(right, nullptr) << "operand[1] should be a RefObj";
  EXPECT_EQ(right->getName(), "c");
}

// ---------------------------------------------------------------------------
// (* mode = "cla" *) on the right operand
// ---------------------------------------------------------------------------
TEST_F(AttributesOperator, RightOperandHasModeAttribute) {
  const uhdm::Operation *const op = getAddOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);

  // Attribute is on the operand Expr, not on the Operation.
  const uhdm::RefObj *const right =
      any_cast<uhdm::RefObj>((*op->getOperands())[1]);
  ASSERT_NE(right, nullptr);
  ASSERT_NE(right->getAttributes(), nullptr)
      << "right operand 'c' should have attributes";
  ASSERT_EQ(right->getAttributes()->size(), 1u);
  EXPECT_EQ((*right->getAttributes())[0]->getName(), "mode");
}

TEST_F(AttributesOperator, ModeAttributeIsStringValued) {
  const uhdm::Operation *const op = getAddOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);

  const uhdm::RefObj *const right =
      any_cast<uhdm::RefObj>((*op->getOperands())[1]);
  ASSERT_NE(right, nullptr);
  ASSERT_NE(right->getAttributes(), nullptr);
  ASSERT_EQ(right->getAttributes()->size(), 1u);

  const uhdm::Attribute *const attr = (*right->getAttributes())[0];
  ASSERT_NE(attr, nullptr);
  const uhdm::Constant *const val = attr->getValue<uhdm::Constant>();
  ASSERT_NE(val, nullptr) << "mode attribute should have a Constant value";
  // vpiStringConst = 6
  EXPECT_EQ(val->getConstType(), 6) << "attribute value should be a string constant";
}

TEST_F(AttributesOperator, ModeAttributeValueIsCla) {
  const uhdm::Operation *const op = getAddOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);

  const uhdm::RefObj *const right =
      any_cast<uhdm::RefObj>((*op->getOperands())[1]);
  ASSERT_NE(right, nullptr);
  ASSERT_NE(right->getAttributes(), nullptr);
  ASSERT_EQ(right->getAttributes()->size(), 1u);

  const uhdm::Constant *const val =
      (*right->getAttributes())[0]->getValue<uhdm::Constant>();
  ASSERT_NE(val, nullptr);
  // getValue() returns the raw string without surrounding quotes
  EXPECT_EQ(val->getValue(), "cla");
}

TEST_F(AttributesOperator, LeftOperandHasNoAttributes) {
  const uhdm::Operation *const op = getAddOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);

  const uhdm::RefObj *const left =
      any_cast<uhdm::RefObj>((*op->getOperands())[0]);
  ASSERT_NE(left, nullptr);
  EXPECT_TRUE(!left->getAttributes() || left->getAttributes()->empty())
      << "left operand 'b' should have no attributes";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
