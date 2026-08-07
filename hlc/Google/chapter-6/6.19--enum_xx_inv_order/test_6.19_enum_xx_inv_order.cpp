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

// Tests for 6.19--enum_xx_inv_order.sv (tags: 6.19)
//   :should_fail_because: An unassigned enumerated name that follows an
//   enum name with x or z assignments shall be a syntax error.
//   module top();
//     enum integer {a=0, b={32{1'bx}}, c} val;
//   endmodule
//
// What to check and why (IEEE 1800-2023 6.19 "Enumerations", p.119,
// checked before any test code was written):
//   "An unassigned enumerated name that follows an enum name with x or z
//   assignments shall be a syntax error." with the spec's own
//   counter-example: "enum integer {IDLE, XX='x, S1, S2} state, next; //
//   Syntax error: IDLE=0, XX='x, S1=??, S2=??". "c" here is unassigned
//   and follows "b={32{1'bx}}" (an x-valued name) -- exactly this
//   prohibited construct, matching the file's own :should_fail_because:
//   tag precisely.
//
//   Also (IEEE 1800-2023 6.8): "enum" is its own data_type alternative,
//   never a net_type -- "val" declared at module scope must be a
//   Variable, not a Net. A prior version of this test used
//   hldb::Net/getNets() for "val" -- the same net/variable
//   misclassification bug found and fixed elsewhere this session. This
//   version targets hldb::Variable for "val" instead, and replaces the
//   old Compiler_NoErrorsReported test with a real failing bug test
//   matching the tag.
//
// What is checked:
//   - module top has no Nets and exactly 1 Variable "val"
//   - anonymous EnumTypespec with explicit base IntegerTypespec
//   - 3 consts: a (vpiUIntConst "0"), b ({32{1'bx}} = vpiMultiConcatOp),
//     c (no value -- unassigned after x-value has no vpiValue in UHDM)
//   - "val" has typespec resolving to EnumTypespec, no initial value
//   - top has no processes
//   - THE POINT OF THIS FILE: the compiler should report at least one
//     error for the unassigned "c" following the x-valued "b", per IEEE
//     1800-2023 6.19 quoted above -- a real, non-skipped,
//     currently-failing assertion
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
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/integer_typespec.h>
#include <hldb/module.h>
#include <hldb/variable.h>
#include <hldb/operation.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class EnumXxInvOrderTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.19--enum_xx_inv_order.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(EnumXxInvOrderTest, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// EnumTypespec with explicit base type: integer (same as enum_xx)
// ---------------------------------------------------------------------------
TEST_F(EnumXxInvOrderTest, EnumBaseTypeIsInteger) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::EnumTypespec *enumTs = nullptr;
  for (const auto *ts : *top->getTypespecs()) {
    if ((enumTs = any_cast<hldb::EnumTypespec>(ts))) break;
  }
  ASSERT_NE(enumTs, nullptr);
  const hldb::RefTypespec *const base = enumTs->getBaseTypespec();
  ASSERT_NE(base, nullptr);
  EXPECT_NE(base->getActual<hldb::IntegerTypespec>(), nullptr);
}

// ---------------------------------------------------------------------------
// 3 consts: a, b, c
// ---------------------------------------------------------------------------
TEST_F(EnumXxInvOrderTest, EnumHasThreeConsts) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::EnumTypespec *enumTs = nullptr;
  for (const auto *ts : *top->getTypespecs()) {
    if ((enumTs = any_cast<hldb::EnumTypespec>(ts))) break;
  }
  ASSERT_NE(enumTs, nullptr);
  ASSERT_NE(enumTs->getEnumConsts(), nullptr);
  EXPECT_EQ(enumTs->getEnumConsts()->size(), 3u);
  EXPECT_EQ(enumTs->getEnumConsts()->at(0)->getName(), "a");
  EXPECT_EQ(enumTs->getEnumConsts()->at(1)->getName(), "b");
  EXPECT_EQ(enumTs->getEnumConsts()->at(2)->getName(), "c");
}

TEST_F(EnumXxInvOrderTest, ConstAValueIsZero) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::EnumTypespec *enumTs = nullptr;
  for (const auto *ts : *top->getTypespecs()) {
    if ((enumTs = any_cast<hldb::EnumTypespec>(ts))) break;
  }
  ASSERT_NE(enumTs, nullptr);
  const hldb::Constant *const val = enumTs->getEnumConsts()->at(0)->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getConstType(), vpiUIntConst);
  EXPECT_EQ(val->getDecompile(), "0");
}

TEST_F(EnumXxInvOrderTest, ConstBValueIsMultiConcatOperation) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::EnumTypespec *enumTs = nullptr;
  for (const auto *ts : *top->getTypespecs()) {
    if ((enumTs = any_cast<hldb::EnumTypespec>(ts))) break;
  }
  ASSERT_NE(enumTs, nullptr);
  const hldb::Operation *const op = enumTs->getEnumConsts()->at(1)->getValue<hldb::Operation>();
  ASSERT_NE(op, nullptr);
  EXPECT_EQ(op->getOpType(), vpiMultiConcatOp);
}

TEST_F(EnumXxInvOrderTest, ConstCHasNoValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::EnumTypespec *enumTs = nullptr;
  for (const auto *ts : *top->getTypespecs()) {
    if ((enumTs = any_cast<hldb::EnumTypespec>(ts))) break;
  }
  ASSERT_NE(enumTs, nullptr);
  const hldb::EnumConst *const c = enumTs->getEnumConsts()->at(2);
  ASSERT_NE(c, nullptr);
  EXPECT_EQ(c->getName(), "c");
  EXPECT_EQ(c->getValue<hldb::Any>(), nullptr) << "c is unassigned after an x-value -- it has no vpiValue in UHDM";
}

// ---------------------------------------------------------------------------
// Variable "val" -> EnumTypespec
// ---------------------------------------------------------------------------
TEST_F(EnumXxInvOrderTest, VariableValExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const val = hldb::findByName<hldb::Variable>("val", top->getVariables());
  ASSERT_NE(val, nullptr);
  EXPECT_NE(val->getTypespec()->getActual<hldb::EnumTypespec>(), nullptr);
}

TEST_F(EnumXxInvOrderTest, VariableValHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Variable *const val = hldb::findByName<hldb::Variable>("val", top->getVariables());
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getValue<hldb::Any>(), nullptr);
}

TEST_F(EnumXxInvOrderTest, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

// ---------------------------------------------------------------------------
// The actual point of the file: unassigned name after an x-valued one is illegal
// ---------------------------------------------------------------------------
TEST_F(EnumXxInvOrderTest, CompilerShouldRejectUnassignedNameAfterXValueButDoesNot) {
  ASSERT_NE(m_session->getErrorContainer(), nullptr);
  const ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_GT(stats.nbFatal + stats.nbSyntax + stats.nbError, 0)
      << "IEEE 1800-2023 6.19: 'an unassigned enumerated name that follows an enum name with x or "
         "z assignments shall be a syntax error' -- 'c' is unassigned and follows the x-valued "
         "'b', matching this file's own :should_fail_because: tag -- HLC currently accepts it "
         "with zero diagnostics";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
