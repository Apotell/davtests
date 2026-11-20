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

// IEEE 1800-2023 Sec 3.13: tasks and functions share the name space of their scope, so a name shall
// be declared only once in it. The second declaration is reported, in a package, a class, an
// interface and a module. u, v, ok, and the extern method e with its out-of-class body are legal.
// Arguments: multiply_defined_subroutine.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class MultiplyDefinedSubroutineTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "multiply_defined_subroutine.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(MultiplyDefinedSubroutineTest, ReportsPackageTask) {
  EXPECT_NE(findEvent(EventType::CompMultiplyDefinedTask, "pt", 21, 3), nullptr) << "task pt declared twice in package p";
}

TEST_F(MultiplyDefinedSubroutineTest, ReportsPackageFunction) {
  EXPECT_NE(findEvent(EventType::CompMultiplyDefinedFunction, "pf", 23, 3), nullptr)
      << "function pf declared twice in package p";
}

TEST_F(MultiplyDefinedSubroutineTest, ReportsClassTask) {
  EXPECT_NE(findEvent(EventType::CompMultiplyDefinedTask, "ct", 27, 3), nullptr) << "task ct declared twice in class C";
}

TEST_F(MultiplyDefinedSubroutineTest, ReportsInterfaceTask) {
  EXPECT_NE(findEvent(EventType::CompMultiplyDefinedTask, "it", 34, 3), nullptr) << "task it declared twice in interface I";
}

TEST_F(MultiplyDefinedSubroutineTest, ReportsModuleTask) {
  EXPECT_NE(findEvent(EventType::CompMultiplyDefinedTask, "t", 38, 3), nullptr) << "task t declared twice in module top";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
