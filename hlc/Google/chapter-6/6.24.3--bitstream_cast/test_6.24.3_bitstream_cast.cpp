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

// Tests for 6.24.3--bitstream_cast.sv (tags: 6.24.3)
//   module top();
//     struct packed {logic [7:0] a; logic [7:0] b; logic [15:0] c;} s;
//     integer a = integer'(s);
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.24.3 "Bit-stream casting",
// p.144, and 6.8 "Variable declarations", p.105, checked before any
// test code was written):
//   6.8's data_type grammar lists both "struct_union [packed [signing]]
//   {struct_union_member ...}" and "integer_atom_type" ("integer" among
//   them) as variable-declaring alternatives -- neither ever appears in
//   IEEE 1800-2023 6.7's net_type list. "struct packed {...} s;" and
//   "integer a = integer'(s);" declared directly in a module body must
//   therefore both be Variables, not Nets, regardless of module-level
//   scope. This file has no :should_fail_because: tag -- it is legal
//   per spec (bit-stream casting a packed struct to an integral type is
//   exactly what 6.24.3 describes).
//
//   A prior version of this test used hldb::Net/getNets() for both "s"
//   and "a" -- the same net/variable misclassification bug found and
//   fixed across 6.5, 6.9.1, 6.12, 6.13, 6.14, 6.16, 6.17, 6.18, 6.19,
//   6.23, and 6.24.1/6.24.2 this session. This version targets
//   hldb::Variable for both "s" and "a" instead.
//
// What is checked:
//   - module top has no Nets and exactly 2 Variables: "s" (packed
//     StructTypespec) and "a" (IntegerTypespec)
//   - struct "s" is packed, has 3 members (a, b, c), all with
//     LogicTypespec; member bit widths: a, b -> [7:0]; c -> [15:0]
//   - "s" has no initial value
//   - "a" vpiValue = Operation(vpiCastOp); cast typespec -> IntegerTypespec
//   - cast operand = RefObj "s" -> Variable
//   - top has no continuous assignments, no processes
//   - compiler reports zero errors (this file is fully legal per 6.24.3)
//
// What is NOT checked and why:
//   - none: every corner above is fully structural and checkable without
//     simulation.

#include <hlc/Common/Session.h>
#include <hlc/ErrorReporting/ErrorContainer.h>
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

class BitstreamCastTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.24.3--bitstream_cast.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Module *getTop() { return hldb::findByName<hldb::Module>("top", m_design->getAllModules()); }
};

TEST_F(BitstreamCastTest, ModuleExists) { EXPECT_NE(getTop(), nullptr); }

// ---------------------------------------------------------------------------
// No processes -- both declarations are module-level Variables
// ---------------------------------------------------------------------------
TEST_F(BitstreamCastTest, NoProcesses) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

// ---------------------------------------------------------------------------
// Module has no Nets and exactly 2 Variables: "s" (struct), "a" (integer)
// ---------------------------------------------------------------------------
TEST_F(BitstreamCastTest, ModuleHasNoNetsAndTwoVariables) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getNets() == nullptr || top->getNets()->empty())
      << "neither 'struct packed {...} s' nor 'integer a' declares a net-type keyword "
         "(IEEE 1800-2023 6.7) anywhere in this file";
  ASSERT_NE(top->getVariables(), nullptr)
      << "'s' and 'a' should be Variables; if this is null, hldb likely misclassified them as Nets";
  EXPECT_EQ(top->getVariables()->size(), 2u) << "expected variables 's' (struct) and 'a' (integer)";
}

// ---------------------------------------------------------------------------
// Variable "s" -> StructTypespec (packed, 3 members: a, b, c)
// ---------------------------------------------------------------------------
TEST_F(BitstreamCastTest, SIsPackedStruct) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const s = hldb::findByName<hldb::Variable>("s", top->getVariables());
  ASSERT_NE(s, nullptr);
  const hldb::StructTypespec *const structTs = s->getTypespec()->getActual<hldb::StructTypespec>();
  ASSERT_NE(structTs, nullptr);
  EXPECT_TRUE(structTs->getPacked());
}

TEST_F(BitstreamCastTest, StructHasThreeMembers) {
  const hldb::Module *const top = getTop();
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

TEST_F(BitstreamCastTest, StructMembersHaveLogicTypespec) {
  const hldb::Module *const top = getTop();
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

// ---------------------------------------------------------------------------
// Variable "a" -> IntegerTypespec (integer keyword -- distinct from IntTypespec/int)
// ---------------------------------------------------------------------------
TEST_F(BitstreamCastTest, AIsIntegerType) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  EXPECT_NE(a->getTypespec()->getActual<hldb::IntegerTypespec>(), nullptr)
      << "integer keyword maps to IntegerTypespec (not IntTypespec which is for int)";
}

// ---------------------------------------------------------------------------
// Variable "a" vpiValue = Operation(vpiCastOp=67) -- integer'(s)
// ---------------------------------------------------------------------------
TEST_F(BitstreamCastTest, AValueIsCastOperation) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Operation *const castOp = a->getValue<hldb::Operation>();
  ASSERT_NE(castOp, nullptr);
  EXPECT_EQ(castOp->getOpType(), vpiCastOp);
}

TEST_F(BitstreamCastTest, CastTypespecIsIntegerTypespec) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const a = hldb::findByName<hldb::Variable>("a", top->getVariables());
  ASSERT_NE(a, nullptr);
  const hldb::Operation *const castOp = a->getValue<hldb::Operation>();
  ASSERT_NE(castOp, nullptr);
  const hldb::RefTypespec *const rts = castOp->getTypespec();
  ASSERT_NE(rts, nullptr);
  EXPECT_NE(rts->getActual<hldb::IntegerTypespec>(), nullptr) << "integer'(...) cast target type is IntegerTypespec";
}

TEST_F(BitstreamCastTest, CastOperandIsRefToVariableS) {
  const hldb::Module *const top = getTop();
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

// ---------------------------------------------------------------------------
// Structural completeness
// ---------------------------------------------------------------------------
TEST_F(BitstreamCastTest, SHasNoInitialValue) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const s = hldb::findByName<hldb::Variable>("s", top->getVariables());
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->getValue<hldb::Any>(), nullptr) << "struct 's' is declared without an initializer";
}

TEST_F(BitstreamCastTest, NoContAssigns) {
  const hldb::Module *const top = getTop();
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getContAssigns() == nullptr || top->getContAssigns()->empty())
      << "integer a = integer'(s) stores the cast as vpiValue, not a ContAssign";
}

// ---------------------------------------------------------------------------
// Struct member bit widths: a, b -> [7:0], c -> [15:0]
// ---------------------------------------------------------------------------
TEST_F(BitstreamCastTest, MembersAAndBAreEightBitsWide) {
  const hldb::Module *const top = getTop();
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

TEST_F(BitstreamCastTest, MemberCIsSixteenBitsWide) {
  const hldb::Module *const top = getTop();
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

TEST_F(BitstreamCastTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbFatal, 0);
  EXPECT_EQ(stats.nbSyntax, 0);
  EXPECT_EQ(stats.nbError, 0);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
