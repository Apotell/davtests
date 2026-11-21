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

// IEEE 1800-2023 Sec 25.7.2: a module connected through a modport "only has access to the target
// tasks" that modport imports. memMod (simple_bus.target a) calls initiatorRead, which modport
// target does not import (line 34).
// Source: tests/Google/chapter-25/ch25_suite2/25.7.2--if_modport_task_not_imported_err.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IfModportTaskNotImportedErrTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.7.2--if_modport_task_not_imported_err.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IfModportTaskNotImportedErrTest, ReportsTaskNotListedInModport) {
  // Line 34: "a.initiatorRead;" starts at column 5.
  EXPECT_NE(findEvent(EventType::CompModportMemberNotListed, "initiatorRead", 34, 5), nullptr);
}

TEST_F(IfModportTaskNotImportedErrTest, CompilerReportsAnError) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  EXPECT_GE(m_session->getEventListener()->getStats().m_errors, 1);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
