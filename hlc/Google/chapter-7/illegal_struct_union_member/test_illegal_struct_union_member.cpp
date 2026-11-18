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

// IEEE 1800-2023 Sec 7.2: a void member is legal only in a tagged union. Sec 7.2.1, 7.3.1, 7.3.2:
// the members of a packed structure or union shall be of packed types. Sec 7.3: dynamic and chandle
// types can only be used in tagged unions. The ok*_t types, including packed arrays of a packed typedef, are legal.
// Arguments: illegal_struct_union_member.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IllegalStructUnionMemberTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "illegal_struct_union_member.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IllegalStructUnionMemberTest, ReportsVoidInStruct) {
  EXPECT_NE(findEvent(EventType::CompIllegalStructUnionMember, "v", 23, 25), nullptr) << "void member of a struct";
}

TEST_F(IllegalStructUnionMemberTest, ReportsVoidInUntaggedUnion) {
  EXPECT_NE(findEvent(EventType::CompIllegalStructUnionMember, "v", 24, 24), nullptr)
      << "void member of an untagged union";
}

TEST_F(IllegalStructUnionMemberTest, ReportsRealInPackedStruct) {
  EXPECT_NE(findEvent(EventType::CompIllegalStructUnionMember, "r", 26, 32), nullptr)
      << "real member of a packed struct";
}

TEST_F(IllegalStructUnionMemberTest, ReportsUnpackedStructInPackedStruct) {
  EXPECT_NE(findEvent(EventType::CompIllegalStructUnionMember, "u", 27, 32), nullptr)
      << "unpacked struct member of a packed struct";
}

TEST_F(IllegalStructUnionMemberTest, ReportsRealThroughTypedef) {
  EXPECT_NE(findEvent(EventType::CompIllegalStructUnionMember, "r", 28, 34), nullptr)
      << "real_t is a real";
}

TEST_F(IllegalStructUnionMemberTest, ReportsShortrealInPackedUnion) {
  EXPECT_NE(findEvent(EventType::CompIllegalStructUnionMember, "r", 29, 36), nullptr)
      << "shortreal member of a packed union";
}

TEST_F(IllegalStructUnionMemberTest, ReportsRealInPackedTaggedUnion) {
  EXPECT_NE(findEvent(EventType::CompIllegalStructUnionMember, "r", 30, 38), nullptr)
      << "real member of a packed tagged union";
}

TEST_F(IllegalStructUnionMemberTest, ReportsQueueInUntaggedUnion) {
  EXPECT_NE(findEvent(EventType::CompIllegalStructUnionMember, "q", 31, 23), nullptr)
      << "queue member of an untagged union";
}

TEST_F(IllegalStructUnionMemberTest, ReportsChandleInUntaggedUnion) {
  EXPECT_NE(findEvent(EventType::CompIllegalStructUnionMember, "h", 32, 27), nullptr)
      << "chandle member of an untagged union";
}

TEST_F(IllegalStructUnionMemberTest, ReportsStringInUntaggedUnion) {
  EXPECT_NE(findEvent(EventType::CompIllegalStructUnionMember, "s", 33, 26), nullptr)
      << "string member of an untagged union";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
