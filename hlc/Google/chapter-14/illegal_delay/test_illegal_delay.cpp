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

// IEEE 1800-2023 Sec 14.11: "If no default clocking has been specified for the current module,
// interface, checker, or program, then the compiler shall issue an error" for a cycle delay: ##5 in
// module no_def (line 20); ##2 in with_def (30) is legal. Sec 14.16.1: in a synchronous drive only a
// cycle delay is allowed ("bus.data <= #4 r; // error"): line 32; cb.data <= ##2 r (31) is legal, and
// data <= #4 r (33) is not a synchronous drive.
// Arguments: illegal_delay.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/assignment.h>
#include <hldb/begin.h>
#include <hldb/delay_control.h>
#include <hldb/design.h>
#include <hldb/initial.h>
#include <hldb/module.h>

#include <gtest/gtest.h>

#include <string_view>

namespace hlc {

class IllegalDelayTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "illegal_delay.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IllegalDelayTest, CycleDelayWithoutDefaultClocking) {
  EXPECT_NE(findEvent(EventType::CompIllegalDelay, "no_def", 20, 5), nullptr);
}

TEST_F(IllegalDelayTest, TimeDelayInSynchronousDrive) {
  EXPECT_NE(findEvent(EventType::CompIllegalDelay, "cb.data", 32, 16), nullptr);
}

TEST_F(IllegalDelayTest, CycleAndTimeDelaysAreTold) {
  // In with_def's initial block: ##2; (cycle), cb.data <= ##2 r (cycle), cb.data <= #4 r (time).
  const hldb::Module *const m = hldb::findByDefName<hldb::Module>("with_def", m_design->getAllModules());
  ASSERT_NE(m, nullptr);
  ASSERT_NE(m->getProcesses(), nullptr);
  ASSERT_EQ(m->getProcesses()->size(), 1u);
  const hldb::Initial *const init = any_cast<hldb::Initial>(m->getProcesses()->front());
  ASSERT_NE(init, nullptr);
  const hldb::Begin *const body = init->getStmt<hldb::Begin>();
  ASSERT_NE(body, nullptr);
  ASSERT_NE(body->getStmts(), nullptr);
  ASSERT_EQ(body->getStmts()->size(), 4u);

  const hldb::DelayControl *const wait = any_cast<hldb::DelayControl>(body->getStmts()->at(0));
  ASSERT_NE(wait, nullptr);
  EXPECT_TRUE(wait->getCycleDelay());

  const hldb::Assignment *const cycleDrive = any_cast<hldb::Assignment>(body->getStmts()->at(1));
  ASSERT_NE(cycleDrive, nullptr);
  ASSERT_NE(cycleDrive->getDelayControl(), nullptr);
  EXPECT_TRUE(cycleDrive->getDelayControl()->getCycleDelay());

  const hldb::Assignment *const timeDrive = any_cast<hldb::Assignment>(body->getStmts()->at(2));
  ASSERT_NE(timeDrive, nullptr);
  ASSERT_NE(timeDrive->getDelayControl(), nullptr);
  EXPECT_FALSE(timeDrive->getDelayControl()->getCycleDelay());
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
