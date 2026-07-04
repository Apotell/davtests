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

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/attribute.h>
#include <hldb/begin.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>

namespace hlc {

class AttributesOperator : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.12-attributes-operator.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

static const hldb::Assignment *getAssignment(const hldb::Design *design) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", design->getAllModules());
  if (!top || !top->getProcesses()) return nullptr;
  for (const hldb::Process *const p : *top->getProcesses()) {
    if (const hldb::Initial *const i = any_cast<hldb::Initial>(p)) {
      const hldb::Begin *const blk = i->getStmt<hldb::Begin>();
      if (!blk || !blk->getStmts()) return nullptr;
      for (const hldb::Any *const s : *blk->getStmts())
        if (const hldb::Assignment *const a = any_cast<hldb::Assignment>(s)) return a;
    }
  }
  return nullptr;
}

// Helper: get the add-operation RHS from the assignment.
static const hldb::Operation *getAddOp(const hldb::Design *design) {
  const hldb::Assignment *const a = getAssignment(design);
  if (!a) return nullptr;
  return a->getRhs<hldb::Operation>();
}

// ---------------------------------------------------------------------------
// Module and nets
// ---------------------------------------------------------------------------
TEST_F(AttributesOperator, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr);
}

TEST_F(AttributesOperator, ThreeNetsExist) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getNets(), nullptr);
  EXPECT_EQ(top->getNets()->size(), 3u);

  bool hasA = false, hasB = false, hasC = false;
  for (const hldb::Net *const n : *top->getNets()) {
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
  const hldb::Assignment *const a = getAssignment(m_design);
  ASSERT_NE(a, nullptr);
  const hldb::RefObj *const lhs = a->getLhs<hldb::RefObj>();
  ASSERT_NE(lhs, nullptr) << "LHS is not a RefObj";
  EXPECT_EQ(lhs->getName(), "a");
}

// ---------------------------------------------------------------------------
// RHS: add Operation
// ---------------------------------------------------------------------------
TEST_F(AttributesOperator, RhsIsAddOperation) {
  const hldb::Operation *const op = getAddOp(m_design);
  ASSERT_NE(op, nullptr) << "RHS is not an Operation";
  EXPECT_EQ(op->getOpType(), vpiAddOp) << "expected vpiAddOp (24)";
}

TEST_F(AttributesOperator, AddOperationHasTwoOperands) {
  const hldb::Operation *const op = getAddOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_NE(op->getOperands(), nullptr);
  EXPECT_EQ(op->getOperands()->size(), 2u);
}

TEST_F(AttributesOperator, LeftOperandIsB) {
  const hldb::Operation *const op = getAddOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);

  const hldb::RefObj *const left = any_cast<hldb::RefObj>((*op->getOperands())[0]);
  ASSERT_NE(left, nullptr) << "operand[0] should be a RefObj";
  EXPECT_EQ(left->getName(), "b");
}

TEST_F(AttributesOperator, RightOperandIsC) {
  const hldb::Operation *const op = getAddOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);

  const hldb::RefObj *const right = any_cast<hldb::RefObj>((*op->getOperands())[1]);
  ASSERT_NE(right, nullptr) << "operand[1] should be a RefObj";
  EXPECT_EQ(right->getName(), "c");
}

// ---------------------------------------------------------------------------
// (* mode = "cla" *) on the right operand
// ---------------------------------------------------------------------------
TEST_F(AttributesOperator, RightOperandHasModeAttribute) {
  const hldb::Operation *const op = getAddOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);

  // Attribute is on the operand Expr, not on the Operation.
  const hldb::RefObj *const right = any_cast<hldb::RefObj>((*op->getOperands())[1]);
  ASSERT_NE(right, nullptr);
  ASSERT_NE(right->getAttributes(), nullptr) << "right operand 'c' should have attributes";
  ASSERT_EQ(right->getAttributes()->size(), 1u);
  EXPECT_EQ((*right->getAttributes())[0]->getName(), "mode");
}

TEST_F(AttributesOperator, ModeAttributeIsStringValued) {
  const hldb::Operation *const op = getAddOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);

  const hldb::RefObj *const right = any_cast<hldb::RefObj>((*op->getOperands())[1]);
  ASSERT_NE(right, nullptr);
  ASSERT_NE(right->getAttributes(), nullptr);
  ASSERT_EQ(right->getAttributes()->size(), 1u);

  const hldb::Attribute *const attr = (*right->getAttributes())[0];
  ASSERT_NE(attr, nullptr);
  const hldb::Constant *const val = attr->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr) << "mode attribute should have a Constant value";
  // vpiStringConst = 6
  EXPECT_EQ(val->getConstType(), 6) << "attribute value should be a string constant";
}

TEST_F(AttributesOperator, ModeAttributeValueIsCla) {
  const hldb::Operation *const op = getAddOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);

  const hldb::RefObj *const right = any_cast<hldb::RefObj>((*op->getOperands())[1]);
  ASSERT_NE(right, nullptr);
  ASSERT_NE(right->getAttributes(), nullptr);
  ASSERT_EQ(right->getAttributes()->size(), 1u);

  const hldb::Constant *const val = (*right->getAttributes())[0]->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr);
  // getValue() returns the raw string without surrounding quotes
  EXPECT_EQ(val->getValue(), "cla");
}

TEST_F(AttributesOperator, LeftOperandHasNoAttributes) {
  const hldb::Operation *const op = getAddOp(m_design);
  ASSERT_NE(op, nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);

  const hldb::RefObj *const left = any_cast<hldb::RefObj>((*op->getOperands())[0]);
  ASSERT_NE(left, nullptr);
  EXPECT_TRUE(!left->getAttributes() || left->getAttributes()->empty()) << "left operand 'b' should have no attributes";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
