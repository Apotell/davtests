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

// IEEE 1800-2023 Annex A (footnote to parameter_port_declaration): only a parameter in a parameter
// port list may omit its value (NP, NT). L and LT are local parameters, and body_p is one too: a
// parameter in the body of a module that has a parameter port list is local (Sec 6.20.4).
// Arguments: localparam_no_value.hlc

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class LocalparamNoValueTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "localparam_no_value.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(LocalparamNoValueTest, ReportsLocalparamWithoutValue) {
  EXPECT_NE(findEvent(EventType::CompLocalparamNoValue, "L", 20, 18), nullptr)
      << "localparam int L has no value";
}

TEST_F(LocalparamNoValueTest, ReportsLocalTypeParameterWithoutType) {
  EXPECT_NE(findEvent(EventType::CompLocalparamNoValue, "LT", 22, 19), nullptr)
      << "localparam type LT has no type";
}

TEST_F(LocalparamNoValueTest, ReportsBodyParameterWithoutValue) {
  EXPECT_NE(findEvent(EventType::CompLocalparamNoValue, "body_p", 23, 17), nullptr)
      << "body_p is a local parameter with no value";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
