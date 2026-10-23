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

// UDP definition properties (tests/UdpDefnProps/dut.sv):
//
//   IEEE 1800-2023 Sec 37.36 detail 2  udp defn -> int vpiPrimType: vpiSeqPrim for sequential
//                                      UDPs, vpiCombPrim for combinational UDPs
//   IEEE 1800-2023 Sec 29.3            a sequential UDP has an output reg and a current-state
//                                      field in its table; it may have an initial statement

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/udp_defn.h>
#include <hldb/vpi_user.h>

#include <gtest/gtest.h>

namespace hlc {

class UdpDefnPropsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "UdpDefnProps.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::UdpDefn *findUdpDefn(std::string_view name) const {
    return hldb::findByName<hldb::UdpDefn>(name, m_design->getAllUdps());
  }
};

TEST_F(UdpDefnPropsTest, CombinationalPrimType) {
  const hldb::UdpDefn *const u = findUdpDefn("comb_u");
  ASSERT_NE(u, nullptr);
  EXPECT_EQ(u->getPrimType(), vpiCombPrim);
}

TEST_F(UdpDefnPropsTest, SequentialPrimType) {
  const hldb::UdpDefn *const u = findUdpDefn("seq_u");
  ASSERT_NE(u, nullptr);
  EXPECT_EQ(u->getPrimType(), vpiSeqPrim);
}

TEST_F(UdpDefnPropsTest, SequentialWithInitialPrimType) {
  const hldb::UdpDefn *const u = findUdpDefn("seq_init");
  ASSERT_NE(u, nullptr);
  EXPECT_EQ(u->getPrimType(), vpiSeqPrim);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
