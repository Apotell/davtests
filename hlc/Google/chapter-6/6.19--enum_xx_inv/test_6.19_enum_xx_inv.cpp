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

// Validates the UHDM graph for an invalid enum with x values on a 2-state type
// (should_fail_because: x/z values are illegal for 2-state enum declarations):
//   module top();
//     enum bit [1:0] {a=0, b=2'bxx, c=1} val;
//   endmodule
//
// Checked:
//   - design has module work@top
//   - anonymous EnumTypespec with explicit base BitTypespec (bit [1:0], 2-state)
//   - 3 consts: a (vpiUIntConst "0"), b (vpiBinaryConst "2'bxx"), c (vpiUIntConst "1")
//     (b is a direct Constant, NOT an Operation like {32{1'bx}} in enum_xx)
//   - net "val" exists with typespec → EnumTypespec
//   - net "val" has no initial value
//   - work@top has no processes
//
// Not checked:
//   - Surelog doesn't flag x values in 2-state enum as a semantic error

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/bit_typespec.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/enum_const.h>
#include <uhdm/enum_typespec.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/vpi_user.h>

namespace SURELOG {

class EnumXxInv : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.19--enum_xx_inv.hlc"});

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

TEST_F(EnumXxInv, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// EnumTypespec with explicit base type: bit [1:0] (2-state)
// ---------------------------------------------------------------------------
TEST_F(EnumXxInv, EnumBaseTypeIsBit) {
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
  EXPECT_NE(base->getActual<uhdm::BitTypespec>(), nullptr)
      << "enum bit[1:0] base type should resolve to BitTypespec (2-state)";
}

// ---------------------------------------------------------------------------
// 3 consts: a, b, c
// ---------------------------------------------------------------------------
TEST_F(EnumXxInv, EnumHasThreeConsts) {
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

TEST_F(EnumXxInv, ConstAValueIsZero) {
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

TEST_F(EnumXxInv, ConstBValueIsBinaryXx) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::EnumTypespec *enumTs = nullptr;
  for (const auto *ts : *top->getTypespecs()) {
    if ((enumTs = any_cast<uhdm::EnumTypespec>(ts))) break;
  }
  ASSERT_NE(enumTs, nullptr);
  // b = 2'bxx is stored as a binary Constant (not an Operation like {32{1'bx}})
  const uhdm::Constant *const val = enumTs->getEnumConsts()->at(1)->getValue<uhdm::Constant>();
  ASSERT_NE(val, nullptr) << "b = 2'bxx should be stored as a Constant, not an Operation";
  EXPECT_EQ(val->getConstType(), vpiBinaryConst)
      << "vpiBinaryConst=3: 2'bxx is a binary-format constant";
  EXPECT_EQ(val->getDecompile(), "2'bxx");
}

TEST_F(EnumXxInv, ConstCValueIsOne) {
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
TEST_F(EnumXxInv, NetValExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const val = uhdm::findByName<uhdm::Net>("val", top->getNets());
  ASSERT_NE(val, nullptr);
  EXPECT_NE(val->getTypespec()->getActual<uhdm::EnumTypespec>(), nullptr);
}

TEST_F(EnumXxInv, NetValHasNoInitialValue) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const val = uhdm::findByName<uhdm::Net>("val", top->getNets());
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getValue<uhdm::Any>(), nullptr);
}

TEST_F(EnumXxInv, NoProcesses) {
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
