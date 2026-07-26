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

// Validates the UHDM graph for a module using an enum with x values:
//   module top();
//     enum integer {a=0, b={32{1'bx}}, c=1} val;
//   endmodule
//
// Checked:
//   - design has module top
//   - anonymous EnumTypespec with explicit base IntegerTypespec
//   - 3 consts: a (vpiUIntConst "0"), b ({32{1'bx}} = vpiMultiConcatOp), c (vpiUIntConst "1")
//   - b's MultiConcatOp has operands: Constant "32" and ConcatOp → Constant "1'bx" (vpiBinaryConst)
//   - net "val" exists with typespec → EnumTypespec
//   - net "val" has no initial value
//   - top has no processes
//   - HLC doesn't flag x-value enums in integer-based enums as a semantic error

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

class EnumXx : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "6.19--enum_xx.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(EnumXx, ModuleExists) {
  ASSERT_NE(hldb::findByName<hldb::Module>("top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// EnumTypespec with explicit base type: integer
// ---------------------------------------------------------------------------
TEST_F(EnumXx, EnumBaseTypeIsInteger) {
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
TEST_F(EnumXx, EnumHasThreeConsts) {
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

TEST_F(EnumXx, ConstAValueIsZero) {
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

TEST_F(EnumXx, ConstBValueIsMultiConcatOperation) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::EnumTypespec *enumTs = nullptr;
  for (const auto *ts : *top->getTypespecs()) {
    if ((enumTs = any_cast<hldb::EnumTypespec>(ts))) break;
  }
  ASSERT_NE(enumTs, nullptr);
  const hldb::Operation *const op = enumTs->getEnumConsts()->at(1)->getValue<hldb::Operation>();
  ASSERT_NE(op, nullptr) << "b = {32{1'bx}} should be stored as an Operation";
  EXPECT_EQ(op->getOpType(), vpiMultiConcatOp) << "vpiMultiConcatOp=34: multi-concatenation {N{val}}";
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const hldb::Constant *const count = any_cast<hldb::Constant>(op->getOperands()->at(0));
  ASSERT_NE(count, nullptr);
  EXPECT_EQ(count->getDecompile(), "32");
  const hldb::Operation *const concat = any_cast<hldb::Operation>(op->getOperands()->at(1));
  ASSERT_NE(concat, nullptr);
  EXPECT_EQ(concat->getOpType(), vpiConcatOp);
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 1u);
  const hldb::Constant *const xbit = any_cast<hldb::Constant>(concat->getOperands()->at(0));
  ASSERT_NE(xbit, nullptr);
  EXPECT_EQ(xbit->getConstType(), vpiBinaryConst);
  EXPECT_EQ(xbit->getDecompile(), "1'bx");
}

TEST_F(EnumXx, ConstCValueIsOne) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::EnumTypespec *enumTs = nullptr;
  for (const auto *ts : *top->getTypespecs()) {
    if ((enumTs = any_cast<hldb::EnumTypespec>(ts))) break;
  }
  ASSERT_NE(enumTs, nullptr);
  const hldb::Constant *const val = enumTs->getEnumConsts()->at(2)->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getConstType(), vpiUIntConst);
  EXPECT_EQ(val->getDecompile(), "1");
}

// ---------------------------------------------------------------------------
// Net "val" → EnumTypespec
// ---------------------------------------------------------------------------
TEST_F(EnumXx, NetValExists) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const val = hldb::findByName<hldb::Net>("val", top->getNets());
  ASSERT_NE(val, nullptr);
  EXPECT_NE(val->getTypespec()->getActual<hldb::EnumTypespec>(), nullptr);
}

TEST_F(EnumXx, NetValHasNoInitialValue) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const val = hldb::findByName<hldb::Net>("val", top->getNets());
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getValue<hldb::Any>(), nullptr);
}

TEST_F(EnumXx, NoProcesses) {
  const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

// ---------------------------------------------------------------------------
// Compiler diagnostics -- x-valued enumerators on a 4-state integer base are not flagged
// ---------------------------------------------------------------------------
TEST_F(EnumXx, Compiler_NoErrorsReported) {
  const hlc::ErrorContainer::Stats stats = m_session->getErrorContainer()->getErrorStats();
  EXPECT_EQ(stats.nbError, 0) << "HLC does not reject x-value enumerators on an integer-based enum at compile time";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
