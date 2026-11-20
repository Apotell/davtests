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

// IEEE 1800-2023 Sec 6.6.2: "It shall be an error to connect a uwire net to a bidirectional
// terminal of a bidirectional pass switch." The bidirectional terminals of tran, rtran,
// tranif0/1 and rtranif0/1 are the first two (Sec 28.8).
//   line 24: tran t0 (u1, w1)          -- u1 on a bidirectional terminal: error
//   line 26: rtranif0 t2 (w2, ub[1], c) -- a bit of the uwire ub on one: error
// The uwire uc on the control input of tranif1 (line 25) and u1 on buf b0 (line 27) are legal.
// Arguments: uwire_on_pass_switch.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class UwireOnPassSwitchTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "uwire_on_pass_switch.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(UwireOnPassSwitchTest, ReportsUwireOnTranTerminal) {
  EXPECT_NE(findEvent(EventType::CompIllegalUwireOnPassSwitch, "u1", 24, 12), nullptr)
      << "u1 is connected to a bidirectional terminal of tran t0";
}

TEST_F(UwireOnPassSwitchTest, ReportsUwireBitOnRtranifTerminal) {
  EXPECT_NE(findEvent(EventType::CompIllegalUwireOnPassSwitch, "ub", 26, 20), nullptr)
      << "a bit of ub is connected to a bidirectional terminal of rtranif0 t2";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
