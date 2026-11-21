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

// IEEE 1800-2023 Sec 25.3.3 with Sec 23.3.3.4: a generic interface port accepts any interface
// instance, but only an interface instance. "sink u_sink(.p(v));" (line 29) connects the logic
// variable v -- the single violation.
// Source: tests/Google/chapter-25/ch25_suite2/25.3.3--if_generic_variable_err.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/port.h>
#include <hldb/variable.h>
#include <hldb/vpi_user.h>

#include <gtest/gtest.h>

namespace hlc {

class IfGenericVariableErrTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3.3--if_generic_variable_err.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IfGenericVariableErrTest, ReportsVariableOnGenericPort) {
  // Line 29: ".p(v)".
  EXPECT_NE(findEvent(EventType::CompIllegalInterfacePortConnection, "v", 29), nullptr) << ".p(v)";
}

TEST_F(IfGenericVariableErrTest, ReportsExactlyOneError) {
  // Lines 24-28 are legal; only line 29 is wrong.
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 1);
}

TEST_F(IfGenericVariableErrTest, PIsGenericInterfacePort) {
  // Line 24: "module sink(interface p);".
  const hldb::Module *const sink = hldb::findByDefName<hldb::Module>("sink", m_design->getAllModules());
  ASSERT_NE(sink, nullptr);
  const hldb::Port *const p = hldb::findByName<hldb::Port>("p", sink->getPorts());
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(p->getPortType(), vpiInterfacePort);
}

TEST_F(IfGenericVariableErrTest, VIsVariable) {
  // Line 28: "logic v;" is a variable, not an interface instance.
  const hldb::Module *const top = hldb::findByDefName<hldb::Module>("top", m_design->getAllModules());
  ASSERT_NE(top, nullptr);
  EXPECT_NE(hldb::findByName<hldb::Variable>("v", top->getVariables()), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
