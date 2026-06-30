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

// Validates the UHDM graph for a module with an invalid enum value size
// (should_fail_because: sized literal size differs from enum base type size):
//   module top();
//     enum logic [2:0] {
//       Global = 4'h2,
//       Local  = 4'h3
//     } myenum;
//   endmodule
//
// Checked:
//   - design has module work@top
//   - anonymous EnumTypespec with explicit base LogicTypespec (logic [2:0])
//   - EnumTypespec has 2 consts: Global (4'h2, vpiHexConst) and Local (4'h3, vpiHexConst)
//   - net "myenum" exists with typespec → EnumTypespec
//   - net "myenum" has no initial value
//   - work@top has no processes
//
// Not checked:
//   - Surelog doesn't flag the size mismatch (4-bit literal assigned to 3-bit base type)

#include <Surelog/Common/Session.h>
#include <Surelog/SourceCompile/Compiler.h>
#include <Surelog/Tests/Test.h>

#include <uhdm/Utils.h>
#include <uhdm/constant.h>
#include <uhdm/design.h>
#include <uhdm/enum_const.h>
#include <uhdm/enum_typespec.h>
#include <uhdm/logic_typespec.h>
#include <uhdm/module.h>
#include <uhdm/net.h>
#include <uhdm/ref_typespec.h>
#include <uhdm/vpi_user.h>

namespace SURELOG {

class EnumValueInv : public Test {
 public:
  static void SetUpTestSuite() {
    Compile(__FILE__, {"-f", "6.19--enum_value_inv.hlc"});

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

TEST_F(EnumValueInv, ModuleExists) {
  ASSERT_NE(uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules()), nullptr);
}

// ---------------------------------------------------------------------------
// EnumTypespec with explicit base type: logic [2:0]
// ---------------------------------------------------------------------------
TEST_F(EnumValueInv, EnumTypespecExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::EnumTypespec *enumTs = nullptr;
  for (const auto *ts : *top->getTypespecs()) {
    if ((enumTs = any_cast<uhdm::EnumTypespec>(ts))) break;
  }
  ASSERT_NE(enumTs, nullptr);
}

TEST_F(EnumValueInv, EnumBaseTypeIsLogic) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::EnumTypespec *enumTs = nullptr;
  for (const auto *ts : *top->getTypespecs()) {
    if ((enumTs = any_cast<uhdm::EnumTypespec>(ts))) break;
  }
  ASSERT_NE(enumTs, nullptr);
  const uhdm::RefTypespec *const base = enumTs->getBaseTypespec();
  ASSERT_NE(base, nullptr) << "enum logic[2:0] should have an explicit base typespec";
  EXPECT_NE(base->getActual<uhdm::LogicTypespec>(), nullptr);
}

// ---------------------------------------------------------------------------
// 2 consts: Global (4'h2) and Local (4'h3) — hexadecimal constants
// ---------------------------------------------------------------------------
TEST_F(EnumValueInv, EnumHasTwoConsts) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::EnumTypespec *enumTs = nullptr;
  for (const auto *ts : *top->getTypespecs()) {
    if ((enumTs = any_cast<uhdm::EnumTypespec>(ts))) break;
  }
  ASSERT_NE(enumTs, nullptr);
  ASSERT_NE(enumTs->getEnumConsts(), nullptr);
  EXPECT_EQ(enumTs->getEnumConsts()->size(), 2u);
  EXPECT_EQ(enumTs->getEnumConsts()->at(0)->getName(), "Global");
  EXPECT_EQ(enumTs->getEnumConsts()->at(1)->getName(), "Local");
}

TEST_F(EnumValueInv, GlobalValueIsHex4h2) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::EnumTypespec *enumTs = nullptr;
  for (const auto *ts : *top->getTypespecs()) {
    if ((enumTs = any_cast<uhdm::EnumTypespec>(ts))) break;
  }
  ASSERT_NE(enumTs, nullptr);
  const uhdm::EnumConst *const global = enumTs->getEnumConsts()->at(0);
  ASSERT_NE(global, nullptr);
  EXPECT_EQ(global->getName(), "Global");
  const uhdm::Constant *const val = global->getValue<uhdm::Constant>();
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getConstType(), vpiHexConst);
  EXPECT_EQ(val->getDecompile(), "4'h2");
}

TEST_F(EnumValueInv, LocalValueIsHex4h3) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::EnumTypespec *enumTs = nullptr;
  for (const auto *ts : *top->getTypespecs()) {
    if ((enumTs = any_cast<uhdm::EnumTypespec>(ts))) break;
  }
  ASSERT_NE(enumTs, nullptr);
  const uhdm::EnumConst *const local = enumTs->getEnumConsts()->at(1);
  ASSERT_NE(local, nullptr);
  EXPECT_EQ(local->getName(), "Local");
  const uhdm::Constant *const val = local->getValue<uhdm::Constant>();
  ASSERT_NE(val, nullptr);
  EXPECT_EQ(val->getConstType(), vpiHexConst);
  EXPECT_EQ(val->getDecompile(), "4'h3");
}

// ---------------------------------------------------------------------------
// Net "myenum" → EnumTypespec
// ---------------------------------------------------------------------------
TEST_F(EnumValueInv, NetMyenumExists) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const myenum =
      uhdm::findByName<uhdm::Net>("myenum", top->getNets());
  ASSERT_NE(myenum, nullptr);
  EXPECT_NE(myenum->getTypespec()->getActual<uhdm::EnumTypespec>(), nullptr);
}

TEST_F(EnumValueInv, NetMyenumHasNoInitialValue) {
  const uhdm::Module *const top =
      uhdm::findByName<uhdm::Module>("work@top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  const uhdm::Net *const myenum =
      uhdm::findByName<uhdm::Net>("myenum", top->getNets());
  ASSERT_NE(myenum, nullptr);
  EXPECT_EQ(myenum->getValue<uhdm::Any>(), nullptr);
}

TEST_F(EnumValueInv, NoProcesses) {
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
