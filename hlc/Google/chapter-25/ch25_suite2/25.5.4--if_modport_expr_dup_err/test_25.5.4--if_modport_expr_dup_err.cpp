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

// IEEE 1800-2023 Sec 25.5.4: port identifiers exist in their own name space for each modport list;
// "Once a port identifier has been defined, there shall not be another port definition with this
// same name." Modport A defines P twice (line 26).
// Source: tests/Google/chapter-25/ch25_suite2/25.5.4--if_modport_expr_dup_err.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IfModportExprDupErrTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.5.4--if_modport_expr_dup_err.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IfModportExprDupErrTest, ReportsSecondPortP) {
  // Line 26: "modport A (input .P(a), output .P(b));" -- the second P is at column 35.
  EXPECT_NE(findEvent(EventType::CompDuplicateDeclaration, "P", 26, 35), nullptr);
}

TEST_F(IfModportExprDupErrTest, CompilerReportsAnError) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  EXPECT_GE(m_session->getEventListener()->getStats().m_errors, 1);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
