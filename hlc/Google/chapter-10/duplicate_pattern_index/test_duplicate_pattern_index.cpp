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

// IEEE 1800-2023 Sec 10.9.1: "It shall be an error to specify the same index more than once in a
// single array pattern expression." '{0:1, 1:2, 0:3} on line 20 gives index 0 twice; it is
// reported at the second 0 (column 29), with the first as the previous position.
// Arguments: duplicate_pattern_index.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class DuplicatePatternIndexTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "duplicate_pattern_index.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(DuplicatePatternIndexTest, ReportsIndexGivenTwice) {
  EXPECT_NE(findEvent(EventType::CompDuplicatePatternKey, "0", 20, 29), nullptr)
      << "index 0 is given twice in the array pattern";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
