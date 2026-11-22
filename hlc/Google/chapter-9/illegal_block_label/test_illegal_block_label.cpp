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

// IEEE 1800-2023 Sec 9.3.5: a block takes either a statement label or a block name, not both.
// A label alone (lbl3: begin ... end : lbl3) or a name alone (begin : nm) is legal.
// Arguments: illegal_block_label.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IllegalBlockLabelTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "illegal_block_label.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IllegalBlockLabelTest, ReportsSameLabelAndName) {
  EXPECT_NE(findEvent(EventType::CompIllegalBlockLabel, "lbl", 20, 24), nullptr) << "lbl: begin : lbl";
}

TEST_F(IllegalBlockLabelTest, ReportsDifferentLabelAndName) {
  EXPECT_NE(findEvent(EventType::CompIllegalBlockLabel, "other", 22, 25), nullptr) << "lbl2: begin : other";
}

TEST_F(IllegalBlockLabelTest, ReportsForkLabelAndName) {
  EXPECT_NE(findEvent(EventType::CompIllegalBlockLabel, "fk", 28, 24), nullptr) << "lbl4: fork : fk";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
