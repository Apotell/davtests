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

// IEEE 1800-2023 Sec 5.6.2: keywords are reserved and shall not be used as identifiers.
// "logic module;" on line 20 uses the keyword module as a variable name. The parser reports the
// column of the offending token as it gets it (0-based, 8).
// Arguments: keyword_as_identifier.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class KeywordAsIdentifierTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "keyword_as_identifier.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(KeywordAsIdentifierTest, ReportsKeywordUsedAsName) {
  EXPECT_NE(findEvent(EventType::PaReservedKeyword, "module", 20, 8), nullptr)
      << "the keyword module cannot be a variable name";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
