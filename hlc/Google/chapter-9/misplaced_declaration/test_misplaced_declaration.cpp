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

// IEEE 1800-2023 Sec 9.3.1 and 9.3.2: in a begin or fork block the block item declarations come
// before the statements. int x = 1; int y; ahead of the statements is legal.
// Arguments: misplaced_declaration.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class MisplacedDeclarationTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "misplaced_declaration.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(MisplacedDeclarationTest, ReportsDeclarationAfterStatementInBegin) {
  EXPECT_NE(findEvent(EventType::CompMisplacedDeclaration, "begin", 23, 9), nullptr) << "int b follows a = 1";
}

TEST_F(MisplacedDeclarationTest, ReportsDeclarationAfterStatementInFork) {
  EXPECT_NE(findEvent(EventType::CompMisplacedDeclaration, "f", 28, 9), nullptr) << "int z follows a = 1 in fork f";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
