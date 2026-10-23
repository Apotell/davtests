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

// Net drive and charge strengths (tests/NetStrength/dut.sv):
//
//   IEEE 1800-2023 Sec 37.16   nets -> int vpiStrength0, vpiStrength1, vpiChargeStrength
//   IEEE 1800-2023 Sec 6.3.2.2 drive strength: one 0-strength and one 1-strength, either order
//   IEEE 1800-2023 Sec 6.3.2.1 charge strength small/medium/large, trireg only;
//                              "The default charge strength of a trireg net shall be medium."
//
// NOT CHECKED: strength of a net with no declared drive strength (no rule in 37.16).

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/net.h>
#include <hldb/vpi_user.h>

#include <gtest/gtest.h>

namespace hlc {

class NetStrengthTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "NetStrength.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::Net *findNet(std::string_view name) const {
    const hldb::Module *const m = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
    if ((m == nullptr) || (m->getNets() == nullptr)) return nullptr;
    return hldb::findByName<hldb::Net>(name, m->getNets());
  }
};

TEST_F(NetStrengthTest, StrongZeroWeakOne) {
  const hldb::Net *const n = findNet("drv_sw");
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(n->getStrength0(), vpiStrongDrive);
  EXPECT_EQ(n->getStrength1(), vpiWeakDrive);
}

TEST_F(NetStrengthTest, PullOneHighzZeroInEitherOrder) {
  const hldb::Net *const n = findNet("drv_ph");
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(n->getStrength0(), vpiHighZ);
  EXPECT_EQ(n->getStrength1(), vpiPullDrive);
}

TEST_F(NetStrengthTest, SupplyStrengths) {
  const hldb::Net *const n = findNet("drv_ss");
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(n->getStrength0(), vpiSupplyDrive);
  EXPECT_EQ(n->getStrength1(), vpiSupplyDrive);
}

TEST_F(NetStrengthTest, PlainWireHasNoChargeStrength) {
  const hldb::Net *const n = findNet("plain");
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(n->getChargeStrength(), 0);
}

TEST_F(NetStrengthTest, ExplicitChargeStrengths) {
  const hldb::Net *const s = findNet("chg_small");
  ASSERT_NE(s, nullptr);
  EXPECT_EQ(s->getChargeStrength(), vpiSmallCharge);
  const hldb::Net *const l = findNet("chg_large");
  ASSERT_NE(l, nullptr);
  EXPECT_EQ(l->getChargeStrength(), vpiLargeCharge);
}

TEST_F(NetStrengthTest, TriregDefaultsToMediumCharge) {
  const hldb::Net *const n = findNet("chg_default");
  ASSERT_NE(n, nullptr);
  EXPECT_EQ(n->getNetType(), vpiTriReg);
  EXPECT_EQ(n->getChargeStrength(), vpiMediumCharge);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
