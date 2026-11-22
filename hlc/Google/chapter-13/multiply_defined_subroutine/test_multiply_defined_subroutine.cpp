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
// interface and a module, whether it repeats a task, a function, or a subroutine of the other kind
// (a task and a function of the same name). u, v, ok, and the extern method e with its out-of-class
// body are legal.
// Arguments: multiply_defined_subroutine.hlc

#include <hlc/Events/EventDefinition.h>
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
  EXPECT_NE(findEvent(EventType::CompMultiplyDefinedTaskFunc, "pt", 21, 3), nullptr) << "task pt declared twice in package p";
}

TEST_F(MultiplyDefinedSubroutineTest, ReportsPackageFunction) {
  EXPECT_NE(findEvent(EventType::CompMultiplyDefinedTaskFunc, "pf", 23, 3), nullptr)
      << "function pf declared twice in package p";
}

TEST_F(MultiplyDefinedSubroutineTest, ReportsClassTask) {
  EXPECT_NE(findEvent(EventType::CompMultiplyDefinedTaskFunc, "ct", 27, 3), nullptr) << "task ct declared twice in class C";
}

TEST_F(MultiplyDefinedSubroutineTest, ReportsInterfaceTask) {
  EXPECT_NE(findEvent(EventType::CompMultiplyDefinedTaskFunc, "it", 34, 3), nullptr) << "task it declared twice in interface I";
}

TEST_F(MultiplyDefinedSubroutineTest, ReportsModuleTask) {
  EXPECT_NE(findEvent(EventType::CompMultiplyDefinedTaskFunc, "t", 38, 3), nullptr) << "task t declared twice in module top";
}

// A task and a function of the same name: the later one is reported, and its previous definition
// (argument 1) is the declaration of the other kind.

TEST_F(MultiplyDefinedSubroutineTest, ReportsPackageFunctionAfterTask) {
  const Event *const event = findEvent(EventType::CompMultiplyDefinedTaskFunc, "qt", 46, 3);
  ASSERT_NE(event, nullptr) << "function qt declared after task qt in package q";
  ASSERT_GE(event->getArguments().size(), 2u);
  EXPECT_EQ(event->getArguments()[1].m_line, 45u) << "previous definition is task qt";
}

TEST_F(MultiplyDefinedSubroutineTest, ReportsPackageTaskAfterFunction) {
  const Event *const event = findEvent(EventType::CompMultiplyDefinedTaskFunc, "qf", 48, 3);
  ASSERT_NE(event, nullptr) << "task qf declared after function qf in package q";
  ASSERT_GE(event->getArguments().size(), 2u);
  EXPECT_EQ(event->getArguments()[1].m_line, 47u) << "previous definition is function qf";
}

TEST_F(MultiplyDefinedSubroutineTest, ReportsClassTaskAfterFunction) {
  const Event *const event = findEvent(EventType::CompMultiplyDefinedTaskFunc, "dm", 52, 3);
  ASSERT_NE(event, nullptr) << "task dm declared after function dm in class D";
  ASSERT_GE(event->getArguments().size(), 2u);
  EXPECT_EQ(event->getArguments()[1].m_line, 51u) << "previous definition is function dm";
}

TEST_F(MultiplyDefinedSubroutineTest, ReportsInterfaceFunctionAfterTask) {
  const Event *const event = findEvent(EventType::CompMultiplyDefinedTaskFunc, "jt", 56, 3);
  ASSERT_NE(event, nullptr) << "function jt declared after task jt in interface J";
  ASSERT_GE(event->getArguments().size(), 2u);
  EXPECT_EQ(event->getArguments()[1].m_line, 55u) << "previous definition is task jt";
}

TEST_F(MultiplyDefinedSubroutineTest, ReportsModuleTaskAfterFunction) {
  const Event *const event = findEvent(EventType::CompMultiplyDefinedTaskFunc, "mf", 60, 3);
  ASSERT_NE(event, nullptr) << "task mf declared after function mf in module m";
  ASSERT_GE(event->getArguments().size(), 2u);
  EXPECT_EQ(event->getArguments()[1].m_line, 59u) << "previous definition is function mf";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
