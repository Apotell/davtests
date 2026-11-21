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

// IEEE 1800-2023 Sec 25.3 / A.1.2: an end label on endinterface shall be the interface name.
// "interface good_if; ... endinterface : other_if" is the single violation in the file.
// Source: tests/Google/chapter-25/ch25_suite2/25.3--if_end_label_mismatch.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/interface.h>

#include <gtest/gtest.h>

namespace hlc {

class IfEndLabelMismatchTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3--if_end_label_mismatch.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IfEndLabelMismatchTest, ReportsUnmatchedLabel) {
  EXPECT_NE(findEvent(EventType::CompUnmatchedLabel), nullptr) << "endinterface : other_if";
}

TEST_F(IfEndLabelMismatchTest, ReportsExactlyOneError) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 1);
}

TEST_F(IfEndLabelMismatchTest, EndLabelDeclaresNothing) {
  // Line 26: "other_if" is only a label; it does not declare an interface.
  EXPECT_EQ(hldb::findByDefName<hldb::Interface>("other_if", m_design->getAllInterfaces()), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
