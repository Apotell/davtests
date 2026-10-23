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

// Gate and UDP instance strengths (tests/PrimitiveStrength/dut.sv):
//
//   IEEE 1800-2023 Sec 37.35   primitive -> int vpiStrength0, vpiStrength1
//   IEEE 1800-2023 Sec 28.3.2  gate drive strength: one 0-strength and one 1-strength, either
//                              order; (highz1, strong0) is legal
//   IEEE 1800-2023 Sec 28.10   pullup takes a 1-strength, pulldown a 0-strength
//   IEEE 1800-2023 Sec 29.3    udp_instantiation ::= udp_identifier [drive_strength] ...
//
// NOT CHECKED: strength of a primitive with no strength specification (no rule in 37.35).

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/gate.h>
#include <hldb/module.h>
#include <hldb/primitive.h>
#include <hldb/udp.h>
#include <hldb/vpi_user.h>

#include <gtest/gtest.h>

namespace hlc {

class PrimitiveStrengthTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PrimitiveStrength.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::Primitive *findPrimitive(std::string_view name) const {
    const hldb::Module *const m = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
    if ((m == nullptr) || (m->getPrimitives() == nullptr)) return nullptr;
    return hldb::findByName<hldb::Primitive>(name, m->getPrimitives());
  }
};

TEST_F(PrimitiveStrengthTest, GateStrongZeroWeakOne) {
  const hldb::Primitive *const p = findPrimitive("g_sw");
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getAnyType(), hldb::AnyType::Gate);
  EXPECT_EQ(p->getStrength0(), vpiStrongDrive);
  EXPECT_EQ(p->getStrength1(), vpiWeakDrive);
}

TEST_F(PrimitiveStrengthTest, GateOneStrengthFirst) {
  const hldb::Primitive *const p = findPrimitive("g_pw");
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getStrength0(), vpiPullDrive);
  EXPECT_EQ(p->getStrength1(), vpiWeakDrive);
}

TEST_F(PrimitiveStrengthTest, GateHighzOne) {
  const hldb::Primitive *const p = findPrimitive("g_sz");
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getStrength0(), vpiStrongDrive);
  EXPECT_EQ(p->getStrength1(), vpiHighZ);
}

TEST_F(PrimitiveStrengthTest, PullupOneStrength) {
  const hldb::Primitive *const p = findPrimitive("p_up");
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getPrimType(), vpiPullupPrim);
  EXPECT_EQ(p->getStrength1(), vpiPullDrive);
}

TEST_F(PrimitiveStrengthTest, PulldownZeroStrength) {
  const hldb::Primitive *const p = findPrimitive("p_dn");
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getPrimType(), vpiPulldownPrim);
  EXPECT_EQ(p->getStrength0(), vpiStrongDrive);
}

TEST_F(PrimitiveStrengthTest, UdpDriveStrength) {
  const hldb::Primitive *const p = findPrimitive("u_sp");
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getAnyType(), hldb::AnyType::Udp);
  EXPECT_EQ(p->getStrength0(), vpiSupplyDrive);
  EXPECT_EQ(p->getStrength1(), vpiPullDrive);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
