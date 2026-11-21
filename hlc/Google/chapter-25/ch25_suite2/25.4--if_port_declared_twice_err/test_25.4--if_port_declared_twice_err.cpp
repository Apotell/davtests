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

// IEEE 1800-2023 Sec 25.4 with Sec 23.2.2.1: each port of a non-ANSI header gets exactly one port
// declaration. In "interface dup_if(q); input q; input q;" the second "input q;" (line 26) is the
// single violation; the first (line 25) is legal.
// Source: tests/Google/chapter-25/ch25_suite2/25.4--if_port_declared_twice_err.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IfPortDeclaredTwiceErrTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.4--if_port_declared_twice_err.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IfPortDeclaredTwiceErrTest, ReportsSecondDeclaration) {
  // Line 26: the second "input q;".
  EXPECT_NE(findEvent(EventType::CompMultiplyDeclaredPort, "q", 26), nullptr) << "second input q";
}

TEST_F(IfPortDeclaredTwiceErrTest, FirstDeclarationIsNotReported) {
  // Line 25: the first "input q;" is the legal declaration.
  EXPECT_EQ(findEvent(EventType::CompMultiplyDeclaredPort, "q", 25), nullptr);
}

TEST_F(IfPortDeclaredTwiceErrTest, ReportsExactlyOneError) {
  // Only line 26 is wrong.
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 1);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
