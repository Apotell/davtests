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

// Spec-based validation of IEEE 1800-2023 Sec 5.10 ("Assignment pattern
// expressions") for a keyed structure assignment pattern that mixes named
// member keys with a `default:` entry.
// SV: tests/DefaultTag/dut.sv
//
//   module top();
//     typedef struct packed {
//       logic [1:0] a;
//       logic [2:0] b;
//       logic [2:0] c;
//       logic [2:0] d;
//     } struct_1;
//     parameter struct_1 X = '{b: 10, d: 20, default: 15};
//   endmodule
//
// -- Sec 5.10 rules under test ------------------------------------------
//   * `'{b: 10, d: 20, default: 15}` is a keyed assignment_pattern with
//     three structure_pattern_key entries. Two are member-name keys ('b',
//     'd'); the third is the `default` keyword, which supplies the value
//     for every member not otherwise listed (here: 'a' and 'c').
//   * Each entry is represented as a TaggedPattern. The 'b'/'d' tags are
//     RefObj (member-name references); the 'default' tag is a RefObj
//     named "default", distinguishing it from an ordinary member-name key.
//   * Source order is preserved: operand[0] = b:10, operand[1] = d:20,
//     operand[2] = default:15.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/param_assign.h>
#include <hldb/parameter.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct_typespec.h>
#include <hldb/tagged_pattern.h>
#include <hldb/typedef_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/vpi_user.h>

#include <string>

namespace hlc {

class DefaultTagTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "DefaultTag.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const hldb::Module *getTop(const hldb::Design *d) {
  return hldb::findByName<hldb::Module>("top", d->getAllModules());
}

static const hldb::ParamAssign *getXParamAssign(const hldb::Design *d) {
  const hldb::Module *const top = getTop(d);
  if (!top) return nullptr;
  return hldb::findByName("X", top->getParamAssigns());
}

static const hldb::Operation *getXPattern(const hldb::Design *d) {
  const hldb::ParamAssign *const pa = getXParamAssign(d);
  if (!pa) return nullptr;
  return pa->getRhs<hldb::Operation>();
}

// ===========================================================================
// Module and typedef
// ===========================================================================

TEST_F(DefaultTagTest, ModuleTopExists) { EXPECT_NE(getTop(m_design), nullptr) << "module 'top' not found"; }

TEST_F(DefaultTagTest, TypedefStruct1Exists) {
  const hldb::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getTypespecs(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::TypedefTypespec>("struct_1", top->getTypespecs()), nullptr)
      << "TypedefTypespec 'struct_1' not found";
}

TEST_F(DefaultTagTest, Struct1HasFourMembers) {
  const hldb::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td = hldb::findByName<hldb::TypedefTypespec>("struct_1", top->getTypespecs());
  ASSERT_NE(td, nullptr);
  const hldb::Typedef *const t = td->getTypedef();
  ASSERT_NE(t, nullptr);
  const hldb::StructTypespec *const st = any_cast<hldb::StructTypespec>(t->getAlias()->getActual());
  ASSERT_NE(st, nullptr) << "struct_1 does not alias a StructTypespec";
  const hldb::Struct *const s = st->getStruct();
  ASSERT_NE(s, nullptr);
  ASSERT_NE(s->getMembers(), nullptr);
  ASSERT_EQ(s->getMembers()->size(), 4u) << "expected members: a, b, c, d";
  EXPECT_EQ((*s->getMembers())[0]->getName(), std::string_view("a"));
  EXPECT_EQ((*s->getMembers())[1]->getName(), std::string_view("b"));
  EXPECT_EQ((*s->getMembers())[2]->getName(), std::string_view("c"));
  EXPECT_EQ((*s->getMembers())[3]->getName(), std::string_view("d"));
}

TEST_F(DefaultTagTest, Struct1IsPacked) {
  const hldb::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  const hldb::TypedefTypespec *const td = hldb::findByName<hldb::TypedefTypespec>("struct_1", top->getTypespecs());
  ASSERT_NE(td, nullptr);
  const hldb::Typedef *const t = td->getTypedef();
  ASSERT_NE(t, nullptr);
  const hldb::StructTypespec *const st = any_cast<hldb::StructTypespec>(t->getAlias()->getActual());
  ASSERT_NE(st, nullptr);
  const hldb::Struct *const s = st->getStruct();
  ASSERT_NE(s, nullptr);
  EXPECT_TRUE(s->getPacked()) << "'struct packed { ... } struct_1' must have getPacked() == true";
}

// ===========================================================================
// parameter struct_1 X = '{b: 10, d: 20, default: 15}
// ===========================================================================

TEST_F(DefaultTagTest, XParameterExists) {
  const hldb::Module *const top = getTop(m_design);
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getParameters(), nullptr);
  EXPECT_NE(hldb::findByName<hldb::Parameter>("X", top->getParameters()), nullptr) << "'X' parameter not found";
}

TEST_F(DefaultTagTest, XRhsIsAssignmentPatternOp) {
  const hldb::Operation *const rhs = getXPattern(m_design);
  ASSERT_NE(rhs, nullptr) << "'{b: 10, d: 20, default: 15}' RHS must be an Operation";
  EXPECT_EQ(rhs->getOpType(), vpiAssignmentPatternOp);
}

TEST_F(DefaultTagTest, XHasThreeOperands) {
  const hldb::Operation *const rhs = getXPattern(m_design);
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  EXPECT_EQ(rhs->getOperands()->size(), 3u) << "three keyed entries: b, d, default";
}

TEST_F(DefaultTagTest, FirstOperandIsBTaggedTen) {
  const hldb::Operation *const rhs = getXPattern(m_design);
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 3u);

  const hldb::TaggedPattern *const tp = any_cast<hldb::TaggedPattern>((*rhs->getOperands())[0]);
  ASSERT_NE(tp, nullptr) << "operand[0] must be a TaggedPattern";
  const hldb::RefObj *const tag = tp->getTag<hldb::RefObj>();
  ASSERT_NE(tag, nullptr) << "'b:' tag must be a RefObj (member name)";
  EXPECT_EQ(tag->getName(), std::string_view("b"));
  const hldb::Constant *const val = tp->getPattern<hldb::Constant>();
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getValue(), std::string_view("10"));
}

TEST_F(DefaultTagTest, SecondOperandIsDTaggedTwenty) {
  const hldb::Operation *const rhs = getXPattern(m_design);
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 3u);

  const hldb::TaggedPattern *const tp = any_cast<hldb::TaggedPattern>((*rhs->getOperands())[1]);
  ASSERT_NE(tp, nullptr) << "operand[1] must be a TaggedPattern";
  const hldb::RefObj *const tag = tp->getTag<hldb::RefObj>();
  ASSERT_NE(tag, nullptr) << "'d:' tag must be a RefObj (member name)";
  EXPECT_EQ(tag->getName(), std::string_view("d"));
  const hldb::Constant *const val = tp->getPattern<hldb::Constant>();
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getValue(), std::string_view("20"));
}

TEST_F(DefaultTagTest, ThirdOperandIsDefaultTaggedFifteen) {
  const hldb::Operation *const rhs = getXPattern(m_design);
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  ASSERT_EQ(rhs->getOperands()->size(), 3u);

  const hldb::TaggedPattern *const tp = any_cast<hldb::TaggedPattern>((*rhs->getOperands())[2]);
  ASSERT_NE(tp, nullptr) << "operand[2] must be a TaggedPattern";
  const hldb::RefObj *const tag = tp->getTag<hldb::RefObj>();
  ASSERT_NE(tag, nullptr) << "'default:' tag must be a RefObj";
  EXPECT_EQ(tag->getName(), std::string_view("default"));
  const hldb::Constant *const val = tp->getPattern<hldb::Constant>();
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getValue(), std::string_view("15"));
}

// The 'default' tag must be distinguishable from an ordinary member-name
// tag: neither 'a' nor 'c' (the two members with no explicit key) appears
// as a tag name among the operands -- only 'b', 'd', and 'default' do.
TEST_F(DefaultTagTest, NoTagNamedAOrC) {
  const hldb::Operation *const rhs = getXPattern(m_design);
  ASSERT_NE(rhs, nullptr);
  ASSERT_NE(rhs->getOperands(), nullptr);
  for (const hldb::Any *const operand : *rhs->getOperands()) {
    const hldb::TaggedPattern *const tp = any_cast<hldb::TaggedPattern>(operand);
    ASSERT_NE(tp, nullptr);
    const hldb::RefObj *const tag = tp->getTag<hldb::RefObj>();
    ASSERT_NE(tag, nullptr);
    EXPECT_NE(tag->getName(), std::string_view("a")) << "'a' has no explicit key; must not appear as a tag";
    EXPECT_NE(tag->getName(), std::string_view("c")) << "'c' has no explicit key; must not appear as a tag";
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
