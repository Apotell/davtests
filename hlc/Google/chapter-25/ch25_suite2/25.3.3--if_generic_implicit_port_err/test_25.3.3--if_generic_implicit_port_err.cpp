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

// IEEE 1800-2023 Sec 25.3.3: "An implicit port cannot be used to reference a generic interface. A
// named port shall be used to reference a generic interface". "memMod mem (.*);" (line 34) leaves
// the generic port sb_intf to the wildcard, even though top has an instance named sb_intf.
// Note: HLC 2026-11 reports CompIllegalExpressionContext at the ".*" here, but it reports the same
// error for the legal form ".*, .a(sb_intf)" (see 25.3.3--if_generic_partial_implicit).
// Source: tests/Google/chapter-25/ch25_suite2/25.3.3--if_generic_implicit_port_err.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Tests/Test.h>

#include <hldb/Event.h>
#include <hldb/EventDefinition.h>

#include <gtest/gtest.h>

namespace hlc {

class IfGenericImplicitPortErrTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3.3--if_generic_implicit_port_err.hlc"}); }
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

TEST_F(IfGenericImplicitPortErrTest, ReportsErrorAtWildcard) {
  // Line 34: "memMod mem (.*);" -- ".*" starts at column 15.
  ASSERT_NE(m_session->getEventListener(), nullptr);
  EXPECT_TRUE(hasErrorAt(34, 15));
}

TEST_F(IfGenericImplicitPortErrTest, CompilerReportsAnError) {
  ASSERT_NE(m_session->getEventListener(), nullptr);
  EXPECT_GE(m_session->getEventListener()->getStats().m_errors, 1);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
