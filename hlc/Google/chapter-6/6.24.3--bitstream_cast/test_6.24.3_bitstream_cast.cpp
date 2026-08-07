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

// Validates the UHDM graph for a module using a bitstream cast:
//   module top();
//     struct packed {logic [7:0] a; logic [7:0] b; logic [15:0] c;} s;
//     integer a = integer'(s);
//   endmodule
//
// ss.6.7 + ss.6.8: neither 's' (a packed struct type) nor 'a' (the 'integer'
// keyword) has an explicit net-type keyword (wire/tri/etc.), so per the
// standard both are variable_declarations, not net_declarations. They must
// be modeled as Variables, found via Module::getVariables(), not as Nets.
//
// Checked:
//   - design has module top
//   - module has exactly 2 variables: 's' (packed StructTypespec) and 'a' (IntegerTypespec)
//   - struct 's' is packed, has 3 members (a, b, c), all with LogicTypespec
//   - 's' has no initial value
//   - 'a' vpiValue = vpiCastOp Operation; cast typespec -> IntegerTypespec
//   - cast operand = RefObj "s" -> Variable 's'
//   - top has no continuous assignments
//   - top has no processes
//   - struct member bit widths: a, b -> [7:0]; c -> [15:0]

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/integer_typespec.h>
#include <hldb/logic_typespec.h>
#include <hldb/module.h>
#include <hldb/operation.h>
#include <hldb/range.h>
#include <hldb/ref_obj.h>
#include <hldb/ref_typespec.h>
#include <hldb/struct_typespec.h>
#include <hldb/typespec_member.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

namespace hlc {

class BitstreamCast : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.24.3--bitstream_cast.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(BitstreamCast, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ----
// No processes -- both declarations are module-level Variables
// ----
TEST_F(BitstreamCast, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

// ----
// Variable "s" -> StructTypespec (packed, 3 members: a, b, c)
// ----
TEST_F(BitstreamCast, VariableSIsPackedStruct) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const s = hldb::findByName<hldb::Variable>("s", top->getVariables());
  ASSERT_NE(s, nullptr);
  const hldb::StructTypespec *const structTs = s->getTypespec()->getActual<hldb::StructTypespec>();
  ASSERT_NE(structTs, nullptr);
  EXPECT_TRUE(structTs->getPacked());
}

TEST_F(BitstreamCast, StructHasThreeMembers) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const s = hldb::findByName<hldb::Variable>("s", top->getVariables());
  ASSERT_NE(s, nullptr);
  const hldb::StructTypespec *const structTs = s->getTypespec()->getActual<hldb::StructTypespec>();
  ASSERT_NE(structTs, nullptr);
  ASSERT_NE(structTs->getMembers(), nullptr);
  EXPECT_EQ(structTs->getMembers()->size(), 3u);
  EXPECT_EQ(structTs->getMembers()->at(0)->getName(), "a");
  EXPECT_EQ(structTs->getMembers()->at(1)->getName(), "b");
  EXPECT_EQ(structTs->getMembers()->at(2)->getName(), "c");
}

TEST_F(BitstreamCast, StructMembersHaveLogicTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const s = hldb::findByName<hldb::Variable>("s", top->getVariables());
  ASSERT_NE(s, nullptr);
  const hldb::StructTypespec *const structTs = s->getTypespec()->getActual<hldb::StructTypespec>();
  ASSERT_NE(structTs, nullptr);
  for (const auto *member : *structTs->getMembers()) {
    EXPECT_NE(member->getTypespec()->getActual<hldb::LogicTypespec>(), nullptr)
        << "struct member " << member->getName() << " should be LogicTypespec";
  }
}

// ----
// Variable "a" -> IntegerTypespec (integer keyword -- distinct from IntTypespec/int)
// ----
TEST_F(BitstreamCast, VariableAIsIntegerType) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec()->getActual<hldb::IntegerTypespec>(), nullptr)
      << "integer keyword maps to IntegerTypespec (not IntTypespec which is for int)";
}

// ----
// Variable "a" vpiValue = Operation(vpiCastOp=67) -- integer'(s)
// ----
TEST_F(BitstreamCast, VariableAValueIsCastOperation) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Operation *const castOp = a->getValue<hldb::Operation>();
  ASSERT_NE(castOp, nullptr);
  EXPECT_EQ(castOp->getOpType(), vpiCastOp);
}

TEST_F(BitstreamCast, CastTypespecIsIntegerTypespec) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Operation *const castOp = a->getValue<hldb::Operation>();
  ASSERT_NE(castOp, nullptr);
  const hldb::RefTypespec *const rts = castOp->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::IntegerTypespec>(), nullptr) << "integer'(...) cast target type is IntegerTypespec";
}

TEST_F(BitstreamCast, CastOperandIsRefToVariableS) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Operation *const castOp = a->getValue<hldb::Operation>();
  ASSERT_NE(castOp, nullptr);
  ASSERT_NE(castOp->getOperands(), nullptr);
  ASSERT_EQ(castOp->getOperands()->size(), 1u);
  const hldb::RefObj *const s = any_cast<hldb::RefObj>(castOp->getOperands()->at(0));
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->getName(), "s");
  EXPECT_NE(s->getActual<hldb::Variable>(), nullptr);
}

// ----
// Structural completeness
// ----
TEST_F(BitstreamCast, TwoVariablesExist) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  ASSERT_NE(top->getVariables(), nullptr);
  EXPECT_EQ(top->getVariables()->size(), 2u) << "expected variables 's' (struct) and 'a' (integer)";
}

TEST_F(BitstreamCast, VariableSHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const s = hldb::findByName<hldb::Variable>("s", top->getVariables());
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->getValue<hldb::Any>(), nullptr) << "struct 's' is declared without an initializer";
}

TEST_F(BitstreamCast, NoContAssigns) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty())
      << "integer a = integer'(s) stores the cast as vpiValue, not a ContAssign";
}

// ----
// Struct member bit widths: a, b -> [7:0], c -> [15:0]
// ----
TEST_F(BitstreamCast, MembersAAndBAreEightBitsWide) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const s = hldb::findByName<hldb::Variable>("s", top->getVariables());
  ASSERT_NE(s, nullptr);
  const hldb::StructTypespec *const structTs = s->getTypespec()->getActual<hldb::StructTypespec>();
  ASSERT_NE(structTs, nullptr);
  for (uint32_t i = 0; i < 2u; ++i) {
    const hldb::LogicTypespec *const lts = structTs->getMembers()->at(i)->getTypespec()->getActual<hldb::LogicTypespec>();
    ASSERT_NE(lts, nullptr);
    ASSERT_NE(lts->getRanges(), nullptr);
    ASSERT_EQ(lts->getRanges()->size(), 1u);
    const hldb::Range *const range = lts->getRanges()->at(0);
    ASSERT_NE(range, nullptr);
    const hldb::Constant *const left = range->getLeftExpr<hldb::Constant>();
    ASSERT_NE(left, nullptr);
    EXPECT_EQ(left->getDecompile(), "7");
    const hldb::Constant *const right = range->getRightExpr<hldb::Constant>();
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(right->getDecompile(), "0");
  }
}

TEST_F(BitstreamCast, MemberCIsSixteenBitsWide) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const s = hldb::findByName<hldb::Variable>("s", top->getVariables());
  ASSERT_NE(s, nullptr);
  const hldb::StructTypespec *const structTs = s->getTypespec()->getActual<hldb::StructTypespec>();
  ASSERT_NE(structTs, nullptr);
  const hldb::LogicTypespec *const lts = structTs->getMembers()->at(2)->getTypespec()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lts, nullptr);
  ASSERT_NE(lts->getRanges(), nullptr);
  ASSERT_EQ(lts->getRanges()->size(), 1u);
  const hldb::Range *const range = lts->getRanges()->at(0);
  ASSERT_NE(range, nullptr);
  const hldb::Constant *const left = range->getLeftExpr<hldb::Constant>();
  ASSERT_NE(left, nullptr);
  EXPECT_EQ(left->getDecompile(), "15");
  const hldb::Constant *const right = range->getRightExpr<hldb::Constant>();
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->getDecompile(), "0");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
