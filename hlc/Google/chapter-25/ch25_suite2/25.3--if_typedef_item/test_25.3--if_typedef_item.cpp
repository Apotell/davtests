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

// IEEE 1800-2023 Sec 25.3 with Sec 6.18: an interface may declare a user-defined type and use it
// ("typedef logic [7:0] byte_t; byte_t data;" inside td_if).
// Source: tests/Google/chapter-25/ch25_suite2/25.3--if_typedef_item.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/constant.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/logic_typespec.h>
#include <hldb/range.h>
#include <hldb/ref_typespec.h>
#include <hldb/typedef.h>
#include <hldb/typedef_typespec.h>
#include <hldb/variable.h>

namespace hlc {

class IfTypedefItemTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3--if_typedef_item.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  static const hldb::Interface *getInterface() {
    return hldb::findByDefName<hldb::Interface>("td_if", m_design->getAllInterfaces());
  }

  static const hldb::Typedef *getByteT() {
    const hldb::Interface *const iface = getInterface();
    if (iface == nullptr) return nullptr;
    return hldb::findByName<hldb::Typedef>("byte_t", iface->getTypedefs());
  }
};

TEST_F(IfTypedefItemTest, CompilerReportsZeroErrors) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_fatals, 0);
  EXPECT_EQ(stats.m_syntaxes, 0);
  EXPECT_EQ(stats.m_errors, 0);
}

TEST_F(IfTypedefItemTest, InterfaceDeclaresByteT) {
  // Line 24: "typedef ... byte_t;" belongs to td_if.
  EXPECT_NE(getByteT(), nullptr);
}

TEST_F(IfTypedefItemTest, ByteTAliasesLogic7To0) {
  // Line 24: byte_t is logic [7:0].
  const hldb::Typedef *const td = getByteT();
  ASSERT_NE(td, nullptr);
  ASSERT_NE(td->getAlias(), nullptr);
  const hldb::LogicTypespec *const lts = td->getAlias()->getActual<hldb::LogicTypespec>();
  ASSERT_NE(lts, nullptr);
  ASSERT_NE(lts->getRanges(), nullptr);
  ASSERT_EQ(lts->getRanges()->size(), 1u);
  const hldb::Range *const range = lts->getRanges()->at(0);
  ASSERT_NE(range->getLeftExpr<hldb::Constant>(), nullptr);
  ASSERT_NE(range->getRightExpr<hldb::Constant>(), nullptr);
  EXPECT_EQ(range->getLeftExpr<hldb::Constant>()->getDecompile(), "7");
  EXPECT_EQ(range->getRightExpr<hldb::Constant>()->getDecompile(), "0");
}

TEST_F(IfTypedefItemTest, DataIsTheOnlyVariable) {
  // Line 25: "byte_t data;" is the single data object.
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  ASSERT_NE(iface->getVariables(), nullptr);
  ASSERT_EQ(iface->getVariables()->size(), 1u);
  EXPECT_EQ(iface->getVariables()->at(0)->getName(), "data");
}

TEST_F(IfTypedefItemTest, DataIsTypedByByteT) {
  // Line 25: the type of data is byte_t.
  const hldb::Interface *const iface = getInterface();
  ASSERT_NE(iface, nullptr);
  const hldb::Variable *const data = hldb::findByName<hldb::Variable>("data", iface->getVariables());
  ASSERT_NE(data, nullptr);
  ASSERT_NE(data->getTypespec(), nullptr);
  const hldb::TypedefTypespec *const tts = data->getTypespec()->getActual<hldb::TypedefTypespec>();
  ASSERT_NE(tts, nullptr) << "data is not typed through a typedef";
  EXPECT_EQ(tts->getTypedef(), getByteT());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
