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

// IEEE 1800-2023 Sec 5.7.1: white space may separate the size from the base format and the base
// format from the value, but "the apostrophe and base format characters shall not be separated by
// any white space". 8 'hFF and 8'h FF are legal.
// Arguments: illegal_space_in_number.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IllegalSpaceInNumberTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "illegal_space_in_number.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IllegalSpaceInNumberTest, ReportsSpaceAfterApostrophe) {
  EXPECT_NE(findEvent(EventType::PaIllegalSpaceInNumber, "8' hFF", 23, 15), nullptr) << "space between ' and h";
}

TEST_F(IllegalSpaceInNumberTest, ReportsSpacesAroundApostrophe) {
  EXPECT_NE(findEvent(EventType::PaIllegalSpaceInNumber, "8 ' h FF", 24, 16), nullptr) << "space between ' and h";
}

TEST_F(IllegalSpaceInNumberTest, ReportsSpaceAfterSignedMarker) {
  EXPECT_NE(findEvent(EventType::PaIllegalSpaceInNumber, "8's hFF", 25, 15), nullptr) << "space between s and h";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
