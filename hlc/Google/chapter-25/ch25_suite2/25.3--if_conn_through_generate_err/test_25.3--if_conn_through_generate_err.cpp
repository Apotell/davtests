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

// IEEE 1800-2023 Sec 25.3: "If the actual of an interface port connection is a hierarchical
// reference to an interface or a modport of a hierarchically referenced interface, the hierarchical
// reference shall refer to an interface instance and shall not resolve through an arrayed instance
// or a generate block." ".p(g.u_if)" (line 36) resolves through generate block g (lines 33-35).
// Source: tests/Google/chapter-25/ch25_suite2/25.3--if_conn_through_generate_err.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Event.h>
#include <hldb/EventDefinition.h>

#include <gtest/gtest.h>

namespace hlc {

class IfConnThroughGenerateErrTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3--if_conn_through_generate_err.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }

 protected:
  // True if an error-or-worse event has an argument at line:column.
  static bool hasErrorAt(uint32_t line, uint16_t column) {
    for (const hldb::Event &e : m_session->getEventListener()->getEvents()) {
      const hldb::EventDefinition *const def = e.getDefinition();
      if ((def == nullptr) || (def->m_severity > hldb::EventSeverity::Error)) continue;
      for (const hldb::EventArg &arg : e.getArguments()) {
        if ((arg.m_line == line) && (arg.m_column == column)) return true;
      }
    }
    return false;
  }
};

TEST_F(IfConnThroughGenerateErrTest, ReportsErrorAtActual) {
  GTEST_SKIP() << "HLC accepts an interface port actual that resolves through a generate block; "
                  "should be an error per IEEE 1800-2023 Sec 25.3. Fix pending.";
  // Line 36: "sink u_sink (.p(g.u_if));" -- g.u_if starts at column 19.
  ASSERT_NE(m_session->getEventListener(), nullptr);
  EXPECT_TRUE(hasErrorAt(36, 19));
}

TEST_F(IfConnThroughGenerateErrTest, CompilerReportsAnError) {
  GTEST_SKIP() << "HLC accepts an interface port actual that resolves through a generate block; "
                  "should be an error per IEEE 1800-2023 Sec 25.3. Fix pending.";
  ASSERT_NE(m_session->getEventListener(), nullptr);
  EXPECT_GE(m_session->getEventListener()->getStats().m_errors, 1);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
