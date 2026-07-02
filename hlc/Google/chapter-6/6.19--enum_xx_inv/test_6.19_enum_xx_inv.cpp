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

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/bit_typespec.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/enum_const.h>
#include <hldb/enum_typespec.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/ref_typespec.h>
#include <hldb/vpi_user.h>

namespace hlc {

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
  ASSERT_NE(hldb::findByName<hldb::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// EnumTypespec with explicit base type: bit [1:0] (2-state)
// ---------------------------------------------------------------------------
TEST_F(EnumXxInv, EnumBaseTypeIsBit) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::EnumTypespec *enumTs = nullptr;
  for (const auto *ts : *top->getTypespecs()) {
    if ((enumTs = any_cast<hldb::EnumTypespec>(ts))) break;
  }
  ASSERT_NE(enumTs, nullptr);
  const hldb::RefTypespec *const base = enumTs->getBaseTypespec();
  ASSERT_NE(base, nullptr);
  EXPECT_NE(base->getActual<hldb::BitTypespec>(), nullptr)
      << "enum bit[1:0] base type should resolve to BitTypespec (2-state)";
}

// ---------------------------------------------------------------------------
// 3 consts: a, b, c
// ---------------------------------------------------------------------------
TEST_F(EnumXxInv, EnumHasThreeConsts) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
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

TEST_F(EnumXxInv, ConstAValueIsZero) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
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

TEST_F(EnumXxInv, ConstBValueIsBinaryXx) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::EnumTypespec *enumTs = nullptr;
  for (const auto *ts : *top->getTypespecs()) {
    if ((enumTs = any_cast<hldb::EnumTypespec>(ts))) break;
  }
  ASSERT_NE(enumTs, nullptr);
  // b = 2'bxx is stored as a binary Constant (not an Operation like {32{1'bx}})
  const hldb::Constant *const val = enumTs->getEnumConsts()->at(1)->getValue<hldb::Constant>();
  ASSERT_NE(val, nullptr) << "b = 2'bxx should be stored as a Constant, not an Operation";
  EXPECT_EQ(val->getConstType(), vpiBinaryConst)
      << "vpiBinaryConst=3: 2'bxx is a binary-format constant";
  EXPECT_EQ(val->getDecompile(), "2'bxx");
}

TEST_F(EnumXxInv, ConstCValueIsOne) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
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
TEST_F(EnumXxInv, NetValExists) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const val = hldb::findByName<hldb::Net>("val", top->getNets());
  ASSERT_NE(val, nullptr);
  EXPECT_NE(val->getTypespec()->getActual<hldb::EnumTypespec>(), nullptr);
}

TEST_F(EnumXxInv, NetValHasNoInitialValue) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const hldb::Net *const val = hldb::findByName<hldb::Net>("val", top->getNets());
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getValue<hldb::Any>(), nullptr);
}

TEST_F(EnumXxInv, NoProcesses) {
  const hldb::Module *const top =
      hldb::findByName<hldb::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_TRUE(top->getProcesses() == nullptr || top->getProcesses()->empty());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
