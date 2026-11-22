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

// IEEE 1800-2023 Sec 11.5.1: in an indexed part-select "the width_expr shall be a positive
// constant expression". Widths of 0, -2, 4'd0 and 1'bx are illegal; a[i+:4] and a[2-:3] are legal.
// Arguments: illegal_range.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IllegalRangeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "illegal_range.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IllegalRangeTest, ReportsZeroWidth) {
  EXPECT_NE(findEvent(EventType::CompIllegalRange, "a[i+:0]", 23, 14), nullptr) << "width 0 is not positive";
}

TEST_F(IllegalRangeTest, ReportsNegativeWidth) {
  EXPECT_NE(findEvent(EventType::CompIllegalRange, "a[i-:-2]", 24, 14), nullptr) << "width -2 is not positive";
}

TEST_F(IllegalRangeTest, ReportsSizedZeroWidth) {
  EXPECT_NE(findEvent(EventType::CompIllegalRange, "a[i+:4'd0]", 25, 14), nullptr) << "width 4'd0 is not positive";
}

TEST_F(IllegalRangeTest, ReportsUnknownWidth) {
  EXPECT_NE(findEvent(EventType::CompIllegalRange, "a[i+:1'bx]", 26, 14), nullptr) << "width 1'bx is not a positive constant";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
