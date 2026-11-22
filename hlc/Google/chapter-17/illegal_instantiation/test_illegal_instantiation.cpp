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

// IEEE 1800-2023 Sec 17.3: "It shall be illegal to instantiate checkers in fork-join, fork-join_any,
// or fork-join_none blocks", and "it shall be illegal to instantiate a checker in a procedure of
// another checker". The checker instance in the begin-end block of module m is legal.
// Arguments: illegal_instantiation.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IllegalInstantiationTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "illegal_instantiation.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IllegalInstantiationTest, ReportsCheckerInCheckerProcedure) {
  EXPECT_NE(findEvent(EventType::CompIllegalInstantiation, "i1", 22, 7), nullptr)
      << "checker instantiated in an always_ff of checker outer";
}

TEST_F(IllegalInstantiationTest, ReportsCheckerInFork) {
  EXPECT_NE(findEvent(EventType::CompIllegalInstantiation, "inst", 28, 7), nullptr)
      << "checker instantiated in a fork-join block";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
