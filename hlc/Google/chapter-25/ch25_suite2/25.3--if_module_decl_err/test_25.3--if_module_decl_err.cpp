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

// IEEE 1800-2023 Sec 25.3: "modules can neither be declared nor instantiated in interfaces". A
// module_declaration is not a non_port_interface_item (Syntax 25-1), so "module inner_m;" inside
// outer_if (line 26) is a syntax error.
// Source: tests/Google/chapter-25/ch25_suite2/25.3--if_module_decl_err.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IfModuleDeclErrTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3--if_module_decl_err.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IfModuleDeclErrTest, ReportsSyntaxErrorAtModule) {
  // Line 26: "  module inner_m;"
  EXPECT_NE(findEvent(EventType::PaSyntaxError, 26), nullptr);
}

TEST_F(IfModuleDeclErrTest, CompilerReportsASyntaxError) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  EXPECT_GE(m_session->getEventListener()->getStats().m_syntaxes, 1);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
