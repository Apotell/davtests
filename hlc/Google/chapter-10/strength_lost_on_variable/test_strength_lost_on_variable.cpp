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

// IEEE 1800-2023 Sec 6.5 and 10.3.4: a variable holds no strength, so a drive strength given on a
// continuous assignment to a variable or a select of one is lost. A strength on a net (w) and an
// assignment without a strength (u) are not reported.
// Arguments: strength_lost_on_variable.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class StrengthLostOnVariableTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "strength_lost_on_variable.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(StrengthLostOnVariableTest, ReportsVariable) {
  EXPECT_NE(findEvent(EventType::StrengthLostOnVariable, "v", 25, 27), nullptr) << "(strong0, weak1) on variable v";
}

TEST_F(StrengthLostOnVariableTest, ReportsSelectOfVariable) {
  EXPECT_NE(findEvent(EventType::StrengthLostOnVariable, "vb", 26, 25), nullptr) << "(pull0, pull1) on vb[0]";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
