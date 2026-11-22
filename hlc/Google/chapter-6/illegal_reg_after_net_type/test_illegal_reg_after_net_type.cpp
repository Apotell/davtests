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

// IEEE 1800-2023 Sec 6.7.1 and 6.11.2: reg declares a variable, so it cannot follow a net type
// keyword. wire logic l, wire [3:0] w and reg q are legal.
// Arguments: illegal_reg_after_net_type.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IllegalRegAfterNetTypeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "illegal_reg_after_net_type.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IllegalRegAfterNetTypeTest, ReportsWireReg) {
  EXPECT_NE(findEvent(EventType::CompIllegalRegAfterNetType, "r", 20, 8), nullptr) << "wire reg r";
}

TEST_F(IllegalRegAfterNetTypeTest, ReportsTriRegVector) {
  EXPECT_NE(findEvent(EventType::CompIllegalRegAfterNetType, "t", 21, 7), nullptr) << "tri reg [3:0] t";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
