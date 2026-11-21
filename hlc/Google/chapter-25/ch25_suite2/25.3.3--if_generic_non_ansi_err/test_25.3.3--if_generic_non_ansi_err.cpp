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

// IEEE 1800-2023 Sec 25.3.3: a generic interface reference can be declared only in the ANSI list
// of port declarations; the non-ANSI (list of ports) style has no form for it. In
// "module sink(p); interface p; endmodule" the body line "interface p;" can only begin a nested
// interface declaration, which is never closed, so the source is syntactically illegal and port
// p never becomes a generic interface port.
// Source: tests/Google/chapter-25/ch25_suite2/25.3.3--if_generic_non_ansi_err.sv

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

class IfGenericNonAnsiErrTest : public Test {
 public:
  static void SetUpTestSuite() { Compile(__FILE__, {"-f", "25.3.3--if_generic_non_ansi_err.hlc"}); }
  static void TearDownTestSuite() { Shutdown(); }
};

TEST_F(IfGenericNonAnsiErrTest, ReportsSyntaxError) {
  // Lines 24-26: the non-ANSI generic declaration is rejected by the grammar.
  ASSERT_NE(m_session->getEventListener(), nullptr);
  const EventListener::Stats stats = m_session->getEventListener()->getStats();
  EXPECT_GT(stats.m_syntaxes, 0);
  EXPECT_NE(findEvent(EventType::PaSyntaxError), nullptr);
}

TEST_F(IfGenericNonAnsiErrTest, NoGenericInterfacePortIsCreated) {
  // Line 24-25: p must not end up as an interface port of sink.
  const hldb::Module *const sink = hldb::findByDefName<hldb::Module>("sink", m_design->getAllModules());
  if (sink == nullptr) return;  // the syntax error may drop the module entirely, which also satisfies the rule
  const hldb::Port *const p = hldb::findByName<hldb::Port>("p", sink->getPorts());
  if (p != nullptr) EXPECT_NE(p->getPortType(), vpiInterfacePort);
}

}  // namespace hlc

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
