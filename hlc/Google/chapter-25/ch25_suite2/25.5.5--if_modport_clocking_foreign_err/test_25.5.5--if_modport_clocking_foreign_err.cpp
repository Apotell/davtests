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

// IEEE 1800-2023 Sec 25.5.5: "All of the clocking blocks used in a modport declaration shall be
// declared by the same interface as the modport itself." Clocking block cb is declared by other_if
// (line 25), but modport STB of a_if lists it (line 30).
// Source: tests/Google/chapter-25/ch25_suite2/25.5.5--if_modport_clocking_foreign_err.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IfModportClockingForeignErrTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.5.5--if_modport_clocking_foreign_err.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IfModportClockingForeignErrTest, ReportsClockingBlockOfOtherInterface) {
  // Line 30: "modport STB (clocking cb);" -- cb at column 25.
  EXPECT_NE(findEvent(EventType::CompUndefinedClockingBlock, "cb", 30, 25), nullptr);
}

TEST_F(IfModportClockingForeignErrTest, CompilerReportsAnError) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  EXPECT_GE(m_session->getEventListener()->getStats().m_errors, 1);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
