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

// IEEE 1800-2023 Sec 25.5: "If a port connection specifies a modport list name in both the module
// instance and module header declaration, then the two modport list names shall be identical."
// The header names i2.initiator (line 30) but the connection passes i.target (line 35).
// Source: tests/Google/chapter-25/ch25_suite2/25.5--if_modport_name_mismatch_err.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IfModportNameMismatchErrTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.5--if_modport_name_mismatch_err.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IfModportNameMismatchErrTest, ReportsModportMismatchAtConnection) {
  // Line 35: "m u1(.i(i.target));" -- i.target starts at column 11.
  EXPECT_NE(findEvent(EventType::CompIllegalInterfacePortConnection, "i.target", 35, 11), nullptr);
}

TEST_F(IfModportNameMismatchErrTest, CompilerReportsAnError) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  EXPECT_GE(m_session->getEventListener()->getStats().m_errors, 1);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
