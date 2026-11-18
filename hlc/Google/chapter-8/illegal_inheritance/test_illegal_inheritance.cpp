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

// IEEE 1800-2023 Sec 8.26 and A.1.2: a class extends a class and implements interface classes; an
// interface class extends interface classes only. Sec 8.26.4: "A class shall not implement a type
// parameter, nor shall an interface class extend a type parameter." IB, Good and R are legal.
// Arguments: illegal_inheritance.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IllegalInheritanceTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "illegal_inheritance.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IllegalInheritanceTest, ReportsClassExtendingInterfaceClass) {
  EXPECT_NE(findEvent(EventType::CompIllegalInheritance, "IA", 29, 26), nullptr)
      << "a class extends the interface class IA";
}

TEST_F(IllegalInheritanceTest, ReportsInterfaceClassExtendingClass) {
  EXPECT_NE(findEvent(EventType::CompIllegalInheritance, "Base", 31, 32), nullptr)
      << "an interface class extends the class Base";
}

TEST_F(IllegalInheritanceTest, ReportsClassImplementingClass) {
  EXPECT_NE(findEvent(EventType::CompIllegalInheritance, "Base", 33, 26), nullptr)
      << "a class implements the class Base";
}

TEST_F(IllegalInheritanceTest, ReportsClassImplementingTypeParameter) {
  EXPECT_NE(findEvent(EventType::CompIllegalInheritance, "T", 35, 35), nullptr)
      << "a class implements its type parameter T (Sec 8.26.4)";
}

TEST_F(IllegalInheritanceTest, ReportsInterfaceClassExtendingTypeParameter) {
  EXPECT_NE(findEvent(EventType::CompIllegalInheritance, "T", 37, 42), nullptr)
      << "an interface class extends its type parameter T (Sec 8.26.4)";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
