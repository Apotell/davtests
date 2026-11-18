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

// IEEE 1800-2023 Sec 29.3.4: a row whose inputs are all x shall specify an x output; the same
// combination of inputs shall not specify different outputs; and each row has one field per input.
// The other rows of p1 and the rows of the sequential p2 are legal.
// Arguments: illegal_udp_table.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IllegalUdpTableTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "illegal_udp_table.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IllegalUdpTableTest, ReportsAllXInputsWithoutXOutput) {
  EXPECT_NE(findEvent(EventType::CompIllegalUdpTable, "x x : 1", 23, 5), nullptr)
      << "all-x inputs shall give an x output";
}

TEST_F(IllegalUdpTableTest, ReportsConflictingOutput) {
  EXPECT_NE(findEvent(EventType::CompIllegalUdpTable, "0 1 : 0", 25, 5), nullptr)
      << "0 1 already gives 1";
}

TEST_F(IllegalUdpTableTest, ReportsWrongFieldCount) {
  EXPECT_NE(findEvent(EventType::CompIllegalUdpTable, "0 : 1", 26, 5), nullptr) << "p1 has two inputs";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
