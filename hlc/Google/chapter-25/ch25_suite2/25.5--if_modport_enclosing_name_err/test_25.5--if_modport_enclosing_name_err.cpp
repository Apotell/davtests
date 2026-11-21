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

// IEEE 1800-2023 Sec 25.5: "All of the names used in a modport declaration shall be declared by the
// same interface as the modport itself. In particular, the names used shall not be those declared
// by another enclosing interface". x and y are declared by the enclosing interface i (line 25), not
// by illegal_i, yet both modports of illegal_i list them (lines 29-30).
// Source: tests/Google/chapter-25/ch25_suite2/25.5--if_modport_enclosing_name_err.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IfModportEnclosingNameErrTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.5--if_modport_enclosing_name_err.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IfModportEnclosingNameErrTest, InitiatorListsEnclosingX) {
  // Line 29: "modport initiator (input a, b, x, output c, d, y);" -- x at column 36.
  EXPECT_NE(findEvent(EventType::CompModportUndefinedPort, "x", 29, 36), nullptr);
}

TEST_F(IfModportEnclosingNameErrTest, InitiatorListsEnclosingY) {
  // Line 29: y at column 52.
  EXPECT_NE(findEvent(EventType::CompModportUndefinedPort, "y", 29, 52), nullptr);
}

TEST_F(IfModportEnclosingNameErrTest, TargetListsEnclosingX) {
  // Line 30: "modport target (output a, b, x, input c, d, y);" -- x at column 34.
  EXPECT_NE(findEvent(EventType::CompModportUndefinedPort, "x", 30, 34), nullptr);
}

TEST_F(IfModportEnclosingNameErrTest, TargetListsEnclosingY) {
  // Line 30: y at column 49.
  EXPECT_NE(findEvent(EventType::CompModportUndefinedPort, "y", 30, 49), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
