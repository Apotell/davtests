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

// IEEE 1800-2023 Sec 12.7.3 and 18.5.7.1: the number of foreach loop variables shall not exceed the
// number of dimensions of the array, and no loop variable shall have the same identifier as the
// array. Checked for a procedural foreach and a foreach constraint; A[k], a[x, y] and a[, y2] are
// legal.
// Arguments: illegal_loop_variable.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IllegalLoopVariableTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "illegal_loop_variable.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IllegalLoopVariableTest, ReportsTooManyInConstraint) {
  EXPECT_NE(findEvent(EventType::CompIllegalLoopVariable, "j", 21, 33), nullptr)
      << "A has one dimension, j is a second loop variable";
}

TEST_F(IllegalLoopVariableTest, ReportsArrayNameInConstraint) {
  EXPECT_NE(findEvent(EventType::CompIllegalLoopVariable, "A", 22, 30), nullptr)
      << "the loop variable is named like the array";
}

TEST_F(IllegalLoopVariableTest, ReportsTooManyInStatement) {
  EXPECT_NE(findEvent(EventType::CompIllegalLoopVariable, "k", 29, 22), nullptr)
      << "a has two dimensions, k is a third loop variable";
}

TEST_F(IllegalLoopVariableTest, ReportsArrayNameInStatement) {
  EXPECT_NE(findEvent(EventType::CompIllegalLoopVariable, "b", 30, 16), nullptr)
      << "the loop variable is named like the array";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
