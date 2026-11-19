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

// IEEE 1800-2023 Sec 6.23: "when [a type reference] is used in a variable declaration, it shall be
// preceded by the var keyword". var type(a) c, wire type(a) d and var type(a) f are legal.
// Arguments: missing_var_keyword.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class MissingVarKeywordTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "missing_var_keyword.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(MissingVarKeywordTest, ReportsModuleItem) {
  EXPECT_NE(findEvent(EventType::CompMissingVarKeyword, "b", 21, 3), nullptr) << "type(a) b without var";
}

TEST_F(MissingVarKeywordTest, ReportsBlockItem) {
  EXPECT_NE(findEvent(EventType::CompMissingVarKeyword, "e", 25, 5), nullptr) << "type(a) e without var";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
