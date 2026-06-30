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
//   - design has module work@top
//   - anonymous EnumTypespec with explicit base IntegerTypespec
//   - 3 consts: a (vpiUIntConst "0"), b ({32{1'bx}} = vpiMultiConcatOp), c (vpiUIntConst "1")
//   - b's MultiConcatOp has operands: Constant "32" and ConcatOp → Constant "1'bx" (vpiBinaryConst)
//   - net "val" exists with typespec → EnumTypespec
//   - net "val" has no initial value
//   - work@top has no processes
//
// Not checked:
//   - Surelog doesn't flag x-value enums in integer-based enums as a semantic error

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/enum_const.h>
#include <uhdm/enum_typespec.h>
#include <uhdm/integer_typespec.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/operation.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/vpi_user.h>

namespace SURELOG {

class EnumXx : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.19--enum_xx.hlc"});

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

TEST_F(EnumXx, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// EnumTypespec with explicit base type: integer
// ---------------------------------------------------------------------------
TEST_F(EnumXx, EnumBaseTypeIsInteger) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::EnumTypespec *enumTs = nullptr;
  for (const auto *ts : *top->getTypespecs()) {
    if ((enumTs = any_cast<uhdm::EnumTypespec>(ts))) break;
  }
  ASSERT_NE(enumTs, nullptr);
  const uhdm::RefTypespec *const base = enumTs->getBaseTypespec();
  ASSERT_NE(base, nullptr);
  EXPECT_NE(base->getActual<uhdm::IntegerTypespec>(), nullptr);
}

// ---------------------------------------------------------------------------
// 3 consts: a, b, c
// ---------------------------------------------------------------------------
TEST_F(EnumXx, EnumHasThreeConsts) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::EnumTypespec *enumTs = nullptr;
  for (const auto *ts : *top->getTypespecs()) {
    if ((enumTs = any_cast<uhdm::EnumTypespec>(ts))) break;
  }
  ASSERT_NE(enumTs, nullptr);
  ASSERT_NE(enumTs->getEnumConsts(), nullptr);
  EXPECT_EQ(enumTs->getEnumConsts()->size(), 3u);
  EXPECT_EQ(enumTs->getEnumConsts()->at(0)->getName(), "a");
  EXPECT_EQ(enumTs->getEnumConsts()->at(1)->getName(), "b");
  EXPECT_EQ(enumTs->getEnumConsts()->at(2)->getName(), "c");
}

TEST_F(EnumXx, ConstAValueIsZero) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::EnumTypespec *enumTs = nullptr;
  for (const auto *ts : *top->getTypespecs()) {
    if ((enumTs = any_cast<uhdm::EnumTypespec>(ts))) break;
  }
  ASSERT_NE(enumTs, nullptr);
  const uhdm::Constant *const val = enumTs->getEnumConsts()->at(0)->getValue<uhdm::Constant>();
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getConstType(), vpiUIntConst);
  EXPECT_EQ(val->getDecompile(), "0");
}

TEST_F(EnumXx, ConstBValueIsMultiConcatOperation) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::EnumTypespec *enumTs = nullptr;
  for (const auto *ts : *top->getTypespecs()) {
    if ((enumTs = any_cast<uhdm::EnumTypespec>(ts))) break;
  }
  ASSERT_NE(enumTs, nullptr);
  const uhdm::Operation *const op =
      enumTs->getEnumConsts()->at(1)->getValue<uhdm::Operation>();
  ASSERT_NE(op, nullptr) << "b = {32{1'bx}} should be stored as an Operation";
  EXPECT_EQ(op->getOpType(), vpiMultiConcatOp)
      << "vpiMultiConcatOp=34: multi-concatenation {N{val}}";
  ASSERT_NE(op->getOperands(), nullptr);
  ASSERT_EQ(op->getOperands()->size(), 2u);
  const uhdm::Constant *const count = any_cast<uhdm::Constant>(op->getOperands()->at(0));
  ASSERT_NE(count, nullptr);
  EXPECT_EQ(count->getDecompile(), "32");
  const uhdm::Operation *const concat = any_cast<uhdm::Operation>(op->getOperands()->at(1));
  ASSERT_NE(concat, nullptr);
  EXPECT_EQ(concat->getOpType(), vpiConcatOp);
  ASSERT_NE(concat->getOperands(), nullptr);
  ASSERT_EQ(concat->getOperands()->size(), 1u);
  const uhdm::Constant *const xbit = any_cast<uhdm::Constant>(concat->getOperands()->at(0));
  ASSERT_NE(xbit, nullptr);
  EXPECT_EQ(xbit->getConstType(), vpiBinaryConst);
  EXPECT_EQ(xbit->getDecompile(), "1'bx");
}

TEST_F(EnumXx, ConstCValueIsOne) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::EnumTypespec *enumTs = nullptr;
  for (const auto *ts : *top->getTypespecs()) {
    if ((enumTs = any_cast<uhdm::EnumTypespec>(ts))) break;
  }
  ASSERT_NE(enumTs, nullptr);
  const uhdm::Constant *const val = enumTs->getEnumConsts()->at(2)->getValue<uhdm::Constant>();
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getConstType(), vpiUIntConst);
  EXPECT_EQ(val->getDecompile(), "1");
}

// ---------------------------------------------------------------------------
// Net "val" → EnumTypespec
// ---------------------------------------------------------------------------
TEST_F(EnumXx, NetValExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const val = uhdm::findByName<uhdm::Net>("val", top->getNets());
  ASSERT_NE(val, nullptr);
  EXPECT_NE(val->getTypespec()->getActual<uhdm::EnumTypespec>(), nullptr);
}

TEST_F(EnumXx, NetValHasNoInitialValue) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const val = uhdm::findByName<uhdm::Net>("val", top->getNets());
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getValue<uhdm::Any>(), nullptr);
}

TEST_F(EnumXx, NoProcesses) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

}  // namespace SURELOG

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
