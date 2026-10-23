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

// Defaults recorded on an interface or a program (tests/InterfaceProgramDefaults/dut.sv):
//
//   IEEE 1800-2023 Sec 16.15  default disable iff may appear in a module, interface,
//                             program, checker or generate block.
//   IEEE 1800-2023 Sec 37.6   interface -> vpiDefaultDisableIff (expr / distribution)
//   IEEE 1800-2023 Sec 37.6   interface -> vpiDefaultClocking, vpiGlobalClocking (clocking block)

#include <hlc/Common/Session.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/clocking_block.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/ref_obj.h>
#include <hldb/vpi_user.h>

#include <gtest/gtest.h>

namespace hlc {

class InterfaceProgramDefaultsTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "InterfaceProgramDefaults.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  const hldb::Interface *findInterface(std::string_view name) const {
    return hldb::findByName<hldb::Interface>(name, m_design->getAllInterfaces());
  }
};

// Sec 37.6: "default disable iff (rst);" in an interface is its vpiDefaultDisableIff.
TEST_F(InterfaceProgramDefaultsTest, InterfaceDefaultDisableIff) {
  const hldb::Interface *const i = findInterface("ifc");
  ASSERT_NE(i, nullptr);
  ASSERT_NE(i->getDefaultDisableIff(), nullptr);
  const hldb::RefObj *const ro = any_cast<hldb::RefObj>(i->getDefaultDisableIff());
  ASSERT_NE(ro, nullptr);
  EXPECT_EQ(ro->getName(), "rst");
  EXPECT_EQ(ro->getParent(), i);
}

// Sec 37.6: "default clocking cb ..." in an interface is its vpiDefaultClocking.
TEST_F(InterfaceProgramDefaultsTest, InterfaceDefaultClocking) {
  const hldb::Interface *const i = findInterface("ifc");
  ASSERT_NE(i, nullptr);
  const hldb::ClockingBlock *const cb = i->getDefaultClocking();
  ASSERT_NE(cb, nullptr);
  EXPECT_EQ(cb->getName(), "cb");
}

// Sec 37.6: "global clocking gcb ..." in an interface is its vpiGlobalClocking.
TEST_F(InterfaceProgramDefaultsTest, InterfaceGlobalClocking) {
  const hldb::Interface *const i = findInterface("ifc");
  ASSERT_NE(i, nullptr);
  const hldb::ClockingBlock *const cb = i->getGlobalClocking();
  ASSERT_NE(cb, nullptr);
  EXPECT_EQ(cb->getName(), "gcb");
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
