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

// IEEE 1800-2023 Sec 25.4 with Sec 23.3.2.2: in a named connection the port name shall be a port
// of the instantiated interface. ".zz(c)" on line 29 names no port of clk_if -- the single
// violation; ".clk(c)" on the same line is legal.
// Source: tests/Google/chapter-25/ch25_suite2/25.4--if_unknown_port_err.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IfUnknownPortErrTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.4--if_unknown_port_err.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IfUnknownPortErrTest, ReportsUnknownPortZz) {
  // Line 29: ".zz(c)".
  EXPECT_NE(findEvent(EventType::CompUnknownPort, "zz", 29), nullptr) << ".zz(c)";
}

TEST_F(IfUnknownPortErrTest, KnownPortClkIsNotReported) {
  // Line 29: ".clk(c)" names a real port.
  EXPECT_EQ(findEvent(EventType::CompUnknownPort, "clk"), nullptr);
}

TEST_F(IfUnknownPortErrTest, ReportsExactlyOneError) {
  // Lines 24-28 are legal; only ".zz" is wrong.
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 1);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
