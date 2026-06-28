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

// Validates nested replication assignment-patterns in UHDM:
//   int n[1:2][1:6] = '{2{'{3{4, 5}}}};
//
// Replication pattern encoding rule (verified from UHDM dump):
//   '{N{v1, v2, ...}} → AssignPatternOp where operand[0] is the count N
//                       and operand[1..k] are the replicated values.
//
// UHDM tree:
//   Net n → outer ArrayTypespec[1:2] → inner ArrayTypespec[1:6] → IntTypespec
//   Value = AssignPatternOp(75):
//             [0] Constant "2"          ← outer replication count
//             [1] AssignPatternOp(75):
//                   [0] Constant "3"    ← inner replication count
//                   [1] Constant "4"
//                   [2] Constant "5"

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/int_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>

namespace hlc {

class ArraysReplication : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.11-arrays-replication.hlc"});

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

static const hldb::Net *getNetN(const hldb::Design *design) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", design->getAllModules());
  if (!top || !top->getNets()) return nullptr;
  for (const hldb::Net *const net : *top->getNets()) {
    if (net->getName() == "n") return net;
  }
  return nullptr;
}

// ---------------------------------------------------------------------------
// Module and net
// ---------------------------------------------------------------------------
TEST_F(ArraysReplication, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr);
}

TEST_F(ArraysReplication, NetNExists) {
  ASSERT_NE(getNetN(m_design), nullptr) << "net 'n' not found in work@top";
}

// ---------------------------------------------------------------------------
// Typespec chain: ArrayTypespec[1:2] → ArrayTypespec[1:6] → IntTypespec
// ---------------------------------------------------------------------------
TEST_F(ArraysReplication, NetNTypespecIsOuterArrayTypespec) {
  const hldb::Net *const n = getNetN(m_design);
  ASSERT_NE(n, nullptr);
  ASSERT_NE(n->getTypespec(), nullptr);
  EXPECT_NE(any_cast<hldb::ArrayTypespec>(n->getTypespec()->getActual()), nullptr)
      << "net 'n' typespec should resolve to an ArrayTypespec";
}

TEST_F(ArraysReplication, OuterArrayRangeIsOneToTwo) {
  const hldb::Net *const n = getNetN(m_design);
  ASSERT_NE(n, nullptr);
  const hldb::ArrayTypespec *const outer =
      any_cast<hldb::ArrayTypespec>(n->getTypespec()->getActual());
  ASSERT_NE(outer, nullptr);
  const hldb::Range *const rng = outer->getRange();
  ASSERT_NE(rng, nullptr) << "outer ArrayTypespec has no range";

  const hldb::Constant *const left  = rng->getLeftExpr<hldb::Constant>();
  const hldb::Constant *const right = rng->getRightExpr<hldb::Constant>();
  ASSERT_NE(left,  nullptr) << "outer left bound is not a Constant";
  ASSERT_NE(right, nullptr) << "outer right bound is not a Constant";
  EXPECT_EQ(left->getDecompile(),  "1") << "outer left bound should be 1";
  EXPECT_EQ(right->getDecompile(), "2") << "outer right bound should be 2";
}

TEST_F(ArraysReplication, OuterArrayElemTypespecIsArrayTypespec) {
  const hldb::Net *const n = getNetN(m_design);
  ASSERT_NE(n, nullptr);
  const hldb::ArrayTypespec *const outer =
      any_cast<hldb::ArrayTypespec>(n->getTypespec()->getActual());
  ASSERT_NE(outer, nullptr);
  ASSERT_NE(outer->getElemTypespec(), nullptr);
  EXPECT_NE(any_cast<hldb::ArrayTypespec>(outer->getElemTypespec()->getActual()), nullptr)
      << "outer elem typespec should be the inner ArrayTypespec";
}

TEST_F(ArraysReplication, InnerArrayRangeIsOneToSix) {
  const hldb::Net *const n = getNetN(m_design);
  ASSERT_NE(n, nullptr);
  const hldb::ArrayTypespec *const outer =
      any_cast<hldb::ArrayTypespec>(n->getTypespec()->getActual());
  ASSERT_NE(outer, nullptr);
  const hldb::ArrayTypespec *const inner =
      any_cast<hldb::ArrayTypespec>(outer->getElemTypespec()->getActual());
  ASSERT_NE(inner, nullptr);
  const hldb::Range *const rng = inner->getRange();
  ASSERT_NE(rng, nullptr) << "inner ArrayTypespec has no range";

  const hldb::Constant *const left  = rng->getLeftExpr<hldb::Constant>();
  const hldb::Constant *const right = rng->getRightExpr<hldb::Constant>();
  ASSERT_NE(left,  nullptr) << "inner left bound is not a Constant";
  ASSERT_NE(right, nullptr) << "inner right bound is not a Constant";
  EXPECT_EQ(left->getDecompile(),  "1") << "inner left bound should be 1";
  EXPECT_EQ(right->getDecompile(), "6") << "inner right bound should be 6";
}

TEST_F(ArraysReplication, InnerArrayElemTypespecIsIntTypespec) {
  const hldb::Net *const n = getNetN(m_design);
  ASSERT_NE(n, nullptr);
  const hldb::ArrayTypespec *const outer =
      any_cast<hldb::ArrayTypespec>(n->getTypespec()->getActual());
  ASSERT_NE(outer, nullptr);
  const hldb::ArrayTypespec *const inner =
      any_cast<hldb::ArrayTypespec>(outer->getElemTypespec()->getActual());
  ASSERT_NE(inner, nullptr);
  ASSERT_NE(inner->getElemTypespec(), nullptr);
  EXPECT_NE(any_cast<hldb::IntTypespec>(inner->getElemTypespec()->getActual()), nullptr)
      << "inner elem typespec should be IntTypespec";
}

// ---------------------------------------------------------------------------
// Outer pattern: '{2{...}} — count operand + sub-pattern operand
// ---------------------------------------------------------------------------
TEST_F(ArraysReplication, InitializerIsAssignPatternWithTwoOperands) {
  const hldb::Net *const n = getNetN(m_design);
  ASSERT_NE(n, nullptr);

  const hldb::Operation *const val = n->getValue<hldb::Operation>();
  ASSERT_NE(val, nullptr) << "net 'n' value is not an Operation";
  EXPECT_EQ(val->getOpType(), vpiAssignmentPatternOp);
  ASSERT_NE(val->getOperands(), nullptr);
  EXPECT_EQ(val->getOperands()->size(), 2u)
      << "outer '{2{...}} should have 2 operands: count + sub-pattern";
}

TEST_F(ArraysReplication, OuterReplicationCountIsTwo) {
  const hldb::Net *const n = getNetN(m_design);
  ASSERT_NE(n, nullptr);

  const hldb::Operation *const val = n->getValue<hldb::Operation>();
  ASSERT_NE(val, nullptr);
  ASSERT_EQ(val->getOperands()->size(), 2u);

  // operand[0] is the replication count
  const hldb::Constant *const count =
      any_cast<hldb::Constant>((*val->getOperands())[0]);
  ASSERT_NE(count, nullptr) << "outer operand[0] should be a Constant (count 2)";
  EXPECT_EQ(count->getDecompile(), "2");
}

// ---------------------------------------------------------------------------
// Inner pattern: '{3{4, 5}} — count + two value operands
// ---------------------------------------------------------------------------
TEST_F(ArraysReplication, InnerPatternIsAssignPatternWithThreeOperands) {
  const hldb::Net *const n = getNetN(m_design);
  ASSERT_NE(n, nullptr);

  const hldb::Operation *const val = n->getValue<hldb::Operation>();
  ASSERT_NE(val, nullptr);
  ASSERT_EQ(val->getOperands()->size(), 2u);

  const hldb::Operation *const inner =
      any_cast<hldb::Operation>((*val->getOperands())[1]);
  ASSERT_NE(inner, nullptr) << "outer operand[1] should be an Operation (inner pattern)";
  EXPECT_EQ(inner->getOpType(), vpiAssignmentPatternOp);
  ASSERT_NE(inner->getOperands(), nullptr);
  EXPECT_EQ(inner->getOperands()->size(), 3u)
      << "inner '{3{4, 5}} should have 3 operands: count + 2 values";
}

TEST_F(ArraysReplication, InnerReplicationCountIsThree) {
  const hldb::Net *const n = getNetN(m_design);
  ASSERT_NE(n, nullptr);

  const hldb::Operation *const val = n->getValue<hldb::Operation>();
  ASSERT_NE(val, nullptr);
  ASSERT_EQ(val->getOperands()->size(), 2u);

  const hldb::Operation *const inner =
      any_cast<hldb::Operation>((*val->getOperands())[1]);
  ASSERT_NE(inner, nullptr);
  ASSERT_EQ(inner->getOperands()->size(), 3u);

  const hldb::Constant *const count =
      any_cast<hldb::Constant>((*inner->getOperands())[0]);
  ASSERT_NE(count, nullptr) << "inner operand[0] should be a Constant (count 3)";
  EXPECT_EQ(count->getDecompile(), "3");
}

TEST_F(ArraysReplication, InnerPatternValuesAreFourAndFive) {
  const hldb::Net *const n = getNetN(m_design);
  ASSERT_NE(n, nullptr);

  const hldb::Operation *const val = n->getValue<hldb::Operation>();
  ASSERT_NE(val, nullptr);
  ASSERT_EQ(val->getOperands()->size(), 2u);

  const hldb::Operation *const inner =
      any_cast<hldb::Operation>((*val->getOperands())[1]);
  ASSERT_NE(inner, nullptr);
  ASSERT_EQ(inner->getOperands()->size(), 3u);

  const hldb::Constant *const v1 =
      any_cast<hldb::Constant>((*inner->getOperands())[1]);
  const hldb::Constant *const v2 =
      any_cast<hldb::Constant>((*inner->getOperands())[2]);
  ASSERT_NE(v1, nullptr) << "inner operand[1] should be a Constant (value 4)";
  ASSERT_NE(v2, nullptr) << "inner operand[2] should be a Constant (value 5)";
  EXPECT_EQ(v1->getDecompile(), "4");
  EXPECT_EQ(v2->getDecompile(), "5");
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
