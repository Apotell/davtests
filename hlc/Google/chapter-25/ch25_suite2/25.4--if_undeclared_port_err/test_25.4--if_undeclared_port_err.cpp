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

// IEEE 1800-2023 Sec 25.4 with Sec 23.2.2.1: every port of a non-ANSI header shall be declared in
// the body. In "interface und_if(a, b); input a;" port b has no declaration (line 24).
// Source: tests/Google/chapter-25/ch25_suite2/25.4--if_undeclared_port_err.sv

#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IfUndeclaredPortErrTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.4--if_undeclared_port_err.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IfUndeclaredPortErrTest, ReportsUndeclaredPortB) {
  EXPECT_NE(findEvent(EventType::CompUnspecifiedPort, "b", 24), nullptr) << "port b of und_if(a, b)";
}

TEST_F(IfUndeclaredPortErrTest, DeclaredPortAIsNotReported) {
  EXPECT_EQ(findEvent(EventType::CompUnspecifiedPort, "a"), nullptr) << "a has 'input a;'";
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
