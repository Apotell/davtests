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

// IEEE 1800-2023 Sec 25.5.4: "The port expression shall resolve to a legal expression for the type
// of module port (see 23.3.3)"; a port bound to a constant "could not be an output or inout".
// ".Q(2)" is declared output (line 26).
// Source: tests/Google/chapter-25/ch25_suite2/25.5.4--if_modport_expr_const_output_err.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IfModportExprConstOutputErrTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.5.4--if_modport_expr_const_output_err.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IfModportExprConstOutputErrTest, ReportsConstantOutputQ) {
  // Line 26: "modport A (output .P(r), output .Q(2));" -- the expression 2 is at column 38.
  EXPECT_NE(findEvent(EventType::CompIllegalModportExpression, "Q", 26, 38), nullptr);
}

TEST_F(IfModportExprConstOutputErrTest, CompilerReportsAnError) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  EXPECT_GE(m_session->getEventListener()->getStats().m_errors, 1);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
