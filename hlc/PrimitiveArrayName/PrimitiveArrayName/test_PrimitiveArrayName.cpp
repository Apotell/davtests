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

// Name of a switch or UDP instance array (tests/PrimitiveArrayName/dut.sv):
//
//   IEEE 1800-2023 Sec 37.11  instance array -> str vpiName
//   IEEE 1800-2023 Sec 28.3.6 the instance name of an array of instances names the array.

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/primitive_array.h>
#include <hldb/switch_array.h>
#include <hldb/udp_array.h>

#include <gtest/gtest.h>

namespace hlc {

class PrimitiveArrayNameTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "PrimitiveArrayName.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::PrimitiveArray *findArray(std::string_view name) const {
    const hldb::Module *const top = hldb::findByName<hldb::Module>("top", m_design->getAllModules());
    if ((top == nullptr) || (top->getPrimitiveArrays() == nullptr)) return nullptr;
    return hldb::findByName<hldb::PrimitiveArray>(name, top->getPrimitiveArrays());
  }

  void expectSwitchArray(std::string_view name, uint32_t line) const {
    const hldb::PrimitiveArray *const pa = findArray(name);
    ASSERT_NE(pa, nullptr) << name;
    EXPECT_EQ(pa->getAnyType(), hldb::AnyType::SwitchArray);
    EXPECT_EQ(pa->getName(), name);
    EXPECT_EQ(pa->getStartLine(), line);
  }
};

TEST_F(PrimitiveArrayNameTest, MosSwitchArrayIsNamed) { expectSwitchArray("mos_arr", 16); }

TEST_F(PrimitiveArrayNameTest, CmosSwitchArrayIsNamed) { expectSwitchArray("cmos_arr", 17); }

TEST_F(PrimitiveArrayNameTest, PassSwitchArrayIsNamed) { expectSwitchArray("tran_arr", 18); }

TEST_F(PrimitiveArrayNameTest, PassEnableSwitchArrayIsNamed) { expectSwitchArray("trif_arr", 19); }

TEST_F(PrimitiveArrayNameTest, UdpArrayIsNamed) {
  const hldb::PrimitiveArray *const pa = findArray("udp_arr");
  ASSERT_NE(pa, nullptr);
  EXPECT_EQ(pa->getAnyType(), hldb::AnyType::UdpArray);
  EXPECT_EQ(pa->getName(), "udp_arr");
  EXPECT_EQ(pa->getStartLine(), 20u);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
