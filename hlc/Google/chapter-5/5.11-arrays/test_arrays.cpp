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

// Validates a 2D int array with a mixed initializer in UHDM:
//   int n[1:2][1:3] = '{'{0,1,2}, '{3{4}}};
// UHDM structure:
//   Net n → outer ArrayTypespec[1:2]
//               └─ elemTypespec → inner ArrayTypespec[1:3]
//                                      └─ elemTypespec → IntTypespec
//   Value = AssignPatternOp(2 items):
//             [0] AssignPatternOp(3 Constants: 0,1,2)   ← '{0,1,2}
//             [1] AssignPatternOp(2 operands: Constant 3, Constant 4) ← '{3{4}}

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/array_typespec.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/int_typespec.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/operation.h>
#include <uhdm/range.h>
#include <uhdm/ref_typespec.h>

namespace SURELOG {

class Arrays : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "5.11-arrays.hlc"});

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

// Helper: return Net "n" from work@top.
static const uhdm::Net *getNetN(const uhdm::Design *design) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", design->getAllModules());
  if (!top || !top->getNets()) return nullptr;
  for (const uhdm::Net *const net : *top->getNets()) {
    if (net->getName() == "n") return net;
  }
  return nullptr;
}

// ---------------------------------------------------------------------------
// Module
// ---------------------------------------------------------------------------
TEST_F(Arrays, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Net n
// ---------------------------------------------------------------------------
TEST_F(Arrays, NetNExists) {
  ASSERT_NE(getNetN(m_design), nullptr) << "net 'n' not found in work@top";
}

// ---------------------------------------------------------------------------
// Typespec chain: ArrayTypespec[1:2] → ArrayTypespec[1:3] → IntTypespec
// ---------------------------------------------------------------------------
TEST_F(Arrays, NetNTypespecIsArrayTypespec) {
  const uhdm::Net *const n = getNetN(m_design);
  ASSERT_NE(n, nullptr);
  ASSERT_NE(n->getTypespec(), nullptr) << "net 'n' has no typespec";
  EXPECT_NE(any_cast<uhdm::ArrayTypespec>(n->getTypespec()->getActual()), nullptr)
      << "net 'n' typespec does not resolve to an ArrayTypespec";
}

TEST_F(Arrays, OuterArrayHasRange) {
  const uhdm::Net *const n = getNetN(m_design);
  ASSERT_NE(n, nullptr);
  const uhdm::ArrayTypespec *const outer =
      any_cast<uhdm::ArrayTypespec>(n->getTypespec()->getActual());
  ASSERT_NE(outer, nullptr);
  ASSERT_NE(outer->getRange(), nullptr) << "outer ArrayTypespec has no range (expected [1:2])";
}

TEST_F(Arrays, OuterArrayRangeBoundsAreOneAndTwo) {
  const uhdm::Net *const n = getNetN(m_design);
  ASSERT_NE(n, nullptr);
  const uhdm::ArrayTypespec *const outer =
      any_cast<uhdm::ArrayTypespec>(n->getTypespec()->getActual());
  ASSERT_NE(outer, nullptr);
  const uhdm::Range *const rng = outer->getRange();
  ASSERT_NE(rng, nullptr);

  const uhdm::Constant *const left = rng->getLeftExpr<uhdm::Constant>();
  const uhdm::Constant *const right = rng->getRightExpr<uhdm::Constant>();
  ASSERT_NE(left, nullptr)  << "outer range left bound is not a Constant";
  ASSERT_NE(right, nullptr) << "outer range right bound is not a Constant";
  EXPECT_EQ(left->getDecompile(),  "1") << "outer left bound should be 1";
  EXPECT_EQ(right->getDecompile(), "2") << "outer right bound should be 2";
}

TEST_F(Arrays, OuterArrayElemTypespecIsArrayTypespec) {
  const uhdm::Net *const n = getNetN(m_design);
  ASSERT_NE(n, nullptr);
  const uhdm::ArrayTypespec *const outer =
      any_cast<uhdm::ArrayTypespec>(n->getTypespec()->getActual());
  ASSERT_NE(outer, nullptr);
  ASSERT_NE(outer->getElemTypespec(), nullptr) << "outer ArrayTypespec has no elemTypespec";
  EXPECT_NE(any_cast<uhdm::ArrayTypespec>(outer->getElemTypespec()->getActual()), nullptr)
      << "outer ArrayTypespec elem does not resolve to an ArrayTypespec (inner dim)";
}

TEST_F(Arrays, InnerArrayHasRange) {
  const uhdm::Net *const n = getNetN(m_design);
  ASSERT_NE(n, nullptr);
  const uhdm::ArrayTypespec *const outer =
      any_cast<uhdm::ArrayTypespec>(n->getTypespec()->getActual());
  ASSERT_NE(outer, nullptr);
  const uhdm::ArrayTypespec *const inner =
      any_cast<uhdm::ArrayTypespec>(outer->getElemTypespec()->getActual());
  ASSERT_NE(inner, nullptr);
  ASSERT_NE(inner->getRange(), nullptr) << "inner ArrayTypespec has no range (expected [1:3])";
}

TEST_F(Arrays, InnerArrayRangeBoundsAreOneAndThree) {
  const uhdm::Net *const n = getNetN(m_design);
  ASSERT_NE(n, nullptr);
  const uhdm::ArrayTypespec *const outer =
      any_cast<uhdm::ArrayTypespec>(n->getTypespec()->getActual());
  ASSERT_NE(outer, nullptr);
  const uhdm::ArrayTypespec *const inner =
      any_cast<uhdm::ArrayTypespec>(outer->getElemTypespec()->getActual());
  ASSERT_NE(inner, nullptr);
  const uhdm::Range *const rng = inner->getRange();
  ASSERT_NE(rng, nullptr);

  const uhdm::Constant *const left = rng->getLeftExpr<uhdm::Constant>();
  const uhdm::Constant *const right = rng->getRightExpr<uhdm::Constant>();
  ASSERT_NE(left, nullptr)  << "inner range left bound is not a Constant";
  ASSERT_NE(right, nullptr) << "inner range right bound is not a Constant";
  EXPECT_EQ(left->getDecompile(),  "1") << "inner left bound should be 1";
  EXPECT_EQ(right->getDecompile(), "3") << "inner right bound should be 3";
}

TEST_F(Arrays, InnerArrayElemTypespecIsIntTypespec) {
  const uhdm::Net *const n = getNetN(m_design);
  ASSERT_NE(n, nullptr);
  const uhdm::ArrayTypespec *const outer =
      any_cast<uhdm::ArrayTypespec>(n->getTypespec()->getActual());
  ASSERT_NE(outer, nullptr);
  const uhdm::ArrayTypespec *const inner =
      any_cast<uhdm::ArrayTypespec>(outer->getElemTypespec()->getActual());
  ASSERT_NE(inner, nullptr);
  ASSERT_NE(inner->getElemTypespec(), nullptr) << "inner ArrayTypespec has no elemTypespec";
  EXPECT_NE(any_cast<uhdm::IntTypespec>(inner->getElemTypespec()->getActual()), nullptr)
      << "inner ArrayTypespec elem should be IntTypespec";
}

// ---------------------------------------------------------------------------
// Initializer: '{'{0,1,2}, '{3{4}}}
// ---------------------------------------------------------------------------
TEST_F(Arrays, InitializerIsAssignPatternOpWithTwoRows) {
  const uhdm::Net *const n = getNetN(m_design);
  ASSERT_NE(n, nullptr);

  const uhdm::Operation *const val = n->getValue<uhdm::Operation>();
  ASSERT_NE(val, nullptr) << "net 'n' value is not an Operation";
  EXPECT_EQ(val->getOpType(), vpiAssignmentPatternOp);
  ASSERT_NE(val->getOperands(), nullptr);
  EXPECT_EQ(val->getOperands()->size(), 2u) << "expected 2 row elements in outer pattern";
}

TEST_F(Arrays, FirstRowIsAssignPatternWithThreeConstants) {
  const uhdm::Net *const n = getNetN(m_design);
  ASSERT_NE(n, nullptr);

  const uhdm::Operation *const val = n->getValue<uhdm::Operation>();
  ASSERT_NE(val, nullptr);
  ASSERT_EQ(val->getOperands()->size(), 2u);

  // '{0,1,2}
  const uhdm::Operation *const row0 =
      any_cast<uhdm::Operation>((*val->getOperands())[0]);
  ASSERT_NE(row0, nullptr) << "first row is not an Operation";
  EXPECT_EQ(row0->getOpType(), vpiAssignmentPatternOp);
  ASSERT_NE(row0->getOperands(), nullptr);
  ASSERT_EQ(row0->getOperands()->size(), 3u) << "'{0,1,2} should have 3 operands";

  for (size_t i = 0; i < 3u; ++i) {
    EXPECT_NE(any_cast<uhdm::Constant>((*row0->getOperands())[i]), nullptr)
        << "'{0,1,2} operand [" << i << "] should be a Constant";
  }
}

TEST_F(Arrays, SecondRowIsAssignPatternWithTwoOperands) {
  const uhdm::Net *const n = getNetN(m_design);
  ASSERT_NE(n, nullptr);

  const uhdm::Operation *const val = n->getValue<uhdm::Operation>();
  ASSERT_NE(val, nullptr);
  ASSERT_EQ(val->getOperands()->size(), 2u);

  // '{3{4}} → AssignPatternOp with count=3 and value=4 as separate operands
  const uhdm::Operation *const row1 =
      any_cast<uhdm::Operation>((*val->getOperands())[1]);
  ASSERT_NE(row1, nullptr) << "second row is not an Operation";
  EXPECT_EQ(row1->getOpType(), vpiAssignmentPatternOp);
  ASSERT_NE(row1->getOperands(), nullptr);
  EXPECT_EQ(row1->getOperands()->size(), 2u) << "'{3{4}} should have 2 operands (count + value)";
}

TEST_F(Arrays, SecondRowOperandsAreConstants) {
  const uhdm::Net *const n = getNetN(m_design);
  ASSERT_NE(n, nullptr);

  const uhdm::Operation *const val = n->getValue<uhdm::Operation>();
  ASSERT_NE(val, nullptr);
  ASSERT_EQ(val->getOperands()->size(), 2u);

  const uhdm::Operation *const row1 =
      any_cast<uhdm::Operation>((*val->getOperands())[1]);
  ASSERT_NE(row1, nullptr);
  ASSERT_EQ(row1->getOperands()->size(), 2u);

  const uhdm::Constant *const count =
      any_cast<uhdm::Constant>((*row1->getOperands())[0]);
  const uhdm::Constant *const value =
      any_cast<uhdm::Constant>((*row1->getOperands())[1]);
  ASSERT_NE(count, nullptr) << "first operand of '{3{4}} should be a Constant (3)";
  ASSERT_NE(value, nullptr) << "second operand of '{3{4}} should be a Constant (4)";
  EXPECT_EQ(count->getDecompile(), "3") << "replication count should be 3";
  EXPECT_EQ(value->getDecompile(), "4") << "replicated value should be 4";
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
