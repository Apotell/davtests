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

// IEEE 1800-2023 Sec 25.3.2 with Sec 23.3.3.4: a port declared "a_if p" shall be connected to an
// interface instance of the identical type. "sink u_sink(.p(u_b));" passes a b_if (line 37).
// Source: tests/Google/chapter-25/ch25_suite2/25.3.2--if_wrong_bundle_type_err.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <gtest/gtest.h>

namespace hlc {

class IfWrongBundleTypeErrTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3.2--if_wrong_bundle_type_err.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IfWrongBundleTypeErrTest, ReportsWrongInterfaceType) {
  EXPECT_NE(findEvent(EventType::CompIllegalInterfacePortConnection, "u_b", 37), nullptr) << ".p(u_b)";
}

TEST_F(IfWrongBundleTypeErrTest, ReportsExactlyOneError) {
  // Only the connection on line 37 is illegal; the declarations on lines 24-36 are legal.
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 1);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
