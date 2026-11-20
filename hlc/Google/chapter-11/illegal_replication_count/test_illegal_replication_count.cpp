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

// IEEE 1800-2023 Sec 11.4.12.1: the multiplier of a replication is "a non-negative, non-x, and
// non-z constant expression"; {1'bz{1'b0}} and {1'bx{1'b0}} are given as illegal. {4{2'b01}} and
// {P{4'h3}} are legal.
// Arguments: illegal_replication_count.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IllegalReplicationCountTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "illegal_replication_count.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IllegalReplicationCountTest, ReportsZMultiplier) {
  EXPECT_NE(findEvent(EventType::CompIllegalReplicationCount, "1'bz", 22, 15), nullptr)
      << "the multiplier is z";
}

TEST_F(IllegalReplicationCountTest, ReportsXMultiplier) {
  EXPECT_NE(findEvent(EventType::CompIllegalReplicationCount, "1'bx", 23, 15), nullptr)
      << "the multiplier is x";
}

TEST_F(IllegalReplicationCountTest, ReportsNegativeMultiplier) {
  EXPECT_NE(findEvent(EventType::CompIllegalReplicationCount, "-1", 24, 15), nullptr)
      << "the multiplier is negative";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
