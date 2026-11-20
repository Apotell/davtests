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

// IEEE 1800-2023 Sec 7.8: "It shall be illegal for the index_type of an associative array declaration
// to declare a type". Sec 7.8.5: the index type shall have equality defined, so real, shortreal and
// a type containing one are illegal. int, string, a class, *, and a typedef of an integral struct are
// legal.
// Arguments: illegal_assoc_index_type.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IllegalAssocIndexTypeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "illegal_assoc_index_type.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IllegalAssocIndexTypeTest, ReportsInlineStruct) {
  EXPECT_NE(findEvent(EventType::CompIllegalAssocIndexType, "struct", 23, 10), nullptr)
      << "the index type declares a struct";
}

TEST_F(IllegalAssocIndexTypeTest, ReportsReal) {
  EXPECT_NE(findEvent(EventType::CompIllegalAssocIndexType, "real", 24, 10), nullptr) << "real index type";
}

TEST_F(IllegalAssocIndexTypeTest, ReportsShortreal) {
  EXPECT_NE(findEvent(EventType::CompIllegalAssocIndexType, "shortreal", 25, 10), nullptr)
      << "shortreal index type";
}

TEST_F(IllegalAssocIndexTypeTest, ReportsTypeContainingReal) {
  EXPECT_NE(findEvent(EventType::CompIllegalAssocIndexType, "Rt", 26, 10), nullptr)
      << "Rt contains a real member";
}

TEST_F(IllegalAssocIndexTypeTest, ReportsInlineEnum) {
  EXPECT_NE(findEvent(EventType::CompIllegalAssocIndexType, "enum", 32, 11), nullptr)
      << "the index type declares an enum";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
