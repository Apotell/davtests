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

// IEEE 1800-2023 Sec 25.3 with Sec 23.3.2: hierarchical_instance ::= name_of_instance
// ( [ list_of_port_connections ] ) -- the parentheses are required even with no connections.
// "simple_if u;" (line 29) is the single violation.
// Source: tests/Google/chapter-25/ch25_suite2/25.3--if_without_parens_err.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/interface.h>

#include <gtest/gtest.h>

namespace hlc {

class IfWithoutParensErrTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3--if_without_parens_err.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IfWithoutParensErrTest, ReportsMissingParentheses) {
  // Line 29: "simple_if u;".
  EXPECT_NE(findEvent(EventType::CompInterfaceInstanceWithoutParentheses, "u", 29), nullptr) << "simple_if u;";
}

TEST_F(IfWithoutParensErrTest, ReportsExactlyOneError) {
  // Lines 24-28 are legal; only line 29 is wrong.
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 1);
}

TEST_F(IfWithoutParensErrTest, InterfaceIsDeclared) {
  // Lines 24-26: the interface declaration itself is legal.
  EXPECT_NE(hldb::findByDefName<hldb::Interface>("simple_if", m_design->getAllInterfaces()), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
