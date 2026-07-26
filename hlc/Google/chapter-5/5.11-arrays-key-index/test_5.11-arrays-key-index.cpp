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

// Validates array assignment-pattern with mixed key types in UHDM:
//   typedef int triple [1:3];
//   triple b = '{1:1, default:0};
// Key type mapping:
//   integer index key  (1:1)       → TaggedPattern.getTag<Constant>()
//   default key        (default:0) → TaggedPattern.getTag<RefObj>("default")

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/array_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/tagged_pattern.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>

namespace hlc {

class ArraysKeyIndex : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "5.11-arrays-key-index.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// Helper: return Variable "b" from top.
static const hldb::Variable *getVarB(const hldb::Design *design) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", design->getAllModules());
  if (!top || !top->getVariables()) return nullptr;
  for (const hldb::Variable *const v : *top->getVariables()) {
    if (v->getName() == "b") return v;
  }
  return nullptr;
}

// ---------------------------------------------------------------------------
// Module
// ---------------------------------------------------------------------------
TEST_F(ArraysKeyIndex, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// Typedef triple = int [1:3]
// ---------------------------------------------------------------------------
TEST_F(ArraysKeyIndex, TypedefTripleExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);

  const hldb::TypedefTypespec *triple = nullptr;
  for (const hldb::Typespec *const ts : *top->getTypespecs()) {
    if (const hldb::TypedefTypespec *const tdt = any_cast<hldb::TypedefTypespec>(ts)) {
      if (tdt->getName() == "triple") {
        triple = tdt;
        break;
      }
    }
  }
  ASSERT_NE(triple, nullptr) << "TypedefTypespec 'triple' not found";
}

TEST_F(ArraysKeyIndex, TripleAliasesIntArray) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);

  const hldb::TypedefTypespec *triple = nullptr;
  for (const hldb::Typespec *const ts : *top->getTypespecs()) {
    if (const hldb::TypedefTypespec *const tdt = any_cast<hldb::TypedefTypespec>(ts)) {
      if (tdt->getName() == "triple") {
        triple = tdt;
        break;
      }
    }
  }
  ASSERT_NE(triple, nullptr);
  ASSERT_NE(triple->getTypedefAlias(), nullptr) << "triple has no typedefAlias";
  EXPECT_NE(any_cast<hldb::ArrayTypespec>(triple->getTypedefAlias()->getActual()), nullptr)
      << "triple should alias an ArrayTypespec";
}

TEST_F(ArraysKeyIndex, TripleArrayHasRangeOneToThree) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);

  const hldb::TypedefTypespec *triple = nullptr;
  for (const hldb::Typespec *const ts : *top->getTypespecs()) {
    if (const hldb::TypedefTypespec *const tdt = any_cast<hldb::TypedefTypespec>(ts)) {
      if (tdt->getName() == "triple") {
        triple = tdt;
        break;
      }
    }
  }
  ASSERT_NE(triple, nullptr);
  const hldb::ArrayTypespec *const at = any_cast<hldb::ArrayTypespec>(triple->getTypedefAlias()->getActual());
  ASSERT_NE(at, nullptr);
  ASSERT_NE(at->getRange(), nullptr) << "triple ArrayTypespec has no range";

  const hldb::Constant *const left = at->getRange()->getLeftExpr<hldb::Constant>();
  const hldb::Constant *const right = at->getRange()->getRightExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr) << "range left bound is not a Constant";
  ASSERT_NE(right, nullptr) << "range right bound is not a Constant";
  EXPECT_EQ(left->getDecompile(), "1") << "range left should be 1";
  EXPECT_EQ(right->getDecompile(), "3") << "range right should be 3";
}

// ---------------------------------------------------------------------------
// Variable b
// ---------------------------------------------------------------------------
TEST_F(ArraysKeyIndex, VariableBExists) {
  ASSERT_NE(getVarB(m_design), nullptr) << "variable 'b' not found in top";
}

TEST_F(ArraysKeyIndex, VariableBTypespecIsTriple) {
  const hldb::Variable *const b = getVarB(m_design);
  ASSERT_NE(b, nullptr);
  ASSERT_NE(b->getTypespec(), nullptr) << "variable 'b' has no typespec";

  const hldb::TypedefTypespec *const tdt = any_cast<hldb::TypedefTypespec>(b->getTypespec()->getActual());
  ASSERT_NE(tdt, nullptr) << "variable 'b' typespec does not resolve to a TypedefTypespec";
  EXPECT_EQ(tdt->getName(), "triple");
}

// ---------------------------------------------------------------------------
// Initializer: '{1:1, default:0}
// ---------------------------------------------------------------------------
TEST_F(ArraysKeyIndex, InitializerIsAssignPatternWithTwoTaggedPatterns) {
  const hldb::Variable *const b = getVarB(m_design);
  ASSERT_NE(b, nullptr);

  const hldb::Operation *const val = b->getValue<hldb::Operation>();
  ASSERT_NE(val, nullptr) << "variable 'b' value is not an Operation";
  EXPECT_EQ(val->getOpType(), vpiAssignmentPatternOp);
  ASSERT_NE(val->getOperands(), nullptr);
  ASSERT_EQ(val->getOperands()->size(), 2u) << "expected 2 key-value pairs";

  for (size_t i = 0; i < 2u; ++i) {
    EXPECT_NE(any_cast<hldb::TaggedPattern>((*val->getOperands())[i]), nullptr)
        << "operand [" << i << "] should be a TaggedPattern";
  }
}

// ---------------------------------------------------------------------------
// First TaggedPattern: 1:1 — integer index key
// ---------------------------------------------------------------------------
TEST_F(ArraysKeyIndex, FirstTagIsIntegerConstant) {
  const hldb::Variable *const b = getVarB(m_design);
  ASSERT_NE(b, nullptr);

  const hldb::Operation *const val = b->getValue<hldb::Operation>();
  ASSERT_NE(val, nullptr);
  ASSERT_EQ(val->getOperands()->size(), 2u);

  const hldb::TaggedPattern *const tp0 = any_cast<hldb::TaggedPattern>((*val->getOperands())[0]);
  ASSERT_NE(tp0, nullptr);

  // Array index key 1 → tag is a Constant, not a RefObj or RefTypespec
  const hldb::Constant *const tag = tp0->getTag<hldb::Constant>();
  ASSERT_NE(tag, nullptr) << "first tag should be a Constant (integer index key '1')";
  EXPECT_EQ(tag->getDecompile(), "1") << "first index key should be 1";
}

TEST_F(ArraysKeyIndex, FirstPatternValueIsOne) {
  const hldb::Variable *const b = getVarB(m_design);
  ASSERT_NE(b, nullptr);

  const hldb::Operation *const val = b->getValue<hldb::Operation>();
  ASSERT_NE(val, nullptr);
  ASSERT_EQ(val->getOperands()->size(), 2u);

  const hldb::TaggedPattern *const tp0 = any_cast<hldb::TaggedPattern>((*val->getOperands())[0]);
  ASSERT_NE(tp0, nullptr);

  const hldb::Constant *const pattern = tp0->getPattern<hldb::Constant>();
  ASSERT_NE(pattern, nullptr) << "first pattern value should be a Constant";
  EXPECT_EQ(pattern->getDecompile(), "1") << "first pattern value should be 1";
}

// ---------------------------------------------------------------------------
// Second TaggedPattern: default:0 — default key
// ---------------------------------------------------------------------------
TEST_F(ArraysKeyIndex, SecondTagIsDefaultRefObj) {
  const hldb::Variable *const b = getVarB(m_design);
  ASSERT_NE(b, nullptr);

  const hldb::Operation *const val = b->getValue<hldb::Operation>();
  ASSERT_NE(val, nullptr);
  ASSERT_EQ(val->getOperands()->size(), 2u);

  const hldb::TaggedPattern *const tp1 = any_cast<hldb::TaggedPattern>((*val->getOperands())[1]);
  ASSERT_NE(tp1, nullptr);

  // default key → tag is a RefObj named "default"
  const hldb::RefObj *const tag = tp1->getTag<hldb::RefObj>();
  ASSERT_NE(tag, nullptr) << "second tag should be a RefObj (the 'default' keyword)";
  EXPECT_EQ(tag->getName(), "default");
}

TEST_F(ArraysKeyIndex, SecondPatternValueIsZero) {
  const hldb::Variable *const b = getVarB(m_design);
  ASSERT_NE(b, nullptr);

  const hldb::Operation *const val = b->getValue<hldb::Operation>();
  ASSERT_NE(val, nullptr);
  ASSERT_EQ(val->getOperands()->size(), 2u);

  const hldb::TaggedPattern *const tp1 = any_cast<hldb::TaggedPattern>((*val->getOperands())[1]);
  ASSERT_NE(tp1, nullptr);

  const hldb::Constant *const pattern = tp1->getPattern<hldb::Constant>();
  ASSERT_NE(pattern, nullptr) << "second pattern value should be a Constant";
  EXPECT_EQ(pattern->getDecompile(), "0") << "default pattern value should be 0";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
