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

// IEEE 1800-2023 Sec 22.4: a file that includes itself never ends.
// Arguments: recursive_include.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class RecursiveIncludeTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "recursive_include.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(RecursiveIncludeTest, ReportsSelfInclude) {
  EXPECT_NE(findEvent(EventType::PpRecursiveIncludeDirective, 19, 1), nullptr) << "the .svh file includes itself";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
