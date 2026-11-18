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

// Source encoding (an HLC rule, not an IEEE 1800 one): a non-ASCII character (in a
// comment) is replaced by a space and reported once.
// Reported at line 18, column 16 of non_ascii_comment.sv.
// Arguments: NonAsciiComment.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class EncodingNonAsciiCommentTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "NonAsciiComment.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(EncodingNonAsciiCommentTest, ReportsPpNonAsciiContent) {
  EXPECT_NE(findEvent(EventType::PpNonAsciiContent, 18, 16), nullptr)
      << "a non-ASCII character (in a comment) is replaced by a space and reported once";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
