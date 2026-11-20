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

// IEEE 1800-2023 Sec 8.18: a local member is visible only within its class, a protected member also
// within the classes derived from it; Sec 8.23: a nested class may access both. In
// 8.18--illegal-member-access.sv the accesses in base itself, in its nested class inner, of
// protected members in derived, and of public members are legal; the five below are not.
// Arguments: illegal_member_access.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IllegalMemberAccessTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "illegal_member_access.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IllegalMemberAccessTest, ReportsLocalPropertyFromSubclass) {
  EXPECT_NE(findEvent(EventType::CompIllegalMemberAccess, "secret", 41, 7), nullptr)
      << "a local property of base used in derived";
}

TEST_F(IllegalMemberAccessTest, ReportsProtectedPropertyFromUnrelatedClass) {
  EXPECT_NE(findEvent(EventType::CompIllegalMemberAccess, "prot", 46, 7), nullptr)
      << "a protected property of base used in an unrelated class";
}

TEST_F(IllegalMemberAccessTest, ReportsLocalMethodFromUnrelatedClass) {
  EXPECT_NE(findEvent(EventType::CompIllegalMemberAccess, "lf", 47, 5), nullptr)
      << "a local method of base called from an unrelated class";
}

TEST_F(IllegalMemberAccessTest, ReportsLocalPropertyFromModule) {
  EXPECT_NE(findEvent(EventType::CompIllegalMemberAccess, "secret", 53, 7), nullptr)
      << "a local property of base used in a module";
}

TEST_F(IllegalMemberAccessTest, ReportsProtectedMethodFromModule) {
  EXPECT_NE(findEvent(EventType::CompIllegalMemberAccess, "pf", 55, 5), nullptr)
      << "a protected method of base called from a module";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
