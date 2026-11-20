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

// IEEE 1800-2023 Sec 7.5.1: new[] "sets the size of a dynamic array". Applying it to a fixed-size
// array, a queue, an associative array or a scalar is illegal; dyn = new[4] and dyn = new[8](dyn)
// are legal.
// Arguments: illegal_new_target.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IllegalNewTargetTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "illegal_new_target.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IllegalNewTargetTest, ReportsFixedSizeArray) {
  EXPECT_NE(findEvent(EventType::CompIllegalNewTarget, "fixed", 28, 5), nullptr)
      << "fixed is a fixed-size array";
}

TEST_F(IllegalNewTargetTest, ReportsQueue) {
  EXPECT_NE(findEvent(EventType::CompIllegalNewTarget, "q", 29, 5), nullptr) << "q is a queue";
}

TEST_F(IllegalNewTargetTest, ReportsAssociativeArray) {
  EXPECT_NE(findEvent(EventType::CompIllegalNewTarget, "assoc", 30, 5), nullptr)
      << "assoc is an associative array";
}

TEST_F(IllegalNewTargetTest, ReportsScalar) {
  EXPECT_NE(findEvent(EventType::CompIllegalNewTarget, "scalar", 31, 5), nullptr) << "scalar is an int";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
