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

// IEEE 1800-2023 Sec 8.4 and 8.6: a property or method is used through a handle of a class (or this),
// so it shall be declared in that class or one of its base classes. f, the inherited bf and bv, the
// extern method e, the property a, and the built-in randomize and srandom are legal.
// Arguments: undefined_class_member.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class UndefinedClassMemberTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "undefined_class_member.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(UndefinedClassMemberTest, ReportsUndefinedMethodThroughThis) {
  EXPECT_NE(findEvent(EventType::CompNoMethodForType, "this", 28, 10), nullptr) << "C and B declare no method g";
}

TEST_F(UndefinedClassMemberTest, ReportsUndefinedMethodThroughHandle) {
  EXPECT_NE(findEvent(EventType::CompNoMethodForType, "c", 41, 7), nullptr) << "C and B declare no method g";
}

TEST_F(UndefinedClassMemberTest, ReportsUndefinedPropertyRead) {
  EXPECT_NE(findEvent(EventType::CompUndefinedMember, "nope", 45, 11), nullptr) << "C and B declare no property nope";
}

TEST_F(UndefinedClassMemberTest, ReportsUndefinedPropertyWrite) {
  EXPECT_NE(findEvent(EventType::CompUndefinedMember, "nope2", 46, 7), nullptr) << "C and B declare no property nope2";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
