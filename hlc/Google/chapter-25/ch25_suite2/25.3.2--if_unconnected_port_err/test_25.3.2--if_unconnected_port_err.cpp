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

// IEEE 1800-2023 Sec 25.3.2 with Sec 23.3.3.4: an interface port has no default and cannot be
// left open; it shall be connected to an interface instance. "sink u_sink();" (line 32) leaves
// port b unconnected -- the single violation.
// Source: tests/Google/chapter-25/ch25_suite2/25.3.2--if_unconnected_port_err.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Utils.h>
#include <hldb/design.h>
#include <hldb/module.h>
#include <hldb/port.h>
#include <hldb/vpi_user.h>

#include <gtest/gtest.h>

namespace hlc {

class IfUnconnectedPortErrTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3.2--if_unconnected_port_err.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IfUnconnectedPortErrTest, ReportsMissingConnectionForB) {
  // Line 32: "sink u_sink();" with no connection for port b.
  EXPECT_NE(findEvent(EventType::CompMissingArgument, "b", 32), nullptr) << "sink u_sink();";
}

TEST_F(IfUnconnectedPortErrTest, ReportsExactlyOneError) {
  // Lines 24-31 are legal; only line 32 is wrong.
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_EQ(stats.m_errors, 1);
}

TEST_F(IfUnconnectedPortErrTest, SinkPortBIsInterfacePort) {
  // Line 28: the port that needs the connection is an interface port.
  const hldb::Module *const sink = hldb::findByDefName<hldb::Module>("sink", m_design->getAllModules());
  ASSERT_NE(sink, nullptr);
  const hldb::Port *const b = hldb::findByName<hldb::Port>("b", sink->getPorts());
  ASSERT_NE(b, nullptr);
  EXPECT_EQ(b->getPortType(), vpiInterfacePort);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
