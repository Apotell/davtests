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

// IEEE 1800-2023 Sec 25.5: names used in a modport shall be declared by the same interface, and "a
// modport declaration shall not implicitly declare new ports". illegal_i declares nothing, yet its
// modports list a, b, c, d (lines 26-27).
// Source: tests/Google/chapter-25/ch25_suite2/25.5--if_modport_undeclared_name_err.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/SourceCompile/Compiler.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/interface.h>
#include <hldb/net.h>
#include <hldb/variable.h>

#include <gtest/gtest.h>

#include <string_view>

namespace hlc {

class IfModportUndeclaredNameErrTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.5--if_modport_undeclared_name_err.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IfModportUndeclaredNameErrTest, InitiatorNamesAreUndefined) {
  // Line 26: "modport initiator (input a, b, output c, d);"
  EXPECT_NE(findEvent(EventType::CompModportUndefinedPort, "a", 26, 28), nullptr);
  EXPECT_NE(findEvent(EventType::CompModportUndefinedPort, "b", 26, 31), nullptr);
  EXPECT_NE(findEvent(EventType::CompModportUndefinedPort, "c", 26, 41), nullptr);
  EXPECT_NE(findEvent(EventType::CompModportUndefinedPort, "d", 26, 44), nullptr);
}

TEST_F(IfModportUndeclaredNameErrTest, TargetNamesAreUndefined) {
  // Line 27: "modport target (output a, b, input c, d);"
  EXPECT_NE(findEvent(EventType::CompModportUndefinedPort, "a", 27, 26), nullptr);
  EXPECT_NE(findEvent(EventType::CompModportUndefinedPort, "b", 27, 29), nullptr);
  EXPECT_NE(findEvent(EventType::CompModportUndefinedPort, "c", 27, 38), nullptr);
  EXPECT_NE(findEvent(EventType::CompModportUndefinedPort, "d", 27, 41), nullptr);
}

TEST_F(IfModportUndeclaredNameErrTest, ModportDoesNotImplicitlyDeclareItems) {
  // The modports shall not create nets or variables a, b, c, d in illegal_i.
  const hldb::Interface *const iface = hldb::findByDefName<hldb::Interface>("illegal_i", m_design->getAllInterfaces());
  ASSERT_NE(iface, nullptr);
  for (std::string_view name : {"a", "b", "c", "d"}) {
    EXPECT_EQ(hldb::findByName<hldb::Net>(name, iface->getNets()), nullptr) << name;
    EXPECT_EQ(hldb::findByName<hldb::Variable>(name, iface->getVariables()), nullptr) << name;
  }
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
