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

// IEEE 1800-2023 Sec 6.21: variables declared in a module, interface, program or package outside any
// procedural context are static, so they cannot be declared automatic. static int ps/s and the
// automatic variables of the initial block and of task t are legal.
// Arguments: illegal_declaration_location.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IllegalDeclarationLocationTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "illegal_declaration_location.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IllegalDeclarationLocationTest, ReportsPackageVariable) {
  EXPECT_NE(findEvent(EventType::CompIllegalAutomaticLifetime, "pa", 20, 17), nullptr) << "automatic int pa in package p";
}

TEST_F(IllegalDeclarationLocationTest, ReportsInterfaceVariable) {
  EXPECT_NE(findEvent(EventType::CompIllegalAutomaticLifetime, "ia", 24, 19), nullptr) << "automatic logic ia in interface I";
}

TEST_F(IllegalDeclarationLocationTest, ReportsModuleVariable) {
  EXPECT_NE(findEvent(EventType::CompIllegalAutomaticLifetime, "a", 27, 17), nullptr) << "automatic int a in module top";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
