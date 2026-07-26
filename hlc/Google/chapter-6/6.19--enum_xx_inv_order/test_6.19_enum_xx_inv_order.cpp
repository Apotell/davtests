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

// Validates the UHDM graph for an enum where an unassigned name follows an x
// value (should_fail_because: unassigned name after x/z value is illegal):
//   module top();
//     enum integer {a=0, b={32{1'bx}}, c} val;
//   endmodule
//
// Checked:
//   - design has module top
//   - anonymous EnumTypespec with explicit base IntegerTypespec
//   - 3 consts: a (vpiUIntConst "0"), b ({32{1'bx}} = vpiMultiConcatOp), c (no value)
//   - c's getValue<Any>() returns nullptr — unassigned after x-value has no vpiValue in UHDM
//   - net "val" exists with typespec → EnumTypespec
//   - net "val" has no initial value
//   - top has no processes
//   - HLC doesn't flag the invalid ordering (unassigned enumerator after x-valued one)

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
#include <hldb/net.h>
#include <hldb/operation.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

class EnumXxInvOrder : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.19--enum_xx_inv_order.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(EnumXxInvOrder, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// EnumTypespec with explicit base type: integer (same as enum_xx)
// ---------------------------------------------------------------------------
TEST_F(EnumXxInvOrder, EnumBaseTypeIsInteger) {
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
TEST_F(EnumXxInvOrder, EnumHasThreeConsts) {
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

TEST_F(EnumXxInvOrder, ConstAValueIsZero) {
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

TEST_F(EnumXxInvOrder, ConstBValueIsMultiConcatOperation) {
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

TEST_F(EnumXxInvOrder, ConstCHasNoValue) {
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
  EXPECT_EQ(c->getValue<hldb::Any>(), nullptr) << "c is unassigned after an x-value — it has no vpiValue in UHDM";
}

// ---------------------------------------------------------------------------
// Net "val" → EnumTypespec
// ---------------------------------------------------------------------------
TEST_F(EnumXxInvOrder, NetValExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const val = hldb::findByName<hldb::Net>("val", top->getNets());
  ASSERT_NE(val, nullptr);
  EXPECT_NE(val->getTypespec()->getActual<hldb::EnumTypespec>(), nullptr);
}

TEST_F(EnumXxInvOrder, NetValHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const val = hldb::findByName<hldb::Net>("val", top->getNets());
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getValue<hldb::Any>(), nullptr);
}

TEST_F(EnumXxInvOrder, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

// ---------------------------------------------------------------------------
// Compiler diagnostics -- an unassigned enumerator following an x-valued one is not flagged
// ---------------------------------------------------------------------------
TEST_F(EnumXxInvOrder, Compiler_NoErrorsReported) {
  const hlc::ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0)
      << "HLC does not reject an unassigned enumerator following an x-valued one at compile time";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
