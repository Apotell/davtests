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

// IEEE 1800-2023 Sec 22.5.1: "It shall be illegal to redefine a compiler directive as a macro name."
// Arguments: reserved_macro_name.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class ReservedMacroNameTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "reserved_macro_name.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(ReservedMacroNameTest, ReportsTimescaleAsMacroName) {
  EXPECT_NE(findEvent(EventType::PpMacroNameReserved, "timescale", 20, 9), nullptr) << "`define timescale";
}

TEST_F(ReservedMacroNameTest, ReportsLineAsMacroName) {
  EXPECT_NE(findEvent(EventType::PpMacroNameReserved, "__LINE__", 21, 9), nullptr) << "`define __LINE__";
}

TEST_F(ReservedMacroNameTest, ReportsIncludeAsMacroName) {
  EXPECT_NE(findEvent(EventType::PpMacroNameReserved, "include", 22, 9), nullptr) << "`define include";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
