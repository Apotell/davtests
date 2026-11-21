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

// IEEE 1800-2023 Sec 25.3: "modules can neither be declared nor instantiated in interfaces."
// "leaf u_leaf();" inside holder_if is the violation (line 28).
// Source: tests/Google/chapter-25/ch25_suite2/25.3--if_module_instance_err.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>

#include <gtest/gtest.h>

namespace hlc {

class IfModuleInstanceErrTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3--if_module_instance_err.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IfModuleInstanceErrTest, ReportsModuleInstanceInInterface) {
  EXPECT_NE(findEvent(EventType::CompIllegalInstantiation, "u_leaf", 28), nullptr) << "leaf u_leaf();";
}

TEST_F(IfModuleInstanceErrTest, ReportsExactlyOneError) {
  // Only line 28 is illegal; the top-level "module leaf;" (line 24) is fine.
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 1);
}

TEST_F(IfModuleInstanceErrTest, LeafModuleIsDeclared) {
  // Lines 24-25: a module declared outside the interface is legal.
  EXPECT_NE(hldb::findByDefName<hldb::Module>("leaf", m_design->getAllModules()), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
