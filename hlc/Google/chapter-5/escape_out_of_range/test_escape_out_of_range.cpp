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

// IEEE 1800-2023 Sec 5.9.1: \ddd is a character given by 1 to 3 octal digits; "Implementations
// may issue an error if the character represented is greater than \377." "\400" is on line 20.
// Arguments: escape_out_of_range.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class EscapeOutOfRangeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "escape_out_of_range.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(EscapeOutOfRangeTest, ReportsOctalEscapeAbove377) {
  EXPECT_NE(findEvent(EventType::PpEscapeSequenceOutOfRange, "\\400", 20, 16), nullptr)
      << "the octal escape \\400 is greater than \\377";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
