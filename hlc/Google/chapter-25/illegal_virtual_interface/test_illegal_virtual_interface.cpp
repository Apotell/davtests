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

// IEEE 1800-2023 Sec 25.9: "Virtual interfaces shall not be used as ports, interface items, or as
// members of unions." A virtual interface as a struct member, a module variable or a class property
// is legal.
// Arguments: illegal_virtual_interface.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IllegalVirtualInterfaceTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "illegal_virtual_interface.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IllegalVirtualInterfaceTest, ReportsInterfaceItem) {
  EXPECT_NE(findEvent(EventType::CompIllegalVirtualInterface, "inner", 21, 16), nullptr)
      << "virtual interface declared as an interface item";
}

TEST_F(IllegalVirtualInterfaceTest, ReportsPort) {
  EXPECT_NE(findEvent(EventType::CompIllegalVirtualInterface, "v", 23, 32), nullptr)
      << "virtual interface used as a port";
}

TEST_F(IllegalVirtualInterfaceTest, ReportsUnionMember) {
  EXPECT_NE(findEvent(EventType::CompIllegalVirtualInterface, "v", 26, 32), nullptr)
      << "virtual interface used as a union member";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
