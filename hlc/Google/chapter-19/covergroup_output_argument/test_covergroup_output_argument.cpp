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

// IEEE 1800-2023 Sec 19.3: "An output or inout formal argument shall be illegal in a covergroup argument list."
// Arguments: covergroup_output_argument.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class CovergroupOutputArgumentTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "covergroup_output_argument.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(CovergroupOutputArgumentTest, ReportsOutputCovergroupArgument) {
  EXPECT_NE(findEvent(EventType::CompIllegalQualifier, "x", 20, 28), nullptr) << "covergroup output x";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
