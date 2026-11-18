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

// IEEE 1800-2023 Sec 13.5.2: "Nets and selects into nets shall not be passed by reference." A
// variable, a select into a variable and an element of an unpacked array are legal.
// Arguments: illegal_ref_argument.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IllegalRefArgumentTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "illegal_ref_argument.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IllegalRefArgumentTest, ReportsNet) {
  EXPECT_NE(findEvent(EventType::CompIllegalRefArgument, "n", 30, 7), nullptr) << "n is a net";
}

TEST_F(IllegalRefArgumentTest, ReportsSelectIntoNet) {
  EXPECT_NE(findEvent(EventType::CompIllegalRefArgument, "n[0]", 31, 8), nullptr) << "n[0] selects into a net";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
