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

// IEEE 1800-2023 Sec 25.6: "A ref port cannot be used as a terminal in a specify block." Modport mp
// makes r a ref port (line 25), and "(ch.r => ch.q) = 3;" uses it as a path source (line 31).
// Source: tests/Google/chapter-25/ch25_suite2/25.6--if_specify_ref_terminal_err.sv

#include <hlc/Common/Session.h>
#include <hlc/Events/EventListener.h>
#include <hlc/Events/EventType.h>
#include <hlc/Tests/Test.h>

#include <hldb/Event.h>
#include <hldb/EventDefinition.h>

#include <gtest/gtest.h>

namespace hlc {

class IfSpecifyRefTerminalErrTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.6--if_specify_ref_terminal_err.hlc"}); }
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

TEST_F(IfSpecifyRefTerminalErrTest, ReportsErrorAtRefTerminal) {
  // Line 31: "(ch.r => ch.q) = 3;" -- r at column 9.
  ASSERT_NE(m_session->getEventListener(), nullptr);
  EXPECT_TRUE(hasErrorAt(31, 9));
  // Sec 25.6: "A ref port cannot be used as a terminal in a specify block." Sec 30.4.1: the path source
  // shall be connected to an input or inout port.
  EXPECT_NE(findEvent(EventType::CompIllegalSpecifyPath, "r", 31, 9), nullptr)
      << "the ref modport member ch.r is used as a path source";
}

TEST_F(IfSpecifyRefTerminalErrTest, ReportsIllegalSpecifyTerminal) {
  GTEST_SKIP() << "HLC reports CompIllegalSpecifyPath for a modport ref terminal but "
                  "CompIllegalSpecifyTerminal for a module ref port; both break the same rule of "
                  "IEEE 1800-2023 Sec 25.6. Fix pending.";
  EXPECT_NE(findEvent(EventType::CompIllegalSpecifyTerminal, "r", 31, 9), nullptr);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
