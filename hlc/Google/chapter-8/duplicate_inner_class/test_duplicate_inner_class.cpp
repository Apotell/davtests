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

// IEEE 1800-2023 Sec 3.13: a name may be declared only once in the name space of a scope.
// Two nested classes named inner in one class (IEEE 1800-2023 Sec 3.13). Reported at the second
// declaration (line 22), with the first as the previous definition.
// Arguments: duplicate_inner_class.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class DuplicateInnerClassTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "duplicate_inner_class.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(DuplicateInnerClassTest, ReportsCompMultiplyDefinedInnerClass) {
  EXPECT_NE(findEvent(EventType::CompMultiplyDefinedInnerClass, "inner", 22, 3), nullptr)
      << "class inner is declared twice in class outer";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
