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

// IEEE 1800-2023 Sec 5.12: "If the same attribute name is defined more than once for the same
// language element, the last attribute value shall be used, and a tool can issue a warning that a
// duplicate attribute specification has occurred." keep is given twice on line 20; the warning is
// at the second keep (column 16).
// Arguments: duplicate_attribute.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class DuplicateAttributeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "duplicate_attribute.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(DuplicateAttributeTest, WarnsAttributeGivenTwice) {
  EXPECT_NE(findEvent(EventType::CompDuplicateAttribute, "keep", 20, 16), nullptr)
      << "the attribute keep is given twice on net w";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
